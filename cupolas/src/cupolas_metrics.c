/**
 * @file cupolas_metrics.c
 * @brief 性能指标导出 - Prometheus 格式
 * @author Spharx
 * @date 2024
 *
 * 本模块实现指标收集和导出功能：
 * - Prometheus exposition format 支持
 * - 多维度标签支持
 * - 低开销采样
 */

#include "cupolas_metrics.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef cupolas_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

/* 预定义指标名称 */
const char* METRIC_PERMISSIONS_TOTAL = "cupolas_permissions_total";
const char* METRIC_PERMISSIONS_DURATION_SECONDS = "cupolas_permissions_duration_seconds";
const char* METRIC_PERMISSIONS_CACHE_HITS = "cupolas_permissions_cache_hits_total";
const char* METRIC_PERMISSIONS_CACHE_MISSES = "cupolas_permissions_cache_misses_total";

const char* METRIC_SANITIZER_INPUT_TOTAL = "cupolas_sanitizer_input_total";
const char* METRIC_SANITIZER_DURATION_SECONDS = "cupolas_sanitizer_duration_seconds";
const char* METRIC_SANITIZER_REJECTED_TOTAL = "cupolas_sanitizer_rejected_total";

const char* METRIC_WORKBENCH_EXECUTIONS_TOTAL = "cupolas_workbench_executions_total";
const char* METRIC_WORKBENCH_DURATION_SECONDS = "cupolas_workbench_duration_seconds";
const char* METRIC_WORKBENCH_MEMORY_BYTES = "cupolas_workbench_memory_bytes";
const char* METRIC_WORKBENCH_CPU_SECONDS = "cupolas_workbench_cpu_seconds";
const char* METRIC_WORKBENCH_OOM_KILLS = "cupolas_workbench_oom_kills_total";

const char* METRIC_AUDIT_EVENTS_TOTAL = "cupolas_audit_events_total";
const char* METRIC_AUDIT_QUEUE_SIZE = "cupolas_audit_queue_size";
const char* METRIC_AUDIT_BYTES_WRITTEN = "cupolas_audit_bytes_written_total";

const char* METRIC_ERRORS_TOTAL = "cupolas_errors_total";

const char* METRIC_PROCESS_MEMORY_BYTES = "cupolas_process_memory_bytes";
const char* METRIC_PROCESS_CPU_SECONDS = "cupolas_process_cpu_seconds_total";
const char* METRIC_THREAD_COUNT = "cupolas_thread_count";

#define MAX_METRICS 256
#define MAX_LABELS 16
#define MAX_LABEL_LENGTH 128
#define MAX_SAMPLES 4096

typedef struct metric_entry {
    const char* name;
    metric_type_t type;
    const char* help;
    const char* const* label_names;
    size_t label_count;

    double* counters;
    double* gauges;
    histogram_bucket_t* histogram_buckets;
    double histogram_sum;
    double summary_quantiles[5];

    double* buckets;
    size_t bucket_count;

    cupolas_atomic64_t counter_value;
    cupolas_atomic64_t gauge_value;
    cupolas_atomic64_t histogram_count;
    cupolas_atomic64_t histogram_sum_ns;

    bool registered;
} metric_entry_t;

typedef struct metrics_state {
    metric_entry_t entries[MAX_METRICS];
    size_t entry_count;
    uint32_t sampling_interval_ms;

    cupolas_thread_t collector_thread;
    bool collector_running;
} metrics_state_t;

static metrics_state_t g_metrics = {0};
static bool g_initialized = false;

static cupolas_rwlock_t g_metrics_lock = {0};

static const double DEFAULT_BUCKETS[] = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
static const size_t DEFAULT_BUCKET_COUNT = 11;

int metrics_init(uint32_t sampling_interval_ms) {
    if (g_initialized) {
        return 0;
    }

    memset(&g_metrics, 0, sizeof(g_metrics));
    g_metrics.sampling_interval_ms = sampling_interval_ms > 0 ? sampling_interval_ms : 1000;

    cupolas_rwlock_init(&g_metrics_lock);

    g_initialized = true;

    return 0;
}

void metrics_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    g_metrics.collector_running = false;

    cupolas_rwlock_destroy(&g_metrics_lock);

    g_initialized = false;
}

