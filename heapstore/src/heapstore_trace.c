/**
 * @file heapstore_trace.c
 * @brief AgentOS 数据分区追踪数据存储实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "heapstore_trace.h"
#include "private.h"
#include "utils.h"

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

#define heapstore_MAX_BATCH_SIZE 1000
#define heapstore_TRACE_FILE_EXT ".json"

typedef struct trace_node {
    heapstore_span_t span;
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

static trace_queue_t s_trace_queue = {0};
static char s_trace_path[512] = {0};
static size_t s_batch_size = 100;
static uint32_t s_export_interval_sec = 10;
static bool s_exporter_enabled = false;
static FILE* s_trace_file = NULL;
static pthread_mutex_t s_file_lock = PTHREAD_MUTEX_INITIALIZER;

static const char* get_trace_base_path(void) {
    static char base_path[256] = "heapstore/traces";
    return base_path;
}

heapstore_error_t heapstore_trace_init(void) {
    memset(&s_trace_queue, 0, sizeof(s_trace_queue));
    pthread_mutex_init(&s_trace_queue.lock, NULL);
    s_trace_queue.max_count = heapstore_MAX_BATCH_SIZE;
    s_trace_queue.initialized = true;

    const char* base = get_trace_base_path();
    strncpy(s_trace_path, base, sizeof(s_trace_path) - 1);
    s_trace_path[sizeof(s_trace_path) - 1] = '\0';

    if (!heapstore_ensure_directory(s_trace_path)) {
        return heapstore_ERR_DIR_CREATE_FAILED;
    }

    char spans_dir[512];
    snprintf(spans_dir, sizeof(spans_dir), "%s/spans", s_trace_path);
    if (!heapstore_ensure_directory(spans_dir)) {
        return heapstore_ERR_DIR_CREATE_FAILED;
    }

    s_exporter_enabled = true;
    s_batch_size = 100;
    s_export_interval_sec = 10;

    return heapstore_SUCCESS;
}

void heapstore_trace_shutdown(void) {
    if (s_trace_queue.initialized) {
        heapstore_trace_flush();

        pthread_mutex_lock(&s_trace_queue.lock);
        trace_node_t* node = s_trace_queue.head;
        while (node) {
            trace_node_t* next = node->next;
            free(node);
            node = next;
        }
        s_trace_queue.head = NULL;
        s_trace_queue.tail = NULL;
        s_trace_queue.count = 0;
        pthread_mutex_unlock(&s_trace_queue.lock);

        pthread_mutex_destroy(&s_trace_queue.lock);
        s_trace_queue.initialized = false;
    }

    if (s_trace_file) {
        fclose(s_trace_file);
        s_trace_file = NULL;
    }
}

static FILE* get_trace_file(void) {
    if (s_trace_file) {
        return s_trace_file;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char filename[256];
    strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", tm_info);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/spans/trace_%s%s", s_trace_path, filename, heapstore_TRACE_FILE_EXT);

    s_trace_file = fopen(filepath, "a");
    return s_trace_file;
}

heapstore_error_t heapstore_trace_write_span(const heapstore_span_t* span) {
    if (!span || !span->trace_id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!s_trace_queue.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    trace_node_t* node = (trace_node_t*)malloc(sizeof(trace_node_t));
    if (!node) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->span, span, sizeof(heapstore_span_t));
    node->next = NULL;

    pthread_mutex_lock(&s_trace_queue.lock);

    if (s_trace_queue.tail) {
        s_trace_queue.tail->next = node;
        s_trace_queue.tail = node;
    } else {
        s_trace_queue.head = node;
        s_trace_queue.tail = node;
    }
    s_trace_queue.count++;

    pthread_mutex_unlock(&s_trace_queue.lock);

    if (s_trace_queue.count >= s_batch_size) {
        heapstore_trace_flush();
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_write_spans_batch(const heapstore_span_t* spans, size_t count) {
    if (!spans || count == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_error_t result = heapstore_SUCCESS;

    for (size_t i = 0; i < count; i++) {
        heapstore_error_t err = heapstore_trace_write_span(&spans[i]);
        if (err != heapstore_SUCCESS) {
            result = err;
        }
    }

    return result;
}

heapstore_error_t heapstore_trace_flush(void) {
    if (!s_trace_queue.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_file_lock);

    FILE* fp = get_trace_file();
    if (!fp) {
        pthread_mutex_unlock(&s_file_lock);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    pthread_mutex_lock(&s_trace_queue.lock);

    trace_node_t* node = s_trace_queue.head;
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

    s_trace_queue.head = NULL;
    s_trace_queue.tail = NULL;
    s_trace_queue.count = 0;

    pthread_mutex_unlock(&s_trace_queue.lock);

    fflush(fp);

    pthread_mutex_unlock(&s_file_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_query_by_trace(const char* trace_id, heapstore_span_t** spans, size_t* count) {
    if (!trace_id || !spans || !count) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *spans = NULL;
    *count = 0;

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_query_by_time_range(uint64_t start_time, uint64_t end_time, heapstore_span_t** spans, size_t* count) {
    if (!spans || !count) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *spans = NULL;
    *count = 0;

    (void)start_time;
    (void)end_time;

    return heapstore_SUCCESS;
}

void heapstore_trace_free_spans(heapstore_span_t* spans) {
    if (spans) {
        free(spans);
    }
}

heapstore_error_t heapstore_trace_config_exporter(const heapstore_trace_exporter_config_t* manager) {
    if (!manager) {
        return heapstore_ERR_INVALID_PARAM;
    }

    s_exporter_enabled = manager->enabled;
    s_batch_size = manager->batch_size > 0 ? manager->batch_size : 100;
    s_export_interval_sec = manager->export_interval_sec > 0 ? manager->export_interval_sec : 10;

    if (manager->export_path[0]) {
        strncpy(s_trace_path, manager->export_path, sizeof(s_trace_path) - 1);
        heapstore_ensure_directory(s_trace_path);
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_trace_get_stats(uint64_t* total_spans, uint64_t* pending_spans, uint64_t* total_size_bytes) {
    if (!total_spans || !pending_spans || !total_size_bytes) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_trace_queue.lock);
    *pending_spans = (uint64_t)s_trace_queue.count;
    pthread_mutex_unlock(&s_trace_queue.lock);

    *total_spans = 0;
    *total_size_bytes = 0;

    return heapstore_SUCCESS;
}

bool heapstore_trace_is_healthy(void) {
    return s_trace_queue.initialized;
}
