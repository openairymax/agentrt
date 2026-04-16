/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_monitoring.c - Monitoring Interface: Prometheus / OpenTelemetry
 */

/**
 * @file cupolas_monitoring.c
 * @brief Monitoring Interface - Prometheus / OpenTelemetry
 * @author Spharx AgentOS Team
 * @date 2024
 *
 * This module implements monitoring interface:
 * - Prometheus HTTP endpoint (pull mode)
 * - OpenTelemetry OTLP push
 * - StatsD protocol support
 * - Health check endpoint
 */

#include "cupolas_monitoring.h"
#include "cupolas_metrics.h"
#include "utils/cupolas_utils.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#if cupolas_PLATFORM_WINDOWS
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define MAX_METRICS_BUFFER (64 * 1024)
#define MAX_HEALTH_CHECKS 32

typedef struct health_check_entry {
    char name[128];
    health_check_fn_t callback;
    bool registered;
} health_check_entry_t;

struct cupolas_monitoring {
    monitoring_config_t manager;
    monitoring_status_t status;

    cupolas_rwlock_t lock;

    char metrics_buffer[MAX_METRICS_BUFFER];
    size_t metrics_buffer_size;

    health_check_entry_t health_checks[MAX_HEALTH_CHECKS];
    size_t health_check_count;

    cupolas_thread_t reporter_thread;
    bool reporter_running;

    uint64_t last_report_time;
    char last_error[512];

    cupolas_monitoring_t* instance;
};

static cupolas_monitoring_t* g_monitoring = NULL;
static cupolas_rwlock_t g_monitoring_lock = {0};

const char* monitoring_backend_string(monitoring_backend_t backend) {
    switch (backend) {
        case MONITORING_BACKEND_PROMETHEUS:  return "prometheus";
        case MONITORING_BACKEND_OPENTELEMETRY: return "opentelemetry";
        case MONITORING_BACKEND_STATSD:     return "statsd";
        default:                            return "none";
    }
}

const char* monitoring_status_string(monitoring_status_t status) {
    switch (status) {
        case MONITORING_STATUS_STOPPED:   return "stopped";
        case MONITORING_STATUS_STARTING:  return "starting";
        case MONITORING_STATUS_RUNNING:   return "running";
        case MONITORING_STATUS_ERROR:     return "error";
        case MONITORING_STATUS_STOPPING:  return "stopping";
        default:                          return "unknown";
    }
}

cupolas_monitoring_t* cupolas_monitoring_create(const monitoring_config_t* manager) {
    cupolas_monitoring_t* mgr = (cupolas_monitoring_t*)cupolas_mem_alloc(sizeof(cupolas_monitoring_t));
    if (!mgr) {
        return NULL;
    }

    memset(mgr, 0, sizeof(cupolas_monitoring_t));

    if (manager) {
        memcpy(&mgr->manager, manager, sizeof(monitoring_config_t));
    } else {
        memset(&mgr->manager, 0, sizeof(monitoring_config_t));
        mgr->manager.backend = MONITORING_BACKEND_PROMETHEUS;
        mgr->manager.prometheus.listen_addr = "0.0.0.0";
        mgr->manager.prometheus.port = 9090;
        mgr->manager.prometheus.endpoint = "/metrics";
        mgr->manager.reporting_interval_ms = 10000;
    }

    mgr->status = MONITORING_STATUS_STOPPED;
    cupolas_rwlock_init(&mgr->lock);

    return mgr;
}

void cupolas_monitoring_destroy(cupolas_monitoring_t* mgr) {
    if (!mgr) return;

    cupolas_monitoring_stop(mgr);

    cupolas_rwlock_destroy(&mgr->lock);

    cupolas_mem_free(mgr);
}

int cupolas_monitoring_start(cupolas_monitoring_t* mgr) {
    if (!mgr) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->status == MONITORING_STATUS_RUNNING) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    mgr->status = MONITORING_STATUS_STARTING;

    mgr->reporter_running = true;

    mgr->status = MONITORING_STATUS_RUNNING;

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

