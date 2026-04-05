/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_overflow.c - Audit Overflow Handler: Disk Spillover for Queue Backpressure
 */

#include "audit_overflow.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#define DEFAULT_OVERFLOW_DIR "/var/log/cupolas"
#define DEFAULT_MAX_FILE_SIZE_MB 100
#define DEFAULT_FLUSH_INTERVAL_MS 1000
#define MAX_FILENAME_LEN 256
#define JSON_ENTRY_MAX_LEN 2048

struct overflow_handler {
    char overflow_dir[512];
    size_t max_file_size_bytes;
    uint32_t flush_interval_ms;
    FILE* current_file;
    char current_filename[MAX_FILENAME_LEN];
    uint64_t current_file_size;
    uint64_t events_written;
    uint64_t disk_write_errors;
    uint64_t total_events_received;
    cupolas_mutex_t lock;
    cupolas_thread_t flush_thread;
    volatile bool running;
    overflow_callback_t callback;
    void* callback_user_data;
    overflow_stats_t stats;
};

static const char* overflow_level_strings[] = {
    "NORMAL",
    "WARNING",
    "CRITICAL",
    "SPILLING"
};

static void get_timestamp_filename(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
#ifdef cupolas_PLATFORM_WINDOWS
    struct tm* tm_info = localtime(&now);
#else
    struct tm tm_buf;
    struct tm* tm_info = localtime_r(&now, &tm_buf);
#endif
    if (tm_info) {
        strftime(buffer, buffer_size, "audit_overflow_%Y%m%d_%H%M%S.log", tm_info);
    } else {
        snprintf(buffer, buffer_size, "audit_overflow_unknown.log");
    }
}

static int ensure_overflow_dir(const char* dir) {
    if (!dir) return -1;
    
#ifdef cupolas_PLATFORM_WINDOWS
    return mkdir(dir);
#else
    return mkdir(dir, 0755);
#endif
}

static FILE* open_new_overflow_file(overflow_handler_t* handler) {
    if (handler->current_file) {
        fclose(handler->current_file);
        handler->current_file = NULL;
    }
    
    char filepath[768];
    snprintf(filepath, sizeof(filepath), "%s/%s",
             handler->overflow_dir, handler->current_filename);
    
    handler->current_file = fopen(filepath, "a");
    if (!handler->current_file) {
        handler->disk_write_errors++;
        return NULL;
    }
    
    handler->current_file_size = 0;
    handler->events_written = 0;
    
    return handler->current_file;
}

static void json_escape_string(const char* src, char* dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) {
        if (dst && dst_size > 0) *dst = '\0';
        return;
    }
    
    size_t pos = 0;
    while (*src && pos < dst_size - 2) {
        switch (*src) {
            case '"':
                if (pos + 2 >= dst_size - 1) goto done;
                dst[pos++] = '\\'; dst[pos++] = '"';
                break;
            case '\\':
                if (pos + 2 >= dst_size - 1) goto done;
                dst[pos++] = '\\'; dst[pos++] = '\\';
                break;
            case '\n':
                if (pos + 2 >= dst_size - 1) goto done;
                dst[pos++] = '\\'; dst[pos++] = 'n';
                break;
            case '\r':
                if (pos + 2 >= dst_size - 1) goto done;
                dst[pos++] = '\\'; dst[pos++] = 'r';
                break;
            case '\t':
                if (pos + 2 >= dst_size - 1) goto done;
                dst[pos++] = '\\'; dst[pos++] = 't';
                break;
            default:
                if ((unsigned char)*src < 0x20) {
                    if (pos + 4 >= dst_size - 1) goto done;
                    pos += snprintf(dst + pos, dst_size - pos, "\\u%04x", (unsigned char)*src);
                } else {
                    dst[pos++] = *src;
                }
                break;
        }
        src++;
    }
done:
    dst[pos] = '\0';
}

