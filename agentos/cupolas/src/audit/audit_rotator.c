/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_rotator.c - Audit Log Rotator Implementation
 */

/**
 * @file audit_rotator.c
 * @brief Audit Log Rotator Implementation
 * @author Spharx AgentOS Team
 * @date 2024
 */

#include "audit_rotator.h"
#include "utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 512
#define MAX_JSON_ESCAPE_LEN 4096

static size_t json_escape(const char* src, char* dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) {
        if (dst && dst_size > 0) dst[0] = '\0';
        return 0;
    }
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
        switch (src[i]) {
            case '"':  dst[j++] = '\\'; dst[j++] = '"'; break;
            case '\\': dst[j++] = '\\'; dst[j++] = '\\'; break;
            case '\n': dst[j++] = '\\'; dst[j++] = 'n'; break;
            case '\r': dst[j++] = '\\'; dst[j++] = 'r'; break;
            case '\t': dst[j++] = '\\'; dst[j++] = 't'; break;
            case '\b': dst[j++] = '\\'; dst[j++] = 'b'; break;
            case '\f': dst[j++] = '\\'; dst[j++] = 'f'; break;
            default:
                if ((unsigned char)src[i] < 0x20) {
                    j += snprintf(dst + j, dst_size - j, "\\u%04x", (unsigned char)src[i]);
                } else {
                    dst[j++] = src[i];
                }
                break;
        }
    }
    dst[j] = '\0';
    return j;
}

struct audit_rotator {
    char* log_dir;
    char* log_prefix;
    char* current_file;
    FILE* fp;
    size_t max_file_size;
    int max_files;
    size_t current_size;
    cupolas_mutex_t lock;
};

static const char* cupolas_audit_event_type_str(audit_event_type_t type) {
    switch (type) {
        case AUDIT_EVENT_PERMISSION: return "permission";
        case AUDIT_EVENT_SANITIZER: return "sanitizer";
        case AUDIT_EVENT_WORKBENCH: return "workbench";
        case AUDIT_EVENT_SYSTEM: return "system";
        case AUDIT_EVENT_CUSTOM: return "custom";
        default: return "unknown";
    }
}

static char* cupolas_audit_build_filename(const char* dir, const char* prefix, int index) {
    char* path = (char*)cupolas_mem_alloc(MAX_PATH_LEN);
    if (!path) return NULL;
    
    if (index < 0) {
        snprintf(path, MAX_PATH_LEN, "%s%s%s.log", dir ? dir : "", 
                 dir ? cupolas_PATH_SEP_STR : "", prefix ? prefix : "audit");
    } else {
        snprintf(path, MAX_PATH_LEN, "%s%s%s.%d.log", dir ? dir : "", 
                 dir ? cupolas_PATH_SEP_STR : "", prefix ? prefix : "audit", index);
    }
    
    return path;
}

static int cupolas_audit_open_current_file(audit_rotator_t* rotator) {
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    cupolas_mem_free(rotator->current_file);
    rotator->current_file = cupolas_audit_build_filename(rotator->log_dir, rotator->log_prefix, -1);
    if (!rotator->current_file) return cupolas_ERROR_NO_MEMORY;
    
    if (rotator->log_dir) {
        cupolas_file_mkdir(rotator->log_dir, true);
    }
    
    rotator->fp = fopen(rotator->current_file, "a");
    if (!rotator->fp) {
        cupolas_mem_free(rotator->current_file);
        rotator->current_file = NULL;
        return cupolas_ERROR_IO;
    }
    
    rotator->current_size = 0;
    
    cupolas_file_stat_t st;
    if (cupolas_file_stat(rotator->current_file, &st) == cupolas_OK) {
        rotator->current_size = (size_t)st.size;
    }
    
    return cupolas_OK;
}

static int cupolas_audit_rotate_files(audit_rotator_t* rotator) {
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    char* oldest = cupolas_audit_build_filename(rotator->log_dir, rotator->log_prefix, rotator->max_files - 1);
    if (oldest) {
        cupolas_file_remove(oldest);
        cupolas_mem_free(oldest);
    }
    
    for (int i = rotator->max_files - 2; i >= 0; i--) {
        char* old_path = cupolas_audit_build_filename(rotator->log_dir, rotator->log_prefix, i);
        char* new_path = cupolas_audit_build_filename(rotator->log_dir, rotator->log_prefix, i + 1);
        
        if (old_path && new_path) {
            cupolas_file_rename(old_path, new_path);
        }
        
        cupolas_mem_free(old_path);
        cupolas_mem_free(new_path);
    }
    
    char* current_new = cupolas_audit_build_filename(rotator->log_dir, rotator->log_prefix, 0);
    if (current_new && rotator->current_file) {
        cupolas_file_rename(rotator->current_file, current_new);
    }
    cupolas_mem_free(current_new);
    
    return cupolas_audit_open_current_file(rotator);
}

