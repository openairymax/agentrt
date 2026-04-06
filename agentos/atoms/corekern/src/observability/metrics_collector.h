/**
 * @file metrics_collector.h
 * @brief 指标收集器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_METRICS_COLLECTOR_H
#define AGENTOS_METRICS_COLLECTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agentos_metric_type {
    AGENTOS_METRIC_COUNTER = 0,
    AGENTOS_METRIC_GAUGE = 1,
    AGENTOS_METRIC_HISTOGRAM = 2,
    AGENTOS_METRIC_SUMMARY = 3
} agentos_metric_type_t;

typedef struct agentos_metric_sample {
    const char* name;
    const char* labels;
    agentos_metric_type_t type;
    double value;
} agentos_metric_sample_t;

int agentos_metrics_collector_init(void);
void agentos_metrics_collector_cleanup(void);

int agentos_metric_counter_create(const char* name, const char* labels);
int agentos_metric_counter_inc(const char* name, const char* labels, double value);
int agentos_metric_gauge_create(const char* name, const char* labels, double initial_value);
int agentos_metric_gauge_set(const char* name, const char* labels, double value);
int agentos_metric_histogram_create(const char* name, const char* labels);
int agentos_metric_histogram_observe(const char* name, const char* labels, double value);
int agentos_metric_summary_create(const char* name, const char* labels);
int agentos_metric_summary_observe(const char* name, const char* labels, double value);
int agentos_metric_record(const agentos_metric_sample_t* sample);
int agentos_metric_unregister(const char* name, const char* labels);
int agentos_metric_get_update_count(const char* name, const char* labels);
int agentos_metrics_export_prometheus(char* buffer, size_t buffer_size);
int agentos_observability_reset_metrics(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_METRICS_COLLECTOR_H */
