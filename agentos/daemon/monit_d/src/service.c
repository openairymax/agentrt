/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file service.c
 * @brief 监控服务核心实现
 *
 * 实现 monitor_service_t 结构体和所有 monitor_service_* 公共API。
 * 整合指标采集、告警管理、分布式追踪和结构化日志四大子系统。
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_ALERTS 1024
#define MAX_LOG_ENTRIES 4096
#define MAX_TRACES 512
#define MAX_TRACE_SPANS 64
#define MAX_REPORT_SIZE 65536
#define MAX_METRICS 256

typedef struct {
    char* alert_id;
    char* message;
    alert_level_t level;
    char* service_name;
    char* resource_id;
    uint64_t timestamp;
    bool is_resolved;
} alert_entry_t;

typedef struct {
    log_level_t level;
    char* message;
    char* service_name;
    char* file;
    int line;
    char* function;
    uint64_t timestamp;
} log_entry_t;

typedef struct {
    char* trace_id;
    char* operation_name;
    uint64_t start_time;
    uint64_t end_time;
    int status;
    char* service_name;
    size_t span_count;
} trace_entry_t;

struct monitor_service {
    monitor_config_t config;

    alert_entry_t alerts[MAX_ALERTS];
    size_t alert_count;
    agentos_mutex_t alert_lock;

    log_entry_t logs[MAX_LOG_ENTRIES];
    size_t log_count;
    size_t log_write_idx;
    agentos_mutex_t log_lock;

    trace_entry_t traces[MAX_TRACES];
    size_t trace_count;
    agentos_mutex_t trace_lock;

    metric_info_t* metric_cache[MAX_METRICS];
    size_t metric_cache_count;
    agentos_mutex_t metric_lock;

    int initialized;
    int running;
};

static uint64_t get_timestamp_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

int monitor_service_create(const monitor_config_t* config, monitor_service_t** service) {
    if (!service) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    monitor_service_t* svc = (monitor_service_t*)calloc(1, sizeof(monitor_service_t));
    if (!svc) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    if (config) {
        memcpy(&svc->config, config, sizeof(monitor_config_t));
        if (config->log_file_path) {
            svc->config.log_file_path = strdup(config->log_file_path);
        }
        if (config->metrics_storage_path) {
            svc->config.metrics_storage_path = strdup(config->metrics_storage_path);
        }
    } else {
        svc->config.metrics_collection_interval_ms = 5000;
        svc->config.health_check_interval_ms = 10000;
        svc->config.log_flush_interval_ms = 30000;
        svc->config.alert_check_interval_ms = 5000;
        svc->config.log_file_path = strdup("monitor.log");
        svc->config.metrics_storage_path = strdup("metrics");
        svc->config.enable_tracing = true;
        svc->config.enable_alerting = true;
    }

    agentos_mutex_init(&svc->alert_lock);
    agentos_mutex_init(&svc->log_lock);
    agentos_mutex_init(&svc->trace_lock);
    agentos_mutex_init(&svc->metric_lock);

    svc->alert_count = 0;
    svc->log_count = 0;
    svc->log_write_idx = 0;
    svc->trace_count = 0;
    svc->metric_cache_count = 0;
    svc->initialized = 1;
    svc->running = 0;

    *service = svc;

    SVC_LOG_INFO("Monitor service created (metrics_interval=%ums, tracing=%s, alerting=%s)",
                 svc->config.metrics_collection_interval_ms,
                 svc->config.enable_tracing ? "on" : "off",
                 svc->config.enable_alerting ? "on" : "off");
    return AGENTOS_SUCCESS;
}

