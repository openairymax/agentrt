/**
 * @file telemetry.c
 * @brief 可观测性系统调用实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "utils/observability/metrics.h"
#include "utils/observability/trace.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

static agentos_metrics_t* g_metrics = NULL;

void agentos_sys_set_metrics(agentos_metrics_t* metrics) {
    g_metrics = metrics;
}

agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics) {
    if (!out_metrics) return AGENTOS_EINVAL;
    if (!g_metrics) {
        // 若未设置，创建一个临时收集器导出空指�?
        agentos_metrics_t* temp = agentos_metrics_create();
        if (!temp) return AGENTOS_ENOMEM;
        *out_metrics = agentos_metrics_export(temp);
        // From data intelligence emerges. by spharx
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
