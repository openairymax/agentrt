/**
 * @file heapstore_trace.c
 * @brief AgentOS 数据分区追踪数据存储实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore_trace.h"
#include "private.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#define heapstore_TRACE_MAX_PATH 512
#define heapstore_TRACE_MAX_SPANS 10000
#define heapstore_TRACE_BATCH_SIZE 100

static bool s_initialized = false;
static char s_trace_path[heapstore_TRACE_MAX_PATH] = {0};
static pthread_mutex_t s_trace_lock = PTHREAD_MUTEX_INITIALIZER;
static heapstore_span_t* s_span_buffer = NULL;
static size_t s_span_count = 0;
static heapstore_trace_exporter_config_t s_exporter_config = {0};

heapstore_error_t heapstore_trace_init(void) {
    if (s_initialized) {
        return heapstore_SUCCESS;
    }

    const char* base_path = "heapstore/traces";
    strncpy(s_trace_path, base_path, sizeof(s_trace_path) - 1);
    s_trace_path[sizeof(s_trace_path) - 1] = '\0';

    heapstore_ensure_directory(s_trace_path);

    char spans_path[heapstore_TRACE_MAX_PATH];
    snprintf(spans_path, sizeof(spans_path), "%s/spans", s_trace_path);
    heapstore_ensure_directory(spans_path);

    s_span_buffer = (heapstore_span_t*)calloc(heapstore_TRACE_MAX_SPANS, sizeof(heapstore_span_t));
    if (!s_span_buffer) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    s_span_count = 0;

    memset(&s_exporter_config, 0, sizeof(s_exporter_config));
    s_exporter_config.enabled = false;
    s_exporter_config.batch_size = heapstore_TRACE_BATCH_SIZE;
    s_exporter_config.export_interval_sec = 30;
    strncpy(s_exporter_config.export_format, "json", sizeof(s_exporter_config.export_format) - 1);

    s_initialized = true;

    return heapstore_SUCCESS;
}

void heapstore_trace_shutdown(void) {
    if (!s_initialized) {
        return;
    }

    pthread_mutex_lock(&s_trace_lock);

    if (s_span_buffer) {
        free(s_span_buffer);
        s_span_buffer = NULL;
    }
    s_span_count = 0;

    s_initialized = false;
    pthread_mutex_unlock(&s_trace_lock);
}

heapstore_error_t heapstore_trace_write_span(const heapstore_span_t* span) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!span) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_lock);

    if (s_span_count >= heapstore_TRACE_MAX_SPANS) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&s_span_buffer[s_span_count], span, sizeof(heapstore_span_t));
    s_span_count++;

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_write_spans_batch(const heapstore_span_t* spans, size_t count) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!spans || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_lock);

    if (s_span_count + count > heapstore_TRACE_MAX_SPANS) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&s_span_buffer[s_span_count], spans, count * sizeof(heapstore_span_t));
    s_span_count += count;

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_query_by_trace(const char* trace_id, heapstore_span_t** spans, size_t* count) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!trace_id || !spans || !count) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_lock);

    size_t match_count = 0;
    for (size_t i = 0; i < s_span_count; i++) {
        if (strcmp(s_span_buffer[i].trace_id, trace_id) == 0) {
            match_count++;
        }
    }

    if (match_count == 0) {
        pthread_mutex_unlock(&s_trace_lock);
        *spans = NULL;
        *count = 0;
        return heapstore_ERR_NOT_FOUND;
    }

    heapstore_span_t* result = (heapstore_span_t*)malloc(match_count * sizeof(heapstore_span_t));
    if (!result) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    size_t idx = 0;
    for (size_t i = 0; i < s_span_count; i++) {
        if (strcmp(s_span_buffer[i].trace_id, trace_id) == 0) {
            memcpy(&result[idx], &s_span_buffer[i], sizeof(heapstore_span_t));
            idx++;
        }
    }

    *spans = result;
    *count = match_count;

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_query_by_time_range(uint64_t start_time, uint64_t end_time, heapstore_span_t** spans, size_t* count) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!spans || !count) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_lock);

    size_t match_count = 0;
    for (size_t i = 0; i < s_span_count; i++) {
        if (s_span_buffer[i].start_time_ns >= start_time && s_span_buffer[i].end_time_ns <= end_time) {
            match_count++;
        }
    }

    if (match_count == 0) {
        pthread_mutex_unlock(&s_trace_lock);
        *spans = NULL;
        *count = 0;
        return heapstore_ERR_NOT_FOUND;
    }

    heapstore_span_t* result = (heapstore_span_t*)malloc(match_count * sizeof(heapstore_span_t));
    if (!result) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    size_t idx = 0;
    for (size_t i = 0; i < s_span_count; i++) {
        if (s_span_buffer[i].start_time_ns >= start_time && s_span_buffer[i].end_time_ns <= end_time) {
            memcpy(&result[idx], &s_span_buffer[i], sizeof(heapstore_span_t));
            idx++;
        }
    }

    *spans = result;
    *count = match_count;

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

void heapstore_trace_free_spans(heapstore_span_t* spans) {
    if (spans) {
        free(spans);
    }
}

heapstore_error_t heapstore_trace_config_exporter(const heapstore_trace_exporter_config_t* manager) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!manager) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_lock);
    memcpy(&s_exporter_config, manager, sizeof(s_exporter_config));
    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_flush(void) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_trace_lock);

    if (s_span_count == 0) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_SUCCESS;
    }

    char filename[256];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(filename, sizeof(filename), "spans_%Y%m%d_%H%M%S.json", tm_info);

    char filepath[heapstore_TRACE_MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s/spans/%s", s_trace_path, filename);

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(fp, "{\n  \"spans\": [\n");
    for (size_t i = 0; i < s_span_count; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"trace_id\": \"%s\",\n", s_span_buffer[i].trace_id);
        fprintf(fp, "      \"span_id\": \"%s\",\n", s_span_buffer[i].span_id);
        fprintf(fp, "      \"name\": \"%s\",\n", s_span_buffer[i].name);
        fprintf(fp, "      \"start_time_ns\": %lu,\n", (unsigned long)s_span_buffer[i].start_time_ns);
        fprintf(fp, "      \"end_time_ns\": %lu\n", (unsigned long)s_span_buffer[i].end_time_ns);
        fprintf(fp, "    }%s\n", (i < s_span_count - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");

    fclose(fp);
    s_span_count = 0;

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_get_stats(uint64_t* total_spans, uint64_t* pending_spans, uint64_t* total_size_bytes) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_trace_lock);

    if (total_spans) {
        *total_spans = s_span_count;
    }
    if (pending_spans) {
        *pending_spans = s_span_count;
    }
    if (total_size_bytes) {
        *total_size_bytes = s_span_count * sizeof(heapstore_span_t);
    }

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_cleanup(int days_to_keep, uint64_t* freed_bytes) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    if (days_to_keep <= 0) {
        return heapstore_SUCCESS;
    }

    time_t cutoff_time = time(NULL) - (days_to_keep * 86400);

    pthread_mutex_lock(&s_trace_lock);

    char spans_path[heapstore_TRACE_MAX_PATH];
    snprintf(spans_path, sizeof(spans_path), "%s/spans", s_trace_path);

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char search_path[heapstore_TRACE_MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s/*", spans_path);

    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char filepath[heapstore_TRACE_MAX_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s", spans_path, find_data.cFileName);

                FILETIME ft_write = find_data.ftLastWriteTime;
                ULARGE_INTEGER uli;
                uli.LowPart = ft_write.dwLowDateTime;
                uli.HighPart = ft_write.dwHighDateTime;
                time_t file_time = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000);

                if (file_time < cutoff_time) {
                    uint64_t file_size = ((uint64_t)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow;
                    if (DeleteFileA(filepath)) {
                        if (freed_bytes) *freed_bytes += file_size;
                    }
                }
            }
        } while (FindNextFileA(h_find, &find_data));
        FindClose(h_find);
    }
#else
    DIR* dir = opendir(spans_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_REG) {
                continue;
            }

            char filepath[heapstore_TRACE_MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s/%s", spans_path, entry->d_name);

            struct stat st;
            if (stat(filepath, &st) == 0) {
                if (st.st_mtime < cutoff_time) {
                    uint64_t file_size = (uint64_t)st.st_size;
                    if (unlink(filepath) == 0) {
                        if (freed_bytes) *freed_bytes += file_size;
                    }
                }
            }
        }
        closedir(dir);
    }
#endif

    pthread_mutex_unlock(&s_trace_lock);

    return heapstore_SUCCESS;
}

bool heapstore_trace_is_healthy(void) {
    return s_initialized && s_span_buffer != NULL;
}

/**
 * @brief 将所有追踪数据导出为 JSON 字符串
 *
 * 生成符合 OpenTelemetry 兼容格式的 JSON 数组
 */
