/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file prometheus_exporter.c
 * @brief C-L10: Prometheus scrape endpoint 实现
 *
 * 实现 /metrics HTTP 端点，暴露 14 项必需指标供 Prometheus 抓取。
 * 工程标准规范手册 16.1 定义的指标：
 *   1-10: 核心可观测性指标
 *   11-14: 内存可观测性指标
 */

#include "prometheus_exporter.h"

#include "platform.h"
#include "svc_logger.h"
#include "unified_metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 14 项必需指标定义 ==================== */

/* --- 核心可观测性指标 (Section 16.1) --- */
static const char *REQUIRED_METRICS[][5] = {
    /* {name, type, help, labels} */
    {"agentos_cognition_latency_ms", "histogram", "Cognitive phase latency in milliseconds",
     "agent_id"},
    {"agentos_llm_request_duration_ms", "histogram", "LLM request duration in milliseconds",
     "provider,model"},
    {"agentos_llm_cost_usd_total", "counter", "Total LLM cost in USD", "provider"},
    {"agentos_tool_call_total", "counter", "Total tool call count", "tool_name,status"},
    {"agentos_memory_operations_total", "counter", "Total memory operations count",
     "layer,operation"},
    {"agentos_hook_execution_ms", "histogram", "Hook execution latency in milliseconds",
     "hook_name,event"},
    {"agentos_connection_health", "gauge", "Connection line health status (0=down, 1=up)",
     "connection_id"},
    {"agentos_plugin_lifecycle_total", "counter", "Plugin lifecycle events total",
     "plugin_name,event"},
    {"agentos_llm_retry_total", "counter", "Total LLM retry count", "provider,error_category"},
    {"agentos_config_reload_total", "counter", "Total config reload count", "status"},

    /* --- 内存可观测性指标 --- */
    {"agentos_memory_rss_bytes", "gauge", "Resident memory size in bytes", ""},
    {"agentos_memory_heap_bytes", "gauge", "Heap memory usage in bytes", ""},
    {"agentos_memory_pool_utilization", "gauge", "Memory pool utilization (0.0~1.0)", ""},
    {"agentos_oom_events_total", "counter", "Total OOM event count", "level"},
};

#define REQUIRED_METRICS_COUNT                                                \
    (sizeof(REQUIRED_METRICS) / sizeof(REQUIRED_METRICS[0]))

/* ==================== 内部状态 ==================== */

static char g_module_name[UM_MODULE_NAME_LEN] = "monit_d";
static int g_initialized = 0;
static uint64_t g_scrape_count = 0;       /* C-L10: 抓取计数 */
static uint64_t g_scrape_errors = 0;      /* C-L10: 抓取错误计数 */

/* ==================== 辅助函数 ==================== */

/**
 * @brief 根据字符串名称获取指标类型枚举
 */
static um_metric_type_t parse_metric_type(const char *type_str)
{
    if (!type_str)
        return UM_TYPE_GAUGE;
    if (strcmp(type_str, "counter") == 0)
        return UM_TYPE_COUNTER;
    if (strcmp(type_str, "gauge") == 0)
        return UM_TYPE_GAUGE;
    if (strcmp(type_str, "histogram") == 0)
        return UM_TYPE_HISTOGRAM;
    if (strcmp(type_str, "summary") == 0)
        return UM_TYPE_SUMMARY;
    return UM_TYPE_GAUGE;
}

/* ==================== 公共接口实现 ==================== */

int prometheus_exporter_init(const char *service_name)
{
    if (g_initialized) {
        SVC_LOG_WARN("C-L10: Prometheus exporter already initialized");
        return 0;
    }

    /* 设置模块名称 */
    if (service_name && service_name[0]) {
        AGENTOS_STRNCPY_TERM(g_module_name, service_name, sizeof(g_module_name));
    }

    /* 初始化统一指标收集器 */
    um_config_t config = um_create_default_config();
    AGENTOS_STRNCPY_TERM(config.service_name, g_module_name, sizeof(config.service_name));
    config.enable_default_metrics = true;
    config.scrape_interval_ms = 15000;  /* 15s scrape interval */

    if (um_init(&config) != 0) {
        SVC_LOG_ERROR("C-L10: Failed to initialize unified metrics for '%s'", g_module_name);
        return -1;
    }

    /* 注册模块 */
    if (um_register_module(g_module_name, NULL) != 0) {
        SVC_LOG_ERROR("C-L10: Failed to register metrics module '%s'", g_module_name);
        um_shutdown();
        return -1;
    }

    g_initialized = 1;
    SVC_LOG_INFO("C-L10: Prometheus exporter initialized for '%s'", g_module_name);
    return 0;
}

void prometheus_exporter_shutdown(void)
{
    if (!g_initialized)
        return;

    um_unregister_module(g_module_name);
    um_shutdown();
    g_initialized = 0;
    SVC_LOG_INFO("C-L10: Prometheus exporter shutdown");
}

