#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief 监控服务守护进程主入口（遵循 daemon 模块统一规范）
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性(成对管理)
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性(platform.h)
 * - ARCHITECTURAL_PRINCIPLES.md E-5 命名语义化(SVC_LOG_*)
 * - ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯(AGENTOS_ERR_*)
 */

#include "atomic_compat.h"
#include "daemon_bootstrap_sd.h"
#include "daemon_bootstrap_ipc.h"
#include "daemon_event_driver.h"
#include "jsonrpc_helpers.h"
#include "logging.h"
#include "method_dispatcher.h"
#include "monitor_service.h"
#include "param_validator.h"
#include "platform.h"
#include "prometheus_exporter.h"
#include "svc_logger.h"
#include "thread_pool.h"

#include <cjson/cJSON.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX AGENTOS_RUNTIME_DIR "/monit.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_monit"
#define DEFAULT_TCP_PORT 9090
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static monitor_service_t *g_service = NULL;
static atomic_int g_running = 1;
static agentos_mutex_t g_running_lock;
static method_dispatcher_t *g_dispatcher = NULL;
static daemon_event_driver_t *g_event_driver = NULL;
static daemon_bootstrap_sd_t *g_bsd = NULL;
static daemon_bootstrap_ipc_t *g_bipc = NULL;

/* ==================== 信号处理 ==================== */

static void signal_handler(int sig __attribute__((unused)))
{
    agentos_mutex_lock(&g_running_lock);
    atomic_store_explicit(&g_running, 0, memory_order_seq_cst);
    agentos_mutex_unlock(&g_running_lock);
    if (g_event_driver)
        daemon_event_driver_stop(g_event_driver);
}

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD ctrl_type __attribute__((unused)))
{
    signal_handler(SIGINT);
    return TRUE;
}
#endif

static void svc_log_toggle_handler(int sig)
{
    (void)sig;
    static int debug_mode = 0;
    debug_mode = !debug_mode;
    log_set_module_level("*", debug_mode ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
}

/* ==================== 错误码定义（统一使用 AGENTOS_ERR_*） ==================== */
#define MONIT_ERR_INVALID_PARAM AGENTOS_ERR_INVALID_PARAM
#define MONIT_ERR_OUT_OF_MEMORY AGENTOS_ERR_OUT_OF_MEMORY
#define MONIT_ERR_NOT_FOUND AGENTOS_ERR_NOT_FOUND
#define MONIT_ERR_INVALID_METRIC (AGENTOS_ERR_DAEMON_BASE + 0x10)
#define MONIT_ERR_ALERT_FAILED (AGENTOS_ERR_DAEMON_BASE + 0x11)

/* ==================== JSON-RPC 错误码 ==================== */


/* ==================== 方法处理器包装函数 ==================== */

/* 前向声明 */
static void handle_record_metric(cJSON *params, int id, agentos_socket_t client_fd);
static void handle_get_metrics(cJSON *params, int id, agentos_socket_t client_fd);
static void handle_trigger_alert(cJSON *params, int id, agentos_socket_t client_fd);
static void handle_get_alerts(int id, agentos_socket_t client_fd);
static void handle_health_check(cJSON *params, int id, agentos_socket_t client_fd);
static void handle_generate_report(int id, agentos_socket_t client_fd);

/**
 * @brief 方法处理器包装函数
 */
static void on_record_metric_method(cJSON *params, int id, void *user_data)
{
    handle_record_metric(params, id, *(agentos_socket_t *)user_data);
}

static void on_get_metrics_method(cJSON *params, int id, void *user_data)
{
    handle_get_metrics(params, id, *(agentos_socket_t *)user_data);
}

static void on_trigger_alert_method(cJSON *params, int id, void *user_data)
{
    handle_trigger_alert(params, id, *(agentos_socket_t *)user_data);
}

static void on_get_alerts_method(cJSON *params, int id, void *user_data)
{
    handle_get_alerts(id, *(agentos_socket_t *)user_data);
}

static void on_health_check_method(cJSON *params, int id, void *user_data)
{
    handle_health_check(params, id, *(agentos_socket_t *)user_data);
}

static void on_generate_report_method(cJSON *params, int id, void *user_data)
{
    handle_generate_report(id, *(agentos_socket_t *)user_data);
}

static void handle_client(agentos_socket_t client_fd);

static int monit_on_client(void *service_ctx, agentos_socket_t client_fd)
{
    (void)service_ctx;
    handle_client(client_fd);
    return 0;
}

/* ==================== 请求处理方法 ==================== */

/**
 * @brief 处理 record_metric 方法
 * @param params 参数对象
 * @param id 请求 ID
 * @param client_fd 客户端描述符
 */
static void handle_record_metric(cJSON *params, int id, agentos_socket_t client_fd)
{
    cJSON *metric_json = jsonrpc_get_object_param(params, "metric");
    if (!metric_json) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS, "Missing metric object", id);
        return;
    }

    metric_info_t metric = {0};
    const char *mname = get_string_field(metric_json, "name", NULL);
    if (!mname) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS, "Missing metric name", id);
        return;
    }

    metric.name = AGENTOS_STRDUP(mname);
    metric.description = (char *)get_string_field(metric_json, "description", NULL);
    metric.type = (metric_type_t)get_int_field(metric_json, "type", 0);
    metric.value = get_double_field(metric_json, "value", 0.0);

    metric.timestamp = (uint64_t)time(NULL) * 1000;

    int ret = monitor_service_record_metric(g_service, &metric);

    AGENTOS_FREE((void *)metric.name);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Record metric failed", id);
        SVC_LOG_ERROR("Failed to record metric: %s (error=%d)", mname, ret);
    } else {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "recorded");
        cJSON_AddStringToObject(result, "metric_name", mname);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
        SVC_LOG_DEBUG("Metric recorded: %s", mname);
    }
}

