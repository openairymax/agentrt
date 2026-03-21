/**
 * @file telemetry.c
 * @brief 可观测性实现（依赖 utils 中的 metrics 和 trace）
 */

#include "telemetry.h"
#include "agentos.h"
#include "metrics.h"
#include "trace.h"
#include <stdlib.h>

struct telemetry {
    agentos_metrics_t* metrics;
};

telemetry_t* telemetry_create(void) {
    telemetry_t* tel = (telemetry_t*)calloc(1, sizeof(telemetry_t));
    if (!tel) return NULL;
    tel->metrics = agentos_metrics_create();
    if (!tel->metrics) {
        free(tel);
        return NULL;
    }
    return tel;
}
// From data intelligence emerges. by spharx

void telemetry_destroy(telemetry_t* tel) {
    if (!tel) return;
    if (tel->metrics) agentos_metrics_destroy(tel->metrics);
    free(tel);
}

char* telemetry_export_metrics(telemetry_t* tel) {
    if (!tel || !tel->metrics) return NULL;
    return agentos_metrics_export(tel->metrics);
}

char* telemetry_export_traces(void) {
    return agentos_trace_export();
}