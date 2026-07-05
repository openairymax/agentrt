#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
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
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum { SAMPLE_ALWAYS, SAMPLE_PROBABILISTIC, SAMPLE_RATE_LIMITED } sampling_strategy_t;

typedef struct {
    char *key;
    char *value;
} span_attribute_t;

typedef struct {
    char *name;
    uint64_t timestamp;
    span_attribute_t attributes[MAX_SPAN_ATTRIBUTES];
    size_t attribute_count;
} span_event_t;

typedef struct {
    char span_id[SPAN_ID_LEN];
    char parent_span_id[SPAN_ID_LEN];
    char *operation_name;
    span_kind_t kind;
    uint64_t start_time;
    uint64_t end_time;
    int status_code;
    char *status_message;
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
    agentrt_mutex_t lock;
} trace_t;

static struct {
    trace_t traces[MAX_TRACES];
    size_t trace_count;
    size_t finished_count;
    agentrt_mutex_t global_lock;
    sampling_strategy_t default_sampling;
    double default_sampling_rate;
    uint32_t rate_limit_per_second;
    uint64_t last_rate_check;
    uint32_t rate_counter;
    int initialized;
} g_tracing = {0};

static void generate_hex_id(char *buf, size_t buf_len)
{
    static const char hex[] = "0123456789abcdef";
    uint64_t t = (uint64_t)time(NULL);
    uint64_t r = (uint64_t)agentrt_random_uint32(0, UINT32_MAX);
    for (size_t i = 0; i < buf_len - 1; i++) {
        buf[i] = hex[(t ^ r ^ (i * 17 + 3)) & 0xF];
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
    }
    buf[buf_len - 1] = '\0';
}