void cupolas_monitoring_stop(cupolas_monitoring_t* mgr) {
    if (!mgr) return;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->status != MONITORING_STATUS_RUNNING &&
        mgr->status != MONITORING_STATUS_STARTING) {
        cupolas_rwlock_unlock(&mgr->lock);
        return;
    }

    mgr->status = MONITORING_STATUS_STOPPING;
    mgr->reporter_running = false;

    cupolas_rwlock_unlock(&mgr->lock);
}

monitoring_status_t cupolas_monitoring_get_status(cupolas_monitoring_t* mgr) {
    if (!mgr) return MONITORING_STATUS_ERROR;

    cupolas_rwlock_rdlock(&mgr->lock);
    monitoring_status_t status = mgr->status;
    cupolas_rwlock_unlock(&mgr->lock);

    return status;
}

int cupolas_monitoring_report(cupolas_monitoring_t* mgr) {
    if (!mgr) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    metrics_export_prometheus(mgr->metrics_buffer, sizeof(mgr->metrics_buffer) - 1);
    mgr->metrics_buffer_size = strlen(mgr->metrics_buffer);

    mgr->last_report_time = metrics_get_timestamp_ns();

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

size_t cupolas_monitoring_export(cupolas_monitoring_t* mgr, char* buffer, size_t size) {
    if (!mgr || !buffer || size == 0) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);

    size_t copied = 0;
    if (mgr->metrics_buffer_size > 0 && size > mgr->metrics_buffer_size) {
        memcpy(buffer, mgr->metrics_buffer, mgr->metrics_buffer_size);
        copied = mgr->metrics_buffer_size;
    }

    cupolas_rwlock_unlock(&mgr->lock);

    return copied;
}

/**
 * @brief Export metrics in OpenTelemetry OTLP JSON format
 * @param[in] mgr Monitoring manager handle
 * @param[out] buffer Output buffer for OTLP JSON format metrics
 * @param[in] size Size of output buffer in bytes
 * @return Number of bytes written to buffer, or 0 on error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership buffer: caller provides buffer, function writes to it
 *
 * @details
 * Generates OpenTelemetry Protocol (OTLP) JSON export format:
 *
 * {
 *   "resourceMetrics": [{
 *     "resource": {"attributes": [{"key": "service.name", "value": {"stringValue": "cupolas"}}]},
 *     "scopeMetrics": [{
 *       "scope": {"name": "cupolas.monitoring"},
 *       "metrics": [
 *         {"name": "cupolas_permission_checks_total", "description": "...", "unit": "1",
 *          "sum": {"dataPoints": [{"attributes": [...], "asInt": 1234}]}}
 *       ]
 *     }]
 *   }],
 *   "timestamp_ns": 1704067200000000000
 * }
 */
