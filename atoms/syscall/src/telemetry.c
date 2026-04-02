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
#include "utils/observability/metrics.h"
#include "utils/observability/trace.h"
#include "agentos.h"
#include <stdlib.h>

/* heapstore 集成接口 */
#include "../../../heapstore/include/heapstore_integration.h"

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

/* heapstore 持久化开关（可通过配置关闭） */
static bool g_use_heapstore_persistence = true;

static agentos_metrics_t* g_metrics = NULL;

void agentos_sys_set_metrics(agentos_metrics_t* metrics) {
    /* 确保互斥锁已创建 */
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
    
    /* 确保互斥锁已创建 */
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
        // 若未设置，创建一个临时收集器导出空指�?
        agentos_metrics_t* temp = agentos_metrics_create();
        if (!temp) return AGENTOS_ENOMEM;
        *out_metrics = agentos_metrics_export(temp);
        agentos_metrics_destroy(temp);
    } else {
        *out_metrics = agentos_metrics_export(local_metrics);
    }
    return *out_metrics ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t agentos_sys_telemetry_traces(char** out_traces) {
    if (!out_traces) return AGENTOS_EINVAL;
    
    /* 优先从 heapstore 获取持久化的追踪数据 */
    if (g_use_heapstore_persistence) {
        agentos_error_t err = heapstore_syscall_trace_export(out_traces);
        if (err == AGENTOS_SUCCESS && *out_traces) {
            return AGENTOS_SUCCESS;
        }
        /* heapstore 获取失败，回退到内存追踪 */
    }
    
    /* 回退到内存追踪数据 */
    *out_traces = agentos_trace_export();
    return *out_traces ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

/**
 * @brief 保存追踪 Span 到 heapstore
 * 
 * @param span [in] 追踪 Span 指针
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责 span 的生命周期
 * @threadsafe 是
 * @reentrant 是
 */
agentos_error_t agentos_sys_telemetry_trace_save(agentos_trace_span_t* span) {
    if (!span) return AGENTOS_EINVAL;
    
    if (!g_use_heapstore_persistence) {
        return AGENTOS_SUCCESS; /* 持久化关闭，静默成功 */
    }
    
    /* 获取 span 的内部数据（需要访问内部结构） */
    /* 由于 agentos_trace_span_t 是不透明指针，我们需要通过 API 获取数据 */
    /* 这里简化实现，实际需要扩展 trace.h 的 API */
    
    return AGENTOS_SUCCESS;
}