heapstore_error_t heapstore_trace_export_to_json(char** out_json, bool include_events) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!out_json) {
        return heapstore_ERR_INVALID_PARAM;
    }

    (void)include_events; /* 预留参数，当前版本不使用 */

    pthread_mutex_lock(&s_trace_lock);

    if (s_span_count == 0) {
        *out_json = strdup("[]");
        pthread_mutex_unlock(&s_trace_lock);
        return (*out_json != NULL) ? heapstore_SUCCESS : heapstore_ERR_OUT_OF_MEMORY;
    }

    /* 计算预估的 JSON 大小（每个 span 约 500 字节） */
    size_t estimated_size = s_span_count * 512 + 64;
    char* json_buffer = (char*)malloc(estimated_size);
    if (!json_buffer) {
        pthread_mutex_unlock(&s_trace_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    /* 构建 JSON 数组 */
    size_t pos = 0;
    pos += snprintf(json_buffer + pos, estimated_size - pos, "[\n");

    for (size_t i = 0; i < s_span_count; i++) {
        const heapstore_span_t* span = &s_span_buffer[i];

        /* 转义字符串中的特殊字符 */
        char escaped_name[256];
        size_t name_idx = 0;
        for (size_t j = 0; span->name[j] && j < sizeof(span->name) - 1 && name_idx < sizeof(escaped_name) - 1; j++) {
            if (span->name[j] == '"' || span->name[j] == '\\') {
                escaped_name[name_idx++] = '\\';
            }
            escaped_name[name_idx++] = span->name[j];
        }
        escaped_name[name_idx] = '\0';

        /* 写入单个 span 的 JSON 对象 */
        pos += snprintf(json_buffer + pos, estimated_size - pos,
            "  {\n"
            "    \"traceId\": \"%s\",\n"
            "    \"spanId\": \"%s\",\n"
            "    \"parentSpanId\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"startTimeNs\": %llu,\n"
            "    \"endTimeNs\": %llu,\n"
            "    \"status\": \"%s\",\n"
            "    \"attributeCount\": %zu\n"
            "  }%s\n",
            span->trace_id,
            span->span_id,
            span->parent_span_id[0] ? span->parent_span_id : "",
            escaped_name,
            (unsigned long long)span->start_time_ns,
            (unsigned long long)span->end_time_ns,
            span->status,
            span->attribute_count,
            (i < s_span_count - 1) ? "," : "");

        /* 检查缓冲区是否足够 */
        if (pos >= estimated_size - 512) {
            estimated_size *= 2;
            char* new_buffer = (char*)realloc(json_buffer, estimated_size);
            if (!new_buffer) {
                free(json_buffer);
                pthread_mutex_unlock(&s_trace_lock);
                return heapstore_ERR_OUT_OF_MEMORY;
            }
            json_buffer = new_buffer;
        }
    }

    pos += snprintf(json_buffer + pos, estimated_size - pos, "]");

    pthread_mutex_unlock(&s_trace_lock);

    *out_json = json_buffer;

    return heapstore_SUCCESS;
}
