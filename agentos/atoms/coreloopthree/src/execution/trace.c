/**
 * @file trace.c
 * @brief 责任链追踪器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/utils/include/atomic_compat.h>

/**
 * @brief 追踪跨度
 */
typedef struct trace_span {
    char* span_id;
    char* trace_id;
    char* parent_id;
    char* name;
    uint64_t start_time;
    uint64_t end_time;
    struct trace_span* next;
} trace_span_t;

static trace_span_t* g_trace_spans = NULL;
static agentos_mutex_t* g_trace_lock = NULL;
static agentos_mutex_t* g_current_lock = NULL;
static trace_span_t* g_current_span = NULL;
static volatile int g_trace_initialized = 0;

/**
 * @brief 确保trace系统已初始化（线程安全）
 * @return AGENTOS_SUCCESS 成功，AGENTOS_ENOMEM 内存不足
 */
static agentos_error_t ensure_trace_init(void) {
    if (g_trace_initialized) return AGENTOS_SUCCESS;

    if (!g_trace_lock) {
        g_trace_lock = agentos_mutex_create();
        if (!g_trace_lock) return AGENTOS_ENOMEM;
    }
    if (!g_current_lock) {
        g_current_lock = agentos_mutex_create();
        if (!g_current_lock) {
            agentos_mutex_destroy(g_trace_lock);
            g_trace_lock = NULL;
            return AGENTOS_ENOMEM;
        }
    }
    g_trace_initialized = 1;
    return AGENTOS_SUCCESS;
}

#ifdef _WIN32
static volatile LONG span_counter = 0;
static void generate_span_id(char* buf, size_t len) {
    LONG id = InterlockedIncrement(&span_counter);
    snprintf(buf, len, "span_%ld", id);
}
#else
static atomic_uint64_t span_counter = 0;
static void generate_span_id(char* buf, size_t len) {
    uint64_t id = (uint64_t)atomic_fetch_add(&span_counter, 1);
    snprintf(buf, len, "span_%llu", (unsigned long long)id);
}
#endif

agentos_error_t agentos_trace_init(void) {
    if (!g_trace_lock) {
        g_trace_lock = agentos_mutex_create();
        if (!g_trace_lock) return AGENTOS_ENOMEM;
    }
    if (!g_current_lock) {
        g_current_lock = agentos_mutex_create();
        if (!g_current_lock) {
            agentos_mutex_destroy(g_trace_lock);
            g_trace_lock = NULL;
            return AGENTOS_ENOMEM;
        }
    }
    return AGENTOS_SUCCESS;
}

void agentos_trace_shutdown(void) {
    if (!g_trace_lock) return;
    agentos_mutex_lock(g_trace_lock);
    trace_span_t* span = g_trace_spans;
    while (span) {
        trace_span_t* next = span->next;
        if (span->span_id) AGENTOS_FREE(span->span_id);
        if (span->trace_id) AGENTOS_FREE(span->trace_id);
        if (span->parent_id) AGENTOS_FREE(span->parent_id);
        if (span->name) AGENTOS_FREE(span->name);
        AGENTOS_FREE(span);
        span = next;
    }
    g_trace_spans = NULL;
    agentos_mutex_unlock(g_trace_lock);
    agentos_mutex_destroy(g_trace_lock);
    g_trace_lock = NULL;

    agentos_mutex_destroy(g_current_lock);
    g_current_lock = NULL;
    g_current_span = NULL;
}

agentos_error_t agentos_trace_start_span(const char* name, const char* trace_id, const char* parent_id, char** out_span_id) {
    if (!name || !out_span_id) return AGENTOS_EINVAL;

    agentos_error_t err = ensure_trace_init();
    if (err != AGENTOS_SUCCESS) return err;

    trace_span_t* span = (trace_span_t*)AGENTOS_CALLOC(1, sizeof(trace_span_t));
    if (!span) return AGENTOS_ENOMEM;

    char id_buf[64];
    generate_span_id(id_buf, sizeof(id_buf));
    span->span_id = AGENTOS_STRDUP(id_buf);
    span->trace_id = trace_id ? AGENTOS_STRDUP(trace_id) : AGENTOS_STRDUP(id_buf);
    span->parent_id = parent_id ? AGENTOS_STRDUP(parent_id) : NULL;
    span->name = AGENTOS_STRDUP(name);
    span->start_time = agentos_time_monotonic_ns();
    span->end_time = 0;

    if (!span->span_id || !span->trace_id || !span->name) {
        if (span->span_id) AGENTOS_FREE(span->span_id);
        if (span->trace_id) AGENTOS_FREE(span->trace_id);
        if (span->parent_id) AGENTOS_FREE(span->parent_id);
        if (span->name) AGENTOS_FREE(span->name);
        AGENTOS_FREE(span);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(g_trace_lock);
    span->next = g_trace_spans;
    g_trace_spans = span;

    g_current_span = span;
    agentos_mutex_unlock(g_trace_lock);

    *out_span_id = AGENTOS_STRDUP(span->span_id);
    return *out_span_id ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t agentos_trace_end_span(const char* span_id) {
    if (!span_id) return AGENTOS_EINVAL;

    agentos_error_t err = ensure_trace_init();
    if (err != AGENTOS_SUCCESS) return err;

    agentos_mutex_lock(g_trace_lock);
    trace_span_t* span = g_trace_spans;
    while (span) {
        if (strcmp(span->span_id, span_id) == 0) {
            span->end_time = agentos_time_monotonic_ns();
            agentos_mutex_unlock(g_trace_lock);
            return AGENTOS_SUCCESS;
        }
        span = span->next;
    }
    agentos_mutex_unlock(g_trace_lock);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_trace_get_current_span(char** out_span_id) {
    if (!out_span_id) return AGENTOS_EINVAL;

    agentos_error_t err = ensure_trace_init();
    if (err != AGENTOS_SUCCESS) return err;

    agentos_mutex_lock(g_trace_lock);
    if (g_current_span) {
        *out_span_id = AGENTOS_STRDUP(g_current_span->span_id);
        agentos_mutex_unlock(g_trace_lock);
        return *out_span_id ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    } else {
        agentos_mutex_unlock(g_trace_lock);
        *out_span_id = NULL;
        return AGENTOS_ENOENT;
    }
}
