/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief 市场服务守护进程主入口（遵循 daemon 模块统一规范）
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性(成对管理)
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性(platform.h)
 * - ARCHITECTURAL_PRINCIPLES.md E-5 命名语义化(SVC_LOG_*)
 * - ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯(AGENTOS_ERR_*)
 */

#include "market_service.h"
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
static void handle_search_agents(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_install_agent(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_register_skill(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_search_skills(cJSON* params, int id, agentos_socket_t client_fd);
static void handle_health_check(int id, agentos_socket_t client_fd);
static void signal_handler(int signum);

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/market.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_market"
#define DEFAULT_TCP_PORT 8082
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static market_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;
static method_dispatcher_t* g_dispatcher = NULL;  /* 方法分发器 */

/* ==================== 错误码定义 ==================== */
#define MARKET_ERR_INVALID_PARAM AGENTOS_ERR_INVALID_PARAM
#define MARKET_ERR_OUT_OF_MEMORY AGENTOS_ERR_OUT_OF_MEMORY
#define MARKET_ERR_NOT_FOUND     AGENTOS_ERR_NOT_FOUND
#define MARKET_ERR_ALREADY_EXISTS (AGENTOS_ERR_DAEMON_BASE + 0x20)
#define MARKET_ERR_INSTALL_FAIL   (AGENTOS_ERR_DAEMON_BASE + 0x21)

/* ==================== JSON-RPC 错误码 ==================== */

#define PARSE_ERROR     -32700
#define INVALID_REQUEST -32600
#define METHOD_NOT_FOUND -32601
#define INVALID_PARAMS  -32602
#define INTERNAL_ERROR  -32000


/**
 * @brief 方法处理器包装函数
 */

static void on_register_agent_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_register_agent(params, id, *(agentos_socket_t*)user_data);
}

static void on_search_agents_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_search_agents(params, id, *(agentos_socket_t*)user_data);
}

static void on_install_agent_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_install_agent(params, id, *(agentos_socket_t*)user_data);
}

static void on_register_skill_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_register_skill(params, id, *(agentos_socket_t*)user_data);
}

static void on_search_skills_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_search_skills(params, id, *(agentos_socket_t*)user_data);
}

static void on_health_check_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    handle_health_check(id, *(agentos_socket_t*)user_data);
}

static int register_rpc_methods(void) {
    g_dispatcher = method_dispatcher_create(16);
    if (!g_dispatcher) {
        SVC_LOG_ERROR("Failed to create method dispatcher");
        return -1;
    }
    
    method_dispatcher_register(g_dispatcher, "register_agent", on_register_agent_method, NULL);
    method_dispatcher_register(g_dispatcher, "search_agents", on_search_agents_method, NULL);
    method_dispatcher_register(g_dispatcher, "install_agent", on_install_agent_method, NULL);
    method_dispatcher_register(g_dispatcher, "register_skill", on_register_skill_method, NULL);
    method_dispatcher_register(g_dispatcher, "search_skills", on_search_skills_method, NULL);
    method_dispatcher_register(g_dispatcher, "health_check", on_health_check_method, NULL);
    
    SVC_LOG_INFO("Registered %d RPC methods", 6);
    return 0;
}

/**
 * @brief 处理 register_agent 方法
 */
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
    info.agent_id = aid;

    info.name = get_string_field(agent_json, "name", NULL);
    info.version = get_string_field(agent_json, "version", NULL);
    info.description = get_string_field(agent_json, "description", NULL);
    info.author = get_string_field(agent_json, "author", NULL);

    int ret = market_service_register_agent(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Register failed", id);
        SVC_LOG_ERROR("Failed to register agent: %s (error=%d)", aid, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "agent_id", aid);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
        SVC_LOG_INFO("Agent registered: %s v%s", aid,
                    info.version ? info.version : "unknown");
    }
}

/**
 * @brief 处理 search_agents 方法
 */
static void handle_search_agents(cJSON* params, int id, agentos_socket_t client_fd) {
    const char* keyword = get_string_field(params, "keyword", "");
    size_t offset = (size_t)get_double_field(params, "offset", 0.0);
    size_t limit = (size_t)get_double_field(params, "limit", 20.0);

    agent_info_t** agents = NULL;
    size_t count = 0;
    int ret = market_service_search_agents(g_service, keyword, offset, limit, &agents, &count);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Search failed", id);
        return;
    }

    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && agents && agents[i]; i++) {
        cJSON* a = cJSON_CreateObject();
        if (agents[i]->agent_id) cJSON_AddStringToObject(a, "agent_id", agents[i]->agent_id);
        if (agents[i]->name) cJSON_AddStringToObject(a, "name", agents[i]->name);
        if (agents[i]->version) cJSON_AddStringToObject(a, "version", agents[i]->version);
        if (agents[i]->description) cJSON_AddStringToObject(a, "description", agents[i]->description);
        if (agents[i]->author) cJSON_AddStringToObject(a, "author", agents[i]->author);
        cJSON_AddBoolToObject(a, "installed", agents[i]->installed);
        cJSON_AddItemToArray(arr, a);
    }
    free(agents);

    JSONRPC_SEND_SUCCESS(client_fd, arr, id);
}

/**
 * @brief 处理 install_agent 方法
 */
