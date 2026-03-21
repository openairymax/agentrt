/**
 * @file trace.c
 * @brief 链路追踪实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "trace.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <cjson/cJSON.h>

typedef struct trace_span {
    char* span_id;
    char* trace_id;
    char* name;
    char* parent_id;
    uint64_t start_ns;
    uint64_t end_ns;
    cJSON* events;
    struct trace_span* next;
} trace_span_t;

// From data intelligence emerges. by spharx
static trace_span_t* all_spans = NULL;
static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t next_span_id = 1;

static uint64_t get_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

agentos_trace_span_t* agentos_trace_begin(const char* name, const char* parent_id) {
    if (!name) return NULL;
    trace_span_t* span = (trace_span_t*)calloc(1, sizeof(trace_span_t));
    if (!span) return NULL;

    // 生成 span_id
    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%llu", (unsigned long long)__sync_fetch_and_add(&next_span_id, 1));
    span->span_id = strdup(id_buf);

    // 获取 trace_id（从日志或生成新）
    const char* trace = agentos_log_get_trace_id();
    if (trace) {
        span->trace_id = strdup(trace);
    } else {
        char trace_buf[32];
        snprintf(trace_buf, sizeof(trace_buf), "%lx%lx", (unsigned long)time(NULL), (unsigned long)pthread_self());
        span->trace_id = strdup(trace_buf);
    }

    span->name = strdup(name);
    if (parent_id) span->parent_id = strdup(parent_id);
    span->start_ns = get_monotonic_ns();
    span->events = cJSON_CreateArray();

    pthread_mutex_lock(&trace_mutex);
    span->next = all_spans;
    all_spans = span;
    pthread_mutex_unlock(&trace_mutex);
    return (agentos_trace_span_t*)span;
}

void agentos_trace_end(agentos_trace_span_t* span) {
    if (!span) return;
    trace_span_t* s = (trace_span_t*)span;
    s->end_ns = get_monotonic_ns();
}

void agentos_trace_add_event(agentos_trace_span_t* span, const char* name, const char* attributes) {
    if (!span || !name) return;
    trace_span_t* s = (trace_span_t*)span;
    cJSON* event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "name", name);
    cJSON_AddNumberToObject(event, "timestamp", (double)time(NULL));
    if (attributes) {
        cJSON* attrs = cJSON_Parse(attributes);
        if (attrs) {
            cJSON_AddItemToObject(event, "attributes", attrs);
        }
    }
    cJSON_AddItemToArray(s->events, event);
}

char* agentos_trace_export(void) {
    cJSON* root = cJSON_CreateArray();
    pthread_mutex_lock(&trace_mutex);
    trace_span_t* s = all_spans;
    while (s) {
        cJSON* span_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(span_obj, "span_id", s->span_id);
        cJSON_AddStringToObject(span_obj, "trace_id", s->trace_id);
        cJSON_AddStringToObject(span_obj, "name", s->name);
        if (s->parent_id) cJSON_AddStringToObject(span_obj, "parent_id", s->parent_id);
        cJSON_AddNumberToObject(span_obj, "start_ns", (double)s->start_ns);
        if (s->end_ns > 0) {
            cJSON_AddNumberToObject(span_obj, "end_ns", (double)s->end_ns);
            cJSON_AddNumberToObject(span_obj, "duration_ms", (s->end_ns - s->start_ns) / 1000000.0);
        }
        if (cJSON_GetArraySize(s->events) > 0) {
            cJSON_AddItemToObject(span_obj, "events", cJSON_Duplicate(s->events, 1));
        }
        cJSON_AddItemToArray(root, span_obj);
        s = s->next;
    }
    pthread_mutex_unlock(&trace_mutex);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}