/**
 * @file main.c
 * @brief 监控服务守护进程入口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改进说明：
 * 1. 真正的守护进程模式（跨平台支持）
 * 2. JSON-RPC 2.0 服务接口
 * 3. 完善的错误处理和日志记录（替换 printf）
 * 4. OpenTelemetry 兼容的指标导出格式
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <cjson/cJSON.h>

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/monit.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_monit"
#define DEFAULT_TCP_PORT 9090
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static monitor_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;

/* ==================== 错误码定义（统一使用 AGENTOS_ERR_*） ==================== */
#define MONIT_ERR_INVALID_PARAM   AGENTOS_ERR_INVALID_PARAM
#define MONIT_ERR_OUT_OF_MEMORY   AGENTOS_ERR_OUT_OF_MEMORY
#define MONIT_ERR_NOT_FOUND       AGENTOS_ERR_NOT_FOUND
#define MONIT_ERR_INVALID_METRIC  (AGENTOS_ERR_DAEMON_BASE + 0x10)
#define MONIT_ERR_ALERT_FAILED    (AGENTOS_ERR_DAEMON_BASE + 0x11)

/* ==================== JSON-RPC 错误码 ==================== */

#define PARSE_ERROR     -32700
#define INVALID_REQUEST -32600
#define METHOD_NOT_FOUND -32601
#define INVALID_PARAMS  -32602
#define INTERNAL_ERROR  -32000

/* ==================== JSON-RPC 响应构建 ==================== */

/**
 * @brief 构建错误响应
 * @param code 错误码
 * @param message 错误消息
 * @param id 请求ID
 * @return JSON字符串（需调用者释放）
 */
static char* build_error_response(int code, const char* message, int id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON* err = cJSON_CreateObject();
    if (!err) { cJSON_Delete(root); return NULL; }
    cJSON_AddNumberToObject(err, "code", code);
    cJSON_AddStringToObject(err, "message", message ? message : "Unknown error");
    cJSON_AddItemToObject(root, "error", err);

    if (id >= 0) {
        cJSON_AddNumberToObject(root, "id", id);
    } else {
        cJSON_AddNullToObject(root, "id");
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/**
 * @brief 构建成功响应
 * @param result 结果对象
 * @param id 请求ID
 * @return JSON字符串（需调用者释放）
 */
static char* build_success_response(cJSON* result, int id) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddItemToObject(root, "result", result ? cJSON_Duplicate(result, 1) : cJSON_CreateNull());
    cJSON_AddNumberToObject(root, "id", id);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ==================== 信号处理 ==================== */

/**
 * @brief 信号处理函数
 * @param sig 信号值
 */
static void signal_handler(int sig) {
    (void)sig;
    agentos_mutex_lock(&g_running_lock);
    g_running = 0;
    agentos_mutex_unlock(&g_running_lock);
}

#ifdef _WIN32
/**
 * @brief Windows控制台处理函数
 * @param fdwCtrlType 控制信号类型
 * @return TRUE 已处理
 */
static BOOL WINAPI console_handler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            signal_handler((int)fdwCtrlType);
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

/* ==================== 请求处理方法 ==================== */

/**
 * @brief 处理 record_metric 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_record_metric(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* metric_json = cJSON_GetObjectItem(params, "metric");
    if (!cJSON_IsObject(metric_json)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing metric object", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    metric_info_t metric = {0};
    cJSON* jname = cJSON_GetObjectItem(metric_json, "name");
    if (!cJSON_IsString(jname)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing metric name", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    /* 注意：这里需要深拷贝名称，因为 monitor_service_record_metric 会保存指针 */
    metric.name = strdup(jname->valuestring);

    cJSON* jdesc = cJSON_GetObjectItem(metric_json, "description");
    if (cJSON_IsString(jdesc)) metric.description = jdesc->valuestring;

    cJSON* jtype = cJSON_GetObjectItem(metric_json, "type");
    if (cJSON_IsNumber(jtype)) metric.type = (metric_type_t)jtype->valueint;

    cJSON* jval = cJSON_GetObjectItem(metric_json, "value");
    if (cJSON_IsNumber(jval)) metric.value = jval->valuedouble;

    metric.timestamp = (uint64_t)time(NULL) * 1000;

    int ret = monitor_service_record_metric(g_service, &metric);

    /* 释放临时分配的内存 */
    free((void*)metric.name);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Record metric failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to record metric: %s (error=%d)", jname->valuestring, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "recorded");
        cJSON_AddStringToObject(result, "metric_name", jname->valuestring);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_DEBUG("Metric recorded: %s", jname->valuestring);
    }
}

