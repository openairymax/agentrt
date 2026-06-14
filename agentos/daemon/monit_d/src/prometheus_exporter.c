/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file prometheus_exporter.c
 * @brief C-L10: Prometheus scrape endpoint 实现
 *
 * 实现功能:
 * - P1.9.1: 暴露 /metrics HTTP 端点
 * - P1.9.2: 注册 14 项必需指标
 * - P1.9.3: 支持 Prometheus scrape
 */

#include "prometheus_exporter.h"

#include "daemon_errors.h"
#include "monitor_service.h"
#include "platform.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 14 项必需指标定义 ==================== */

/* 10 项核心指标 (工程标准规范手册 16.1) */
#define METRIC_COGNITION_LATENCY    "agentos_cognition_latency_ms"
#define METRIC_LLM_REQUEST_DURATION "agentos_llm_request_duration_ms"
#define METRIC_LLM_COST_USD         "agentos_llm_cost_usd_total"
#define METRIC_TOOL_CALL            "agentos_tool_call_total"
#define METRIC_MEMORY_OPERATIONS    "agentos_memory_operations_total"
#define METRIC_HOOK_EXECUTION       "agentos_hook_execution_ms"
#define METRIC_CONNECTION_HEALTH    "agentos_connection_health"
#define METRIC_PLUGIN_LIFECYCLE     "agentos_plugin_lifecycle_total"
#define METRIC_LLM_RETRY            "agentos_llm_retry_total"
#define METRIC_CONFIG_RELOAD        "agentos_config_reload_total"

/* 4 项内存可观测性指标 (工程标准规范手册 16.4) */
#define METRIC_MEMORY_RSS            "agentos_memory_rss_bytes"
#define METRIC_MEMORY_HEAP           "agentos_memory_heap_bytes"
#define METRIC_MEMORY_POOL_UTIL      "agentos_memory_pool_utilization"
#define METRIC_OOM_EVENTS            "agentos_oom_events_total"

/* ==================== 内部状态 ==================== */

static int g_exporter_initialized = 0;

/* 前向声明 metrics.c 的接口 */
extern int metrics_init(const void *config);
extern void metrics_shutdown(void);
extern int metrics_register(const char *name, const char *description, const char *unit,
                            metric_type_t type, const double *histogram_buckets,
                            size_t bucket_count);
extern char *metrics_export_prometheus(void);
extern int metrics_counter_inc(const char *name, const void *labels, size_t label_count);
extern int metrics_counter_add(const char *name, double value, const void *labels,
                               size_t label_count);
extern int metrics_gauge_set(const char *name, double value, const void *labels,
                             size_t label_count);
extern int metrics_histogram_observe(const char *name, double value, const void *labels,
                                     size_t label_count);
extern int metrics_get_value(const char *name, const void *labels, size_t label_count,
                             double *value);
extern size_t metrics_get_count(void);

/* ==================== 默认直方图桶 ==================== */

static const double k_default_latency_buckets[] = {
    1.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0,
    1000.0, 2500.0, 5000.0, 10000.0, 30000.0, 60000.0
};

static const double k_default_size_buckets[] = {
    1024.0, 4096.0, 16384.0, 65536.0, 262144.0,
    1048576.0, 4194304.0, 16777216.0, 67108864.0
};

/* ==================== 公共接口实现 ==================== */

int prometheus_exporter_init(const char *service_name)
{
    if (g_exporter_initialized) {
        SVC_LOG_WARN("C-L10: Prometheus exporter already initialized");
        return 0;
    }

    /* 初始化指标系统 */
    int ret = metrics_init(NULL);
    if (ret != 0) {
        SVC_LOG_ERROR("C-L10: Failed to initialize metrics system (err=%d)", ret);
        return ret;
    }

    /* 注册 14 项必需指标 */
    ret = prometheus_exporter_register_required_metrics();
    if (ret != 0) {
        SVC_LOG_ERROR("C-L10: Failed to register required metrics (err=%d)", ret);
        metrics_shutdown();
        return ret;
    }

    g_exporter_initialized = 1;
    SVC_LOG_INFO("C-L10: Prometheus exporter initialized for '%s' "
                 "(14 required metrics registered)", service_name);
    return 0;
}

void prometheus_exporter_shutdown(void)
{
    if (!g_exporter_initialized) {
        return;
    }

    metrics_shutdown();
    g_exporter_initialized = 0;
    SVC_LOG_INFO("C-L10: Prometheus exporter shutdown");
}

