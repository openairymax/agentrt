/**
 * @file telemetry.c
 * @brief 可观测性系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "syscalls.h"
#include "utils/observability/metrics.h"
#include "utils/observability/trace.h"
#include "agentos.h"
#include <stdlib.h>

static agentos_metrics_t* g_metrics = NULL;

void agentos_sys_set_metrics(agentos_metrics_t* metrics) {
    g_metrics = metrics;
}

agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics) {
    if (!out_metrics) return AGENTOS_EINVAL;
    if (!g_metrics) {
        // 若未设置，创建一个临时收集器导出空指标
        agentos_metrics_t* temp = agentos_metrics_create();
        if (!temp) return AGENTOS_ENOMEM;
        *out_metrics = agentos_metrics_export(temp);
        agentos_metrics_destroy(temp);
    } else {
        *out_metrics = agentos_metrics_export(g_metrics);
    }
    return *out_metrics ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t agentos_sys_telemetry_traces(char** out_traces) {
    if (!out_traces) return AGENTOS_EINVAL;
    *out_traces = agentos_trace_export();
    return *out_traces ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}