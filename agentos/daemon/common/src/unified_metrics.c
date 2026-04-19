// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file unified_metrics.c
 * @brief 统一指标收集器实现
 *
 * @see unified_metrics.h
 */

#include "unified_metrics.h"
#include "svc_logger.h"
#include "platform.h"
#include "safe_string_utils.h"

#include <agentos/utils/memory/memory_compat.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部状态 ==================== */

static struct {
    um_config_t config;
    um_module_metrics_t modules[UM_MAX_MODULES];
    uint32_t module_count;
    um_stats_t stats;
    bool initialized;
    agentos_platform_mutex_t mutex;
} g_um = {0};

/* ==================== 辅助函数 ==================== */

static um_module_metrics_t* find_module(const char* name) {
    for (uint32_t i = 0; i < g_um.module_count; i++) {
        if (strcmp(g_um.modules[i].module_name, name) == 0)
            return &g_um.modules[i];
    }
    return NULL;
}

static um_metric_entry_t* find_metric(um_module_metrics_t* mod, const char* name) {
    for (uint32_t i = 0; i < mod->metric_count; i++) {
        if (strcmp(mod->metrics[i].name, name) == 0)
            return &mod->metrics[i];
    }
    return NULL;
}

static int sanitize_prom_name(const char* in, char* out, size_t out_size) {
    if (!in || !out || out_size == 0) return -1;
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_size - 1; i++) {
        char ch = in[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_') {
            out[j++] = ch;
        } else if (ch == '.' || ch == '-' || ch == ' ') {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
    return (j > 0) ? 0 : -1;
}

static const char* metric_type_string(um_metric_type_t type) {
    switch (type) {
        case UM_TYPE_COUNTER:   return "counter";
        case UM_TYPE_GAUGE:     return "gauge";
        case UM_TYPE_HISTOGRAM: return "histogram";
        case UM_TYPE_SUMMARY:   return "summary";
        default:                return "untyped";
    }
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API um_config_t um_create_default_config(void) {
    um_config_t config;
    memset(&config, 0, sizeof(um_config_t));
    safe_strcpy(config.service_name, "agentos", sizeof(config.service_name));
    config.scrape_interval_ms = 15000;
    config.retention_ms = 300000;
    config.enable_default_metrics = true;
    return config;
}

AGENTOS_API int um_init(const um_config_t* config) {
    if (g_um.initialized) return 0;

    if (config) {
        memcpy(&g_um.config, config, sizeof(um_config_t));
    } else {
        g_um.config = um_create_default_config();
    }

    agentos_error_t err = agentos_platform_mutex_init(&g_um.mutex);
    if (err != AGENTOS_SUCCESS) return -1;

    memset(g_um.modules, 0, sizeof(g_um.modules));
    g_um.module_count = 0;
    memset(&g_um.stats, 0, sizeof(um_stats_t));
    g_um.initialized = true;

    if (g_um.config.enable_default_metrics) {
        um_register_default_metrics();
    }

    LOG_INFO("Unified metrics collector initialized (service=%s)",
             g_um.config.service_name);
    return 0;
}

AGENTOS_API void um_shutdown(void) {
    if (!g_um.initialized) return;

    agentos_platform_mutex_lock(&g_um.mutex);
    g_um.initialized = false;
    agentos_platform_mutex_unlock(&g_um.mutex);

    agentos_platform_mutex_destroy(&g_um.mutex);

    LOG_INFO("Unified metrics collector shutdown");
}

AGENTOS_API bool um_is_initialized(void) {
    return g_um.initialized;
}

/* ==================== 模块注册 ==================== */

AGENTOS_API int um_register_module(const char* module_name, const char* instance_id) {
    if (!module_name) return -1;
    if (!g_um.initialized) um_init(NULL);

    agentos_platform_mutex_lock(&g_um.mutex);

    if (find_module(module_name)) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return 0;
    }

    if (g_um.module_count >= UM_MAX_MODULES) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        LOG_ERROR("Max metric modules reached");
        return -1;
    }

    um_module_metrics_t* mod = &g_um.modules[g_um.module_count];
    memset(mod, 0, sizeof(um_module_metrics_t));
    safe_strcpy(mod->module_name, module_name, UM_MODULE_NAME_LEN);
    if (instance_id) {
        safe_strcpy(mod->instance_id, instance_id, sizeof(mod->instance_id));
    } else {
        snprintf(mod->instance_id, sizeof(mod->instance_id), "%u",
#ifdef _WIN32
                 (uint32_t)GetCurrentProcessId()
#else
                 (uint32_t)getpid()
#endif
        );
    }
    mod->active = true;
    g_um.module_count++;
    g_um.stats.active_modules = g_um.module_count;

    agentos_platform_mutex_unlock(&g_um.mutex);

    LOG_INFO("Metrics module registered: %s (instance=%s)", module_name, mod->instance_id);
    return 0;
}