int prometheus_exporter_register_required_metrics(void)
{
    SVC_LOG_INFO("C-L10: Registering 14 required metrics...");

    /*
     * 10 项核心指标 (工程标准规范手册 16.1)
     */

    /* 1. agentos_cognition_latency_ms - Histogram */
    metrics_register(METRIC_COGNITION_LATENCY,
                     "认知阶段延迟（毫秒）",
                     "ms",
                     METRIC_TYPE_HISTOGRAM,
                     k_default_latency_buckets,
                     sizeof(k_default_latency_buckets) / sizeof(k_default_latency_buckets[0]));

    /* 2. agentos_llm_request_duration_ms - Histogram */
    metrics_register(METRIC_LLM_REQUEST_DURATION,
                     "LLM 请求延迟（毫秒）",
                     "ms",
                     METRIC_TYPE_HISTOGRAM,
                     k_default_latency_buckets,
                     sizeof(k_default_latency_buckets) / sizeof(k_default_latency_buckets[0]));

    /* 3. agentos_llm_cost_usd_total - Counter */
    metrics_register(METRIC_LLM_COST_USD,
                     "LLM 累计费用（美元）",
                     "usd",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /* 4. agentos_tool_call_total - Counter */
    metrics_register(METRIC_TOOL_CALL,
                     "工具调用次数",
                     "calls",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /* 5. agentos_memory_operations_total - Counter */
    metrics_register(METRIC_MEMORY_OPERATIONS,
                     "记忆操作次数",
                     "operations",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /* 6. agentos_hook_execution_ms - Histogram */
    metrics_register(METRIC_HOOK_EXECUTION,
                     "Hook 执行延迟（毫秒）",
                     "ms",
                     METRIC_TYPE_HISTOGRAM,
                     k_default_latency_buckets,
                     sizeof(k_default_latency_buckets) / sizeof(k_default_latency_buckets[0]));

    /* 7. agentos_connection_health - Gauge */
    metrics_register(METRIC_CONNECTION_HEALTH,
                     "连接线健康状态（1=健康, 0=不健康）",
                     "boolean",
                     METRIC_TYPE_GAUGE,
                     NULL, 0);

    /* 8. agentos_plugin_lifecycle_total - Counter */
    metrics_register(METRIC_PLUGIN_LIFECYCLE,
                     "Plugin 生命周期事件计数",
                     "events",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /* 9. agentos_llm_retry_total - Counter */
    metrics_register(METRIC_LLM_RETRY,
                     "LLM 重试次数",
                     "retries",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /* 10. agentos_config_reload_total - Counter */
    metrics_register(METRIC_CONFIG_RELOAD,
                     "配置重载次数",
                     "reloads",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    /*
     * 4 项内存可观测性指标 (工程标准规范手册 16.4)
     */

    /* 11. agentos_memory_rss_bytes - Gauge */
    metrics_register(METRIC_MEMORY_RSS,
                     "各 daemon 常驻内存（字节）",
                     "bytes",
                     METRIC_TYPE_GAUGE,
                     NULL, 0);

    /* 12. agentos_memory_heap_bytes - Gauge */
    metrics_register(METRIC_MEMORY_HEAP,
                     "堆内存使用量（字节）",
                     "bytes",
                     METRIC_TYPE_GAUGE,
                     NULL, 0);

    /* 13. agentos_memory_pool_utilization - Gauge */
    metrics_register(METRIC_MEMORY_POOL_UTIL,
                     "内存池使用率（0~1）",
                     "ratio",
                     METRIC_TYPE_GAUGE,
                     NULL, 0);

    /* 14. agentos_oom_events_total - Counter */
    metrics_register(METRIC_OOM_EVENTS,
                     "OOM 事件计数",
                     "events",
                     METRIC_TYPE_COUNTER,
                     NULL, 0);

    size_t count = metrics_get_count();
    SVC_LOG_INFO("C-L10: Registered %zu metrics (14 required)", count);

    return (count >= 14) ? 0 : -1;
}

/* ==================== HTTP 端点处理 ==================== */

int prometheus_exporter_handle_http(const char *request, size_t request_len,
                                    char **response, size_t *response_len)
{
    if (!request || !response || !response_len) {
        return -1;
    }

    /*
     * 检测 HTTP GET /metrics 请求
     * Prometheus scrape 发送:
     *   GET /metrics HTTP/1.1\r\n
     *   Host: localhost:9090\r\n
     *   ...
     */
    if (request_len < 14 ||
        strncmp(request, "GET /metrics", 12) != 0 ||
        (request[12] != ' ' && request[12] != '?')) {
        return -1; /* 非 /metrics 请求 */
    }

    SVC_LOG_DEBUG("C-L10: Handling Prometheus scrape request");

    /* 导出 Prometheus 格式指标 */
    char *metrics_text = prometheus_exporter_get_metrics();
    if (!metrics_text) {
        SVC_LOG_ERROR("C-L10