static metric_entry_t* find_or_create_entry(const char* name) {
    cupolas_rwlock_rdlock(&g_metrics_lock);

    for (size_t i = 0; i < g_metrics.entry_count; i++) {
        if (strcmp(g_metrics.entries[i].name, name) == 0) {
            cupolas_rwlock_unlock(&g_metrics_lock);
            return &g_metrics.entries[i];
        }
    }

    cupolas_rwlock_unlock(&g_metrics_lock);

    cupolas_rwlock_wrlock(&g_metrics_lock);

    if (g_metrics.entry_count >= MAX_METRICS) {
        cupolas_rwlock_unlock(&g_metrics_lock);
        return NULL;
    }

    metric_entry_t* entry = &g_metrics.entries[g_metrics.entry_count++];
    memset(entry, 0, sizeof(metric_entry_t));
    entry->name = name;

    cupolas_rwlock_unlock(&g_metrics_lock);

    return entry;
}

int metrics_register(const metric_desc_t* desc) {
    if (!desc || !desc->name) {
        return -1;
    }

    metric_entry_t* entry = find_or_create_entry(desc->name);
    if (!entry) {
        return -1;
    }

    cupolas_rwlock_wrlock(&g_metrics_lock);

    entry->type = desc->type;
    entry->help = desc->help;
    entry->label_names = desc->label_names;
    entry->label_count = desc->label_count;
    entry->buckets = (double*)desc->buckets;
    entry->bucket_count = desc->bucket_count > 0 ? desc->bucket_count : DEFAULT_BUCKET_COUNT;
    entry->registered = true;

    cupolas_rwlock_unlock(&g_metrics_lock);

    return 0;
}

void metrics_counter_inc(const char* name, const char** label_values, double count) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_add64(&entry->counter_value, (int64_t)(count * 1000));
}

void metrics_gauge_set(const char* name, const char** label_values, double value) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_store64(&entry->gauge_value, (int64_t)(value * 1000));
}

void metrics_gauge_add(const char* name, const char** label_values, double value) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_add64(&entry->gauge_value, (int64_t)(value * 1000));
}

void metrics_gauge_sub(const char* name, const char** label_values, double value) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_sub64(&entry->gauge_value, (int64_t)(value * 1000));
}

void metrics_histogram_observe(const char* name, const char** label_values, double value) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_add64(&entry->histogram_count, 1);
    cupolas_atomic_add64(&entry->histogram_sum_ns, (int64_t)(value * 1000000000));
}

void metrics_summary_observe(const char* name, const char** label_values, double value) {
    if (!name) return;

    metric_entry_t* entry = find_or_create_entry(name);
    if (!entry) return;

    cupolas_atomic_add64(&entry->histogram_count, 1);
    cupolas_atomic_add64(&entry->histogram_sum_ns, (int64_t)(value * 1000000000));
}

uint64_t metrics_get_timestamp_ns(void) {
#ifdef cupolas_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULONGLONG ticks = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ticks * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#endif
}

metric_iterator_t* metrics_iter_create(const char* pattern) {
    return (metric_iterator_t*)1;
}

bool metrics_iter_next(metric_iterator_t* iter, metric_sample_t* sample) {
    return false;
}

void metrics_iter_destroy(metric_iterator_t* iter) {
}

static void format_label_values(char* buffer, size_t size,
                                const char** label_names,
                                const char** label_values,
                                size_t label_count) {
    if (!label_values || label_count == 0) {
        buffer[0] = '\0';
        return;
    }

    size_t offset = 0;
    for (size_t i = 0; i < label_count && offset < size - 1; i++) {
        if (i > 0) {
            offset += snprintf(buffer + offset, size - offset, ",");
        }
        offset += snprintf(buffer + offset, size - offset, "%s=\"%s\"",
                          label_names[i] ? label_names[i] : "",
                          label_values[i] ? label_values[i] : "");
    }
}

