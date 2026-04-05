/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_metrics.h - Performance Metrics Export: Prometheus Format
 *
 * Design Principles:
 * - Standard Format: Prometheus exposition format
 * - Low Overhead: Atomic operations, lock-free sampling
 * - Configurable: Sampling interval, retention window
 * - Multi-Dimensional: Support for labels
 *
 * Exported Metrics:
 * - cupolas_permissions_total{agent, action, resource, result}
 * - cupolas_permissions_duration_seconds{agent, action, resource}
 * - cupolas_sanitizer_input_total{type, result}
 * - cupolas_sanitizer_duration_seconds{type}
 * - cupolas_workbench_execution_total{command, result}
 * - cupolas_workbench_duration_seconds{command}
 * - cupolas_workbench_memory_bytes{command}
 * - cupolas_workbench_cpu_seconds{command}
 * - cupolas_errors_total{module, error_type}
 */

#ifndef cupolas_METRICS_H
#define cupolas_METRICS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Metric Type */
typedef enum metric_type {
    METRIC_TYPE_COUNTER = 0,
    METRIC_TYPE_GAUGE,
    METRIC_TYPE_HISTOGRAM,
    METRIC_TYPE_SUMMARY
} metric_type_t;

/* Metric Descriptor */
typedef struct metric_desc {
    const char* name;
    const char* help;
    metric_type_t type;
    const char* const* label_names;
    size_t label_count;
    double* buckets;
    size_t bucket_count;
} metric_desc_t;

/* Metric Value */
typedef struct metric_value {
    double value;
    uint64_t timestamp_ns;
} metric_value_t;

/* Histogram Bucket */
typedef struct histogram_bucket {
    double cumulative_count;
    double sum;
} histogram_bucket_t;

/* Metric Sample */
typedef struct metric_sample {
    const char* name;
    const char** label_values;
    size_t label_count;
    double value;
    uint64_t timestamp_ns;
} metric_sample_t;

/* Metric Iterator Handle */
typedef struct metric_iterator metric_iterator_t;

/**
 * @brief Initialize metrics system
 * @param[in] sampling_interval_ms Sampling interval in milliseconds
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No (init/shutdown must be paired)
 */
int metrics_init(uint32_t sampling_interval_ms);

/**
 * @brief Shutdown metrics system
 * @note Thread-safe: No, ensure no other threads access metrics
 * @reentrant No
 */
void metrics_shutdown(void);

/**
 * @brief Register metric descriptor
 * @param[in] desc Metric descriptor (caller retains ownership)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership desc: caller retains ownership
 */
int metrics_register(const metric_desc_t* desc);

/**
 * @brief Increment counter
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] count Increment value
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_counter_inc(const char* name, const char** label_values, double count);

/**
 * @brief Set gauge value
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] value Gauge value
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_gauge_set(const char* name, const char** label_values, double value);

/**
 * @brief Add to gauge
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] value Value to add
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_gauge_add(const char* name, const char** label_values, double value);

/**
 * @brief Subtract from gauge
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] value Value to subtract
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_gauge_sub(const char* name, const char** label_values, double value);

/**
 * @brief Observe histogram value
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] value Observed value
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_histogram_observe(const char* name, const char** label_values, double value);

/**
 * @brief Observe summary value
 * @param[in] name Metric name (must not be NULL)
 * @param[in] label_values Label values array (may be NULL if no labels)
 * @param[in] value Observed value
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership name, label_values: caller retains ownership
 */
void metrics_summary_observe(const char* name, const char** label_values, double value);

/**
 * @brief Get current timestamp in nanoseconds
 * @return Current timestamp in nanoseconds since epoch
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
uint64_t metrics_get_timestamp_ns(void);

/**
 * @brief Create metric iterator
 * @param[in] pattern Match pattern (NULL for all metrics)
 * @return Iterator handle, NULL on failure
 * @note Thread-safe: Yes
 * @reentrant No
 * @ownership Returned handle: caller owns, must call metrics_iter_destroy
 */
metric_iterator_t* metrics_iter_create(const char* pattern);

/**
 * @brief Get next sample from iterator
 * @param[in] iter Iterator handle (must not be NULL)
 * @param[out] sample Sample output (must not be NULL)
 * @return true if more samples available, false if done
 * @note Thread-safe: No, iterator is single-threaded
 * @reentrant No
 * @ownership sample: callee writes, caller owns
 */
bool metrics_iter_next(metric_iterator_t* iter, metric_sample_t* sample);

/**
 * @brief Destroy iterator
 * @param[in] iter Iterator handle (must not be NULL)
 * @note Thread-safe: No
 * @reentrant No
 * @ownership iter: caller transfers ownership
 */
void metrics_iter_destroy(metric_iterator_t* iter);

/**
 * @brief Export all metrics in Prometheus format
 * @param[out] buffer Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Bytes written on success, 0 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buffer: caller owns
 */
size_t metrics_export_prometheus(char* buffer, size_t size);

/**
 * @brief Export all metrics in JSON format
 * @param[out] buffer Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Bytes written on success, 0 on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buffer: caller owns
 */
size_t metrics_export_json(char* buffer, size_t size);

/**
 * @brief Reset all metrics
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void metrics_reset(void);

/**
 * @brief Get metric count
 * @return Number of registered metrics
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
size_t metrics_get_count(void);

/**
 * @brief Get sampling interval
 * @return Sampling interval in milliseconds
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
uint32_t metrics_get_sampling_interval(void);

/* ============================================================================
 * Predefined Metrics
 * ============================================================================ */

/* Permission System Metrics */
extern const char* METRIC_PERMISSIONS_TOTAL;
extern const char* METRIC_PERMISSIONS_DURATION_SECONDS;
extern const char* METRIC_PERMISSIONS_CACHE_HITS;
extern const char* METRIC_PERMISSIONS_CACHE_MISSES;

/* Sanitizer Metrics */
extern const char* METRIC_SANITIZER_INPUT_TOTAL;
extern const char* METRIC_SANITIZER_DURATION_SECONDS;
extern const char* METRIC_SANITIZER_REJECTED_TOTAL;

/* Workbench Metrics */
extern const char* METRIC_WORKBENCH_EXECUTIONS_TOTAL;
extern const char* METRIC_WORKBENCH_DURATION_SECONDS;
extern const char* METRIC_WORKBENCH_MEMORY_BYTES;
extern const char* METRIC_WORKBENCH_CPU_SECONDS;
extern const char* METRIC_WORKBENCH_OOM_KILLS;

/* Audit Log Metrics */
extern const char* METRIC_AUDIT_EVENTS_TOTAL;
extern const char* METRIC_AUDIT_QUEUE_SIZE;
extern const char* METRIC_AUDIT_BYTES_WRITTEN;

/* Error Metrics */
extern const char* METRIC_ERRORS_TOTAL;

/* System Metrics */
extern const char* METRIC_PROCESS_MEMORY_BYTES;
extern const char* METRIC_PROCESS_CPU_SECONDS;
extern const char* METRIC_THREAD_COUNT;

#ifdef __cplusplus
}
#endif

#endif /* cupolas_METRICS_H */