AGENTOS_API int um_unregister_module(const char* module_name) {
    if (!module_name) return -1;

    agentos_platform_mutex_lock(&g_um.mutex);

    um_module_metrics_t* mod = find_module(module_name);
    if (!mod) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    uint32_t idx = (uint32_t)(mod - g_um.modules);
    g_um.stats.total_metrics -= mod->metric_count;

    if (idx < g_um.module_count - 1) {
        g_um.modules[idx] = g_um.modules[g_um.module_count - 1];
    }
    memset(&g_um.modules[g_um.module_count - 1], 0, sizeof(um_module_metrics_t));
    g_um.module_count--;
    g_um.stats.active_modules = g_um.module_count;

    agentos_platform_mutex_unlock(&g_um.mutex);

    return 0;
}

/* ==================== 指标操作 ==================== */

AGENTOS_API int um_register_metric(
    const char* module_name,
    const char* name,
    um_metric_type_t type,
    const char* help,
    const char* labels
) {
    if (!module_name || !name) return -1;
    if (!g_um.initialized) um_init(NULL);

    agentos_platform_mutex_lock(&g_um.mutex);

    um_module_metrics_t* mod = find_module(module_name);
    if (!mod) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        um_register_module(module_name, NULL);
        agentos_platform_mutex_lock(&g_um.mutex);
        mod = find_module(module_name);
        if (!mod) {
            agentos_platform_mutex_unlock(&g_um.mutex);
            return -1;
        }
    }

    if (find_metric(mod, name)) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return 0;
    }

    if (mod->metric_count >= UM_MAX_METRICS_PER_MOD) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    um_metric_entry_t* entry = &mod->metrics[mod->metric_count];
    memset(entry, 0, sizeof(um_metric_entry_t));
    safe_strcpy(entry->name, name, UM_METRIC_NAME_LEN);
    if (help) safe_strcpy(entry->help, help, sizeof(entry->help));
    if (labels) safe_strcpy(entry->labels, labels, sizeof(entry->labels));
    entry->type = type;
    entry->timestamp_ms = agentos_platform_get_time_ms();
    mod->metric_count++;

    g_um.stats.total_registrations++;
    g_um.stats.total_metrics = 0;
    for (uint32_t i = 0; i < g_um.module_count; i++) {
        g_um.stats.total_metrics += g_um.modules[i].metric_count;
    }

    agentos_platform_mutex_unlock(&g_um.mutex);

    return 0;
}

AGENTOS_API int um_increment(const char* module_name, const char* name, uint64_t value) {
    if (!module_name || !name) return -1;

    agentos_platform_mutex_lock(&g_um.mutex);

    um_module_metrics_t* mod = find_module(module_name);
    if (!mod) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    um_metric_entry_t* entry = find_metric(mod, name);
    if (!entry) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    entry->value += (double)value;
    entry->count += value;
    entry->timestamp_ms = agentos_platform_get_time_ms();

    g_um.stats.total_increments++;

    agentos_platform_mutex_unlock(&g_um.mutex);
    return 0;
}

AGENTOS_API int um_gauge_set(const char* module_name, const char* name, double value) {
    if (!module_name || !name) return -1;

    agentos_platform_mutex_lock(&g_um.mutex);

    um_module_metrics_t* mod = find_module(module_name);
    if (!mod) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    um_metric_entry_t* entry = find_metric(mod, name);
    if (!entry) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    entry->value = value;
    entry->timestamp_ms = agentos_platform_get_time_ms();

    g_um.stats.total_updates++;

    agentos_platform_mutex_unlock(&g_um.mutex);
    return 0;
}