int prometheus_exporter_register_required_metrics(void)
{
    if (!g_initialized) {
        SVC_LOG_ERROR("C-L10: Prometheus exporter not initialized, cannot register metrics");
        return -1;
    }

    int registered = 0;
    int failed = 0;

    for (size_t i = 0; i < REQUIRED_METRICS_COUNT; i++) {
        const char *name = REQUIRED_METRICS[i][0];
        const char *type_str = REQUIRED_METRICS[i][1];
        const char *help = REQUIRED_METRICS[i][2];
        const char *labels = REQUIRED_METRICS[i][3];

        um_metric_type_t type = parse_metric_type(type_str);

        if (um_register_metric(g_module_name, name, type, help, labels) == 0) {
            SVC_LOG_DEBUG("C-L10: Registered metric [%zu/%zu] %s (type=%s, labels=%s)",
                          i + 1, REQUIRED_METRICS_COUNT, name, type_str,
                          labels[0] ? labels : "none");
            registered++;
        } else {
            SVC_LOG_WARN("C-L10: Failed to register metric %s", name);
            failed++;
        }
    }

    SVC_LOG_INFO("C-L10: Registered %d/%zu required metrics (%d failed)", registered,
                 REQUIRED_METRICS_COUNT, failed);

    return (failed > 0) ? -1 : 0;
}

/* ==================== HTTP /metrics 端点 ==================== */

int prometheus_exporter_handle_http(const char *request, size_t request_len,
                                    char **response, size_t *response_len)
{
    if (!request || !response || !response_len)
        return -1;

    /* 检测是否为 HTTP GET /metrics 请求 */
    if (request_len < 14)
        return -1;

    /* 匹配 "GET /metrics" 前缀 */
    if (strncmp(request, "GET /metrics", 12) != 0)
        return -1;

    SVC_LOG_DEBUG("C-L10: Prometheus scrape request received (scrape #%llu)",
                  (unsigned long long)(g_scrape_count + 1));

    /* 获取 Prometheus 格式指标 */
    char *metrics_text = prometheus_exporter_get_metrics();
    if (!metrics_text) {
        g_scrape_errors++;
        SVC_LOG_ERROR("C-L10: Failed to collect metrics for scrape #%llu",
                      (unsigned long long)(g_scrape_count + 1));
        /* 500 Internal Server Error */
        const char *err_body = "Failed to collect metrics\n";
        size_t body_len = strlen(err_body);

        size_t buf_size = 256 + body_len;
        char *resp = (char *)AGENTOS_MALLOC(buf_size);
        if (!resp)
            return -1;

        int header_len = snprintf(resp, buf_size,
                                  "HTTP/1.1 500 Internal Server Error\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  body_len);
        AGENTOS_MEMCPY(resp + header_len, err_body, body_len);
        *response = resp;
        *response_len = (size_t)header_len + body_len;
        return 0;
    }

    g_scrape_count++;
    size_t metrics_len = strlen(metrics_text);

    SVC_LOG_INFO("C-L10: Prometheus scrape #%llu — %zu bytes of metrics",
                 (unsigned long long)g_scrape_count, metrics_len);

    /* 构建 HTTP 响应 */
    char header_buf[256];
    int header_len = snprintf(header_buf, sizeof(header_buf),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain; version=0.0.4\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              metrics_len);

    size_t total_len = (size_t)header_len + metrics_len;
    char *resp = (char *)AGENTOS_MALLOC(total_len + 1);
    if (!resp) {
        AGENTOS_FREE(metrics_text);
        return -1;
    }

    AGENTOS_MEMCPY(resp, header_buf, (size_t)header_len);
    AGENTOS_MEMCPY(resp + header_len, metrics_text, metrics_len);
    resp[total_len] = '\0';

    AGENTOS_FREE(metrics_text);

    *response = resp;
    *response_len = total_len;
    return 0;
}

/* ==================== 指标值更新 ==================== */

void prometheus_counter_inc(const char *name, double value)
{
    if (!g_initialized || !name)
        return;
    um_increment(g_module_name, name, (uint64_t)value);
}

void prometheus_gauge_set(const char *name, double value)
{
    if (!g_initialized || !name)
        return;
    um_gauge_set(g_module_name, name, value);
}

void prometheus_histogram_observe(const char *name, double value)
{
    if (!g_initialized || !name)
        return;
    um_observe(g_module_name, name, value);
}

char *prometheus_exporter_get_metrics(void)
{
    if (!g_initialized) {
        SVC_LOG_WARN("C-L10: Prometheus exporter not initialized");
        return NULL;
    }

    /* 更新默认系统指标（CPU/内存等） */
    um_update_default_metrics();

    /* C-L10: 更新 scrape 统计指标 */
    prometheus_gauge_set("agentos_monit_scrape_count", (double)g_scrape_count);
    prometheus_gauge_set("agentos_monit_scrape_errors", (double)g_scrape_errors);

    /* 导出 Prometheus 格式 */
    char *result = um_export_prometheus_module(g_module_name);
    if (!result) {
        SVC_LOG_WARN("C-L10: Failed to export Prometheus metrics");
    }

    return result;
}

/* ==================== C-L10: 抓取统计 ==================== */

void prometheus_exporter_get_scrape_stats(uint64_t *out_count, uint64_t *out_errors)
{
    if (out_count)  *out_count = g_scrape_count;
    if (out_errors) *out_errors = g_scrape_errors;
}