static bool should_sample(void)
{
    switch (g_tracing.default_sampling) {
    case SAMPLE_ALWAYS:
        return true;
    case SAMPLE_PROBABILISTIC:
        return ((double)agentrt_random_float()) < g_tracing.default_sampling_rate;
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

int tracing_init(sampling_strategy_t strategy, double rate)
{
    if (g_tracing.initialized) {
        return AGENTRT_SUCCESS;
    }

    agentrt_mutex_init(&g_tracing.global_lock);
    g_tracing.trace_count = 0;
    g_tracing.finished_count = 0;
    g_tracing.default_sampling = strategy;
    g_tracing.default_sampling_rate = rate > 0 ? rate : 1.0;
    g_tracing.rate_limit_per_second = 100;
    g_tracing.last_rate_check = 0;
    g_tracing.rate_counter = 0;
    g_tracing.initialized = 1;

    for (size_t i = 0; i < MAX_TRACES; i++) {
        agentrt_mutex_init(&g_tracing.traces[i].lock);
    }

    SVC_LOG_INFO("Tracing system initialized (strategy=%d, rate=%.2f)", strategy, rate);
    return AGENTRT_SUCCESS;
}

void tracing_shutdown(void)
{
    if (!g_tracing.initialized)
        return;

    agentrt_mutex_lock(&g_tracing.global_lock);

    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        trace_t *trace = &g_tracing.traces[i];
        agentrt_mutex_lock(&trace->lock);
        for (size_t j = 0; j < trace->span_count; j++) {
            span_t *span = &trace->spans[j];
            AGENTRT_FREE(span->operation_name);
            AGENTRT_FREE(span->status_message);
            for (size_t k = 0; k < span->attribute_count; k++) {
                AGENTRT_FREE(span->attributes[k].key);
                AGENTRT_FREE(span->attributes[k].value);
            }
            for (size_t k = 0; k < span->event_count; k++) {
                AGENTRT_FREE(span->events[k].name);
                for (size_t l = 0; l < span->events[k].attribute_count; l++) {
                    AGENTRT_FREE(span->events[k].attributes[l].key);
                    AGENTRT_FREE(span->events[k].attributes[l].value);
                }
            }
        }
        agentrt_mutex_unlock(&trace->lock);
        agentrt_mutex_destroy(&trace->lock);
    }

    g_tracing.trace_count = 0;
    g_tracing.finished_count = 0;
    g_tracing.initialized = 0;

    agentrt_mutex_unlock(&g_tracing.global_lock);
    agentrt_mutex_destroy(&g_tracing.global_lock);
}

int tracing_start_trace(const char *operation_name, const char *parent_trace_id, char *out_trace_id,
                        size_t trace_id_buf_len)
{
    if (!operation_name || !out_trace_id) {
        SVC_LOG_ERROR("tracing_start_trace: NULL parameter (operation_name=%p, out_trace_id=%p)", (const void *)operation_name, (const void *)out_trace_id);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    if (!g_tracing.initialized) {
        tracing_init(SAMPLE_ALWAYS, 1.0);
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

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

        trace_t *old = &g_tracing.traces[oldest];
        agentrt_mutex_lock(&old->lock);
        for (size_t j = 0; j < old->span_count; j++) {
            AGENTRT_FREE(old->spans[j].operation_name);
            AGENTRT_FREE(old->spans[j].status_message);
        }
        old->span_count = 0;
        old->finished = 0;
        agentrt_mutex_unlock(&old->lock);

        if (oldest != g_tracing.trace_count - 1) {
            trace_t temp = g_tracing.traces[oldest];
            g_tracing.traces[oldest] = g_tracing.traces[g_tracing.trace_count - 1];
            g_tracing.traces[g_tracing.trace_count - 1] = temp;
        }
        g_tracing.trace_count--;
    }

    trace_t *trace = &g_tracing.traces[g_tracing.trace_count];
    __builtin_memset(trace, 0, sizeof(trace_t));
    agentrt_mutex_init(&trace->lock);

    if (parent_trace_id) {
AGENTRT_STRNCPY_TERM(trace->trace_id, parent_trace_id, TRACE_ID_LEN);
    } else {
        generate_hex_id(trace->trace_id, TRACE_ID_LEN);
    }

    trace->span_count = 0;
    trace->sampling = g_tracing.default_sampling;
    trace->sampling_rate = g_tracing.default_sampling_rate;
    trace->sampled = should_sample() ? 1 : 0;
    trace->finished = 0;

    span_t *root_span = &trace->spans[0];
    generate_hex_id(root_span->span_id, SPAN_ID_LEN);
    root_span->parent_span_id[0] = '\0';
    root_span->operation_name = AGENTRT_STRDUP(operation_name);
    if (!root_span->operation_name) {
        SVC_LOG_ERROR("tracing_start_trace: strdup failed for operation_name (operation=%s)", operation_name);
        g_tracing.traces[g_tracing.trace_count].trace_id[0] = '\0';
        return AGENTRT_ERR_OUT_OF_MEMORY;
    }
    root_span->kind = SPAN_KIND_SERVER;
    root_span->start_time = (uint64_t)time(NULL) * 1000000;
    root_span->end_time = 0;
    root_span->status_code = 0;
    root_span->status_message = NULL;
    root_span->attribute_count = 0;
    root_span->event_count = 0;
    trace->span_count = 1;

    g_tracing.trace_count++;

AGENTRT_STRNCPY_TERM(out_trace_id, trace->trace_id, trace_id_buf_len);

    agentrt_mutex_unlock(&g_tracing.global_lock);

    SVC_LOG_DEBUG("Trace started: %s (operation=%s, sampled=%d)", trace->trace_id, operation_name,
                  trace->sampled);
    return AGENTRT_SUCCESS;
}

int tracing_start_span(const char *trace_id, const char *operation_name, span_kind_t kind,
                       char *out_span_id, size_t span_id_buf_len)
{
    if (!trace_id || !operation_name || !out_span_id) {
        SVC_LOG_ERROR("tracing_start_span: NULL parameter (trace_id=%p, operation_name=%p, out_span_id=%p)", (const void *)trace_id, (const void *)operation_name, (const void *)out_span_id);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

    trace_t *trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        SVC_LOG_ERROR("tracing_start_span: trace not found (trace_id=%s)", trace_id ? trace_id : "NULL");
        agentrt_mutex_unlock(&g_tracing.global_lock);
        return AGENTRT_ERR_NOT_FOUND;
    }

    agentrt_mutex_lock(&trace->lock);
    agentrt_mutex_unlock(&g_tracing.global_lock);

    if (trace->span_count >= MAX_SPANS_PER_TRACE) {
        SVC_LOG_ERROR("tracing_start_span: max spans exceeded for trace (trace_id=%s, span_count=%zu, max=%d)", trace_id, trace->span_count, MAX_SPANS_PER_TRACE);
        agentrt_mutex_unlock(&trace->lock);
        return AGENTRT_ERR_OVERFLOW;
    }

    span_t *span = &trace->spans[trace->span_count];
    generate_hex_id(span->span_id, SPAN_ID_LEN);

    if (trace->span_count > 0) {
AGENTRT_STRNCPY_TERM(span->parent_span_id, trace->spans[trace->span_count - 1].span_id, SPAN_ID_LEN);
    } else {
        span->parent_span_id[0] = '\0';
    }

    span->operation_name = AGENTRT_STRDUP(operation_name);
    span->kind = kind;
    span->start_time = (uint64_t)time(NULL) * 1000000;
    span->end_time = 0;
    span->status_code = 0;
    span->status_message = NULL;
    span->attribute_count = 0;
    span->event_count = 0;
    trace->span_count++;

AGENTRT_STRNCPY_TERM(out_span_id, span->span_id, span_id_buf_len);

    agentrt_mutex_unlock(&trace->lock);
    return AGENTRT_SUCCESS;
}

int tracing_end_span(const char *trace_id, const char *span_id, int status_code,
                     const char *status_message)
{
    if (!trace_id || !span_id) {
        SVC_LOG_ERROR("tracing_end_span: NULL parameter (trace_id=%p, span_id=%p)", (const void *)trace_id, (const void *)span_id);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

    trace_t *trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        SVC_LOG_ERROR("tracing_end_span: trace not found (trace_id=%s)", trace_id ? trace_id : "NULL");
        agentrt_mutex_unlock(&g_tracing.global_lock);
        return AGENTRT_ERR_NOT_FOUND;
    }

    agentrt_mutex_lock(&trace->lock);
    agentrt_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            trace->spans[i].end_time = (uint64_t)time(NULL) * 1000000;
            trace->spans[i].status_code = status_code;
            trace->spans[i].status_message = status_message ? AGENTRT_STRDUP(status_message) : NULL;
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

    agentrt_mutex_unlock(&trace->lock);
    return AGENTRT_SUCCESS;
}

int tracing_add_span_attribute(const char *trace_id, const char *span_id, const char *key,
                               const char *value)
{
    if (!trace_id || !span_id || !key || !value) {
        SVC_LOG_ERROR("tracing_add_span_attribute: NULL parameter (trace_id=%p, span_id=%p, key=%p, value=%p)", (const void *)trace_id, (const void *)span_id, (const void *)key, (const void *)value);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

    trace_t *trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        SVC_LOG_ERROR("tracing_add_span_attribute: trace not found (trace_id=%s)", trace_id ? trace_id : "NULL");
        agentrt_mutex_unlock(&g_tracing.global_lock);
        return AGENTRT_ERR_NOT_FOUND;
    }

    agentrt_mutex_lock(&trace->lock);
    agentrt_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            span_t *span = &trace->spans[i];
            if (span->attribute_count < MAX_SPAN_ATTRIBUTES) {
                char *dup_key = AGENTRT_STRDUP(key);
                char *dup_value = AGENTRT_STRDUP(value);
                if (dup_key && dup_value) {
                    span->attributes[span->attribute_count].key = dup_key;
                    span->attributes[span->attribute_count].value = dup_value;
                    span->attribute_count++;
                } else {
                    AGENTRT_FREE(dup_key);
                    AGENTRT_FREE(dup_value);
                }
            }
            break;
        }
    }

    agentrt_mutex_unlock(&trace->lock);
    return AGENTRT_SUCCESS;
}

