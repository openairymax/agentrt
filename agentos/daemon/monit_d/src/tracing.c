/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file tracing.c
 * @brief 分布式追踪系统实现
 *
 * 功能：
 * 1. 追踪和Span生命周期管理
 * 2. 上下文传播（W3C TraceContext兼容）
 * 3. 采样策略（始终/概率/速率限制）
 * 4. Span属性和事件
 * 5. 线程安全
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SPANS_PER_TRACE 128
#define MAX_TRACES 1024
#define MAX_SPAN_EVENTS 32
#define MAX_SPAN_ATTRIBUTES 16
#define MAX_TRACE_EXPORT_SIZE 65536
#define TRACE_ID_LEN 33
#define SPAN_ID_LEN 17

typedef enum {
    SPAN_KIND_INTERNAL,
    SPAN_KIND_SERVER,
    SPAN_KIND_CLIENT,
    SPAN_KIND_PRODUCER,
    SPAN_KIND_CONSUMER
} span_kind_t;

typedef enum {
    SAMPLE_ALWAYS,
    SAMPLE_PROBABILISTIC,
    SAMPLE_RATE_LIMITED
} sampling_strategy_t;

typedef struct {
    char* key;
    char* value;
} span_attribute_t;

typedef struct {
    char* name;
    uint64_t timestamp;
    span_attribute_t attributes[MAX_SPAN_ATTRIBUTES];
    size_t attribute_count;
} span_event_t;

typedef struct {
    char span_id[SPAN_ID_LEN];
    char parent_span_id[SPAN_ID_LEN];
    char* operation_name;
    span_kind_t kind;
    uint64_t start_time;
    uint64_t end_time;
    int status_code;
    char* status_message;
    span_attribute_t attributes[MAX_SPAN_ATTRIBUTES];
    size_t attribute_count;
    span_event_t events[MAX_SPAN_EVENTS];
    size_t event_count;
} span_t;

typedef struct {
    char trace_id[TRACE_ID_LEN];
    span_t spans[MAX_SPANS_PER_TRACE];
    size_t span_count;
    sampling_strategy_t sampling;
    double sampling_rate;
    int sampled;
    int finished;
    agentos_mutex_t lock;
} trace_t;

static struct {
    trace_t traces[MAX_TRACES];
    size_t trace_count;
    size_t finished_count;
    agentos_mutex_t global_lock;
    sampling_strategy_t default_sampling;
    double default_sampling_rate;
    uint32_t rate_limit_per_second;
    uint64_t last_rate_check;
    uint32_t rate_counter;
    int initialized;
} g_tracing = {0};

static void generate_hex_id(char* buf, size_t buf_len) {
    static const char hex[] = "0123456789abcdef";
    uint64_t t = (uint64_t)time(NULL);
    uint64_t r = (uint64_t)rand();
    for (size_t i = 0; i < buf_len - 1; i++) {
        buf[i] = hex[(t ^ r ^ (i * 17 + 3)) & 0xF];
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
    }
    buf[buf_len - 1] = '\0';
}

static bool should_sample(void) {
    switch (g_tracing.default_sampling) {
        case SAMPLE_ALWAYS:
            return true;
        case SAMPLE_PROBABILISTIC:
            return ((double)rand() / RAND_MAX) < g_tracing.default_sampling_rate;
        case SAMPLE_RATE_LIMITED: {
            uint64_t now = (uint64_t)time(NULL);
            if (now != g_tracing.last_rate_check) {
                g_tracing.last_rate_check = now;
                g_tracing.rate_counter = 0;
            }
            g_tracing.rate_counter++;
            return g_tracing.rate_counter <= g_tracing.rate_limit_per_second;
        }
    }
    return true;
}

int tracing_init(sampling_strategy_t strategy, double rate) {
    if (g_tracing.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_init(&g_tracing.global_lock);
    g_tracing.trace_count = 0;
    g_tracing.finished_count = 0;
    g_tracing.default_sampling = strategy;
    g_tracing.default_sampling_rate = rate > 0 ? rate : 1.0;
    g_tracing.rate_limit_per_second = 100;
    g_tracing.last_rate_check = 0;
    g_tracing.rate_counter = 0;
    g_tracing.initialized = 1;

    for (size_t i = 0; i < MAX_TRACES; i++) {
        agentos_mutex_init(&g_tracing.traces[i].lock);
    }

    SVC_LOG_INFO("Tracing system initialized (strategy=%d, rate=%.2f)", strategy, rate);
    return AGENTOS_SUCCESS;
}

void tracing_shutdown(void) {
    if (!g_tracing.initialized) return;

    agentos_mutex_lock(&g_tracing.global_lock);

    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        trace_t* trace = &g_tracing.traces[i];
        agentos_mutex_lock(&trace->lock);
        for (size_t j = 0; j < trace->span_count; j++) {
            span_t* span = &trace->spans[j];
            free(span->operation_name);
            free(span->status_message);
            for (size_t k = 0; k < span->attribute_count; k++) {
                free(span->attributes[k].key);
                free(span->attributes[k].value);
            }
            for (size_t k = 0; k < span->event_count; k++) {
                free(span->events[k].name);
                for (size_t l = 0; l < span->events[k].attribute_count; l++) {
                    free(span->events[k].attributes[l].key);
                    free(span->events[k].attributes[l].value);
                }
            }
        }
        agentos_mutex_unlock(&trace->lock);
        agentos_mutex_destroy(&trace->lock);
    }

    g_tracing.trace_count = 0;
    g_tracing.finished_count = 0;
    g_tracing.initialized = 0;

    agentos_mutex_unlock(&g_tracing.global_lock);
    agentos_mutex_destroy(&g_tracing.global_lock);
}

