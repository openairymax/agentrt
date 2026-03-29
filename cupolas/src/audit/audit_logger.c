/**
 * @file audit_logger.c
 * @brief 审计日志记录器实�?
 * @author Spharx
 * @date 2024
 */

#include "audit.h"
#include "audit_queue.h"
#include "audit_rotator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_QUEUE_SIZE 10000
#define DEFAULT_BATCH_SIZE 100
#define DEFAULT_FLUSH_INTERVAL_MS 1000

struct audit_logger {
    audit_queue_t* queue;
    audit_rotator_t* rotator;
    cupolas_thread_t writer_thread;
    cupolas_atomic32_t running;
    cupolas_atomic64_t total_logged;
    cupolas_atomic64_t total_failed;
    char* log_dir;
    char* log_prefix;
    size_t max_file_size;
    int max_files;
};

static void* audit_writer_thread(void* arg) {
    audit_logger_t* logger = (audit_logger_t*)arg;
    audit_entry_t* batch[DEFAULT_BATCH_SIZE];
    
    while (cupolas_atomic_load32(&logger->running)) {
        size_t actual_count = 0;
        int ret = audit_queue_timed_pop(logger->queue, batch, DEFAULT_BATCH_SIZE, 
                                         DEFAULT_FLUSH_INTERVAL_MS, &actual_count);
        
        if (ret == cupolas_OK && actual_count > 0) {
            for (size_t i = 0; i < actual_count; i++) {
                if (audit_rotator_write(logger->rotator, batch[i]) == cupolas_OK) {
                    cupolas_atomic_add64(&logger->total_logged, 1);
                } else {
                    cupolas_atomic_add64(&logger->total_failed, 1);
                }
                audit_entry_destroy(batch[i]);
            }
        }
    }
    
    size_t remaining = audit_queue_size(logger->queue);
    while (remaining > 0) {
        audit_entry_t* entry = NULL;
        if (audit_queue_try_pop(logger->queue, &entry) == cupolas_OK) {
            if (audit_rotator_write(logger->rotator, entry) == cupolas_OK) {
                cupolas_atomic_add64(&logger->total_logged, 1);
            } else {
                cupolas_atomic_add64(&logger->total_failed, 1);
            }
            audit_entry_destroy(entry);
        }
        remaining = audit_queue_size(logger->queue);
    }
    
    return NULL;
}

audit_logger_t* audit_logger_create(const char* log_dir, const char* log_prefix,
                                     size_t max_file_size, int max_files) {
    audit_logger_t* logger = (audit_logger_t*)cupolas_mem_alloc(sizeof(audit_logger_t));
    if (!logger) return NULL;
    
    memset(logger, 0, sizeof(audit_logger_t));
    
    if (log_dir) {
        logger->log_dir = cupolas_strdup(log_dir);
        if (!logger->log_dir) goto error;
    }
    
    if (log_prefix) {
        logger->log_prefix = cupolas_strdup(log_prefix);
        if (!logger->log_prefix) goto error;
    }
    
    logger->max_file_size = max_file_size > 0 ? max_file_size : 10 * 1024 * 1024;
    logger->max_files = max_files > 0 ? max_files : 10;
    
    logger->queue = audit_queue_create(DEFAULT_QUEUE_SIZE);
    if (!logger->queue) goto error;
    
    logger->rotator = audit_rotator_create(log_dir, log_prefix, max_file_size, max_files);
    if (!logger->rotator) goto error;
    
    cupolas_atomic_store32(&logger->running, 1);
    
    if (cupolas_thread_create(&logger->writer_thread, audit_writer_thread, logger) != cupolas_OK) {
        goto error;
    }
    
    return logger;
    
error:
    if (logger->queue) audit_queue_destroy(logger->queue);
    if (logger->rotator) audit_rotator_destroy(logger->rotator);
    cupolas_mem_free(logger->log_dir);
    cupolas_mem_free(logger->log_prefix);
    cupolas_mem_free(logger);
    return NULL;
}

void audit_logger_destroy(audit_logger_t* logger) {
    if (!logger) return;
    
    cupolas_atomic_store32(&logger->running, 0);
    audit_queue_shutdown(logger->queue, false);
    cupolas_thread_join(logger->writer_thread, NULL);
    
    audit_queue_destroy(logger->queue);
    audit_rotator_destroy(logger->rotator);
    
    cupolas_mem_free(logger->log_dir);
    cupolas_mem_free(logger->log_prefix);
    cupolas_mem_free(logger);
}

int audit_logger_log(audit_logger_t* logger, audit_event_type_t type,
                      const char* agent_id, const char* action,
                      const char* resource, const char* detail, int result) {
    if (!logger) return cupolas_ERROR_INVALID_ARG;
    
    audit_entry_t* entry = audit_entry_create(type, agent_id, action, resource, detail, result);
    if (!entry) return cupolas_ERROR_NO_MEMORY;
    
    int ret = audit_queue_try_push(logger->queue, entry);
    if (ret != cupolas_OK) {
        audit_entry_destroy(entry);
        cupolas_atomic_add64(&logger->total_failed, 1);
    }
    
    return ret;
}

int audit_logger_log_permission(audit_logger_t* logger, const char* agent_id,
                                 const char* action, const char* resource, int allowed) {
    return audit_logger_log(logger, AUDIT_EVENT_PERMISSION, agent_id, action, 
                            resource, NULL, allowed);
}

int audit_logger_log_sanitizer(audit_logger_t* logger, const char* agent_id,
                                const char* input, const char* output, int passed) {
    return audit_logger_log(logger, AUDIT_EVENT_SANITIZER, agent_id, "sanitize",
                            input, output, passed);
}

int audit_logger_log_workbench(audit_logger_t* logger, const char* agent_id,
                                const char* command, int exit_code) {
    return audit_logger_log(logger, AUDIT_EVENT_WORKBENCH, agent_id, "execute",
                            command, NULL, exit_code);
}

void audit_logger_flush(audit_logger_t* logger) {
    if (!logger) return;
    
    while (audit_queue_size(logger->queue) > 0) {
        cupolas_sleep_ms(10);
    }
}

void audit_logger_stats(audit_logger_t* logger, uint64_t* total_logged, uint64_t* total_failed) {
    if (!logger) {
        if (total_logged) *total_logged = 0;
        if (total_failed) *total_failed = 0;
        return;
    }
    
    if (total_logged) *total_logged = cupolas_atomic_load64(&logger->total_logged);
    if (total_failed) *total_failed = cupolas_atomic_load64(&logger->total_failed);
}
