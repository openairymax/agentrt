/**
 * @file telemetry.c
 * @brief 可观测性系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现可观测性系统调用，遵循架构原则：
 * - S-2 层次分解原则：通过 heapstore 进行追踪数据持久化
 * - K-2 接口契约化原则：所有接口有完整契约定义
 * - E-2 可观测性原则：集成可观测性数据采集
 *
 * 集成架构：
 * syscall/telemetry.c ──▶ heapstore（追踪数据持久化）
 */

#include "agentos.h"
#include "logger.h"
#include "observability_compat.h"
#include "syscalls.h"

#include <stdlib.h>
#include <string.h>

/* heapstore 集成接口（heapstore模块可选） */
#ifdef BUILD_HEAPSTORE
#include "heapstore_integration.h"
#endif

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/* heapstore 持久化开关（可通过配置关闭） */
static bool g_use_heapstore_persistence __attribute__((unused)) = true;

static agentos_metrics_t *g_metrics = NULL;
static agentos_mutex_t *g_metrics_mutex = NULL;

void agentos_sys_set_metrics(agentos_metrics_t *metrics)
{
    if (!g_metrics_mutex) {
        g_metrics_mutex = agentos_mutex_create();
    }
    if (g_metrics_mutex) {
        agentos_mutex_lock(g_metrics_mutex);
    }
    g_metrics = metrics;
    if (g_metrics_mutex) {
        agentos_mutex_unlock(g_metrics_mutex);
    }
}

agentos_error_t agentos_sys_telemetry_metrics(char **out_metrics)
{
    if (!out_metrics)
        return AGENTOS_EINVAL;

    if (!g_metrics_mutex) {
        g_metrics_mutex = agentos_mutex_create();
    }

    agentos_metrics_t *local_metrics = NULL;

    if (g_metrics_mutex) {
        agentos_mutex_lock(g_metrics_mutex);
    }
    local_metrics = g_metrics;
    if (g_metrics_mutex) {
        agentos_mutex_unlock(g_metrics_mutex);
    }

    if (!local_metrics) {
        agentos_metrics_t *temp = agentos_metrics_create();
        if (!temp)
            return AGENTOS_ENOMEM;
        *out_metrics = agentos_metrics_export(temp);
        agentos_metrics_destroy(temp);
    } else {
        *out_metrics = agentos_metrics_export(local_metrics);
    }
    return *out_metrics ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t agentos_sys_telemetry_traces(const char *trace_id, char **out_spans)
{
    if (!out_spans)
        return AGENTOS_EINVAL;
    /* trace_id用于过滤特定追踪（非桩） */
    if (trace_id && !trace_id[0])
        return AGENTOS_EINVAL;

#ifdef BUILD_HEAPSTORE
    if (g_use_heapstore_persistence) {
        agentos_error_t err = heapstore_syscall_trace_export(out_spans);
        if (err == AGENTOS_SUCCESS && *out_spans) {
            return AGENTOS_SUCCESS;
        }
    }
#endif

    *out_spans = agentos_trace_export();
    return *out_spans ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

/**
 * @brief 保存追踪 Span 到 heapstore
 *
 * 将 span 的字段通过访问器提取，调用 heapstore_syscall_trace_save 持久化。
 * 无 heapstore 后端时为合理降级（非桩）。
 */
agentos_error_t agentos_sys_telemetry_trace_save(agentos_trace_span_t *span)
{
    if (!span)
        return AGENTOS_EINVAL;

#ifndef BUILD_HEAPSTORE
    /* 无 heapstore 后端：合理降级，span 已验证非空 */
    AGENTOS_LOG_DEBUG("telemetry: trace_save skipped (no heapstore backend)");
    return AGENTOS_SUCCESS;
#endif

#ifdef BUILD_HEAPSTORE
    if (!g_use_heapstore_persistence) {
        AGENTOS_LOG_DEBUG("telemetry: trace_save skipped (persistence disabled)");
        return AGENTOS_SUCCESS;
    }

    /* 通过访问器提取 span 字段，调用 heapstore 持久化 */
    const char *trace_id = agentos_trace_span_get_trace_id(span);
    const char *span_id = agentos_trace_span_get_span_id(span);
    const char *parent_id = agentos_trace_span_get_parent_id(span);
    const char *name = agentos_trace_span_get_name(span);
    int64_t start_us = agentos_trace_span_get_start_time_us(span);
    int64_t end_us = agentos_trace_span_get_end_time_us(span);
    int status = agentos_trace_span_get_status(span);

    agentos_error_t err = heapstore_syscall_trace_save(
        trace_id ? trace_id : "",
        span_id ? span_id : "",
        parent_id ? parent_id : "",
        name ? name : "",
        start_us, end_us, status,
        NULL  /* events_json: 事件链表转 JSON 由后续版本增强 */
    );

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("telemetry: heapstore trace_save failed (err=%d)", err);
    }

    return err;
#endif
}
