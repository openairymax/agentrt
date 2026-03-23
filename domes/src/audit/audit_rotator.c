/**
 * @file audit_rotator.c
 * @brief 审计日志轮转器实现
 * @author Spharx
 * @date 2024
 */

#include "audit_rotator.h"
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 512

struct audit_rotator {
    char* log_dir;
    char* log_prefix;
    char* current_file;
    FILE* fp;
    size_t max_file_size;
    int max_files;
    size_t current_size;
    domes_mutex_t lock;
};

static const char* event_type_str(audit_event_type_t type) {
    switch (type) {
        case AUDIT_EVENT_PERMISSION: return "permission";
        case AUDIT_EVENT_SANITIZER: return "sanitizer";
        case AUDIT_EVENT_WORKBENCH: return "workbench";
        case AUDIT_EVENT_SYSTEM: return "system";
        case AUDIT_EVENT_CUSTOM: return "custom";
        default: return "unknown";
    }
}

static char* build_filename(const char* dir, const char* prefix, int index) {
    char* path = (char*)domes_mem_alloc(MAX_PATH_LEN);
    if (!path) return NULL;
    
    if (index < 0) {
        snprintf(path, MAX_PATH_LEN, "%s%s%s.log", dir ? dir : "", 
                 dir ? DOMES_PATH_SEP_STR : "", prefix ? prefix : "audit");
    } else {
        snprintf(path, MAX_PATH_LEN, "%s%s%s.%d.log", dir ? dir : "", 
                 dir ? DOMES_PATH_SEP_STR : "", prefix ? prefix : "audit", index);
    }
    
    return path;
}

static int open_current_file(audit_rotator_t* rotator) {
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    domes_mem_free(rotator->current_file);
    rotator->current_file = build_filename(rotator->log_dir, rotator->log_prefix, -1);
    if (!rotator->current_file) return DOMES_ERROR_NO_MEMORY;
    
    if (rotator->log_dir) {
        domes_file_mkdir(rotator->log_dir, true);
    }
    
    rotator->fp = fopen(rotator->current_file, "a");
    if (!rotator->fp) {
        domes_mem_free(rotator->current_file);
        rotator->current_file = NULL;
        return DOMES_ERROR_IO;
    }
    
    rotator->current_size = 0;
    
    domes_file_stat_t st;
    if (domes_file_stat(rotator->current_file, &st) == DOMES_OK) {
        rotator->current_size = (size_t)st.size;
    }
    
    return DOMES_OK;
}

static int rotate_files(audit_rotator_t* rotator) {
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    char* oldest = build_filename(rotator->log_dir, rotator->log_prefix, rotator->max_files - 1);
    if (oldest) {
        domes_file_remove(oldest);
        domes_mem_free(oldest);
    }
    
    for (int i = rotator->max_files - 2; i >= 0; i--) {
        char* old_path = build_filename(rotator->log_dir, rotator->log_prefix, i);
        char* new_path = build_filename(rotator->log_dir, rotator->log_prefix, i + 1);
        
        if (old_path && new_path) {
            domes_file_rename(old_path, new_path);
        }
        
        domes_mem_free(old_path);
        domes_mem_free(new_path);
    }
    
    char* current_new = build_filename(rotator->log_dir, rotator->log_prefix, 0);
    if (current_new && rotator->current_file) {
        domes_file_rename(rotator->current_file, current_new);
    }
    domes_mem_free(current_new);
    
    return open_current_file(rotator);
}

audit_rotator_t* audit_rotator_create(const char* log_dir, const char* log_prefix,
                                       size_t max_file_size, int max_files) {
    audit_rotator_t* rotator = (audit_rotator_t*)domes_mem_alloc(sizeof(audit_rotator_t));
    if (!rotator) return NULL;
    
    memset(rotator, 0, sizeof(audit_rotator_t));
    
    if (log_dir) {
        rotator->log_dir = domes_strdup(log_dir);
        if (!rotator->log_dir) goto error;
    }
    
    if (log_prefix) {
        rotator->log_prefix = domes_strdup(log_prefix);
        if (!rotator->log_prefix) goto error;
    }
    
    rotator->max_file_size = max_file_size > 0 ? max_file_size : 10 * 1024 * 1024;
    rotator->max_files = max_files > 0 ? max_files : 10;
    
    if (domes_mutex_init(&rotator->lock) != DOMES_OK) {
        goto error;
    }
    
    if (open_current_file(rotator) != DOMES_OK) {
        domes_mutex_destroy(&rotator->lock);
        goto error;
    }
    
    return rotator;
    
error:
    domes_mem_free(rotator->log_dir);
    domes_mem_free(rotator->log_prefix);
    domes_mem_free(rotator);
    return NULL;
}

void audit_rotator_destroy(audit_rotator_t* rotator) {
    if (!rotator) return;
    
    domes_mutex_lock(&rotator->lock);
    
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    domes_mem_free(rotator->current_file);
    domes_mem_free(rotator->log_dir);
    domes_mem_free(rotator->log_prefix);
    
    domes_mutex_unlock(&rotator->lock);
    domes_mutex_destroy(&rotator->lock);
    domes_mem_free(rotator);
}

int audit_rotator_write(audit_rotator_t* rotator, const audit_entry_t* entry) {
    if (!rotator || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&rotator->lock);
    
    if (!rotator->fp) {
        domes_mutex_unlock(&rotator->lock);
        return DOMES_ERROR_IO;
    }
    
    if (rotator->current_size >= rotator->max_file_size) {
        if (rotate_files(rotator) != DOMES_OK) {
            domes_mutex_unlock(&rotator->lock);
            return DOMES_ERROR_IO;
        }
    }
    
    char timestamp[32];
    time_t ts = (time_t)(entry->timestamp_ms / 1000);
    struct tm* tm_info = localtime(&ts);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    int written = fprintf(rotator->fp,
        "{\"ts\":\"%s.%03u\",\"type\":\"%s\",\"agent\":\"%s\",\"action\":\"%s\","
        "\"resource\":\"%s\",\"detail\":\"%s\",\"result\":%d}\n",
        timestamp, (unsigned)(entry->timestamp_ms % 1000),
        event_type_str(entry->type),
        entry->agent_id ? entry->agent_id : "",
        entry->action ? entry->action : "",
        entry->resource ? entry->resource : "",
        entry->detail ? entry->detail : "",
        entry->result);
    
    if (written > 0) {
        rotator->current_size += (size_t)written;
        fflush(rotator->fp);
    }
    
    domes_mutex_unlock(&rotator->lock);
    
    return written > 0 ? DOMES_OK : DOMES_ERROR_IO;
}

int audit_rotator_rotate(audit_rotator_t* rotator) {
    if (!rotator) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&rotator->lock);
    int ret = rotate_files(rotator);
    domes_mutex_unlock(&rotator->lock);
    
    return ret;
}

size_t audit_rotator_current_size(audit_rotator_t* rotator) {
    if (!rotator) return 0;
    
    domes_mutex_lock(&rotator->lock);
    size_t size = rotator->current_size;
    domes_mutex_unlock(&rotator->lock);
    
    return size;
}