static int write_entry_to_file(FILE* file, audit_entry_t* entry) {
    if (!file || !entry) return -1;
    
    char agent_escaped[256], action_escaped[256], resource_escaped[256], detail_escaped[512];
    
    json_escape_string(entry->agent_id ? entry->agent_id : "", 
                      agent_escaped, sizeof(agent_escaped));
    json_escape_string(entry->action ? entry->action : "", 
                      action_escaped, sizeof(action_escaped));
    json_escape_string(entry->resource ? entry->resource : "", 
                      resource_escaped, sizeof(resource_escaped));
    json_escape_string(entry->detail ? entry->detail : "", 
                      detail_escaped, sizeof(detail_escaped));
    
    char json_buffer[JSON_ENTRY_MAX_LEN];
    int len = snprintf(json_buffer, sizeof(json_buffer),
        "{\"ts\":%llu,\"type\":%d,\"agent\":\"%s\",\"action\":\"%s\",\"resource\":\"%s\",\"detail\":\"%s\",\"result\":%d}\n",
        (unsigned long long)entry->timestamp_ms,
        entry->type,
        agent_escaped,
        action_escaped,
        resource_escaped,
        detail_escaped,
        entry->result);
    
    if (len < 0 || (size_t)len >= sizeof(json_buffer)) {
        return -1;
    }
    
    if (fwrite(json_buffer, 1, len, file) != (size_t)len) {
        return -1;
    }
    
    return len;
}

static void* flush_thread_func(void* arg) {
    overflow_handler_t* handler = (overflow_handler_t*)arg;
    
    while (handler->running) {
        cupolas_sleep_ms(handler->flush_interval_ms);
        
        cupolas_mutex_lock(&handler->lock);
        
        if (handler->current_file) {
            fflush(handler->current_file);
            handler->stats.last_flush_time_ms = cupolas_time_ms();
        }
        
        cupolas_mutex_unlock(&handler->lock);
    }
    
    return NULL;
}

overflow_handler_t* overflow_handler_create(const char* overflow_dir,
                                           size_t max_file_size_mb,
                                           uint32_t flush_interval_ms) {
    overflow_handler_t* handler = (overflow_handler_t*)cupolas_mem_alloc(sizeof(overflow_handler_t));
    if (!handler) return NULL;
    
    memset(handler, 0, sizeof(overflow_handler_t));
    
    snprintf(handler->overflow_dir, sizeof(handler->overflow_dir),
             overflow_dir ? overflow_dir : DEFAULT_OVERFLOW_DIR);
    handler->max_file_size_bytes = (max_file_size_mb > 0 ? max_file_size_mb : DEFAULT_MAX_FILE_SIZE_MB) * 1024 * 1024;
    handler->flush_interval_ms = flush_interval_ms > 0 ? flush_interval_ms : DEFAULT_FLUSH_INTERVAL_MS;
    
    if (ensure_overflow_dir(handler->overflow_dir) != 0) {
        snprintf(handler->overflow_dir, sizeof(handler->overflow_dir), "/tmp/cupolas_audit");
        ensure_overflow_dir(handler->overflow_dir);
    }
    
    if (cupolas_mutex_init(&handler->lock) != cupolas_OK) {
        cupolas_mem_free(handler);
        return NULL;
    }
    
    get_timestamp_filename(handler->current_filename, sizeof(handler->current_filename));
    
    if (open_new_overflow_file(handler) == NULL) {
        cupolas_mutex_destroy(&handler->lock);
        cupolas_mem_free(handler);
        return NULL;
    }
    
    handler->running = true;
    if (cupolas_thread_create(&handler->flush_thread, flush_thread_func, handler) != cupolas_OK) {
        handler->running = false;
        fclose(handler->current_file);
        cupolas_mutex_destroy(&handler->lock);
        cupolas_mem_free(handler);
        return NULL;
    }
    
    return handler;
}

void overflow_handler_destroy(overflow_handler_t* handler) {
    if (!handler) return;
    
    handler->running = false;
    
    if (handler->flush_thread) {
        cupolas_thread_join(handler->flush_thread, NULL);
    }
    
    cupolas_mutex_lock(&handler->lock);
    
    if (handler->current_file) {
        fflush(handler->current_file);
        fclose(handler->current_file);
        handler->current_file = NULL;
    }
    
    cupolas_mutex_unlock(&handler->lock);
    
    cupolas_mutex_destroy(&handler->lock);
    cupolas_mem_free(handler);
}

int overflow_handler_write(overflow_handler_t* handler, audit_entry_t* entry) {
    if (!handler || !entry) return -1;
    
    cupolas_mutex_lock(&handler->lock);
    
    handler->total_events_received++;
    
    if (!handler->current_file ||
        handler->current_file_size >= handler->max_file_size_bytes) {
        if (open_new_overflow_file(handler) == NULL) {
            cupolas_mutex_unlock(&handler->lock);
            return -1;
        }
    }
    
    int written = write_entry_to_file(handler->current_file, entry);
    if (written < 0) {
        handler->disk_write_errors++;
        cupolas_mutex_unlock(&handler->lock);
        return -1;
    }
    
    handler->current_file_size += written;
    handler->events_written++;
    handler->stats.events_written_to_disk++;
    
    cupolas_mutex_unlock(&handler->lock);
    
    return 0;
}