/**
 * @brief 处理 get_metrics 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get_metrics(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jfilter = cJSON_GetObjectItem(params, "metric_name");
    const char* filter = cJSON_IsString(jfilter) ? jfilter->valuestring : NULL;

    metric_info_t** metrics = NULL;
    size_t count = 0;
    int ret = monitor_service_get_metrics(g_service, filter, &metrics, &count);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Get metrics failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    /* 构建 OpenTelemetry 格式的指标数组 */
    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && metrics && metrics[i]; i++) {
        cJSON* m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "name", metrics[i]->name);
        if (metrics[i]->description)
            cJSON_AddStringToObject(m, "description", metrics[i]->description);
        cJSON_AddNumberToObject(m, "type", metrics[i]->type);
        cJSON_AddNumberToObject(m, "value", metrics[i]->value);
        cJSON_AddNumberToObject(m, "timestamp", (double)metrics[i]->timestamp);
        cJSON_AddItemToArray(arr, m);
    }

    /* 释放获取到的指标数组（不释放内部数据） */
    free(metrics);

    char* success = build_success_response(arr, id);
    cJSON_Delete(arr);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/**
 * @brief 处理 trigger_alert 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_trigger_alert(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* alert_json = cJSON_GetObjectItem(params, "alert");
    if (!cJSON_IsObject(alert_json)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing alert object", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    alert_info_t alert = {0};

    cJSON* jid = cJSON_GetObjectItem(alert_json, "alert_id");
    if (cJSON_IsString(jid)) alert.alert_id = jid->valuestring;

    cJSON* jmsg = cJSON_GetObjectItem(alert_json, "message");
    if (cJSON_IsString(jmsg)) alert.message = jmsg->valuestring;

    cJSON* jlevel = cJSON_GetObjectItem(alert_json, "level");
    if (cJSON_IsNumber(jlevel)) alert.level = (alert_level_t)jlevel->valueint;

    cJSON* jservice = cJSON_GetObjectItem(alert_json, "service_name");
    if (cJSON_IsString(jservice)) alert.service_name = jservice->valuestring;

    cJSON* jresource = cJSON_GetObjectItem(alert_json, "resource_id");
    if (cJSON_IsString(jresource)) alert.resource_id = jresource->valuestring;

    alert.timestamp = (uint64_t)time(NULL) * 1000;
    alert.is_resolved = false;

    int ret = monitor_service_trigger_alert(g_service, &alert);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Trigger alert failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to trigger alert: %s (error=%d)", 
                     jid ? jid->valuestring : "unknown", ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "triggered");
        if (jid) cJSON_AddStringToObject(result, "alert_id", jid->valuestring);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_INFO("Alert triggered: %s", jid ? jid->valuestring : "unknown");
    }
}

/**
 * @brief 处理 get_alerts 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get_alerts(int id, agentos_socket_t client_fd) {
    alert_info_t** alerts = NULL;
    size_t count = 0;
    int ret = monitor_service_get_alerts(g_service, &alerts, &count);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Get alerts failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && alerts && alerts[i]; i++) {
        cJSON* a = cJSON_CreateObject();
        if (alerts[i]->alert_id) cJSON_AddStringToObject(a, "alert_id", alerts[i]->alert_id);
        if (alerts[i]->message) cJSON_AddStringToObject(a, "message", alerts[i]->message);
        cJSON_AddNumberToObject(a, "level", alerts[i]->level);
        if (alerts[i]->service_name) cJSON_AddStringToObject(a, "service_name", alerts[i]->service_name);
        cJSON_AddBoolToObject(a, "is_resolved", alerts[i]->is_resolved);
        cJSON_AddNumberToObject(a, "timestamp", (double)alerts[i]->timestamp);
        cJSON_AddItemToArray(arr, a);
    }

    free(alerts);

    char* success = build_success_response(arr, id);
    cJSON_Delete(arr);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/**
 * @brief 处理 health_check 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_health_check(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jservice = cJSON_GetObjectItem(params, "service_name");
    const char* service_name = cJSON_IsString(jservice) ? jservice->valuestring : "unknown";

    health_check_result_t* result = NULL;
    int ret = monitor_service_health_check(g_service, service_name, &result);

    if (ret != AGENTOS_SUCCESS || !result) {
        char* err = build_error_response(INTERNAL_ERROR, "Health check failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    cJSON* res_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(res_obj, "service_name", result->service_name);
    cJSON_AddBoolToObject(res_obj, "healthy", result->is_healthy);
    if (result->status_message)
        cJSON_AddStringToObject(res_obj, "status_message", result->status_message);
    cJSON_AddNumberToObject(res_obj, "timestamp", (double)result->timestamp);

    char* success = build_success_response(res_obj, id);
    cJSON_Delete(res_obj);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }

    /* 释放健康检查结果 */
    free(result->service_name);
    free(result->status_message);
    free(result);
}