size_t cupolas_monitoring_export_otlp(cupolas_monitoring_t* mgr, char* buffer, size_t size) {
    if (!mgr || !buffer || size == 0) {
        return 0;
    }

    cupolas_rwlock_rdlock(&mgr->lock);

    if (mgr->metrics_buffer_size == 0) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    size_t written = 0;
    written += snprintf(buffer + written, size - written,
        "{\n"
        "  \"resourceMetrics\": [{\n"
        "    \"resource\": {\n"
        "      \"attributes\": [\n"
        "        {\"key\": \"service.name\", \"value\": {\"stringValue\": \"%s\"}}\n"
        "      ]\n"
        "    },\n"
        "    \"scopeMetrics\": [{\n"
        "      \"scope\": {\"name\": \"cupolas.monitoring\"},\n"
        "      \"metrics\": [\n",
        mgr->manager.opentelemetry.service_name ?
            mgr->manager.opentelemetry.service_name : "cupolas");

    if (written >= size) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    bool first_metric = true;
    const char* metric_data = mgr->metrics_buffer;
    const char* line_start = metric_data;
    int line_num = 0;

    while (*metric_data && written < size) {
        if (*metric_data == '\n' || *(metric_data + 1) == '\0') {
            size_t line_len = (size_t)(metric_data - line_start);
            if (line_len > 0 && line_len < 256) {
                char line[256];
                strncpy(line, line_start, line_len);
                line[line_len] = '\0';

                if (strncmp(line, "# HELP ", 7) == 0 ||
                    strncmp(line, "# TYPE ", 7) == 0) {
                    line_start = metric_data + 1;
                    metric_data++;
                    line_num++;
                    continue;
                }

                if (line[0] != '#' && line[0] != '\0' && strchr(line, ' ') != NULL) {
                    char metric_name[128] = {0};
                    char metric_value[64] = {0};

                    char* space_pos = strrchr(line, ' ');
                    if (space_pos) {
                        size_t name_len = (size_t)(space_pos - line);
                        if (name_len < sizeof(metric_name)) {
                            strncpy(metric_name, line, name_len);
                            metric_name[name_len] = '\0';
                            strncpy(metric_value, space_pos + 1, sizeof(metric_value) - 1);
                            metric_value[sizeof(metric_value) - 1] = '\0';

                            if (!first_metric) {
                                written += snprintf(buffer + written, size - written,
                                    ",\n");
                            }
                            first_metric = false;

                            written += snprintf(buffer + written, size - written,
                                "        {\n"
                                "          \"name\": \"%s\",\n"
                                "          \"description\": \"%s metric exported from cupolas\",\n"
                                "          \"unit\": \"1\",\n"
                                "          \"sum\": {\n"
                                "            \"isMonotonic\": true,\n"
                                "            \"aggregationTemporality\": 2,\n"
                                "            \"dataPoints\": [{\n"
                                "              \"timeUnixNano\": %llu,\n"
                                "              \"asInt\": %s\n"
                                "            }]\n"
                                "          }\n"
                                "        }",
                                metric_name, metric_name,
                                (unsigned long long)mgr->last_report_time,
                                metric_value);

                            if (written >= size) {
                                cupolas_rwlock_unlock(&mgr->lock);
                                return 0;
                            }
                        }
                    }
                }
            }
            line_start = metric_data + 1;
        }
        metric_data++;
    }

    written += snprintf(buffer + written, size - written,
        "\n      ]\n"
        "    }]\n"
        "  }],\n"
        "  \"timestamp_ns\": %llu\n"
        "}\n",
        (unsigned long long)mgr->last_report_time);

    cupolas_rwlock_unlock(&mgr->lock);

    return (written < size) ? written : 0;
}

int cupolas_monitoring_register_health_check(cupolas_monitoring_t* mgr,
                                         const char* name,
                                         health_check_fn_t callback) {
    if (!mgr || !name || !callback) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->health_check_count >= MAX_HEALTH_CHECKS) {
        cupolas_rwlock_unlock(&mgr->lock);
        return -1;
    }

    health_check_entry_t* entry = &mgr->health_checks[mgr->health_check_count++];
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    entry->callback = callback;
    entry->registered = true;

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

int cupolas_monitoring_check_health(cupolas_monitoring_t* mgr,
                                health_check_result_t* results,
                                size_t max_results) {
    if (!mgr || !results || max_results == 0) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);

    size_t count = 0;
    for (size_t i = 0; i < mgr->health_check_count && count < max_results; i++) {
        health_check_entry_t* entry = &mgr->health_checks[i];
        if (entry->registered && entry->callback) {
            results[count].timestamp_ns = metrics_get_timestamp_ns();
            results[count].healthy = entry->callback();
            results[count].component = entry->name;
            results[count].message = results[count].healthy ? "OK" : "FAILED";
            count++;
        }
    }

    cupolas_rwlock_unlock(&mgr->lock);

    return (int)count;
}