/**
 * @brief 处理 get_metrics 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get_metrics(cJSON *params, int id, agentos_socket_t client_fd)
{
    const char *filter = get_string_field(params, "metric_name", NULL);

    metric_info_t **metrics = NULL;
    size_t count = 0;
    int ret = monitor_service_get_metrics(g_service, filter, &metrics, &count);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Get metrics failed", id);
        return;
    }

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && metrics && metrics[i]; i++) {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "name", metrics[i]->name);
        if (metrics[i]->description)
            cJSON_AddStringToObject(m, "description", metrics[i]->description);
        cJSON_AddNumberToObject(m, "type", metrics[i]->type);
        cJSON_AddNumberToObject(m, "value", metrics[i]->value);
        cJSON_AddNumberToObject(m, "timestamp", (double)metrics[i]->timestamp);
        cJSON_AddItemToArray(arr, m);
    }

    AGENTOS_FREE(metrics);

    JSONRPC_SEND_SUCCESS(client_fd, arr, id);
}

/**
 * @brief 处理 trigger_alert 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_trigger_alert(cJSON *params, int id, agentos_socket_t client_fd)
{
    cJSON *alert_json = jsonrpc_get_object_param(params, "alert");
    if (!alert_json) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS, "Missing alert object", id);
        return;
    }

    alert_info_t alert = {0};
    alert.alert_id = (char *)get_string_field(alert_json, "alert_id", NULL);
    alert.message = (char *)get_string_field(alert_json, "message", NULL);
    alert.level = (alert_level_t)get_int_field(alert_json, "level", 0);
    alert.service_name = (char *)get_string_field(alert_json, "service_name", NULL);
    alert.resource_id = (char *)get_string_field(alert_json, "resource_id", NULL);

    alert.timestamp = (uint64_t)time(NULL) * 1000;
    alert.is_resolved = false;

    int ret = monitor_service_trigger_alert(g_service, &alert);
    const char *alert_id = alert.alert_id ? alert.alert_id : "unknown";

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Trigger alert failed", id);
        SVC_LOG_ERROR("Failed to trigger alert: %s (error=%d)", alert_id, ret);
    } else {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "triggered");
        if (alert.alert_id)
            cJSON_AddStringToObject(result, "alert_id", alert.alert_id);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
        SVC_LOG_INFO("Alert triggered: %s", alert_id);
    }
}

/**
 * @brief 处理 get_alerts 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get_alerts(int id, agentos_socket_t client_fd)
{
    alert_info_t **alerts = NULL;
    size_t count = 0;
    int ret = monitor_service_get_alerts(g_service, &alerts, &count);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Get alerts failed", id);
        return;
    }

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && alerts && alerts[i]; i++) {
        cJSON *a = cJSON_CreateObject();
        if (alerts[i]->alert_id)
            cJSON_AddStringToObject(a, "alert_id", alerts[i]->alert_id);
        if (alerts[i]->message)
            cJSON_AddStringToObject(a, "message", alerts[i]->message);
        cJSON_AddNumberToObject(a, "level", alerts[i]->level);
        if (alerts[i]->service_name)
            cJSON_AddStringToObject(a, "service_name", alerts[i]->service_name);
        cJSON_AddBoolToObject(a, "is_resolved", alerts[i]->is_resolved);
        cJSON_AddNumberToObject(a, "timestamp", (double)alerts[i]->timestamp);
        cJSON_AddItemToArray(arr, a);
    }

    AGENTOS_FREE(alerts);

    JSONRPC_SEND_SUCCESS(client_fd, arr, id);
}

/**
 * @brief 处理 health_check 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_health_check(cJSON *params, int id, agentos_socket_t client_fd)
{
    const char *service_name = get_string_field(params, "service_name", "unknown");

    health_check_result_t *result = NULL;
    int ret = monitor_service_health_check(g_service, service_name, &result);

    if (ret != AGENTOS_SUCCESS || !result) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Health check failed", id);
        return;
    }

    cJSON *res_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(res_obj, "service_name", result->service_name);
    cJSON_AddBoolToObject(res_obj, "healthy", result->is_healthy);
    if (result->status_message)
        cJSON_AddStringToObject(res_obj, "status_message", result->status_message);
    cJSON_AddNumberToObject(res_obj, "timestamp", (double)result->timestamp);

    JSONRPC_SEND_SUCCESS(client_fd, res_obj, id);

    AGENTOS_FREE(result->service_name);
    AGENTOS_FREE(result->status_message);
    AGENTOS_FREE(result);
}

/**
 * @brief 处理 generate_report 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_generate_report(int id, agentos_socket_t client_fd)
{
    char *report = NULL;
    int ret = monitor_service_generate_report(g_service, &report);

    if (ret != AGENTOS_SUCCESS || !report) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Generate report failed", id);
        return;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "report", report);
    cJSON_AddNumberToObject(result, "generated_at", (double)(uint64_t)time(NULL) * 1000);
    AGENTOS_FREE(report);

    JSONRPC_SEND_SUCCESS(client_fd, result, id);
}

/* ==================== 客户端连接处理 ==================== */