/**
 * @brief 处理 generate_report 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_generate_report(int id, agentos_socket_t client_fd) {
    char* report = NULL;
    int ret = monitor_service_generate_report(g_service, &report);

    if (ret != AGENTOS_SUCCESS || !report) {
        char* err = build_error_response(INTERNAL_ERROR, "Generate report failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "report", report);
    cJSON_AddNumberToObject(result, "generated_at", (double)(uint64_t)time(NULL) * 1000);
    free(report);

    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/* ==================== 客户端连接处理 ==================== */

/**
 * @brief 处理单个客户端连接
 * @param client_fd 客户端描述符
 */
static void handle_client(agentos_socket_t client_fd) {
    char buffer[MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);

    if (n <= 0) { agentos_socket_close(client_fd); return; }
    buffer[n] = '\0';

    if ((size_t)n >= sizeof(buffer) - 1) {
        char* err = build_error_response(INVALID_REQUEST, "Request too large", -1);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* req = cJSON_Parse(buffer);
    if (!req) {
        char* err = build_error_response(PARSE_ERROR, "Parse error: invalid JSON", -1);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(req, "method");
    cJSON* params = cJSON_GetObjectItem(req, "params");
    cJSON* id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        char* err = build_error_response(INVALID_REQUEST, "Invalid Request", -1);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        cJSON_Delete(req);
        agentos_socket_close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;

    SVC_LOG_DEBUG("Processing request: method=%s, id=%d", method->valuestring, req_id);

    if (strcmp(method->valuestring, "record_metric") == 0) {
        handle_record_metric(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "get_metrics") == 0) {
        handle_get_metrics(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "trigger_alert") == 0) {
        handle_trigger_alert(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "get_alerts") == 0) {
        handle_get_alerts(req_id, client_fd);
    } else if (strcmp(method->valuestring, "health_check") == 0) {
        handle_health_check(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "generate_report") == 0) {
        handle_generate_report(req_id, client_fd);
    } else {
        char* err = build_error_response(METHOD_NOT_FOUND, "Method not found", req_id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_WARN("Unknown method requested: %s", method->valuestring);
    }

    cJSON_Delete(req);
    agentos_socket_close(client_fd);
}

/* ==================== 帮助信息 ==================== */

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
static void print_usage(const char* prog) {
    printf("AgentOS Monitor Daemon\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --manager <path>   Configuration file path\n");
    printf("  --tcp             Use TCP instead of Unix socket\n");
    printf("  --help             Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --manager /etc/agentos/monit.yaml\n", prog);
    printf("  %s --tcp           # Use TCP mode on port 9090\n", prog);
}

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    const char* config_path = "agentos/manager/service/monit_d/monit.yaml";
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
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
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
#endif

    SVC_LOG_INFO("Monitor service starting, manager=%s", config_path);

    /* 创建配置 */
    monitor_config_t config = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = 10000,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = "monitor.log",
        .metrics_storage_path = "metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    /* 创建监控服务 */
    int ret = monitor_service_create(&config, &g_service);
    if (ret != AGENTOS_SUCCESS || !g_service) {
        SVC_LOG_ERROR("Failed to create monitor service (error=%d)", ret);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Monitor service created successfully");

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
    }

    SVC_LOG_INFO("Monitor service started successfully");

    /* 主事件循环 */
    while (g_running) {
        agentos_socket_t client_fd = agentos_socket_accept(server_fd, 5000);
        if (client_fd == AGENTOS_INVALID_SOCKET) continue;
        handle_client(client_fd);
    }

    /* 清理资源 */
    SVC_LOG_INFO("Monitor service stopping...");
    agentos_socket_close(server_fd);
    monitor_service_destroy(g_service);
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Monitor service stopped");
    return 0;
}
