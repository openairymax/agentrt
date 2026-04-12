/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file logging.c
 * @brief 结构化日志系统实现
 *
 * 功能：
 * 1. 结构化日志记录（JSON格式）
 * 2. 日志级别过滤
 * 3. 多输出目标（文件、回调、环形缓冲区）
 * 4. 日志上下文传播
 * 5. 线程安全
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_RING_BUFFER_ENTRIES 8192
#define MAX_LOG_MESSAGE_LEN 2048
#define MAX_CONTEXT_FIELDS 16
#define MAX_OUTPUT_TARGETS 8
#define MAX_FIELD_KEY_LEN 64
#define MAX_FIELD_VALUE_LEN 512

typedef struct {
    char key[MAX_FIELD_KEY_LEN];
    char value[MAX_FIELD_VALUE_LEN];
} context_field_t;

typedef enum {
    TARGET_FILE,
    TARGET_CALLBACK,
    TARGET_RING_BUFFER
} target_type_t;

typedef struct {
    target_type_t type;
    union {
        struct {
            FILE* fp;
            char path[512];
            uint64_t max_size_bytes;
            uint64_t current_size;
            uint32_t max_rotations;
        } file;
        struct {
            void (*callback)(const char* json_line, void* user_data);
            void* user_data;
        } callback;
    } config;
    log_level_t min_level;
    bool enabled;
} output_target_t;

typedef struct {
    log_level_t level;
    char message[MAX_LOG_MESSAGE_LEN];
    char service_name[128];
    char file[256];
    int line;
    char function[128];
    uint64_t timestamp;
    context_field_t context[MAX_CONTEXT_FIELDS];
    size_t context_count;
    char trace_id[64];
    char span_id[64];
} ring_entry_t;

static struct {
    ring_entry_t ring[MAX_RING_BUFFER_ENTRIES];
    size_t write_idx;
    size_t entry_count;
    agentos_mutex_t ring_lock;

    output_target_t targets[MAX_OUTPUT_TARGETS];
    size_t target_count;
    agentos_mutex_t target_lock;

    context_field_t global_context[MAX_CONTEXT_FIELDS];
    size_t global_context_count;
    agentos_mutex_t context_lock;

    log_level_t min_level;
    int initialized;
} g_structured_log = {0};

static const char* level_names[] = {
    "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
};