AGENTOS_API int um_observe(const char* module_name, const char* name, double value) {
    if (!module_name || !name) return -1;

    agentos_platform_mutex_lock(&g_um.mutex);

    um_module_metrics_t* mod = find_module(module_name);
    if (!mod) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    um_metric_entry_t* entry = find_metric(mod, name);
    if (!entry) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return -1;
    }

    entry->sum += value;
    entry->count++;
    entry->value = entry->count > 0 ? entry->sum / entry->count : 0;
    entry->timestamp_ms = agentos_platform_get_time_ms();

    g_um.stats.total_updates++;

    agentos_platform_mutex_unlock(&g_um.mutex);
    return 0;
}

/* ==================== 导出 ==================== */

AGENTOS_API char* um_export_prometheus(void) {
    return um_export_prometheus_module(NULL);
}

AGENTOS_API char* um_export_prometheus_module(const char* module_name) {
    if (!g_um.initialized) return NULL;

    agentos_platform_mutex_lock(&g_um.mutex);

    size_t buf_size = 8192;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return NULL;
    }
    size_t pos = 0;

#define PAPPEND(fmt, ...) do { \
    int w = snprintf(buf + pos, buf_size - pos, fmt, ##__VA_ARGS__); /* flawfinder: ignore - bounded snprintf in PAPPEND macro */ \
    if (w < 0) { AGENTOS_FREE(buf); agentos_platform_mutex_unlock(&g_um.mutex); return NULL; } \
    if ((size_t)w >= buf_size - pos) { \
        buf_size *= 2; \
        char* nb = (char*)AGENTOS_MALLOC(buf_size); \
        if (!nb) { AGENTOS_FREE(buf); agentos_platform_mutex_unlock(&g_um.mutex); return NULL; } \
        memcpy(nb, buf, pos); AGENTOS_FREE(buf); buf = nb; \
        w = snprintf(buf + pos, buf_size - pos, fmt, ##__VA_ARGS__); /* flawfinder: ignore - bounded realloc+snprintf retry */ \
        if (w < 0 || (size_t)w >= buf_size - pos) { AGENTOS_FREE(buf); agentos_platform_mutex_unlock(&g_um.mutex); return NULL; } \
    } \
    pos += (size_t)w; \
} while(0)

    char safe_name[256];
    char prefixed_name[384];

    for (uint32_t m = 0; m < g_um.module_count; m++) {
        um_module_metrics_t* mod = &g_um.modules[m];

        if (module_name && strcmp(mod->module_name, module_name) != 0)
            continue;

        for (uint32_t i = 0; i < mod->metric_count; i++) {
            um_metric_entry_t* entry = &mod->metrics[i];

            if (sanitize_prom_name(entry->name, safe_name, sizeof(safe_name)) != 0)
                continue;

            snprintf(prefixed_name, sizeof(prefixed_name), "agentos_%s_%s",
                     mod->module_name, safe_name);

            if (entry->help[0]) {
                PAPPEND("# HELP %s %s\n", prefixed_name, entry->help);
            }
            PAPPEND("# TYPE %s %s\n", prefixed_name, metric_type_string(entry->type));

            if (entry->labels[0]) {
                PAPPEND("%s{%s,module=\"%s\",instance=\"%s\"}",
                        prefixed_name, entry->labels,
                        mod->module_name, mod->instance_id);
            } else {
                PAPPEND("%s{module=\"%s\",instance=\"%s\"}",
                        prefixed_name, mod->module_name, mod->instance_id);
            }

            switch (entry->type) {
                case UM_TYPE_COUNTER:
                    PAPPEND(" %.17g\n", entry->value);
                    break;
                case UM_TYPE_GAUGE:
                    PAPPEND(" %.17g\n", entry->value);
                    break;
                case UM_TYPE_HISTOGRAM:
                case UM_TYPE_SUMMARY:
                    PAPPEND("_sum %.17g\n%s_count{module=\"%s\",instance=\"%s\"} %llu\n",
                            entry->sum, prefixed_name,
                            mod->module_name, mod->instance_id,
                            (unsigned long long)entry->count);
                    break;
            }
        }
    }

#undef PAPPEND

    g_um.stats.total_exports++;
    agentos_platform_mutex_unlock(&g_um.mutex);

    return buf;
}

