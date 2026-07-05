/**
 * @file trace.c
 * @brief 责任链追踪器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "execution.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <string.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/**
 * @brief 追踪跨度
 */
typedef struct trace_span {
    char *span_id;
    char *trace_id;
    char *parent_id;
    char *name;
    uint64_t start_time;
    uint64_t end_time;
    struct trace_span *next;
} trace_span_t;

static trace_span_t *g_trace_spans = NULL;
static agentrt_mutex_t *g_trace_lock = NULL;
static agentrt_mutex_t *g_current_lock = NULL;
static trace_span_t *g_current_span = NULL;
static atomic_int g_trace_initialized = 0;

static agentrt_error_t ensure_trace_init(void)
{
    if (atomic_load_explicit(&g_trace_initialized, memory_order_acquire))
        return AGENTRT_SUCCESS;

    if (!g_trace_lock) {
        g_trace_lock = agentrt_mutex_create();
        if (!g_trace_lock)
            return AGENTRT_ENOMEM;
    }
    if (!g_current_lock) {
        g_current_lock = agentrt_mutex_create();
        if (!g_current_lock) {
            agentrt_mutex_free(g_trace_lock);
            g_trace_lock = NULL;
            return AGENTRT_ENOMEM;
        }
    }
    atomic_store_explicit(&g_trace_initialized, 1, memory_order_seq_cst);
    return AGENTRT_SUCCESS;
}

static atomic_uint64_t span_counter = 0;
static void generate_span_id(char *buf, size_t len)
{
    uint64_t id = atomic_fetch_add_explicit(&span_counter, 1, memory_order_seq_cst);
    snprintf(buf, len, "span_%llu", (unsigned long long)id);
}

agentrt_error_t agentrt_trace_init(void)
{
    if (!g_trace_lock) {
        g_trace_lock = agentrt_mutex_create();
        if (!g_trace_lock)
            return AGENTRT_ENOMEM;
    }
    if (!g_current_lock) {
        g_current_lock = agentrt_mutex_create();
        if (!g_current_lock) {
            agentrt_mutex_free(g_trace_lock);
            g_trace_lock = NULL;
            return AGENTRT_ENOMEM;
        }
    }
    return AGENTRT_SUCCESS;
}

void agentrt_trace_shutdown(void)
{
    if (!g_trace_lock)
        return;
    agentrt_mutex_lock(g_trace_lock);
    trace_span_t *span = g_trace_spans;
    while (span) {
        trace_span_t *next = span->next;
        if (span->span_id)
            AGENTRT_FREE(span->span_id);
        if (span->trace_id)
            AGENTRT_FREE(span->trace_id);
        if (span->parent_id)
            AGENTRT_FREE(span->parent_id);
        if (span->name)
            AGENTRT_FREE(span->name);
        AGENTRT_FREE(span);
        span = next;
    }
    g_trace_spans = NULL;
    agentrt_mutex_unlock(g_trace_lock);
    agentrt_mutex_free(g_trace_lock);
    g_trace_lock = NULL;

    agentrt_mutex_free(g_current_lock);
    g_current_lock = NULL;
    g_current_span = NULL;
}

agentrt_error_t agentrt_trace_start_span(const char *name, const char *trace_id,
                                         const char *parent_id, char **out_span_id)
{
    if (!name || !out_span_id)
        return AGENTRT_EINVAL;

    agentrt_error_t err = ensure_trace_init();
    if (err != AGENTRT_SUCCESS)
        return err;

    trace_span_t *span = (trace_span_t *)AGENTRT_CALLOC(1, sizeof(trace_span_t));
    if (!span)
        return AGENTRT_ENOMEM;

    char id_buf[64];
    generate_span_id(id_buf, sizeof(id_buf));
    span->span_id = AGENTRT_STRDUP(id_buf);
    span->trace_id = trace_id ? AGENTRT_STRDUP(trace_id) : AGENTRT_STRDUP(id_buf);
    span->parent_id = parent_id ? AGENTRT_STRDUP(parent_id) : NULL;
    span->name = AGENTRT_STRDUP(name);
    span->start_time = agentrt_time_monotonic_ns();
    span->end_time = 0;

    if (!span->span_id || !span->trace_id || !span->name) {
        if (span->span_id)
            AGENTRT_FREE(span->span_id);
        if (span->trace_id)
            AGENTRT_FREE(span->trace_id);
        if (span->parent_id)
            AGENTRT_FREE(span->parent_id);
        if (span->name)
            AGENTRT_FREE(span->name);
        AGENTRT_FREE(span);
        return AGENTRT_ENOMEM;
    }

    agentrt_mutex_lock(g_trace_lock);
    span->next = g_trace_spans;
    g_trace_spans = span;

    g_current_span = span;
    agentrt_mutex_unlock(g_trace_lock);

    *out_span_id = AGENTRT_STRDUP(span->span_id);
    return *out_span_id ? AGENTRT_SUCCESS : AGENTRT_ENOMEM;
}

agentrt_error_t agentrt_trace_end_span(const char *span_id)
{
    if (!span_id)
        return AGENTRT_EINVAL;

    agentrt_error_t err = ensure_trace_init();
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_mutex_lock(g_trace_lock);
    trace_span_t *span = g_trace_spans;
    while (span) {
        if (strcmp(span->span_id, span_id) == 0) {
            span->end_time = agentrt_time_monotonic_ns();
            agentrt_mutex_unlock(g_trace_lock);
            return AGENTRT_SUCCESS;
        }
        span = span->next;
    }
    agentrt_mutex_unlock(g_trace_lock);
    return AGENTRT_ENOENT;
}

agentrt_error_t agentrt_trace_get_current_span(char **out_span_id)
{
    if (!out_span_id)
        return AGENTRT_EINVAL;

    agentrt_error_t err = ensure_trace_init();
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_mutex_lock(g_trace_lock);
    if (g_current_span) {
        *out_span_id = AGENTRT_STRDUP(g_current_span->span_id);
        agentrt_mutex_unlock(g_trace_lock);
        return *out_span_id ? AGENTRT_SUCCESS : AGENTRT_ENOMEM;
    } else {
        agentrt_mutex_unlock(g_trace_lock);
        *out_span_id = NULL;
        return AGENTRT_ENOENT;
    }
}