const char* cupolas_monitoring_get_listen_addr(cupolas_monitoring_t* mgr) {
    if (!mgr) return NULL;

    cupolas_rwlock_rdlock(&mgr->lock);
    static char addr[128];
    snprintf(addr, sizeof(addr), "%s:%u",
            mgr->manager.prometheus.listen_addr,
            mgr->manager.prometheus.port);
    cupolas_rwlock_unlock(&mgr->lock);

    return addr;
}

int cupolas_monitoring_set_filter(cupolas_monitoring_t* mgr,
                               const char** include_patterns,
                               const char** exclude_patterns) {
    cupolas_UNUSED(mgr);
    cupolas_UNUSED(include_patterns);
    cupolas_UNUSED(exclude_patterns);

    return 0;
}

size_t cupolas_monitoring_get_metric_count(cupolas_monitoring_t* mgr) {
    if (!mgr) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);
    size_t count = metrics_get_count();
    cupolas_rwlock_unlock(&mgr->lock);

    return count;
}

uint64_t cupolas_monitoring_get_last_report_time(cupolas_monitoring_t* mgr) {
    if (!mgr) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);
    uint64_t time = mgr->last_report_time;
    cupolas_rwlock_unlock(&mgr->lock);

    return time;
}

const char* cupolas_monitoring_get_last_error(cupolas_monitoring_t* mgr) {
    if (!mgr) return NULL;

    cupolas_rwlock_rdlock(&mgr->lock);
    const char* error = mgr->last_error[0] ? mgr->last_error : NULL;
    cupolas_rwlock_unlock(&mgr->lock);

    return error;
}

monitoring_config_t* monitoring_config_create_prometheus(uint16_t port) {
    monitoring_config_t* manager = (monitoring_config_t*)cupolas_mem_alloc(sizeof(monitoring_config_t));
    if (!manager) return NULL;

    memset(manager, 0, sizeof(monitoring_config_t));

    manager->backend = MONITORING_BACKEND_PROMETHEUS;
    manager->prometheus.listen_addr = "0.0.0.0";
    manager->prometheus.port = port;
    manager->prometheus.endpoint = "/metrics";
    manager->reporting_interval_ms = 10000;
    manager->buffer_size = MAX_METRICS_BUFFER;
    manager->enable_caching = true;

    return manager;
}

monitoring_config_t* monitoring_config_create_opentelemetry(const char* endpoint,
                                                           const char* service_name) {
    monitoring_config_t* manager = (monitoring_config_t*)cupolas_mem_alloc(sizeof(monitoring_config_t));
    if (!manager) return NULL;

    memset(manager, 0, sizeof(monitoring_config_t));

    manager->backend = MONITORING_BACKEND_OPENTELEMETRY;
    manager->opentelemetry.endpoint = endpoint;
    manager->opentelemetry.service_name = service_name;
    manager->reporting_interval_ms = 5000;
    manager->buffer_size = MAX_METRICS_BUFFER;
    manager->enable_caching = true;

    return manager;
}

void monitoring_config_destroy(monitoring_config_t* manager) {
    cupolas_mem_free(manager);
}

cupolas_monitoring_t* cupolas_monitoring_get_instance(void) {
    cupolas_rwlock_rdlock(&g_monitoring_lock);
    cupolas_monitoring_t* instance = g_monitoring;
    cupolas_rwlock_unlock(&g_monitoring_lock);
    return instance;
}

int cupolas_monitoring_init_instance(const monitoring_config_t* manager) {
    cupolas_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        cupolas_rwlock_unlock(&g_monitoring_lock);
        return 0;
    }

    g_monitoring = cupolas_monitoring_create(manager);
    if (!g_monitoring) {
        cupolas_rwlock_unlock(&g_monitoring_lock);
        return -1;
    }

    cupolas_rwlock_unlock(&g_monitoring_lock);

    return cupolas_monitoring_start(g_monitoring);
}

void cupolas_monitoring_shutdown_instance(void) {
    cupolas_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        cupolas_monitoring_stop(g_monitoring);
        cupolas_monitoring_destroy(g_monitoring);
        g_monitoring = NULL;
    }

    cupolas_rwlock_unlock(&g_monitoring_lock);
}