void overflow_handler_flush(overflow_handler_t* handler) {
    if (!handler) return;
    
    cupolas_mutex_lock(&handler->lock);
    
    if (handler->current_file) {
        fflush(handler->current_file);
        handler->stats.last_flush_time_ms = cupolas_time_ms();
    }
    
    cupolas_mutex_unlock(&handler->lock);
}

void overflow_handler_get_stats(overflow_handler_t* handler, overflow_stats_t* stats) {
    if (!handler || !stats) return;
    
    cupolas_mutex_lock(&handler->lock);
    
    stats->total_events_received = handler->total_events_received;
    stats->events_written_to_disk = handler->stats.events_written_to_disk;
    stats->events_dropped = handler->stats.events_dropped;
    stats->disk_write_errors = handler->disk_write_errors;
    stats->last_flush_time_ms = handler->stats.last_flush_time_ms;
    
    cupolas_mutex_unlock(&handler->lock);
}

void overflow_handler_reset_stats(overflow_handler_t* handler) {
    if (!handler) return;
    
    cupolas_mutex_lock(&handler->lock);
    
    handler->total_events_received = 0;
    handler->stats.events_written_to_disk = 0;
    handler->stats.events_dropped = 0;
    handler->disk_write_errors = 0;
    
    cupolas_mutex_unlock(&handler->lock);
}

overflow_level_t overflow_handler_check_level(size_t current_size, size_t max_size) {
    if (max_size == 0) return OVERFLOW_LEVEL_SPILLING;
    
    double ratio = (double)current_size / (double)max_size;
    
    if (ratio >= 1.0) return OVERFLOW_LEVEL_SPILLING;
    if (ratio >= 0.95) return OVERFLOW_LEVEL_CRITICAL;
    if (ratio >= 0.80) return OVERFLOW_LEVEL_WARNING;
    
    return OVERFLOW_LEVEL_NORMAL;
}

int overflow_handler_set_callback(overflow_handler_t* handler,
                                  overflow_callback_t callback,
                                  void* user_data) {
    if (!handler) return -1;
    
    handler->callback = callback;
    handler->callback_user_data = user_data;
    
    return 0;
}

struct audit_queue_ex {
    audit_queue_t* queue;
    overflow_handler_t* overflow;
    size_t max_size;
    overflow_callback_t overflow_callback;
    void* callback_user_data;
    cupolas_atomic64_t total_dropped;
    overflow_level_t current_level;
    cupolas_mutex_t level_lock;
};

audit_queue_ex_t* audit_queue_ex_create(size_t max_size,
                                         const char* overflow_dir,
                                         size_t max_file_size_mb) {
    audit_queue_ex_t* queue_ex = (audit_queue_ex_t*)cupolas_mem_alloc(sizeof(audit_queue_ex_t));
    if (!queue_ex) return NULL;
    
    memset(queue_ex, 0, sizeof(audit_queue_ex_t));
    
    queue_ex->queue = audit_queue_create(max_size);
    if (!queue_ex->queue) {
        cupolas_mem_free(queue_ex);
        return NULL;
    }
    
    queue_ex->max_size = max_size;
    
    if (overflow_dir) {
        queue_ex->overflow = overflow_handler_create(overflow_dir, max_file_size_mb, 1000);
    }
    
    if (cupolas_mutex_init(&queue_ex->level_lock) != cupolas_OK) {
        if (queue_ex->overflow) overflow_handler_destroy(queue_ex->overflow);
        audit_queue_destroy(queue_ex->queue);
        cupolas_mem_free(queue_ex);
        return NULL;
    }
    
    return queue_ex;
}

void audit_queue_ex_destroy(audit_queue_ex_t* queue) {
    if (!queue) return;
    
    if (queue->overflow) {
        overflow_handler_destroy(queue->overflow);
    }
    
    audit_queue_destroy(queue->queue);
    
    cupolas_mutex_destroy(&queue->level_lock);
    
    cupolas_mem_free(queue);
}

int audit_queue_ex_push(audit_queue_ex_t* queue, audit_entry_t* entry) {
    return audit_queue_ex_push_with_callback(queue, entry, NULL, NULL);
}

