/**
 * @file observability.c
 * @brief AgentOS 可观测性子系统实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供生产级可观测性功能：指标收集、健康检查、分布式追踪、性能监控
 */

#include "observability.h"
#include "agentos_time.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <ctype.h>
#endif

#define OBS_MAX_METRICS 256
#define OBS_MAX_HEALTH_CHECKS 64
#define OBS_MAX_TRACE_SPANS 128
#define OBS_TRACE_ID_SIZE 16
#define OBS_SPAN_ID_SIZE 8

typedef struct {
    char name[128];
    char labels[256];
    agentos_metric_type_t type;
    double value;
    uint64_t timestamp_ns;
} obs_metric_entry_t;

typedef struct {
    char name[64];
    agentos_health_check_cb callback;
    void* user_data;
} obs_health_entry_t;

typedef struct {
    int initialized;
    agentos_observability_config_t config;
    obs_metric_entry_t metrics[OBS_MAX_METRICS];
    uint32_t metric_count;
    obs_health_entry_t health_checks[OBS_MAX_HEALTH_CHECKS];
    uint32_t health_check_count;
    agentos_mutex_t* lock;
} obs_state_t;

static obs_state_t g_obs = {0};

static void generate_hex_id(char* out, size_t out_len, size_t byte_count) {
    static const char hex_chars[] = "0123456789abcdef";
    uint64_t ns = agentos_time_monotonic_ns();
    uint64_t pid = (uint64_t)getpid();
    uint64_t counter = ns ^ (pid << 32);
    for (size_t i = 0; i < byte_count && (i * 2 + 1) < out_len; i++) {
        unsigned char byte = (unsigned char)((counter >> ((i % 8) * 8)) & 0xFF);
        counter = counter * 6364136223846793005ULL + 1442695040888963407ULL;
        out[i * 2] = hex_chars[(byte >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[byte & 0x0F];
    }
    size_t end = byte_count * 2;
    if (end < out_len) {
        out[end] = '\0';
    }
}

static obs_metric_entry_t* find_metric(const char* name, const char* labels) {
    for (uint32_t i = 0; i < g_obs.metric_count; i++) {
        if (strcmp(g_obs.metrics[i].name, name) == 0) {
            if (labels == NULL || strcmp(g_obs.metrics[i].labels, labels) == 0) {
                return &g_obs.metrics[i];
            }
        }
    }
    return NULL;
}

int agentos_observability_init(const agentos_observability_config_t* config) {
    if (g_obs.initialized) {
        return AGENTOS_SUCCESS;
    }

    if (!g_obs.lock) {
        g_obs.lock = agentos_mutex_create();
        if (!g_obs.lock) return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(g_obs.lock);

    if (config) {
        memcpy(&g_obs.config, config, sizeof(agentos_observability_config_t));
    } else {
        memset(&g_obs.config, 0, sizeof(agentos_observability_config_t));
        g_obs.config.enable_metrics = 1;
        g_obs.config.enable_tracing = 1;
        g_obs.config.enable_health_check = 1;
        g_obs.config.metrics_interval_ms = 10000;
        g_obs.config.health_check_interval_ms = 30000;
    }

    g_obs.metric_count = 0;
    g_obs.health_check_count = 0;
    g_obs.initialized = 1;

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

void agentos_observability_shutdown(void) {
    if (!g_obs.initialized) return;

    agentos_mutex_lock(g_obs.lock);
    g_obs.metric_count = 0;
    g_obs.health_check_count = 0;
    g_obs.initialized = 0;
    agentos_mutex_unlock(g_obs.lock);

    if (g_obs.lock) {
        agentos_mutex_free(g_obs.lock);
        g_obs.lock = NULL;
    }
}

int agentos_health_check_register(const char* name,
                                  agentos_health_check_cb callback,
                                  void* user_data) {
    if (!name || !callback) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;
    if (g_obs.health_check_count >= OBS_MAX_HEALTH_CHECKS) return AGENTOS_ENOSPC;

    agentos_mutex_lock(g_obs.lock);

    obs_health_entry_t* entry = &g_obs.health_checks[g_obs.health_check_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->callback = callback;
    entry->user_data = user_data;
    g_obs.health_check_count++;

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

agentos_health_status_t agentos_health_check_run(int timeout_ms) {
    (void)timeout_ms;
    if (!g_obs.initialized) return AGENTOS_HEALTH_FAIL;

    agentos_health_status_t worst = AGENTOS_HEALTH_PASS;

    agentos_mutex_lock(g_obs.lock);

    for (uint32_t i = 0; i < g_obs.health_check_count; i++) {
        agentos_health_status_t status = g_obs.health_checks[i].callback(
            g_obs.health_checks[i].user_data);
        if (status > worst) {
            worst = status;
        }
        if (worst == AGENTOS_HEALTH_FAIL) break;
    }

    agentos_mutex_unlock(g_obs.lock);
    return worst;
}

int agentos_metric_record(const agentos_metric_sample_t* sample) {
    if (!sample) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    obs_metric_entry_t* existing = find_metric(sample->name, sample->labels);
    if (existing) {
        existing->value = sample->value;
        existing->timestamp_ns = sample->timestamp_ns;
        existing->type = sample->type;
    } else {
        if (g_obs.metric_count >= OBS_MAX_METRICS) {
            agentos_mutex_unlock(g_obs.lock);
            return AGENTOS_ENOSPC;
        }
        obs_metric_entry_t* entry = &g_obs.metrics[g_obs.metric_count];
        strncpy(entry->name, sample->name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    if (sample->labels[0] != '\0') {
            strncpy(entry->labels, sample->labels, sizeof(entry->labels) - 1);
            entry->labels[sizeof(entry->labels) - 1] = '\0';
        } else {
            entry->labels[0] = '\0';
        }
        entry->type = sample->type;
        entry->value = sample->value;
        entry->timestamp_ns = sample->timestamp_ns;
        g_obs.metric_count++;
    }

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

int agentos_metric_counter_create(const char* name, const char* labels) {
    if (!name) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    if (find_metric(name, labels)) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_EEXIST;
    }
    if (g_obs.metric_count >= OBS_MAX_METRICS) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_ENOSPC;
    }

    obs_metric_entry_t* entry = &g_obs.metrics[g_obs.metric_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    if (labels) {
        strncpy(entry->labels, labels, sizeof(entry->labels) - 1);
        entry->labels[sizeof(entry->labels) - 1] = '\0';
    } else {
        entry->labels[0] = '\0';
    }
    entry->type = AGENTOS_METRIC_COUNTER_E;
    entry->value = 0.0;
    entry->timestamp_ns = agentos_time_monotonic_ns();
    g_obs.metric_count++;

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

int agentos_metric_counter_inc(const char* name, const char* labels, double value) {
    if (!name) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    obs_metric_entry_t* entry = find_metric(name, labels);
    if (!entry) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_ENOENT;
    }
    if (entry->type != AGENTOS_METRIC_COUNTER_E) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_EINVAL;
    }
    entry->value += value;
    entry->timestamp_ns = agentos_time_monotonic_ns();

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

int agentos_metric_gauge_create(const char* name, const char* labels, double initial_value) {
    if (!name) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    if (find_metric(name, labels)) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_EEXIST;
    }
    if (g_obs.metric_count >= OBS_MAX_METRICS) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_ENOSPC;
    }

    obs_metric_entry_t* entry = &g_obs.metrics[g_obs.metric_count];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    if (labels) {
        strncpy(entry->labels, labels, sizeof(entry->labels) - 1);
        entry->labels[sizeof(entry->labels) - 1] = '\0';
    } else {
        entry->labels[0] = '\0';
    }
    entry->type = AGENTOS_METRIC_GAUGE_E;
    entry->value = initial_value;
    entry->timestamp_ns = agentos_time_monotonic_ns();
    g_obs.metric_count++;

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

int agentos_metric_gauge_set(const char* name, const char* labels, double value) {
    if (!name) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    obs_metric_entry_t* entry = find_metric(name, labels);
    if (!entry) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_ENOENT;
    }
    if (entry->type != AGENTOS_METRIC_GAUGE_E) {
        agentos_mutex_unlock(g_obs.lock);
        return AGENTOS_EINVAL;
    }
    entry->value = value;
    entry->timestamp_ns = agentos_time_monotonic_ns();

    agentos_mutex_unlock(g_obs.lock);
    return AGENTOS_SUCCESS;
}

int agentos_trace_span_start(agentos_trace_context_t* context,
                             const char* service_name,
                             const char* operation_name) {
    if (!context) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    generate_hex_id(context->trace_id, sizeof(context->trace_id), OBS_TRACE_ID_SIZE);
    generate_hex_id(context->span_id, sizeof(context->span_id), OBS_SPAN_ID_SIZE);
    context->parent_span_id[0] = '\0';
    context->start_ns = agentos_time_monotonic_ns();
    context->end_ns = 0;
    context->error_code = 0;

    if (service_name) {
        strncpy(context->service_name, service_name, sizeof(context->service_name) - 1);
        context->service_name[sizeof(context->service_name) - 1] = '\0';
    } else {
        context->service_name[0] = '\0';
    }

    if (operation_name) {
        strncpy(context->operation_name, operation_name, sizeof(context->operation_name) - 1);
        context->operation_name[sizeof(context->operation_name) - 1] = '\0';
    } else {
        context->operation_name[0] = '\0';
    }

    return AGENTOS_SUCCESS;
}

int agentos_trace_span_end(agentos_trace_context_t* context, int error_code) {
    if (!context) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    context->end_ns = agentos_time_monotonic_ns();
    context->error_code = error_code;

    return AGENTOS_SUCCESS;
}

int agentos_trace_set_tag(agentos_trace_context_t* context,
                          const char* key, const char* value) {
    (void)context;
    (void)key;
    (void)value;
    if (!context || !key) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    return AGENTOS_SUCCESS;
}

int agentos_trace_log(agentos_trace_context_t* context, const char* message) {
    (void)context;
    (void)message;
    if (!context || !message) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    return AGENTOS_SUCCESS;
}

int agentos_performance_get_metrics(double* out_cpu_usage,
                                   double* out_memory_usage,
                                   int* out_thread_count) {
    if (!out_cpu_usage || !out_memory_usage || !out_thread_count) {
        return AGENTOS_EINVAL;
    }

#ifdef _WIN32
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    *out_memory_usage = (double)mem_info.dwMemoryLoad;

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    *out_cpu_usage = 0.0;

    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    *out_thread_count = 1;
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        double total_ram = (double)si.totalram * si.mem_unit;
        double free_ram = (double)si.freeram * si.mem_unit;
        if (total_ram > 0) {
            *out_memory_usage = ((total_ram - free_ram) / total_ram) * 100.0;
        } else {
            *out_memory_usage = 0.0;
        }
    } else {
        *out_memory_usage = 0.0;
    }

    *out_cpu_usage = 0.0;

    long num_processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_processors <= 0) num_processors = 1;
    *out_thread_count = (int)num_processors;
#endif

    return AGENTOS_SUCCESS;
}

int agentos_metrics_export_prometheus(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    int written = 0;
    for (uint32_t i = 0; i < g_obs.metric_count; i++) {
        obs_metric_entry_t* m = &g_obs.metrics[i];
        const char* type_str = "untyped";
        switch (m->type) {
            case AGENTOS_METRIC_COUNTER_E: type_str = "counter"; break;
        case AGENTOS_METRIC_GAUGE_E: type_str = "gauge"; break;
        case AGENTOS_METRIC_HISTOGRAM_E: type_str = "histogram"; break;
        case AGENTOS_METRIC_SUMMARY_E: type_str = "summary"; break;
        }

        int n;
        if (m->labels[0] != '\0') {
            n = snprintf(buffer + written, buffer_size - written,
                        "# TYPE %s %s\n%s{%s} %.17g\n",
                        m->name, type_str, m->name, m->labels, m->value);
        } else {
            n = snprintf(buffer + written, buffer_size - written,
                        "# TYPE %s %s\n%s %.17g\n",
                        m->name, type_str, m->name, m->value);
        }
        if (n < 0 || (size_t)n >= buffer_size - written) {
            agentos_mutex_unlock(g_obs.lock);
            return written > 0 ? written : AGENTOS_EOVERFLOW;
        }
        written += n;
    }

    agentos_mutex_unlock(g_obs.lock);
    return written;
}

int agentos_health_export_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return AGENTOS_EINVAL;
    if (!g_obs.initialized) return AGENTOS_ENOTSUP;

    agentos_mutex_lock(g_obs.lock);

    int written = snprintf(buffer, buffer_size,
                          "{\"status\":\"%s\",\"checks\":[",
                          g_obs.health_check_count > 0 ? "checking" : "unknown");

    for (uint32_t i = 0; i < g_obs.health_check_count; i++) {
        obs_health_entry_t* h = &g_obs.health_checks[i];
        agentos_health_status_t status = h->callback(h->user_data);
        const char* status_str = "pass";
        switch (status) {
            case AGENTOS_HEALTH_WARN: status_str = "warn"; break;
            case AGENTOS_HEALTH_FAIL: status_str = "fail"; break;
            default: break;
        }

        int n = snprintf(buffer + written, buffer_size - written,
                        "%s{\"name\":\"%s\",\"status\":\"%s\"}",
                        i > 0 ? "," : "",
                        h->name, status_str);
        if (n < 0 || (size_t)n >= buffer_size - written) {
            agentos_mutex_unlock(g_obs.lock);
            return written > 0 ? written : AGENTOS_EOVERFLOW;
        }
        written += n;
    }

    int n = snprintf(buffer + written, buffer_size - written, "]}");
    if (n < 0 || (size_t)n >= buffer_size - written) {
        agentos_mutex_unlock(g_obs.lock);
        return written > 0 ? written : AGENTOS_EOVERFLOW;
    }
    written += n;

    agentos_mutex_unlock(g_obs.lock);
    return written;
}
