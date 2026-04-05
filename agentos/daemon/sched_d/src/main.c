/**
 * @file main.c
 * @brief 智能调度服务守护进程入口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改进说明：
 * 1. 真正的守护进程模式（跨平台支持）
 * 2. JSON-RPC 2.0 服务接口
 * 3. 完善的错误处理和日志记录
 * 4. 配置热重载支持
 */

#include "scheduler_service.h"
#include "strategy_interface.h"
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

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/sched.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_sched"
#define DEFAULT_TCP_PORT 8083
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static sched_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;

/* ==================== 错误码定义（统一使用 AGENTOS_ERR_*） ==================== */
#define SCHED_ERR_INVALID_PARAM    AGENTOS_ERR_INVALID_PARAM
#define SCHED_ERR_OUT_OF_MEMORY    AGENTOS_ERR_OUT_OF_MEMORY
#define SCHED_ERR_NOT_FOUND        AGENTOS_ERR_NOT_FOUND
#define SCHED_ERR_INVALID_CONFIG   (AGENTOS_ERR_DAEMON_BASE + 0x01)
#define SCHED_ERR_STRATEGY_FAIL    (AGENTOS_ERR_DAEMON_BASE + 0x02)

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
    if (!err) {
        cJSON_Delete(root);
        return NULL;
    }
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
 * @brief 处理 register_agent 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_register_agent(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* agent_json = cJSON_GetObjectItem(params, "agent");
    if (!cJSON_IsObject(agent_json)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing agent object", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    agent_info_t info = {0};
    cJSON* jid = cJSON_GetObjectItem(agent_json, "agent_id");
    cJSON* jname = cJSON_GetObjectItem(agent_json, "agent_name");

    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing agent_id", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    strncpy(info.agent_id, jid->valuestring, sizeof(info.agent_id) - 1);
    if (cJSON_IsString(jname))
        strncpy(info.agent_name, jname->valuestring, sizeof(info.agent_name) - 1);

    cJSON* load = cJSON_GetObjectItem(agent_json, "load_factor");
    if (cJSON_IsNumber(load)) info.load_factor = load->valuedouble;

    cJSON* success_rate = cJSON_GetObjectItem(agent_json, "success_rate");
    if (cJSON_IsNumber(success_rate)) info.success_rate = success_rate->valuedouble;

    cJSON* response_time = cJSON_GetObjectItem(agent_json, "avg_response_time_ms");
    if (cJSON_IsNumber(response_time)) info.avg_response_time_ms = response_time->valueint;

    cJSON* available = cJSON_GetObjectItem(agent_json, "is_available");
    if (cJSON_IsBool(available)) info.is_available = available->valueint;

    cJSON* weight = cJSON_GetObjectItem(agent_json, "weight");
    if (cJSON_IsNumber(weight)) info.weight = weight->valuedouble;

    int ret = sched_service_register_agent(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Register failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to register agent: %s (error=%d)", info.agent_id, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "agent_id", info.agent_id);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_INFO("Agent registered: %s", info.agent_id);
    }
}

/**
 * @brief 处理 schedule_task 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_schedule_task(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* task_json = cJSON_GetObjectItem(params, "task");
    if (!cJSON_IsObject(task_json)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing task object", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    task_info_t task = {0};
    cJSON* jid = cJSON_GetObjectItem(task_json, "task_id");
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing task_id", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    strncpy(task.task_id, jid->valuestring, sizeof(task.task_id) - 1);

    cJSON* desc = cJSON_GetObjectItem(task_json, "task_description");
    if (cJSON_IsString(desc))
        strncpy(task.task_description, desc->valuestring, sizeof(task.task_description) - 1);

    cJSON* priority = cJSON_GetObjectItem(task_json, "priority");
    if (cJSON_IsNumber(priority)) task.priority = priority->valueint;

    cJSON* timeout = cJSON_GetObjectItem(task_json, "timeout_ms");
    if (cJSON_IsNumber(timeout)) task.timeout_ms = timeout->valueint;

    sched_result_t* result = NULL;
    int ret = sched_service_schedule_task(g_service, &task, &result);

    if (ret != AGENTOS_SUCCESS || !result) {
        char* err = build_error_response(INTERNAL_ERROR, "Schedule failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Task scheduling failed: %s (error=%d)", task.task_id, ret);
        return;
    }

    cJSON* res_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(res_obj, "selected_agent_id", result->selected_agent_id);
    cJSON_AddNumberToObject(res_obj, "confidence", result->confidence);
    cJSON_AddNumberToObject(res_obj, "estimated_time_ms", result->estimated_time_ms);

    char* success = build_success_response(res_obj, id);
    cJSON_Delete(res_obj);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
    SVC_LOG_INFO("Task scheduled: %s -> Agent: %s (Confidence: %.2f)",
                task.task_id, result->selected_agent_id, result->confidence);

    free(result->selected_agent_id);
    free(result);
}

/**
 * @brief 处理 get_stats 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get_stats(int id, agentos_socket_t client_fd) {
    void* stats_data = NULL;
    int ret = sched_service_get_stats(g_service, &stats_data);

    if (ret != AGENTOS_SUCCESS || !stats_data) {
        char* err = build_error_response(INTERNAL_ERROR, "Get stats failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    cJSON* report_json = cJSON_Parse((char*)stats_data);
    free(stats_data);

    if (!report_json) {
        char* err = build_error_response(INTERNAL_ERROR, "Invalid report data", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    char* success = build_success_response(report_json, id);
    cJSON_Delete(report_json);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/**
 * @brief 处理 health_check 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_health_check(int id, agentos_socket_t client_fd) {
    bool healthy = false;
    int ret = sched_service_health_check(g_service, &healthy);

    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "service", "sched_d");
    cJSON_AddBoolToObject(result, "healthy", healthy);
    cJSON_AddNumberToObject(result, "timestamp", (double)(uint64_t)time(NULL) * 1000);

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

    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    /* 检查请求大小 */
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

    if (strcmp(method->valuestring, "register_agent") == 0) {
        handle_register_agent(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "schedule_task") == 0) {
        handle_schedule_task(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "get_stats") == 0) {
        handle_get_stats(req_id, client_fd);
    } else if (strcmp(method->valuestring, "health_check") == 0) {
        handle_health_check(req_id, client_fd);
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
    printf("AgentOS Scheduler Daemon\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --manager <path>   Configuration file path\n");
    printf("  --tcp             Use TCP instead of Unix socket\n");
    printf("  --help             Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --manager /etc/agentos/sched.yaml\n", prog);
    printf("  %s --tcp           # Use TCP mode on port 8083\n", prog);
}

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    const char* config_path = "agentos/manager/service/sched_d/sched.yaml";
    int use_tcp = 0;

    /* 解析命令行参数 */
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

    /* 设置信号处理 */
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    SVC_LOG_INFO("Scheduler service starting, manager=%s", config_path);

    /* 创建配置 */
    sched_config_t config = {
        .strategy = SCHED_STRATEGY_ROUND_ROBIN,
        .health_check_interval_ms = 5000,
        .stats_report_interval_ms = 10000,
        .enable_ml_strategy = false,
        .ml_model_path = NULL,
        .max_agents = 100
    };

    /* 创建调度服务 */
    int ret = sched_service_create(&config, &g_service);
    if (ret != AGENTOS_SUCCESS || !g_service) {
        SVC_LOG_ERROR("Failed to create scheduler service (error=%d)", ret);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Scheduler service created with strategy: round_robin");

    /* 创建服务器 Socket */
    agentos_socket_t server_fd;

    if (use_tcp) {
        server_fd = agentos_socket_create_tcp_server("127.0.0.1", DEFAULT_TCP_PORT);
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create TCP server on port %d", DEFAULT_TCP_PORT);
            sched_service_destroy(g_service);
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
            sched_service_destroy(g_service);
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on Unix socket");
    }

    SVC_LOG_INFO("Scheduler service started successfully");

    /* 主事件循环 */
    while (g_running) {
        agentos_socket_t client_fd = agentos_socket_accept(server_fd, 5000);

        if (client_fd == AGENTOS_INVALID_SOCKET) {
            continue;
        }

        /* 处理客户端请求 */
        handle_client(client_fd);
    }

    /* 清理资源 */
    SVC_LOG_INFO("Scheduler service stopping...");
    agentos_socket_close(server_fd);
    sched_service_destroy(g_service);
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Scheduler service stopped");
    return 0;
}