int tracing_start_trace(const char* operation_name, const char* parent_trace_id,
                        char* out_trace_id, size_t trace_id_buf_len) {
    if (!operation_name || !out_trace_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!g_tracing.initialized) {
        tracing_init(SAMPLE_ALWAYS, 1.0);
    }

    agentos_mutex_lock(&g_tracing.global_lock);

    if (g_tracing.trace_count >= MAX_TRACES) {
        size_t oldest = 0;
        for (size_t i = 1; i < g_tracing.trace_count; i++) {
            if (g_tracing.traces[i].finished &&
                (!g_tracing.traces[oldest].finished ||
                 g_tracing.traces[i].spans[0].start_time <
                 g_tracing.traces[oldest].spans[0].start_time)) {
                oldest = i;
            }
        }

        trace_t* old = &g_tracing.traces[oldest];
        agentos_mutex_lock(&old->lock);
        for (size_t j = 0; j < old->span_count; j++) {
            free(old->spans[j].operation_name);
            free(old->spans[j].status_message);
        }
        old->span_count = 0;
        old->finished = 0;
        agentos_mutex_unlock(&old->lock);

        if (oldest != g_tracing.trace_count - 1) {
            trace_t temp = g_tracing.traces[oldest];
            g_tracing.traces[oldest] = g_tracing.traces[g_tracing.trace_count - 1];
            g_tracing.traces[g_tracing.trace_count - 1] = temp;
        }
        g_tracing.trace_count--;
    }

    trace_t* trace = &g_tracing.traces[g_tracing.trace_count];
    memset(trace, 0, sizeof(trace_t));
    agentos_mutex_init(&trace->lock);

    if (parent_trace_id) {
        strncpy(trace->trace_id, parent_trace_id, TRACE_ID_LEN - 1);
    } else {
        generate_hex_id(trace->trace_id, TRACE_ID_LEN);
    }

    trace->span_count = 0;
    trace->sampling = g_tracing.default_sampling;
    trace->sampling_rate = g_tracing.default_sampling_rate;
    trace->sampled = should_sample() ? 1 : 0;
    trace->finished = 0;

    span_t* root_span = &trace->spans[0];
    generate_hex_id(root_span->span_id, SPAN_ID_LEN);
    root_span->parent_span_id[0] = '\0';
    root_span->operation_name = strdup(operation_name);
    root_span->kind = SPAN_KIND_SERVER;
    root_span->start_time = (uint64_t)time(NULL) * 1000000;
    root_span->end_time = 0;
    root_span->status_code = 0;
    root_span->status_message = NULL;
    root_span->attribute_count = 0;
    root_span->event_count = 0;
    trace->span_count = 1;

    g_tracing.trace_count++;

    strncpy(out_trace_id, trace->trace_id, trace_id_buf_len - 1);
    out_trace_id[trace_id_buf_len - 1] = '\0';

    agentos_mutex_unlock(&g_tracing.global_lock);

    SVC_LOG_DEBUG("Trace started: %s (operation=%s, sampled=%d)",
                  trace->trace_id, operation_name, trace->sampled);
    return AGENTOS_SUCCESS;
}