int structured_log_init(log_level_t min_level) {
    if (g_structured_log.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_init(&g_structured_log.ring_lock);
    agentos_mutex_init(&g_structured_log.target_lock);
    agentos_mutex_init(&g_structured_log.context_lock);

    g_structured_log.write_idx = 0;
    g_structured_log.entry_count = 0;
    g_structured_log.target_count = 0;
    g_structured_log.global_context_count = 0;
    g_structured_log.min_level = min_level;
    g_structured_log.initialized = 1;

    return AGENTOS_SUCCESS;
}

void structured_log_shutdown(void) {
    if (!g_structured_log.initialized) return;

    agentos_mutex_lock(&g_structured_log.target_lock);
    for (size_t i = 0; i < g_structured_log.target_count; i++) {
        if (g_structured_log.targets[i].type == TARGET_FILE &&
            g_structured_log.targets[i].config.file.fp) {
            fflush(g_structured_log.targets[i].config.file.fp);
            fclose(g_structured_log.targets[i].config.file.fp);
        }
    }
    g_structured_log.target_count = 0;
    agentos_mutex_unlock(&g_structured_log.target_lock);

    agentos_mutex_destroy(&g_structured_log.ring_lock);
    agentos_mutex_destroy(&g_structured_log.target_lock);
    agentos_mutex_destroy(&g_structured_log.context_lock);

    g_structured_log.initialized = 0;
}

int structured_log_add_file_target(const char* path, log_level_t min_level,
                                   uint64_t max_size_bytes, uint32_t max_rotations) {
    if (!path) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_structured_log.initialized) structured_log_init(LOG_LEVEL_INFO);

    agentos_mutex_lock(&g_structured_log.target_lock);

    if (g_structured_log.target_count >= MAX_OUTPUT_TARGETS) {
        agentos_mutex_unlock(&g_structured_log.target_lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    output_target_t* target = &g_structured_log.targets[g_structured_log.target_count];
    target->type = TARGET_FILE;
    target->min_level = min_level;
    target->enabled = true;
    strncpy(target->config.file.path, path, sizeof(target->config.file.path) - 1);
    target->config.file.max_size_bytes = max_size_bytes > 0 ? max_size_bytes : 100 * 1024 * 1024;
    target->config.file.max_rotations = max_rotations > 0 ? max_rotations : 5;
    target->config.file.current_size = 0;

    target->config.file.fp = fopen(path, "a");
    if (!target->config.file.fp) {
        agentos_mutex_unlock(&g_structured_log.target_lock);
        SVC_LOG_ERROR("Failed to open log file: %s", path);
        return AGENTOS_ERR_IO;
    }

    g_structured_log.target_count++;
    agentos_mutex_unlock(&g_structured_log.target_lock);

    SVC_LOG_INFO("Structured log file target added: %s", path);
    return AGENTOS_SUCCESS;
}

int structured_log_add_callback_target(
    void (*callback)(const char* json_line, void* user_data),
    void* user_data, log_level_t min_level) {
    if (!callback) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_structured_log.initialized) structured_log_init(LOG_LEVEL_INFO);

    agentos_mutex_lock(&g_structured_log.target_lock);

    if (g_structured_log.target_count >= MAX_OUTPUT_TARGETS) {
        agentos_mutex_unlock(&g_structured_log.target_lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    output_target_t* target = &g_structured_log.targets[g_structured_log.target_count];
    target->type = TARGET_CALLBACK;
    target->min_level = min_level;
    target->enabled = true;
    target->config.callback.callback = callback;
    target->config.callback.user_data = user_data;

    g_structured_log.target_count++;
    agentos_mutex_unlock(&g_structured_log.target_lock);

    return AGENTOS_SUCCESS;
}

int structured_log_set_context(const char* key, const char* value) {
    if (!key || !value) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_structured_log.initialized) structured_log_init(LOG_LEVEL_INFO);

    agentos_mutex_lock(&g_structured_log.context_lock);

    for (size_t i = 0; i < g_structured_log.global_context_count; i++) {
        if (strcmp(g_structured_log.global_context[i].key, key) == 0) {
            strncpy(g_structured_log.global_context[i].value, value,
                    MAX_FIELD_VALUE_LEN - 1);
            agentos_mutex_unlock(&g_structured_log.context_lock);
            return AGENTOS_SUCCESS;
        }
    }

    if (g_structured_log.global_context_count >= MAX_CONTEXT_FIELDS) {
        agentos_mutex_unlock(&g_structured_log.context_lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    context_field_t* field = &g_structured_log.global_context[g_structured_log.global_context_count];
    strncpy(field->key, key, MAX_FIELD_KEY_LEN - 1);
    strncpy(field->value, value, MAX_FIELD_VALUE_LEN - 1);
    g_structured_log.global_context_count++;

    agentos_mutex_unlock(&g_structured_log.context_lock);
    return AGENTOS_SUCCESS;
}

static void format_json_log(const ring_entry_t* entry, char* buf, size_t buf_size) {
    size_t pos = 0;

    pos += snprintf(buf + pos, buf_size - pos,
                    "{\"timestamp\":\"%llu\","
                    "\"level\":\"%s\","
                    "\"message\":\"%s\","
                    "\"service\":\"%s\","
                    "\"file\":\"%s\","
                    "\"line\":%d,"
                    "\"function\":\"%s\"",
                    (unsigned long long)entry->timestamp,
                    entry->level < LOG_LEVEL_COUNT ? level_names[entry->level] : "UNKNOWN",
                    entry->message,
                    entry->service_name,
                    entry->file,
                    entry->line,
                    entry->function);

    if (entry->trace_id[0]) {
        pos += snprintf(buf + pos, buf_size - pos,
                       ",\"traceId\":\"%s\"", entry->trace_id);
    }
    if (entry->span_id[0]) {
        pos += snprintf(buf + pos, buf_size - pos,
                       ",\"spanId\":\"%s\"", entry->span_id);
    }

    for (size_t i = 0; i < entry->context_count && pos < buf_size - 128; i++) {
        pos += snprintf(buf + pos, buf_size - pos,
                       ",\"%s\":\"%s\"",
                       entry->context[i].key, entry->context[i].value);
    }

    pos += snprintf(buf + pos, buf_size - pos, "}");
}

static void dispatch_to_targets(const ring_entry_t* entry) {
    char json_buf[MAX_LOG_MESSAGE_LEN + 1024];
    format_json_log(entry, json_buf, sizeof(json_buf));

    agentos_mutex_lock(&g_structured_log.target_lock);

    for (size_t i = 0; i < g_structured_log.target_count; i++) {
        output_target_t* target = &g_structured_log.targets[i];
        if (!target->enabled || entry->level < target->min_level) continue;

        switch (target->type) {
            case TARGET_FILE:
                if (target->config.file.fp) {
                    fprintf(target->config.file.fp, "%s\n", json_buf);
                    fflush(target->config.file.fp);
                    target->config.file.current_size += strlen(json_buf) + 1;

                    if (target->config.file.current_size >= target->config.file.max_size_bytes) {
                        fclose(target->config.file.fp);
                        target->config.file.fp = fopen(target->config.file.path, "w");
                        if (target->config.file.fp) {
                            target->config.file.current_size = 0;
                        }
                    }
                }
                break;
            case TARGET_CALLBACK:
                if (target->config.callback.callback) {
                    target->config.callback.callback(json_buf, target->config.callback.user_data);
                }
                break;
            case TARGET_RING_BUFFER:
                break;
        }
    }

    agentos_mutex_unlock(&g_structured_log.target_lock);
}

int structured_log_write(log_level_t level, const char* service_name,
                         const char* file, int line, const char* function,
                         const char* message) {
    if (!message) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_structured_log.initialized) structured_log_init(LOG_LEVEL_INFO);

    if (level < g_structured_log.min_level) {
        return AGENTOS_SUCCESS;
    }

    ring_entry_t entry = {0};
    entry.level = level;
    entry.timestamp = (uint64_t)time(NULL) * 1000;
    strncpy(entry.message, message, MAX_LOG_MESSAGE_LEN - 1);
    if (service_name) strncpy(entry.service_name, service_name, sizeof(entry.service_name) - 1);
    if (file) strncpy(entry.file, file, sizeof(entry.file) - 1);
    entry.line = line;
    if (function) strncpy(entry.function, function, sizeof(entry.function) - 1);

    agentos_mutex_lock(&g_structured_log.context_lock);
    entry.context_count = g_structured_log.global_context_count < MAX_CONTEXT_FIELDS
                          ? g_structured_log.global_context_count
                          : MAX_CONTEXT_FIELDS;
    memcpy(entry.context, g_structured_log.global_context,
           entry.context_count * sizeof(context_field_t));
    agentos_mutex_unlock(&g_structured_log.context_lock);

    agentos_mutex_lock(&g_structured_log.ring_lock);
    size_t idx = g_structured_log.write_idx % MAX_RING_BUFFER_ENTRIES;
    g_structured_log.ring[idx] = entry;
    g_structured_log.write_idx++;
    if (g_structured_log.entry_count < MAX_RING_BUFFER_ENTRIES) {
        g_structured_log.entry_count++;
    }
    agentos_mutex_unlock(&g_structured_log.ring_lock);

    dispatch_to_targets(&entry);

    return AGENTOS_SUCCESS;
}

int structured_log_query(log_level_t level_filter, const char* service_filter,
                         uint64_t start_time, uint64_t end_time,
                         char*** results, size_t* count) {
    if (!results || !count) return AGENTOS_ERR_INVALID_PARAM;
    if (!g_structured_log.initialized) {
        *results = NULL;
        *count = 0;
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_lock(&g_structured_log.ring_lock);

    size_t match_count = 0;
    for (size_t i = 0; i < g_structured_log.entry_count; i++) {
        size_t idx = (g_structured_log.write_idx - g_structured_log.entry_count + i)
                     % MAX_RING_BUFFER_ENTRIES;
        ring_entry_t* entry = &g_structured_log.ring[idx];

        if (level_filter != LOG_LEVEL_DEBUG && entry->level < level_filter) continue;
        if (service_filter && entry->service_name[0] &&
            strstr(entry->service_name, service_filter) == NULL) continue;
        if (start_time > 0 && entry->timestamp < start_time) continue;
        if (end_time > 0 && entry->timestamp > end_time) continue;

        match_count++;
    }

    if (match_count == 0) {
        *results = NULL;
        *count = 0;
        agentos_mutex_unlock(&g_structured_log.ring_lock);
        return AGENTOS_SUCCESS;
    }

    char** res = (char**)calloc(match_count, sizeof(char*));
    size_t idx_out = 0;

    for (size_t i = 0; i < g_structured_log.entry_count && idx_out < match_count; i++) {
        size_t idx = (g_structured_log.write_idx - g_structured_log.entry_count + i)
                     % MAX_RING_BUFFER_ENTRIES;
        ring_entry_t* entry = &g_structured_log.ring[idx];

        if (level_filter != LOG_LEVEL_DEBUG && entry->level < level_filter) continue;
        if (service_filter && entry->service_name[0] &&
            strstr(entry->service_name, service_filter) == NULL) continue;
        if (start_time > 0 && entry->timestamp < start_time) continue;
        if (end_time > 0 && entry->timestamp > end_time) continue;

        char* json = (char*)malloc(MAX_LOG_MESSAGE_LEN + 1024);
        if (json) {
            format_json_log(entry, json, MAX_LOG_MESSAGE_LEN + 1024);
            res[idx_out++] = json;
        }
    }

    *results = res;
    *count = idx_out;

    agentos_mutex_unlock(&g_structured_log.ring_lock);
    return AGENTOS_SUCCESS;
}

size_t structured_log_get_entry_count(void) {
    if (!g_structured_log.initialized) return 0;
    return g_structured_log.entry_count;
}

void structured_log_free_results(char** results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) {
        free(results[i]);
    }
    free(results);
}