/**
 * @brief 处理单个客户端连接
 * @param client_fd 客户端描述符
 */
static void handle_client(agentos_socket_t client_fd)
{
    char buffer[MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);

    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    if ((size_t)n >= sizeof(buffer) - 1) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "Request too large", -1);
        agentos_socket_close(client_fd);
        return;
    }

    /* C-L10: 检测 HTTP GET /metrics 请求，响应 Prometheus 格式指标 */
    char *http_response = NULL;
    size_t http_response_len = 0;
    if (prometheus_exporter_handle_http(buffer, (size_t)n, &http_response,
                                        &http_response_len) == 0) {
        agentos_socket_send(client_fd, http_response, http_response_len);
        AGENTOS_FREE(http_response);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON *req = cJSON_Parse(buffer);
    if (!req) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_PARSE_ERROR, "Parse error: invalid JSON", -1);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON *method = cJSON_GetObjectItem(req, "method");
    (void)cJSON_GetObjectItem(req, "params");
    cJSON *id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "Invalid Request", -1);
        cJSON_Delete(req);
        agentos_socket_close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;

    SVC_LOG_DEBUG("Processing request: method=%s, id=%d", method->valuestring, req_id);

    method_dispatcher_dispatch(g_dispatcher, req, jsonrpc_build_error, &client_fd);

    cJSON_Delete(req);
    agentos_socket_close(client_fd);
}

