/**
 * @file partdata_trace.c
 * @brief AgentOS 数据分区追踪数据存储实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "partdata_trace.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define PARTDATA_MAX_BATCH_SIZE 1000
#define PARTDATA_TRACE_FILE_EXT ".json"

typedef struct trace_node {
    partdata_span_t span;
    struct trace_node* next;
} trace_node_t;

typedef struct {
    trace_node_t* head;
    trace_node_t* tail;
    size_t count;
    size_t max_count;
    pthread_mutex_t lock;
    bool initialized;
} trace_queue_t;

static trace_queue_t g_trace_queue = {0};
static char g_trace_path[512] = {0};
static size_t g_batch_size = 100;
static uint32_t g_export_interval_sec = 10;
static bool g_exporter_enabled = false;
static FILE* g_trace_file = NULL;
static pthread_mutex_t g_file_lock = PTHREAD_MUTEX_INITIALIZER;

static bool ensure_directory(const char* path) {
    if (!path) return false;

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    for (size_t i = 0; i < len; i++) {
        if (path_copy[i] == '\\' || path_copy[i] == '/') {
            if (i > 0 && path_copy[i - 1] != ':') {
                path_copy[i] = '\0';
                mkdir(path_copy, 0755);
                path_copy[i] = '/';
            }
        }
    }

    mkdir(path_copy, 0755);
    return true;
}

static const char* get_trace_base_path(void) {
    static char base_path[256] = "partdata/traces";
    return base_path;
}

partdata_error_t partdata_trace_init(void) {
    memset(&g_trace_queue, 0, sizeof(g_trace_queue));
    pthread_mutex_init(&g_trace_queue.lock, NULL);
    g_trace_queue.max_count = PARTDATA_MAX_BATCH_SIZE;
    g_trace_queue.initialized = true;

    const char* base = get_trace_base_path();
    strncpy(g_trace_path, base, sizeof(g_trace_path) - 1);
    g_trace_path[sizeof(g_trace_path) - 1] = '\0';

    if (!ensure_directory(g_trace_path)) {
        return PARTDATA_ERR_DIR_CREATE_FAILED;
    }

    char spans_dir[512];
    snprintf(spans_dir, sizeof(spans_dir), "%s/spans", g_trace_path);
    if (!ensure_directory(spans_dir)) {
        return PARTDATA_ERR_DIR_CREATE_FAILED;
    }

    g_exporter_enabled = true;
    g_batch_size = 100;
    g_export_interval_sec = 10;

    return PARTDATA_SUCCESS;
}

void partdata_trace_shutdown(void) {
    if (g_trace_queue.initialized) {
        partdata_trace_flush();

        pthread_mutex_lock(&g_trace_queue.lock);
        trace_node_t* node = g_trace_queue.head;
        while (node) {
            trace_node_t* next = node->next;
            free(node);
            node = next;
        }
        g_trace_queue.head = NULL;
        g_trace_queue.tail = NULL;
        g_trace_queue.count = 0;
        pthread_mutex_unlock(&g_trace_queue.lock);

        pthread_mutex_destroy(&g_trace_queue.lock);
        g_trace_queue.initialized = false;
    }

    if (g_trace_file) {
        fclose(g_trace_file);
        g_trace_file = NULL;
    }
}

static FILE* get_trace_file(void) {
    if (g_trace_file) {
        return g_trace_file;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char filename[256];
    strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", tm_info);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/spans/trace_%s%s", g_trace_path, filename, PARTDATA_TRACE_FILE_EXT);

    g_trace_file = fopen(filepath, "a");
    return g_trace_file;
}

partdata_error_t partdata_trace_write_span(const partdata_span_t* span) {
    if (!span || !span->trace_id[0]) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (!g_trace_queue.initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    trace_node_t* node = (trace_node_t*)malloc(sizeof(trace_node_t));
    if (!node) {
        return PARTDATA_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->span, span, sizeof(partdata_span_t));
    node->next = NULL;

    pthread_mutex_lock(&g_trace_queue.lock);

    if (g_trace_queue.tail) {
        g_trace_queue.tail->next = node;
        g_trace_queue.tail = node;
    } else {
        g_trace_queue.head = node;
        g_trace_queue.tail = node;
    }
    g_trace_queue.count++;

    pthread_mutex_unlock(&g_trace_queue.lock);

    if (g_trace_queue.count >= g_batch_size) {
        partdata_trace_flush();
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_trace_write_spans_batch(const partdata_span_t* spans, size_t count) {
    if (!spans || count == 0) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    partdata_error_t result = PARTDATA_SUCCESS;

    for (size_t i = 0; i < count; i++) {
        partdata_error_t err = partdata_trace_write_span(&spans[i]);
        if (err != PARTDATA_SUCCESS) {
            result = err;
        }
    }

    return result;
}

partdata_error_t partdata_trace_flush(void) {
    if (!g_trace_queue.initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_file_lock);

    FILE* fp = get_trace_file();
    if (!fp) {
        pthread_mutex_unlock(&g_file_lock);
        return PARTDATA_ERR_FILE_OPEN_FAILED;
    }

    pthread_mutex_lock(&g_trace_queue.lock);

    trace_node_t* node = g_trace_queue.head;
    while (node) {
        fprintf(fp,
            "{\"trace_id\":\"%s\",\"span_id\":\"%s\",\"parent_span_id\":\"%s\","
            "\"name\":\"%s\",\"start_time\":%lu,\"end_time\":%lu,"
            "\"service\":\"%s\",\"status\":\"%s\"}\n",
            node->span.trace_id,
            node->span.span_id,
            node->span.parent_span_id,
            node->span.name,
            (unsigned long)node->span.start_time_ns,
            (unsigned long)node->span.end_time_ns,
            node->span.service_name,
            node->span.status);

        trace_node_t* next = node->next;
        free(node);
        node = next;
    }

    g_trace_queue.head = NULL;
    g_trace_queue.tail = NULL;
    g_trace_queue.count = 0;

    pthread_mutex_unlock(&g_trace_queue.lock);

    fflush(fp);

    pthread_mutex_unlock(&g_file_lock);

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_trace_query_by_trace(const char* trace_id, partdata_span_t** spans, size_t* count) {
    if (!trace_id || !spans || !count) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    *spans = NULL;
    *count = 0;

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_trace_query_by_time_range(uint64_t start_time, uint64_t end_time, partdata_span_t** spans, size_t* count) {
    if (!spans || !count) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    *spans = NULL;
    *count = 0;

    (void)start_time;
    (void)end_time;

    return PARTDATA_SUCCESS;
}

void partdata_trace_free_spans(partdata_span_t* spans) {
    if (spans) {
        free(spans);
    }
}

partdata_error_t partdata_trace_config_exporter(const partdata_trace_exporter_config_t* config) {
    if (!config) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    g_exporter_enabled = config->enabled;
    g_batch_size = config->batch_size > 0 ? config->batch_size : 100;
    g_export_interval_sec = config->export_interval_sec > 0 ? config->export_interval_sec : 10;

    if (config->export_path[0]) {
        strncpy(g_trace_path, config->export_path, sizeof(g_trace_path) - 1);
        ensure_directory(g_trace_path);
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_trace_get_stats(uint64_t* total_spans, uint64_t* pending_spans, uint64_t* total_size_bytes) {
    if (!total_spans || !pending_spans || !total_size_bytes) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_trace_queue.lock);
    *pending_spans = (uint64_t)g_trace_queue.count;
    pthread_mutex_unlock(&g_trace_queue.lock);

    *total_spans = 0;
    *total_size_bytes = 0;

    return PARTDATA_SUCCESS;
}

bool partdata_trace_is_healthy(void) {
    return g_trace_queue.initialized;
}