size_t metrics_export_prometheus(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    size_t offset = 0;
    uint64_t now = metrics_get_timestamp_ns() / 1000000000;

    cupolas_rwlock_rdlock(&g_metrics_lock);

    for (size_t i = 0; i < g_metrics.entry_count && offset < size - 1; i++) {
        metric_entry_t* entry = &g_metrics.entries[i];

        if (!entry->registered || !entry->name) {
            continue;
        }

        offset += snprintf(buffer + offset, size - offset,
                          "# HELP %s %s\n",
                          entry->name,
                          entry->help ? entry->help : "");

        const char* type_str = "untyped";
        switch (entry->type) {
            case METRIC_TYPE_COUNTER:    type_str = "counter"; break;
            case METRIC_TYPE_GAUGE:      type_str = "gauge"; break;
            case METRIC_TYPE_HISTOGRAM:  type_str = "histogram"; break;
            case METRIC_TYPE_SUMMARY:    type_str = "summary"; break;
        }

        offset += snprintf(buffer + offset, size - offset,
                          "# TYPE %s %s\n",
                          entry->name, type_str);

        double counter_val = cupolas_atomic_load64(&entry->counter_value) / 1000.0;
        double gauge_val = cupolas_atomic_load64(&entry->gauge_value) / 1000.0;

        if (entry->type == METRIC_TYPE_COUNTER) {
            offset += snprintf(buffer + offset, size - offset,
                              "%s %f %lu\n",
                              entry->name, counter_val, (unsigned long)now);
        } else if (entry->type == METRIC_TYPE_GAUGE) {
            offset += snprintf(buffer + offset, size - offset,
                              "%s %f %lu\n",
                              entry->name, gauge_val, (unsigned long)now);
        } else if (entry->type == METRIC_TYPE_HISTOGRAM) {
            int64_t count = cupolas_atomic_load64(&entry->histogram_count);
            double sum = cupolas_atomic_load64(&entry->histogram_sum_ns) / 1000000000.0;

            offset += snprintf(buffer + offset, size - offset,
                              "%s_sum %f %lu\n",
                              entry->name, sum, (unsigned long)now);
            offset += snprintf(buffer + offset, size - offset,
                              "%s_count %ld %lu\n",
                              entry->name, count, (unsigned long)now);

            double cumulative = 0;
            for (size_t b = 0; b < entry->bucket_count && b < DEFAULT_BUCKET_COUNT; b++) {
                cumulative += entry->buckets[b];
                offset += snprintf(buffer + offset, size - offset,
                                  "%s_bucket{le=\"%f\"} %f %lu\n",
                                  entry->name, DEFAULT_BUCKETS[b], cumulative,
                                  (unsigned long)now);
            }
        }
    }

    cupolas_rwlock_unlock(&g_metrics_lock);

    return offset;
}

size_t metrics_export_json(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    size_t offset = snprintf(buffer, size, "{\"metrics\":[");

    cupolas_rwlock_rdlock(&g_metrics_lock);

    for (size_t i = 0; i < g_metrics.entry_count && offset < size - 1; i++) {
        metric_entry_t* entry = &g_metrics.entries[i];

        if (!entry->registered || !entry->name) {
            continue;
        }

        if (i > 0) {
            offset += snprintf(buffer + offset, size - offset, ",");
        }

        offset += snprintf(buffer + offset, size - offset,
                          "{\"name\":\"%s\",\"type\":%d,",
                          entry->name, entry->type);

        double counter_val = cupolas_atomic_load64(&entry->counter_value) / 1000.0;
        double gauge_val = cupolas_atomic_load64(&entry->gauge_value) / 1000.0;

        if (entry->type == METRIC_TYPE_COUNTER) {
            offset += snprintf(buffer + offset, size - offset,
                              "\"value\":%f}", counter_val);
        } else if (entry->type == METRIC_TYPE_GAUGE) {
            offset += snprintf(buffer + offset, size - offset,
                              "\"value\":%f}", gauge_val);
        } else if (entry->type == METRIC_TYPE_HISTOGRAM) {
            int64_t count = cupolas_atomic_load64(&entry->histogram_count);
            double sum = cupolas_atomic_load64(&entry->histogram_sum_ns) / 1000000000.0;
            offset += snprintf(buffer + offset, size - offset,
                              "\"count\":%ld,\"sum\":%f}", count, sum);
        }
    }

    cupolas_rwlock_unlock(&g_metrics_lock);

    offset += snprintf(buffer + offset, size - offset, "]}");

    return offset;
}

void metrics_reset(void) {
    cupolas_rwlock_wrlock(&g_metrics_lock);

    for (size_t i = 0; i < g_metrics.entry_count; i++) {
        metric_entry_t* entry = &g_metrics.entries[i];
        cupolas_atomic_store64(&entry->counter_value, 0);
        cupolas_atomic_store64(&entry->gauge_value, 0);
        cupolas_atomic_store64(&entry->histogram_count, 0);
        cupolas_atomic_store64(&entry->histogram_sum_ns, 0);
    }

    cupolas_rwlock_unlock(&g_metrics_lock);
}

size_t metrics_get_count(void) {
    cupolas_rwlock_rdlock(&g_metrics_lock);
    size_t count = g_metrics.entry_count;
    cupolas_rwlock_unlock(&g_metrics_lock);
    return count;
}

uint32_t metrics_get_sampling_interval(void) {
    return g_metrics.sampling_interval_ms;
}