int monitor_service_destroy(monitor_service_t* service) {
    if (!service) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->alert_lock);
    for (size_t i = 0; i < service->alert_count; i++) {
        free(service->alerts[i].alert_id);
        free(service->alerts[i].message);
        free(service->alerts[i].service_name);
        free(service->alerts[i].resource_id);
    }
    agentos_mutex_unlock(&service->alert_lock);
    agentos_mutex_destroy(&service->alert_lock);

    agentos_mutex_lock(&service->log_lock);
    for (size_t i = 0; i < service->log_count; i++) {
        size_t idx = (service->log_write_idx - service->log_count + i) % MAX_LOG_ENTRIES;
        free(service->logs[idx].message);
        free(service->logs[idx].service_name);
        free(service->logs[idx].file);
        free(service->logs[idx].function);
    }
    agentos_mutex_unlock(&service->log_lock);
    agentos_mutex_destroy(&service->log_lock);

    agentos_mutex_lock(&service->trace_lock);
    for (size_t i = 0; i < service->trace_count; i++) {
        free(service->traces[i].trace_id);
        free(service->traces[i].operation_name);
        free(service->traces[i].service_name);
    }
    agentos_mutex_unlock(&service->trace_lock);
    agentos_mutex_destroy(&service->trace_lock);

    agentos_mutex_lock(&service->metric_lock);
    for (size_t i = 0; i < service->metric_cache_count; i++) {
        if (service->metric_cache[i]) {
            free(service->metric_cache[i]->name);
            free(service->metric_cache[i]->description);
            free(service->metric_cache[i]);
        }
    }
    agentos_mutex_unlock(&service->metric_lock);
    agentos_mutex_destroy(&service->metric_lock);

    free(service->config.log_file_path);
    free(service->config.metrics_storage_path);

    service->initialized = 0;
    free(service);

    SVC_LOG_INFO("Monitor service destroyed");
    return AGENTOS_SUCCESS;
}