/* ==================== 帮助信息 ==================== */

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
static void print_usage(const char *prog)
{
    char buf[256];
    fputs("AgentOS Monitor Daemon\n", stdout);
    snprintf(buf, sizeof(buf), "Usage: %s [options]\n\n", prog);
    fputs(buf, stdout);
    fputs("Options:\n", stdout);
    fputs("  --manager <path>   Configuration file path\n", stdout);
    fputs("  --tcp             Use TCP instead of Unix socket\n", stdout);
    fputs("  --help             Show this help\n", stdout);
    fputs("\n", stdout);
    fputs("Examples:\n", stdout);
    snprintf(buf, sizeof(buf), "  %s --manager AGENTOS_CONFIG_DIR \"/monit.yaml\"\n", prog);
    fputs(buf, stdout);
    snprintf(buf, sizeof(buf), "  %s --tcp           # Use TCP mode on port 9090\n", prog);
    fputs(buf, stdout);
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv)
{
    const char *config_path = "agentos/manager/service/monit_d/monit.yaml";
    int use_tcp = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--manager") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            use_tcp = 1;
        } else {
            SVC_LOG_ERROR("Unknown option: %s", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 初始化平台层 */
    agentos_socket_init();
    agentos_mutex_init(&g_running_lock);

#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, svc_log_toggle_handler);
#endif

    agentos_log_init(NULL);
    atexit(log_cleanup);

    SVC_LOG_INFO("Monitor service starting, manager=%s", config_path);

    /* 创建配置 */
    monitor_config_t config = {.metrics_collection_interval_ms = 5000,
                               .health_check_interval_ms = 10000,
                               .log_flush_interval_ms = 30000,
                               .alert_check_interval_ms = 5000,
                               .log_file_path = "monitor.log",
                               .metrics_storage_path = "metrics",
                               .enable_tracing = true,
                               .enable_alerting = true};

    /* 创建监控服务 */
    int ret = monitor_service_create(&config, &g_service);
    if (ret != AGENTOS_SUCCESS || !g_service) {
        SVC_LOG_ERROR("Failed to create monitor service (error=%d)", ret);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Monitor service created successfully");

    /* C-L10: 初始化 Prometheus exporter 并注册 14 项必需指标 */
    if (prometheus_exporter_init("monit_d") == 0) {
        int metrics_ret = prometheus_exporter_register_required_metrics();
        if (metrics_ret != 0) {
            SVC_LOG_WARN("C-L10: Some required metrics failed to register (ret=%d)", metrics_ret);
        }
    } else {
        SVC_LOG_ERROR("C-L10: Failed to initialize Prometheus exporter");
    }

    /* 创建服务器 Socket */
    agentos_socket_t server_fd;

    if (use_tcp) {
        server_fd = agentos_socket_create_tcp_server("127.0.0.1", DEFAULT_TCP_PORT);
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create TCP server on port %d", DEFAULT_TCP_PORT);
            monitor_service_destroy(g_service);
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on TCP port %d", DEFAULT_TCP_PORT);
        g_bsd = daemon_bootstrap_sd_start("monit_d", "monitor", "127.0.0.1",
                                          DEFAULT_TCP_PORT, "monitor,core", 0);
        g_bipc = daemon_bootstrap_ipc_start("monit_d", "monitor", "127.0.0.1",
                                            DEFAULT_TCP_PORT, IPC_BUS_PROTO_JSON_RPC);
    } else {
#if defined(AGENTOS_PLATFORM_WINDOWS)
        server_fd = agentos_socket_create_named_pipe_server(DEFAULT_SOCKET_PATH_WIN);
#else
        server_fd = agentos_socket_create_unix_server(DEFAULT_SOCKET_PATH_UNIX);
#endif
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create socket at default path");
            monitor_service_destroy(g_service);
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on Unix socket");
        g_bsd = daemon_bootstrap_sd_start("monit_d", "monitor", DEFAULT_SOCKET_PATH_UNIX,
                                          0, "monitor,core", 0);
        g_bipc = daemon_bootstrap_ipc_start("monit_d", "monitor", DEFAULT_SOCKET_PATH_UNIX,
                                            0, IPC_BUS_PROTO_JSON_RPC);
    }

    SVC_LOG_INFO("Monitor service started successfully");

    daemon_event_config_t ev_config;
    __builtin_memset(&ev_config, 0, sizeof(ev_config));
    ev_config.max_events = 64;
    ev_config.thread_pool_min = 2;
    ev_config.thread_pool_max = 4;
    ev_config.thread_pool_queue_size = 128;
    ev_config.use_jsonrpc = true;
    ev_config.on_client = monit_on_client;
    ev_config.service_ctx = NULL;

    g_event_driver = daemon_event_driver_create(&ev_config);
    if (!g_event_driver) {
        SVC_LOG_ERROR("Failed to create event driver");
        agentos_socket_close(server_fd);
        monitor_service_destroy(g_service);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    g_dispatcher = daemon_event_driver_get_dispatcher(g_event_driver);
    method_dispatcher_register(g_dispatcher, "record_metric", on_record_metric_method, NULL);
    method_dispatcher_register(g_dispatcher, "get_metrics", on_get_metrics_method, NULL);
    method_dispatcher_register(g_dispatcher, "trigger_alert", on_trigger_alert_method, NULL);
    method_dispatcher_register(g_dispatcher, "get_alerts", on_get_alerts_method, NULL);
    method_dispatcher_register(g_dispatcher, "health_check", on_health_check_method, NULL);
    method_dispatcher_register(g_dispatcher, "generate_report", on_generate_report_method, NULL);
    SVC_LOG_INFO("Registered %d RPC methods", 6);

    if (daemon_event_driver_add_server_fd(g_event_driver, (int)server_fd) != 0) {
        SVC_LOG_ERROR("Failed to add server fd to event driver");
        daemon_event_driver_destroy(g_event_driver);
        agentos_socket_close(server_fd);
        monitor_service_destroy(g_service);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Monitor service running (event-driven mode)");
    daemon_event_driver_run(g_event_driver);

    /* 清理资源 */
    daemon_bootstrap_ipc_stop(g_bipc);
    daemon_bootstrap_sd_stop(g_bsd);
    prometheus_exporter_shutdown();
    SVC_LOG_INFO("Monitor service stopping...");
    daemon_event_driver_destroy(g_event_driver);
    agentos_socket_close(server_fd);
    monitor_service_destroy(g_service);
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Monitor service stopped");
    log_cleanup();
    return 0;
}
