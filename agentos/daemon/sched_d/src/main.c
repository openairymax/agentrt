/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief 调度服务守护进程主入口（遵循 daemon 模块统一规范）
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性(成对管理)
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性(platform.h)
 * - ARCHITECTURAL_PRINCIPLES.md E-5 命名语义化(SVC_LOG_*)
 * - ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯(AGENTOS_ERR_*)
 */

#include "scheduler_service.h"
#include "strategy_interface.h"
#include "../../monit_d/include/monitor_service.h"

#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include "jsonrpc_helpers.h"
#include "method_dispatcher.h"
#include "param_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <cjson/cJSON.h>

/* ==================== 前向声明 ==================== */

static void handle_register_agent(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_schedule_task(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_get_stats(int id, agentos_socket_t client_fd);
static void handle_health_check(int id, agentos_socket_t client_fd);
static void signal_handler(int signum);

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/sched.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_sched"
#define DEFAULT_TCP_PORT 8083
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static sched_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;
static method_dispatcher_t* g_dispatcher = NULL;  /* 方法分发器 */

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


/**
 * @brief 处理 register_agent 方法
 * @param params 参数对象
 * @param id 请求 ID
 * @param client_fd 客户端描述符
 */
static void on_register_agent_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_register_agent(params, id, *(agentos_socket_t*)user_data);
}

/**
 * @brief 处理 schedule_task 方法
 */
static void on_schedule_task_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_schedule_task(params, id, *(agentos_socket_t*)user_data);
}

/**
 * @brief 处理 get_stats 方法
 */
static void on_get_stats_method(cJSON* params, int id, void* user_data) {
    (void)params;
    handle_get_stats(id, *(agentos_socket_t*)user_data);
}

/**
 * @brief 处理 health_check 方法
 */
static void on_health_check_method(cJSON* params, int id, void* user_data) {
    (void)params;
    handle_health_check(id, *(agentos_socket_t*)user_data);
}

/**
 * @brief 注册所有 JSON-RPC 方法
 */
static int register_rpc_methods(void) {
    g_dispatcher = method_dispatcher_create(16);
    if (!g_dispatcher) {
        SVC_LOG_ERROR("Failed to create method dispatcher");
        return -1;
    }
    
    method_dispatcher_register(g_dispatcher, "register_agent", on_register_agent_method, NULL);
    method_dispatcher_register(g_dispatcher, "schedule_task", on_schedule_task_method, NULL);
    method_dispatcher_register(g_dispatcher, "get_stats", on_get_stats_method, NULL);
    method_dispatcher_register(g_dispatcher, "health_check", on_health_check_method, NULL);
    
    SVC_LOG_INFO("Registered %d RPC methods", 4);
    return 0;
}

static void handle_register_agent(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* agent_json = jsonrpc_get_object_param(params, "agent");
    if (!agent_json) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing agent object", id);
        return;
    }

    agent_info_t info = {0};
    const char* aid = get_string_field(agent_json, "agent_id", NULL);
    if (!aid) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing agent_id", id);
        return;
    }

    strncpy(info.agent_id, aid, sizeof(info.agent_id) - 1);
    const char* aname = get_string_field(agent_json, "agent_name", NULL);
    if (aname)
        strncpy(info.agent_name, aname, sizeof(info.agent_name) - 1);

    info.load_factor = get_double_field(agent_json, "load_factor", 0.0);
    info.success_rate = get_double_field(agent_json, "success_rate", 0.0);
    info.avg_response_time_ms = get_int_field(agent_json, "avg_response_time_ms", 0);
    info.is_available = get_bool_field(agent_json, "is_available", false);
    info.weight = get_double_field(agent_json, "weight", 1.0);

    int ret = sched_service_register_agent(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Register failed", id);
        SVC_LOG_ERROR("Failed to register agent: %s (error=%d)", info.agent_id, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "agent_id", info.agent_id);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
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
    cJSON* task_json = jsonrpc_get_object_param(params, "task");
    if (!task_json) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing task object", id);
        return;
    }

    task_info_t task = {0};
    const char* tid = get_string_field(task_json, "task_id", NULL);
    if (!tid) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing task_id", id);
        return;
    }

    strncpy(task.task_id, tid, sizeof(task.task_id) - 1);

    const char* desc = get_string_field(task_json, "task_description", NULL);
    if (desc)
        strncpy(task.task_description, desc, sizeof(task.task_description) - 1);

    task.priority = get_int_field(task_json, "priority", 0);
    task.timeout_ms = get_int_field(task_json, "timeout_ms", 30000);

    sched_result_t* result = NULL;
    int ret = sched_service_schedule_task(g_service, &task, &result);

    if (ret != AGENTOS_SUCCESS || !result) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Schedule failed", id);
        SVC_LOG_ERROR("Task scheduling failed: %s (error=%d)", task.task_id, ret);
        return;
    }

    cJSON* res_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(res_obj, "selected_agent_id", result->selected_agent_id);
    cJSON_AddNumberToObject(res_obj, "confidence", result->confidence);
    cJSON_AddNumberToObject(res_obj, "estimated_time_ms", result->estimated_time_ms);

    JSONRPC_SEND_SUCCESS(client_fd, res_obj, id);
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
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Get stats failed", id);
        return;
    }

    cJSON* report_json = cJSON_Parse((char*)stats_data);
    free(stats_data);

    if (!report_json) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Invalid report data", id);
        return;
    }

    JSONRPC_SEND_SUCCESS(client_fd, report_json, id);
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

    JSONRPC_SEND_SUCCESS(client_fd, result, id);
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

    if ((size_t)n >= sizeof(buffer) - 1) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_REQUEST, "Request too large", -1);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* req = cJSON_Parse(buffer);
    if (!req) {
        JSONRPC_SEND_ERROR(client_fd, PARSE_ERROR, "Parse error: invalid JSON", -1);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(req, "method");
    cJSON* params = cJSON_GetObjectItem(req, "params");
    cJSON* id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_REQUEST, "Invalid Request", -1);
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

static void signal_handler(int signum) {
    (void)signum;
    g_running = 0;
    SVC_LOG_INFO("Received shutdown signal");
}

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
    
    /* 注册 RPC 方法 */
    if (register_rpc_methods() != 0) {
        SVC_LOG_ERROR("Failed to register RPC methods");
        sched_service_destroy(g_service);
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
    if (g_dispatcher) method_dispatcher_destroy(g_dispatcher);
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Scheduler service stopped");
    return 0;
}