int tracing_start_span(const char* trace_id, const char* operation_name,
                       span_kind_t kind, char* out_span_id, size_t span_id_buf_len) {
    if (!trace_id || !operation_name || !out_span_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&g_tracing.global_lock);

    trace_t* trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        agentos_mutex_unlock(&g_tracing.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }

    agentos_mutex_lock(&trace->lock);
    agentos_mutex_unlock(&g_tracing.global_lock);

    if (trace->span_count >= MAX_SPANS_PER_TRACE) {
        agentos_mutex_unlock(&trace->lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    span_t* span = &trace->spans[trace->span_count];
    generate_hex_id(span->span_id, SPAN_ID_LEN);

    if (trace->span_count > 0) {
        strncpy(span->parent_span_id,
                trace->spans[trace->span_count - 1].span_id,
                SPAN_ID_LEN - 1);
    } else {
        span->parent_span_id[0] = '\0';
    }

    span->operation_name = strdup(operation_name);
    span->kind = kind;
    span->start_time = (uint64_t)time(NULL) * 1000000;
    span->end_time = 0;
    span->status_code = 0;
    span->status_message = NULL;
    span->attribute_count = 0;
    span->event_count = 0;
    trace->span_count++;

    strncpy(out_span_id, span->span_id, span_id_buf_len - 1);
    out_span_id[span_id_buf_len - 1] = '\0';

    agentos_mutex_unlock(&trace->lock);
    return AGENTOS_SUCCESS;
}

int tracing_end_span(const char* trace_id, const char* span_id,
                     int status_code, const char* status_message) {
    if (!trace_id || !span_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&g_tracing.global_lock);

    trace_t* trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        agentos_mutex_unlock(&g_tracing.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }

    agentos_mutex_lock(&trace->lock);
    agentos_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            trace->spans[i].end_time = (uint64_t)time(NULL) * 1000000;
            trace->spans[i].status_code = status_code;
            trace->spans[i].status_message = status_message ? strdup(status_message) : NULL;
            break;
        }
    }

    bool all_ended = true;
    for (size_t i = 0; i < trace->span_count; i++) {
        if (trace->spans[i].end_time == 0) {
            all_ended = false;
            break;
        }
    }

    if (all_ended) {
        trace->finished = 1;
        g_tracing.finished_count++;
    }

    agentos_mutex_unlock(&trace->lock);
    return AGENTOS_SUCCESS;
}

int tracing_add_span_attribute(const char* trace_id, const char* span_id,
                               const char* key, const char* value) {
    if (!trace_id || !span_id || !key || !value) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&g_tracing.global_lock);

    trace_t* trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        agentos_mutex_unlock(&g_tracing.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }

    agentos_mutex_lock(&trace->lock);
    agentos_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            span_t* span = &trace->spans[i];
            if (span->attribute_count < MAX_SPAN_ATTRIBUTES) {
                span->attributes[span->attribute_count].key = strdup(key);
                span->attributes[span->attribute_count].value = strdup(value);
                span->attribute_count++;
            }
            break;
        }
    }

    agentos_mutex_unlock(&trace->lock);
    return AGENTOS_SUCCESS;
}

int tracing_add_span_event(const char* trace_id, const char* span_id,
                           const char* event_name) {
    if (!trace_id || !span_id || !event_name) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&g_tracing.global_lock);

    trace_t* trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        agentos_mutex_unlock(&g_tracing.global_lock);
        return AGENTOS_ERR_NOT_FOUND;
    }

    agentos_mutex_lock(&trace->lock);
    agentos_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            span_t* span = &trace->spans[i];
            if (span->event_count < MAX_SPAN_EVENTS) {
                span->events[span->event_count].name = strdup(event_name);
                span->events[span->event_count].timestamp = (uint64_t)time(NULL) * 1000000;
                span->events[span->event_count].attribute_count = 0;
                span->event_count++;
            }
            break;
        }
    }

    agentos_mutex_unlock(&trace->lock);
    return AGENTOS_SUCCESS;
}

char* tracing_export_json(const char* trace_id) {
    if (!trace_id) return NULL;

    agentos_mutex_lock(&g_tracing.global_lock);

    trace_t* trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        agentos_mutex_unlock(&g_tracing.global_lock);
        return NULL;
    }

    agentos_mutex_lock(&trace->lock);
    agentos_mutex_unlock(&g_tracing.global_lock);

    char* buf = (char*)malloc(MAX_TRACE_EXPORT_SIZE);
    if (!buf) {
        agentos_mutex_unlock(&trace->lock);
        return NULL;
    }

    size_t pos = 0;
    pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos,
                    "{\"traceId\":\"%s\",\"sampled\":%s,\"spans\":[",
                    trace->trace_id, trace->sampled ? "true" : "false");

    for (size_t i = 0; i < trace->span_count; i++) {
        span_t* span = &trace->spans[i];
        if (i > 0) buf[pos++] = ',';

        pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos,
                       "{\"spanId\":\"%s\",\"operationName\":\"%s\","
                       "\"parentId\":\"%s\",\"kind\":%d,"
                       "\"startTime\":%llu,\"endTime\":%llu,"
                       "\"statusCode\":%d,\"statusMessage\":\"%s\"}",
                       span->span_id,
                       span->operation_name ? span->operation_name : "",
                       span->parent_span_id,
                       span->kind,
                       (unsigned long long)span->start_time,
                       (unsigned long long)span->end_time,
                       span->status_code,
                       span->status_message ? span->status_message : "");
    }

    pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos, "]}");

    agentos_mutex_unlock(&trace->lock);
    return buf;
}

size_t tracing_get_active_trace_count(void) {
    if (!g_tracing.initialized) return 0;
    size_t count = 0;
    agentos_mutex_lock(&g_tracing.global_lock);
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (!g_tracing.traces[i].finished) count++;
    }
    agentos_mutex_unlock(&g_tracing.global_lock);
    return count;
}

size_t tracing_get_finished_trace_count(void) {
    if (!g_tracing.initialized) return 0;
    return g_tracing.finished_count;
}