int tracing_add_span_event(const char *trace_id, const char *span_id, const char *event_name)
{
    if (!trace_id || !span_id || !event_name) {
        SVC_LOG_ERROR("tracing_add_span_event: NULL parameter (trace_id=%p, span_id=%p, event_name=%p)", (const void *)trace_id, (const void *)span_id, (const void *)event_name);
        return AGENTRT_ERR_INVALID_PARAM;
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

    trace_t *trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        SVC_LOG_ERROR("tracing_add_span_event: trace not found (trace_id=%s)", trace_id ? trace_id : "NULL");
        agentrt_mutex_unlock(&g_tracing.global_lock);
        return AGENTRT_ERR_NOT_FOUND;
    }

    agentrt_mutex_lock(&trace->lock);
    agentrt_mutex_unlock(&g_tracing.global_lock);

    for (size_t i = 0; i < trace->span_count; i++) {
        if (strcmp(trace->spans[i].span_id, span_id) == 0) {
            span_t *span = &trace->spans[i];
            if (span->event_count < MAX_SPAN_EVENTS) {
                char *dup_name = AGENTRT_STRDUP(event_name);
                if (dup_name) {
                    span->events[span->event_count].name = dup_name;
                    span->events[span->event_count].timestamp = (uint64_t)time(NULL) * 1000000;
                    span->events[span->event_count].attribute_count = 0;
                    span->event_count++;
                }
            }
            break;
        }
    }

    agentrt_mutex_unlock(&trace->lock);
    return AGENTRT_SUCCESS;
}