AGENTOS_API char* um_export_json(void) {
    if (!g_um.initialized) return NULL;

    agentos_platform_mutex_lock(&g_um.mutex);

    size_t buf_size = 8192;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_platform_mutex_unlock(&g_um.mutex);
        return NULL;
    }
    size_t pos = 0;

    pos += snprintf(buf + pos, buf_size - pos, "{\"service\":\"%s\",\"modules\":{",
                    g_um.config.service_name);

    for (uint32_t m = 0; m < g_um.module_count; m++) {
        um_module_metrics_t* mod = &g_um.modules[m];
        if (m > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        pos += snprintf(buf + pos, buf_size - pos, "\"%s\":{", mod->module_name);

        for (uint32_t i = 0; i < mod->metric_count; i++) {
            um_metric_entry_t* entry = &mod->metrics[i];
            if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
            pos += snprintf(buf + pos, buf_size - pos,
                            "\"%s\":{\"type\":\"%s\",\"value\":%.17g,\"count\":%llu}",
                            entry->name, metric_type_string(entry->type),
                            entry->value, (unsigned long long)entry->count);
        }
        pos += snprintf(buf + pos, buf_size - pos, "}");
    }

    pos += snprintf(buf + pos, buf_size - pos, "}}");

    g_um.stats.total_exports++;
    agentos_platform_mutex_unlock(&g_um.mutex);

    return buf;
}

/* ==================== 默认指标 ==================== */

AGENTOS_API int um_register_default_metrics(void) {
    um_register_module("system", "default");

    um_register_metric("system", "process_cpu_seconds", UM_TYPE_COUNTER,
                       "Total CPU seconds used by the process", NULL);
    um_register_metric("system", "process_memory_bytes", UM_TYPE_GAUGE,
                       "Process memory usage in bytes", NULL);
    um_register_metric("system", "process_open_fds", UM_TYPE_GAUGE,
                       "Number of open file descriptors", NULL);
    um_register_metric("system", "process_threads", UM_TYPE_GAUGE,
                       "Number of threads", NULL);
    um_register_metric("system", "process_uptime_seconds", UM_TYPE_COUNTER,
                       "Process uptime in seconds", NULL);

    um_register_module("agentos_runtime", "default");

    um_register_metric("agentos_runtime", "requests_total", UM_TYPE_COUNTER,
                       "Total number of requests processed", "method=\"\",path=\"\"");
    um_register_metric("agentos_runtime", "request_duration_seconds", UM_TYPE_SUMMARY,
                       "Request duration in seconds", "method=\"\",path=\"\"");
    um_register_metric("agentos_runtime", "errors_total", UM_TYPE_COUNTER,
                       "Total number of errors", "type=\"\"");
    um_register_metric("agentos_runtime", "active_sessions", UM_TYPE_GAUGE,
                       "Number of active sessions", NULL);
    um_register_metric("agentos_runtime", "active_connections", UM_TYPE_GAUGE,
                       "Number of active connections", NULL);

    return 0;
}

AGENTOS_API void um_update_default_metrics(void) {
    if (!g_um.initialized) return;

#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        um_gauge_set("system", "process_memory_bytes", (double)ms.dwMemoryLoad);
    }
#else
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) {
        long rss = 0;
        if (fscanf(f, "%*s %ld", &rss) == 1) {
            um_gauge_set("system", "process_memory_bytes", (double)(rss * 4096));
        }
        fclose(f);
    }
#endif

    uint64_t uptime = agentos_platform_get_time_ms() / 1000;
    um_increment("system", "process_uptime_seconds", 1);
}

/* ==================== 统计 ==================== */

AGENTOS_API int um_get_stats(um_stats_t* stats) {
    if (!stats) return -1;

    agentos_platform_mutex_lock(&g_um.mutex);
    memcpy(stats, &g_um.stats, sizeof(um_stats_t));
    agentos_platform_mutex_unlock(&g_um.mutex);

    return 0;
}
