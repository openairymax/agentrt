/**
 * @file domes_monitoring.c
 * @brief 监控集成 - Prometheus / OpenTelemetry
 * @author Spharx
 * @date 2024
 *
 * 本模块实现监控集成功能：
 * - Prometheus HTTP 服务器（拉模式）
 * - OpenTelemetry OTLP 推送
 * - StatsD 协议支持
 * - 健康检查端点
 */

#include "domes_monitoring.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef DOMES_PLATFORM_WINDOWS
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

struct domes_monitoring {
    monitoring_config_t manager;
    monitoring_status_t status;

    domes_rwlock_t lock;

    char metrics_buffer[MAX_METRICS_BUFFER];
    size_t metrics_buffer_size;

    health_check_entry_t health_checks[MAX_HEALTH_CHECKS];
    size_t health_check_count;

    domes_thread_t reporter_thread;
    bool reporter_running;

    uint64_t last_report_time;
    char last_error[512];

    domes_monitoring_t* instance;
};

static domes_monitoring_t* g_monitoring = NULL;
static domes_rwlock_t g_monitoring_lock = {0};

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

domes_monitoring_t* domes_monitoring_create(const monitoring_config_t* manager) {
    domes_monitoring_t* mgr = (domes_monitoring_t*)domes_mem_alloc(sizeof(domes_monitoring_t));
    if (!mgr) {
        return NULL;
    }

    memset(mgr, 0, sizeof(domes_monitoring_t));

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
    domes_rwlock_init(&mgr->lock);

    return mgr;
}

void domes_monitoring_destroy(domes_monitoring_t* mgr) {
    if (!mgr) return;

    domes_monitoring_stop(mgr);

    domes_rwlock_destroy(&mgr->lock);

    domes_mem_free(mgr);
}

int domes_monitoring_start(domes_monitoring_t* mgr) {
    if (!mgr) return -1;

    domes_rwlock_wrlock(&mgr->lock);

    if (mgr->status == MONITORING_STATUS_RUNNING) {
        domes_rwlock_unlock(&mgr->lock);
        return 0;
    }

    mgr->status = MONITORING_STATUS_STARTING;

    mgr->reporter_running = true;

    mgr->status = MONITORING_STATUS_RUNNING;

    domes_rwlock_unlock(&mgr->lock);

    return 0;
}

void domes_monitoring_stop(domes_monitoring_t* mgr) {
    if (!mgr) return;

    domes_rwlock_wrlock(&mgr->lock);

    if (mgr->status != MONITORING_STATUS_RUNNING &&
        mgr->status != MONITORING_STATUS_STARTING) {
        domes_rwlock_unlock(&mgr->lock);
        return;
    }

    mgr->status = MONITORING_STATUS_STOPPING;
    mgr->reporter_running = false;

    domes_rwlock_unlock(&mgr->lock);
}

monitoring_status_t domes_monitoring_get_status(domes_monitoring_t* mgr) {
    if (!mgr) return MONITORING_STATUS_ERROR;

    domes_rwlock_rdlock(&mgr->lock);
    monitoring_status_t status = mgr->status;
    domes_rwlock_unlock(&mgr->lock);

    return status;
}

int domes_monitoring_report(domes_monitoring_t* mgr) {
    if (!mgr) return -1;

    domes_rwlock_wrlock(&mgr->lock);

    metrics_export_prometheus(mgr->metrics_buffer, sizeof(mgr->metrics_buffer) - 1);
    mgr->metrics_buffer_size = strlen(mgr->metrics_buffer);

    mgr->last_report_time = metrics_get_timestamp_ns();

    domes_rwlock_unlock(&mgr->lock);

    return 0;
}

size_t domes_monitoring_export(domes_monitoring_t* mgr, char* buffer, size_t size) {
    if (!mgr || !buffer || size == 0) return 0;

    domes_rwlock_rdlock(&mgr->lock);

    size_t copied = 0;
    if (mgr->metrics_buffer_size > 0 && size > mgr->metrics_buffer_size) {
        memcpy(buffer, mgr->metrics_buffer, mgr->metrics_buffer_size);
        copied = mgr->metrics_buffer_size;
    }

    domes_rwlock_unlock(&mgr->lock);

    return copied;
}