char *tracing_export_json(const char *trace_id)
{
    if (!trace_id) {
        SVC_LOG_ERROR("tracing_export_json: NULL trace_id parameter");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    agentrt_mutex_lock(&g_tracing.global_lock);

    trace_t *trace = NULL;
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (strcmp(g_tracing.traces[i].trace_id, trace_id) == 0) {
            trace = &g_tracing.traces[i];
            break;
        }
    }

    if (!trace) {
        SVC_LOG_ERROR("tracing_export_json: trace not found (trace_id=%s)", trace_id ? trace_id : "NULL");
        agentrt_mutex_unlock(&g_tracing.global_lock);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_mutex_lock(&trace->lock);
    agentrt_mutex_unlock(&g_tracing.global_lock);

    char *buf = (char *)AGENTRT_MALLOC(MAX_TRACE_EXPORT_SIZE);
    if (!buf) {
        SVC_LOG_ERROR("tracing_export_json: malloc failed for export buffer (size=%d)", MAX_TRACE_EXPORT_SIZE);
        agentrt_mutex_unlock(&trace->lock);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    size_t pos = 0;
    pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos,
                    "{\"traceId\":\"%s\",\"sampled\":%s,\"spans\":[", trace->trace_id,
                    trace->sampled ? "true" : "false");

    for (size_t i = 0; i < trace->span_count; i++) {
        span_t *span = &trace->spans[i];
        if (i > 0)
            buf[pos++] = ',';

        pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos,
                        "{\"spanId\":\"%s\",\"operationName\":\"%s\","
                        "\"parentId\":\"%s\",\"kind\":%d,"
                        "\"startTime\":%llu,\"endTime\":%llu,"
                        "\"statusCode\":%d,\"statusMessage\":\"%s\"}",
                        span->span_id, span->operation_name ? span->operation_name : "",
                        span->parent_span_id, span->kind, (unsigned long long)span->start_time,
                        (unsigned long long)span->end_time, span->status_code,
                        span->status_message ? span->status_message : "");
    }

    pos += snprintf(buf + pos, MAX_TRACE_EXPORT_SIZE - pos, "]}");

    agentrt_mutex_unlock(&trace->lock);
    return buf;
}

size_t tracing_get_active_trace_count(void)
{
    if (!g_tracing.initialized)
        return 0;
    size_t count = 0;
    agentrt_mutex_lock(&g_tracing.global_lock);
    for (size_t i = 0; i < g_tracing.trace_count; i++) {
        if (!g_tracing.traces[i].finished)
            count++;
    }
    agentrt_mutex_unlock(&g_tracing.global_lock);
    return count;
}

size_t tracing_get_finished_trace_count(void)
{
    if (!g_tracing.initialized)
        return 0;
    return g_tracing.finished_count;
}