static void handle_install_agent(cJSON* params, int id, agentos_socket_t client_fd) {
    const char* aid = get_string_field(params, "agent_id", NULL);
    if (!aid) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing agent_id", id);
        return;
    }

    const char* version = get_string_field(params, "version", "latest");

    int ret = market_service_install_agent(g_service, aid, version);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Install failed", id);
        SVC_LOG_ERROR("Failed to install agent: %s@%s (error=%d)", aid, version, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "installed");
        cJSON_AddStringToObject(result, "agent_id", aid);
        cJSON_AddStringToObject(result, "installed_version", version);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
        SVC_LOG_INFO("Agent installed: %s@%s", aid, version);
    }
}

/**
 * @brief 处理 register_skill 方法
 */
static void handle_register_skill(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* skill_json = jsonrpc_get_object_param(params, "skill");
    if (!skill_json) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing skill object", id);
        return;
    }

    skill_info_t info = {0};
    const char* sid = get_string_field(skill_json, "skill_id", NULL);
    if (!sid) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_PARAMS, "Missing skill_id", id);
        return;
    }
    info.skill_id = sid;

    info.name = get_string_field(skill_json, "name", NULL);
    info.version = get_string_field(skill_json, "version", NULL);

    int ret = market_service_register_skill(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Register failed", id);
        SVC_LOG_ERROR("Failed to register skill: %s (error=%d)", sid, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "skill_id", sid);
        JSONRPC_SEND_SUCCESS(client_fd, result, id);
        SVC_LOG_INFO("Skill registered: %s", sid);
    }
}

/**
 * @brief 处理 search_skills 方法
 */
static void handle_search_skills(cJSON* params, int id, agentos_socket_t client_fd) {
    const char* keyword = get_string_field(params, "keyword", "");

    skill_info_t** skills = NULL;
    size_t count = 0;
    int ret = market_service_search_skills(g_service, keyword, 0, 20, &skills, &count);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, INTERNAL_ERROR, "Search failed", id);
        return;
    }

    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < count && skills && skills[i]; i++) {
        cJSON* s = cJSON_CreateObject();
        if (skills[i]->skill_id) cJSON_AddStringToObject(s, "skill_id", skills[i]->skill_id);
        if (skills[i]->name) cJSON_AddStringToObject(s, "name", skills[i]->name);
        if (skills[i]->version) cJSON_AddStringToObject(s, "version", skills[i]->version);
        if (skills[i]->description) cJSON_AddStringToObject(s, "description", skills[i]->description);
        cJSON_AddItemToArray(arr, s);
    }
    free(skills);

    JSONRPC_SEND_SUCCESS(client_fd, arr, id);
}

/**
 * @brief 处理 health_check 方法
 */
static void handle_health_check(int id, agentos_socket_t client_fd) {
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "service", "market_d");
    cJSON_AddBoolToObject(result, "healthy", true);
    cJSON_AddNumberToObject(result, "timestamp", (double)(uint64_t)time(NULL) * 1000);

    JSONRPC_SEND_SUCCESS(client_fd, result, id);
}

/* ==================== 客户端连接处理 ==================== */

static void handle_client(agentos_socket_t client_fd) {
    char buffer[MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);

    if (n <= 0) { agentos_socket_close(client_fd); return; }
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

static void print_usage(const char* prog) {
    printf("AgentOS Market Daemon\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --manager <path>   Configuration file path\n");
    printf("  --tcp             Use TCP instead of Unix socket\n");
    printf("  --help             Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --manager /etc/agentos/market.yaml\n", prog);
    printf("  %s --tcp           # Use TCP mode on port 8082\n", prog);
}

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    const char* config_path = "agentos/manager/service/market_d/market.yaml";
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

    SVC_LOG_INFO("Market service starting, manager=%s", config_path);

    /* 创建配置 */
    market_config_t config = {
        .max_agents = 1000,
        .max_skills = 5000,
        .enable_auto_update = false,
        .repository_url = NULL,
        .cache_ttl_sec = 3600,
        .log_file_path = "market.log"
    };

    /* 创建市场服务 */
    int ret = market_service_create(&config, &g_service);
    if (ret != AGENTOS_SUCCESS || !g_service) {
        SVC_LOG_ERROR("Failed to create market service (error=%d)", ret);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }
    
    /* 注册 RPC 方法 */
    if (register_rpc_methods() != 0) {
        SVC_LOG_ERROR("Failed to register RPC methods");
        market_service_destroy(g_service);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }
    
    SVC_LOG_INFO("Market service created successfully");

    /* 创建服务器 Socket */
    agentos_socket_t server_fd;

    if (use_tcp) {
        server_fd = agentos_socket_create_tcp_server("127.0.0.1", DEFAULT_TCP_PORT);
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create TCP server on port %d", DEFAULT_TCP_PORT);
            market_service_destroy(g_service);
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
            market_service_destroy(g_service);
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on Unix socket");
    }

    SVC_LOG_INFO("Market service started successfully");

    /* 主事件循环 */
    while (g_running) {
        agentos_socket_t client_fd = agentos_socket_accept(server_fd, 5000);
        if (client_fd == AGENTOS_INVALID_SOCKET) continue;
        handle_client(client_fd);
    }

    /* 清理资源 */
    SVC_LOG_INFO("Market service stopping...");
    agentos_socket_close(server_fd);
    market_service_destroy(g_service);
    if (g_dispatcher) method_dispatcher_destroy(g_dispatcher);
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Market service stopped");
    return 0;
}
