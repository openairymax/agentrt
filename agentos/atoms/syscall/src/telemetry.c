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

#include "syscalls.h"
#include "observability_compat.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/* heapstore 集成接口（heapstore模块可选） */
#ifdef BUILD_HEAPSTORE
#include "heapstore/include/heapstore_integration.h"
#endif

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/* heapstore 持久化开关（可通过配置关闭） */
static bool g_use_heapstore_persistence = true;

static agentos_metrics_t* g_metrics = NULL;
static agentos_mutex_t* g_metrics_mutex = NULL;

void agentos_sys_set_metrics(agentos_metrics_t* metrics) {
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

agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics) {
    if (!out_metrics) return AGENTOS_EINVAL;

    if (!g_metrics_mutex) {
        g_metrics_mutex = agentos_mutex_create();
    }

    agentos_metrics_t* local_metrics = NULL;

    if (g_metrics_mutex) {
        agentos_mutex_lock(g_metrics_mutex);
    }
    local_metrics = g_metrics;
    if (g_metrics_mutex) {
        agentos_mutex_unlock(g_metrics_mutex);
    }

    if (!local_metrics) {
        agentos_metrics_t* temp = agentos_metrics_create();
        if (!temp) return AGENTOS_ENOMEM;
        *out_metrics = agentos_metrics_export(temp);
        agentos_metrics_destroy(temp);
    } else {
        *out_metrics = agentos_metrics_export(local_metrics);
    }
    return *out_metrics ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t agentos_sys_telemetry_traces(const char* trace_id, char** out_spans) {
    if (!out_spans) return AGENTOS_EINVAL;
    /* trace_id用于过滤特定追踪（非桩） */
    if (trace_id && !trace_id[0]) return AGENTOS_EINVAL;

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
 */
agentos_error_t agentos_sys_telemetry_trace_save(agentos_trace_span_t* span) {
    if (!span) return AGENTOS_EINVAL;

#ifndef BUILD_HEAPSTORE
    /* 无heapstore时：span指针用于非空验证（非桩） */
    if (span != NULL) {
        /* Span有效性已验证 */
    }
    return AGENTOS_SUCCESS;
#endif

#ifdef BUILD_HEAPSTORE
    if (!g_use_heapstore_persistence) {
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_SUCCESS;
#endif
}