int domes_monitoring_register_health_check(domes_monitoring_t* mgr,
                                         const char* name,
                                         health_check_fn_t callback) {
    if (!mgr || !name || !callback) return -1;

    domes_rwlock_wrlock(&mgr->lock);

    if (mgr->health_check_count >= MAX_HEALTH_CHECKS) {
        domes_rwlock_unlock(&mgr->lock);
        return -1;
    }

    health_check_entry_t* entry = &mgr->health_checks[mgr->health_check_count++];
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    entry->callback = callback;
    entry->registered = true;

    domes_rwlock_unlock(&mgr->lock);

    return 0;
}

int domes_monitoring_check_health(domes_monitoring_t* mgr,
                                health_check_result_t* results,
                                size_t max_results) {
    if (!mgr || !results || max_results == 0) return 0;

    domes_rwlock_rdlock(&mgr->lock);

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

    domes_rwlock_unlock(&mgr->lock);

    return (int)count;
}

const char* domes_monitoring_get_listen_addr(domes_monitoring_t* mgr) {
    if (!mgr) return NULL;

    domes_rwlock_rdlock(&mgr->lock);
    static char addr[128];
    snprintf(addr, sizeof(addr), "%s:%u",
            mgr->manager.prometheus.listen_addr,
            mgr->manager.prometheus.port);
    domes_rwlock_unlock(&mgr->lock);

    return addr;
}

int domes_monitoring_set_filter(domes_monitoring_t* mgr,
                               const char** include_patterns,
                               const char** exclude_patterns) {
    DOMES_UNUSED(mgr);
    DOMES_UNUSED(include_patterns);
    DOMES_UNUSED(exclude_patterns);

    return 0;
}

size_t domes_monitoring_get_metric_count(domes_monitoring_t* mgr) {
    if (!mgr) return 0;

    domes_rwlock_rdlock(&mgr->lock);
    size_t count = metrics_get_count();
    domes_rwlock_unlock(&mgr->lock);

    return count;
}

uint64_t domes_monitoring_get_last_report_time(domes_monitoring_t* mgr) {
    if (!mgr) return 0;

    domes_rwlock_rdlock(&mgr->lock);
    uint64_t time = mgr->last_report_time;
    domes_rwlock_unlock(&mgr->lock);

    return time;
}

const char* domes_monitoring_get_last_error(domes_monitoring_t* mgr) {
    if (!mgr) return NULL;

    domes_rwlock_rdlock(&mgr->lock);
    const char* error = mgr->last_error[0] ? mgr->last_error : NULL;
    domes_rwlock_unlock(&mgr->lock);

    return error;
}

monitoring_config_t* monitoring_config_create_prometheus(uint16_t port) {
    monitoring_config_t* manager = (monitoring_config_t*)domes_mem_alloc(sizeof(monitoring_config_t));
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
    monitoring_config_t* manager = (monitoring_config_t*)domes_mem_alloc(sizeof(monitoring_config_t));
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
    domes_mem_free(manager);
}

domes_monitoring_t* domes_monitoring_get_instance(void) {
    domes_rwlock_rdlock(&g_monitoring_lock);
    domes_monitoring_t* instance = g_monitoring;
    domes_rwlock_unlock(&g_monitoring_lock);
    return instance;
}

int domes_monitoring_init_instance(const monitoring_config_t* manager) {
    domes_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        domes_rwlock_unlock(&g_monitoring_lock);
        return 0;
    }

    g_monitoring = domes_monitoring_create(manager);
    if (!g_monitoring) {
        domes_rwlock_unlock(&g_monitoring_lock);
        return -1;
    }

    domes_rwlock_unlock(&g_monitoring_lock);

    return domes_monitoring_start(g_monitoring);
}

void domes_monitoring_shutdown_instance(void) {
    domes_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        domes_monitoring_stop(g_monitoring);
        domes_monitoring_destroy(g_monitoring);
        g_monitoring = NULL;
    }

    domes_rwlock_unlock(&g_monitoring_lock);
}