int monitor_service_record_metric(monitor_service_t* service, const metric_info_t* metric) {
    if (!service || !metric) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->metric_lock);

    if (service->metric_cache_count < MAX_METRICS) {
        metric_info_t* entry = (metric_info_t*)calloc(1, sizeof(metric_info_t));
        if (entry) {
            entry->name = metric->name ? strdup(metric->name) : NULL;
            entry->description = metric->description ? strdup(metric->description) : NULL;
            entry->type = metric->type;
            entry->value = metric->value;
            entry->timestamp = metric->timestamp ? metric->timestamp : get_timestamp_ms();
            service->metric_cache[service->metric_cache_count++] = entry;
        }
    }

    agentos_mutex_unlock(&service->metric_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_log(monitor_service_t* service, const log_info_t* log) {
    if (!service || !log) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->log_lock);

    size_t idx = service->log_write_idx % MAX_LOG_ENTRIES;

    free(service->logs[idx].message);
    free(service->logs[idx].service_name);
    free(service->logs[idx].file);
    free(service->logs[idx].function);

    service->logs[idx].level = log->level;
    service->logs[idx].message = log->message ? strdup(log->message) : NULL;
    service->logs[idx].service_name = log->service_name ? strdup(log->service_name) : NULL;
    service->logs[idx].file = log->file ? strdup(log->file) : NULL;
    service->logs[idx].line = log->line;
    service->logs[idx].function = log->function ? strdup(log->function) : NULL;
    service->logs[idx].timestamp = log->timestamp ? log->timestamp : get_timestamp_ms();

    service->log_write_idx++;
    if (service->log_count < MAX_LOG_ENTRIES) {
        service->log_count++;
    }

    agentos_mutex_unlock(&service->log_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_trigger_alert(monitor_service_t* service, const alert_info_t* alert) {
    if (!service || !alert) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->alert_lock);

    if (service->alert_count >= MAX_ALERTS) {
        free(service->alerts[0].alert_id);
        free(service->alerts[0].message);
        free(service->alerts[0].service_name);
        free(service->alerts[0].resource_id);
        memmove(&service->alerts[0], &service->alerts[1],
                (service->alert_count - 1) * sizeof(alert_entry_t));
        service->alert_count--;
    }

    alert_entry_t* entry = &service->alerts[service->alert_count];
    entry->alert_id = alert->alert_id ? strdup(alert->alert_id) : NULL;
    entry->message = alert->message ? strdup(alert->message) : NULL;
    entry->level = alert->level;
    entry->service_name = alert->service_name ? strdup(alert->service_name) : NULL;
    entry->resource_id = alert->resource_id ? strdup(alert->resource_id) : NULL;
    entry->timestamp = alert->timestamp ? alert->timestamp : get_timestamp_ms();
    entry->is_resolved = false;
    service->alert_count++;

    agentos_mutex_unlock(&service->alert_lock);

    const char* level_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    SVC_LOG_WARN("Alert triggered: [%s] %s (service=%s)",
                 level_str[alert->level < ALERT_LEVEL_COUNT ? alert->level : 0],
                 alert->message ? alert->message : "N/A",
                 alert->service_name ? alert->service_name : "N/A");
    return AGENTOS_SUCCESS;
}

int monitor_service_resolve_alert(monitor_service_t* service, const char* alert_id) {
    if (!service || !alert_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->alert_lock);

    for (size_t i = 0; i < service->alert_count; i++) {
        if (service->alerts[i].alert_id && strcmp(service->alerts[i].alert_id, alert_id) == 0) {
            service->alerts[i].is_resolved = true;
            agentos_mutex_unlock(&service->alert_lock);
            SVC_LOG_INFO("Alert resolved: %s", alert_id);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_mutex_unlock(&service->alert_lock);
    return AGENTOS_ERR_NOT_FOUND;
}

int monitor_service_health_check(monitor_service_t* service, const char* service_name,
                                 health_check_result_t** result) {
    if (!service || !result) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    health_check_result_t* hr = (health_check_result_t*)calloc(1, sizeof(health_check_result_t));
    if (!hr) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    hr->service_name = service_name ? strdup(service_name) : strdup("monitor_service");
    hr->is_healthy = service->initialized ? true : false;
    hr->timestamp = get_timestamp_ms();
    hr->error_code = 0;

    agentos_mutex_lock(&service->alert_lock);
    size_t unresolved_critical = 0;
    for (size_t i = 0; i < service->alert_count; i++) {
        if (!service->alerts[i].is_resolved && service->alerts[i].level >= ALERT_LEVEL_ERROR) {
            unresolved_critical++;
        }
    }
    agentos_mutex_unlock(&service->alert_lock);

    if (unresolved_critical > 5) {
        hr->is_healthy = false;
        hr->status_message = strdup("Too many unresolved critical alerts");
        hr->error_code = 1;
    } else if (unresolved_critical > 0) {
        hr->status_message = strdup("Some unresolved alerts present");
    } else {
        hr->status_message = strdup("Healthy");
    }

    *result = hr;
    return AGENTOS_SUCCESS;
}

int monitor_service_get_metrics(monitor_service_t* service, const char* metric_name,
                                metric_info_t*** metrics, size_t* count) {
    if (!service || !metrics || !count) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->metric_lock);

    size_t result_count = 0;
    for (size_t i = 0; i < service->metric_cache_count; i++) {
        if (!metric_name || (service->metric_cache[i]->name &&
                             strstr(service->metric_cache[i]->name, metric_name))) {
            result_count++;
        }
    }

    if (result_count == 0) {
        *metrics = NULL;
        *count = 0;
        agentos_mutex_unlock(&service->metric_lock);
        return AGENTOS_SUCCESS;
    }

    metric_info_t** result = (metric_info_t**)calloc(result_count, sizeof(metric_info_t*));
    if (!result) {
        agentos_mutex_unlock(&service->metric_lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t idx = 0;
    for (size_t i = 0; i < service->metric_cache_count && idx < result_count; i++) {
        if (!metric_name || (service->metric_cache[i]->name &&
                             strstr(service->metric_cache[i]->name, metric_name))) {
            result[idx++] = service->metric_cache[i];
        }
    }

    *metrics = result;
    *count = result_count;

    agentos_mutex_unlock(&service->metric_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_get_alerts(monitor_service_t* service, alert_info_t*** alerts, size_t* count) {
    if (!service || !alerts || !count) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->alert_lock);

    if (service->alert_count == 0) {
        *alerts = NULL;
        *count = 0;
        agentos_mutex_unlock(&service->alert_lock);
        return AGENTOS_SUCCESS;
    }

    alert_info_t** result = (alert_info_t**)calloc(service->alert_count, sizeof(alert_info_t*));
    if (!result) {
        agentos_mutex_unlock(&service->alert_lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < service->alert_count; i++) {
        alert_info_t* info = (alert_info_t*)calloc(1, sizeof(alert_info_t));
        if (info) {
            info->alert_id = service->alerts[i].alert_id;
            info->message = service->alerts[i].message;
            info->level = service->alerts[i].level;
            info->service_name = service->alerts[i].service_name;
            info->resource_id = service->alerts[i].resource_id;
            info->timestamp = service->alerts[i].timestamp;
            info->is_resolved = service->alerts[i].is_resolved;
        }
        result[i] = info;
    }

    *alerts = result;
    *count = service->alert_count;

    agentos_mutex_unlock(&service->alert_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_reload_config(monitor_service_t* service, const monitor_config_t* config) {
    if (!service || !config) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    free(service->config.log_file_path);
    free(service->config.metrics_storage_path);

    memcpy(&service->config, config, sizeof(monitor_config_t));
    service->config.log_file_path = config->log_file_path ? strdup(config->log_file_path) : NULL;
    service->config.metrics_storage_path = config->metrics_storage_path ? strdup(config->metrics_storage_path) : NULL;

    SVC_LOG_INFO("Monitor service config reloaded");
    return AGENTOS_SUCCESS;
}

int monitor_service_generate_report(monitor_service_t* service, char** report) {
    if (!service || !report) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char* buf = (char*)malloc(MAX_REPORT_SIZE);
    if (!buf) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t pos = 0;
    pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                    "=== AgentOS Monitor Report ===\n"
                    "Generated at: %llu\n\n",
                    (unsigned long long)get_timestamp_ms());

    agentos_mutex_lock(&service->metric_lock);
    pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                    "--- Metrics (%zu recorded) ---\n", service->metric_cache_count);
    for (size_t i = 0; i < service->metric_cache_count && pos < MAX_REPORT_SIZE - 256; i++) {
        metric_info_t* m = service->metric_cache[i];
        if (m && m->name) {
            pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                           "  %s: %.4f (type=%d, ts=%llu)\n",
                           m->name, m->value, m->type,
                           (unsigned long long)m->timestamp);
        }
    }
    agentos_mutex_unlock(&service->metric_lock);

    agentos_mutex_lock(&service->alert_lock);
    size_t unresolved = 0;
    for (size_t i = 0; i < service->alert_count; i++) {
        if (!service->alerts[i].is_resolved) unresolved++;
    }
    pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                    "\n--- Alerts (%zu total, %zu unresolved) ---\n",
                    service->alert_count, unresolved);
    const char* level_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    for (size_t i = 0; i < service->alert_count && pos < MAX_REPORT_SIZE - 256; i++) {
        alert_entry_t* a = &service->alerts[i];
        pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                       "  [%s] %s: %s%s\n",
                       level_str[a->level < ALERT_LEVEL_COUNT ? a->level : 0],
                       a->alert_id ? a->alert_id : "N/A",
                       a->message ? a->message : "N/A",
                       a->is_resolved ? " (resolved)" : "");
    }
    agentos_mutex_unlock(&service->alert_lock);

    agentos_mutex_lock(&service->trace_lock);
    pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                    "\n--- Traces (%zu recorded) ---\n", service->trace_count);
    for (size_t i = 0; i < service->trace_count && pos < MAX_REPORT_SIZE - 256; i++) {
        trace_entry_t* t = &service->traces[i];
        pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                       "  %s: %s (status=%d, duration=%llums)\n",
                       t->trace_id ? t->trace_id : "N/A",
                       t->operation_name ? t->operation_name : "N/A",
                       t->status,
                       (unsigned long long)(t->end_time - t->start_time));
    }
    agentos_mutex_unlock(&service->trace_lock);

    pos += snprintf(buf + pos, MAX_REPORT_SIZE - pos,
                    "\n--- Logs (%zu entries) ---\n", service->log_count);

    *report = buf;
    return AGENTOS_SUCCESS;
}

int monitor_service_start_agent_trace(monitor_service_t* service, const char* agent_id,
                                      const char* task_id, const loop_detection_config_t* loop_config,
                                      agent_execution_trace_t** trace) {
    if (!service || !trace) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!service->config.enable_tracing) {
        return AGENTOS_ERR_STATE_ERROR;
    }

    agentos_mutex_lock(&service->trace_lock);

    if (service->trace_count >= MAX_TRACES) {
        free(service->traces[0].trace_id);
        free(service->traces[0].operation_name);
        free(service->traces[0].service_name);
        memmove(&service->traces[0], &service->traces[1],
                (service->trace_count - 1) * sizeof(trace_entry_t));
        service->trace_count--;
    }

    trace_entry_t* entry = &service->traces[service->trace_count];
    char tid[64];
    snprintf(tid, sizeof(tid), "trace-%zu-%llu", service->trace_count, get_timestamp_ms());
    entry->trace_id = strdup(tid);
    entry->operation_name = task_id ? strdup(task_id) : strdup("unknown");
    entry->service_name = agent_id ? strdup(agent_id) : NULL;
    entry->start_time = get_timestamp_ms();
    entry->end_time = 0;
    entry->status = 0;
    entry->span_count = 0;
    service->trace_count++;

    agent_execution_trace_t* t = (agent_execution_trace_t*)calloc(1, sizeof(agent_execution_trace_t));
    if (t) {
        t->agent_id = agent_id ? strdup(agent_id) : NULL;
        t->task_id = task_id ? strdup(task_id) : NULL;
    }

    *trace = t;
    agentos_mutex_unlock(&service->trace_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_update_agent_state(monitor_service_t* service,
                                       agent_execution_trace_t* trace,
                                       agent_execution_state_t new_state,
                                       const char* location) {
    if (!service || !trace) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    (void)new_state;
    (void)location;
    return AGENTOS_SUCCESS;
}

int monitor_service_check_loop(monitor_service_t* service,
                               agent_execution_trace_t* trace,
                               bool* is_loop, double* confidence) {
    if (!service || !is_loop || !confidence) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    *is_loop = false;
    *confidence = 0.0;
    return AGENTOS_SUCCESS;
}

int monitor_service_end_agent_trace(monitor_service_t* service,
                                    agent_execution_trace_t* trace,
                                    agent_execution_state_t final_state) {
    if (!service || !trace) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    (void)final_state;

    agentos_mutex_lock(&service->trace_lock);
    for (size_t i = 0; i < service->trace_count; i++) {
        if (service->traces[i].trace_id && trace->agent_id && 
            strcmp(service->traces[i].trace_id, trace->agent_id) == 0) {
            service->traces[i].end_time = get_timestamp_ms();
            service->traces[i].status = 0;
            break;
        }
    }
    agentos_mutex_unlock(&service->trace_lock);

    free(trace);
    return AGENTOS_SUCCESS;
}

int monitor_service_get_agent_summary(monitor_service_t* service, const char* agent_id,
                                      uint64_t start_time, uint64_t end_time, char** summary) {
    if (!service || !summary) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char buf[4096];
    snprintf(buf, sizeof(buf),
             "Agent summary for %s (time range: %llu - %llu)\n"
             "Traces: %zu, Alerts: %zu, Metrics: %zu\n",
             agent_id ? agent_id : "all",
             (unsigned long long)start_time, (unsigned long long)end_time,
             service->trace_count, service->alert_count, service->metric_cache_count);

    *summary = strdup(buf);
    return AGENTOS_SUCCESS;
}

int monitor_service_export_agent_trace(monitor_service_t* service,
                                       agent_execution_trace_t* trace,
                                       const char* format, char** data, size_t* size) {
    if (!service || !data || !size) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    const char* fmt = format ? format : "json";
    char buf[2048];

    if (strcmp(fmt, "json") == 0) {
        snprintf(buf, sizeof(buf),
                 "{\"trace_id\":\"%s\",\"format\":\"json\"}",
                 trace && trace->agent_id ? trace->agent_id : "unknown");
    } else {
        snprintf(buf, sizeof(buf),
                 "trace_id=%s\nformat=%s\n",
                 trace && trace->agent_id ? trace->agent_id : "unknown", fmt);
    }

    *data = strdup(buf);
    *size = strlen(buf);
    return AGENTOS_SUCCESS;
}

int monitor_service_get_active_agents(monitor_service_t* service,
                                      char*** agent_ids, size_t* count) {
    if (!service || !agent_ids || !count) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&service->trace_lock);

    size_t active = 0;
    for (size_t i = 0; i < service->trace_count; i++) {
        if (service->traces[i].end_time == 0) {
            active++;
        }
    }

    if (active == 0) {
        *agent_ids = NULL;
        *count = 0;
        agentos_mutex_unlock(&service->trace_lock);
        return AGENTOS_SUCCESS;
    }

    char** ids = (char**)calloc(active, sizeof(char*));
    size_t idx = 0;
    for (size_t i = 0; i < service->trace_count && idx < active; i++) {
        if (service->traces[i].end_time == 0 && service->traces[i].service_name) {
            ids[idx++] = strdup(service->traces[i].service_name);
        }
    }

    *agent_ids = ids;
    *count = active;

    agentos_mutex_unlock(&service->trace_lock);
    return AGENTOS_SUCCESS;
}

int monitor_service_reset_loop_detection(monitor_service_t* service,
                                         agent_execution_trace_t* trace) {
    if (!service) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    (void)trace;
    return AGENTOS_SUCCESS;
}