int audit_queue_ex_push_with_callback(audit_queue_ex_t* queue,
                                       audit_entry_t* entry,
                                       overflow_callback_t callback,
                                       void* user_data) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    overflow_callback_t cb = callback ? callback : queue->overflow_callback;
    void* cb_user_data = user_data ? user_data : queue->callback_user_data;
    
    int result = audit_queue_try_push(queue->queue, entry);
    
    if (result == cupolas_ERROR_WOULD_BLOCK) {
        overflow_level_t level = overflow_handler_check_level(
            audit_queue_size(queue->queue), queue->max_size);
        
        if (level >= OVERFLOW_LEVEL_CRITICAL && queue->overflow) {
            overflow_handler_write(queue->overflow, entry);
            audit_entry_destroy(entry);
            
            cupolas_mutex_lock(&queue->level_lock);
            queue->current_level = level;
            cupolas_mutex_unlock(&queue->level_lock);
            
            if (cb) {
                cb(level, audit_queue_size(queue->queue), queue->max_size, cb_user_data);
            }
            
            return 0;
        } else {
            cupolas_atomic_add64(&queue->total_dropped, 1);
            
            cupolas_mutex_lock(&queue->level_lock);
            queue->current_level = level;
            cupolas_mutex_unlock(&queue->level_lock);
            
            if (cb) {
                cb(level, audit_queue_size(queue->queue), queue->max_size, cb_user_data);
            }
            
            return cupolas_ERROR_WOULD_BLOCK;
        }
    }
    
    overflow_level_t level = overflow_handler_check_level(
        audit_queue_size(queue->queue), queue->max_size);
    
    cupolas_mutex_lock(&queue->level_lock);
    overflow_level_t prev_level = queue->current_level;
    if (level != prev_level) {
        queue->current_level = level;
        cupolas_mutex_unlock(&queue->level_lock);
        
        if (cb && level > prev_level) {
            cb(level, audit_queue_size(queue->queue), queue->max_size, cb_user_data);
        }
    } else {
        cupolas_mutex_unlock(&queue->level_lock);
    }
    
    return result;
}

int audit_queue_ex_pop(audit_queue_ex_t* queue, audit_entry_t** entry) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    return audit_queue_pop(queue->queue, entry);
}

int audit_queue_ex_pop_batch(audit_queue_ex_t* queue,
                               audit_entry_t** entries,
                               size_t max_count,
                               size_t* actual_count) {
    if (!queue || !entries || !actual_count) return cupolas_ERROR_INVALID_ARG;
    
    return audit_queue_pop_batch(queue->queue, entries, max_count, actual_count);
}

void audit_queue_ex_shutdown(audit_queue_ex_t* queue, bool wait_empty) {
    if (!queue) return;
    
    audit_queue_shutdown(queue->queue, wait_empty);
}

size_t audit_queue_ex_size(audit_queue_ex_t* queue) {
    if (!queue) return 0;
    
    return audit_queue_size(queue->queue);
}

void audit_queue_ex_get_stats(audit_queue_ex_t* queue,
                               uint64_t* pushed,
                               uint64_t* popped,
                               uint64_t* spilled,
                               uint64_t* dropped) {
    if (!queue) return;
    
    if (pushed || popped) {
        uint64_t p, pp;
        audit_queue_stats(queue->queue, &p, &pp);
        if (pushed) *pushed = p;
        if (popped) *popped = pp;
    }
    
    if (spilled && queue->overflow) {
        overflow_stats_t stats;
        overflow_handler_get_stats(queue->overflow, &stats);
        *spilled = stats.events_written_to_disk;
    } else if (spilled) {
        *spilled = 0;
    }
    
    if (dropped) {
        *dropped = cupolas_atomic_load64(&queue->total_dropped);
    }
}

overflow_level_t audit_queue_ex_get_overflow_level(audit_queue_ex_t* queue) {
    if (!queue) return OVERFLOW_LEVEL_NORMAL;
    
    cupolas_mutex_lock(&queue->level_lock);
    overflow_level_t level = queue->current_level;
    cupolas_mutex_unlock(&queue->level_lock);
    
    return level;
}

int audit_queue_ex_set_overflow_callback(audit_queue_ex_t* queue,
                                          overflow_callback_t callback,
                                          void* user_data) {
    if (!queue) return -1;
    
    queue->overflow_callback = callback;
    queue->callback_user_data = user_data;
    
    return 0;
}