audit_rotator_t* audit_rotator_create(const char* log_dir, const char* log_prefix,
                                       size_t max_file_size, int max_files) {
    audit_rotator_t* rotator = (audit_rotator_t*)cupolas_mem_alloc(sizeof(audit_rotator_t));
    if (!rotator) return NULL;
    
    memset(rotator, 0, sizeof(audit_rotator_t));
    
    if (log_dir) {
        rotator->log_dir = cupolas_strdup(log_dir);
        if (!rotator->log_dir) goto error;
    }
    
    if (log_prefix) {
        rotator->log_prefix = cupolas_strdup(log_prefix);
        if (!rotator->log_prefix) goto error;
    }
    
    rotator->max_file_size = max_file_size > 0 ? max_file_size : 10 * 1024 * 1024;
    rotator->max_files = max_files > 0 ? max_files : 10;
    
    if (cupolas_mutex_init(&rotator->lock) != cupolas_OK) {
        goto error;
    }
    
    if (cupolas_audit_open_current_file(rotator) != cupolas_OK) {
        cupolas_mutex_destroy(&rotator->lock);
        goto error;
    }
    
    return rotator;
    
error:
    cupolas_mem_free(rotator->log_dir);
    cupolas_mem_free(rotator->log_prefix);
    cupolas_mem_free(rotator);
    return NULL;
}

void audit_rotator_destroy(audit_rotator_t* rotator) {
    if (!rotator) return;
    
    cupolas_mutex_lock(&rotator->lock);
    
    if (rotator->fp) {
        fclose(rotator->fp);
        rotator->fp = NULL;
    }
    
    cupolas_mem_free(rotator->current_file);
    cupolas_mem_free(rotator->log_dir);
    cupolas_mem_free(rotator->log_prefix);
    
    cupolas_mutex_unlock(&rotator->lock);
    cupolas_mutex_destroy(&rotator->lock);
    cupolas_mem_free(rotator);
}

int audit_rotator_write(audit_rotator_t* rotator, const audit_entry_t* entry) {
    if (!rotator || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&rotator->lock);
    
    if (!rotator->fp) {
        cupolas_mutex_unlock(&rotator->lock);
        return cupolas_ERROR_IO;
    }
    
    if (rotator->current_size >= rotator->max_file_size) {
        if (cupolas_audit_rotate_files(rotator) != cupolas_OK) {
            cupolas_mutex_unlock(&rotator->lock);
            return cupolas_ERROR_IO;
        }
    }
    
    char timestamp[32];
    time_t ts = (time_t)(entry->timestamp_ms / 1000);
    struct tm* tm_info = localtime(&ts);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char esc_agent[MAX_JSON_ESCAPE_LEN];
    char esc_action[MAX_JSON_ESCAPE_LEN];
    char esc_resource[MAX_JSON_ESCAPE_LEN];
    char esc_detail[MAX_JSON_ESCAPE_LEN];
    
    json_escape(entry->agent_id ? entry->agent_id : "", esc_agent, sizeof(esc_agent));
    json_escape(entry->action ? entry->action : "", esc_action, sizeof(esc_action));
    json_escape(entry->resource ? entry->resource : "", esc_resource, sizeof(esc_resource));
    json_escape(entry->detail ? entry->detail : "", esc_detail, sizeof(esc_detail));
    
    int written = fprintf(rotator->fp,
        "{\"ts\":\"%s.%03u\",\"type\":\"%s\",\"agent\":\"%s\",\"action\":\"%s\","
        "\"resource\":\"%s\",\"detail\":\"%s\",\"result\":%d}\n",
        timestamp, (unsigned)(entry->timestamp_ms % 1000),
        cupolas_audit_event_type_str(entry->type),
        esc_agent, esc_action, esc_resource, esc_detail,
        entry->result);
    
    if (written > 0) {
        rotator->current_size += (size_t)written;
        fflush(rotator->fp);
    }
    
    cupolas_mutex_unlock(&rotator->lock);
    
    return written > 0 ? cupolas_OK : cupolas_ERROR_IO;
}

int audit_rotator_rotate(audit_rotator_t* rotator) {
    if (!rotator) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&rotator->lock);
    int ret = cupolas_audit_rotate_files(rotator);
    cupolas_mutex_unlock(&rotator->lock);
    
    return ret;
}

size_t audit_rotator_current_size(audit_rotator_t* rotator) {
    if (!rotator) return 0;
    
    cupolas_mutex_lock(&rotator->lock);
    size_t size = rotator->current_size;
    cupolas_mutex_unlock(&rotator->lock);
    
    return size;
}
