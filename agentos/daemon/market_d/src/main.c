/**
 * @file main.c
 * @brief 市场服务守护进程入口（Agent/Skill 应用商店）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改进说明：
 * 1. 真正的守护进程模式（跨平台支持）
 * 2. JSON-RPC 2.0 服务接口
 * 3. 完善的错误处理和日志记录
 * 4. 统一错误码体系
 */

#include "market_service.h"
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

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/market.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_market"
#define DEFAULT_TCP_PORT 8082
#define MAX_BUFFER 65536

/* ==================== 全局状态 ==================== */

static market_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;

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

/* ==================== JSON-RPC 响应构建 ==================== */

/**
 * @brief 构建错误响应
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
    if (id >= 0) cJSON_AddNumberToObject(root, "id", id);
    else cJSON_AddNullToObject(root, "id");
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/**
 * @brief 构建成功响应
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

static void signal_handler(int sig) {
    (void)sig;
    agentos_mutex_lock(&g_running_lock);
    g_running = 0;
    agentos_mutex_unlock(&g_running_lock);
}

#ifdef _WIN32
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
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing agent_id", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }
    info.agent_id = jid->valuestring;

    cJSON* jname = cJSON_GetObjectItem(agent_json, "name");
    if (cJSON_IsString(jname)) info.name = jname->valuestring;

    cJSON* jver = cJSON_GetObjectItem(agent_json, "version");
    if (cJSON_IsString(jver)) info.version = jver->valuestring;

    cJSON* jdesc = cJSON_GetObjectItem(agent_json, "description");
    if (cJSON_IsString(jdesc)) info.description = jdesc->valuestring;

    cJSON* jauthor = cJSON_GetObjectItem(agent_json, "author");
    if (cJSON_IsString(jauthor)) info.author = jauthor->valuestring;

    int ret = market_service_register_agent(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Register failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to register agent: %s (error=%d)", jid->valuestring, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "agent_id", jid->valuestring);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_INFO("Agent registered: %s v%s", jid->valuestring, 
                    jver ? jver->valuestring : "unknown");
    }
}

/**
 * @brief 处理 search_agents 方法
 */
static void handle_search_agents(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jkeyword = cJSON_GetObjectItem(params, "keyword");
    const char* keyword = cJSON_IsString(jkeyword) ? jkeyword->valuestring : "";

    cJSON* joffset = cJSON_GetObjectItem(params, "offset");
    size_t offset = cJSON_IsNumber(joffset) ? (size_t)joffset->valuedouble : 0;

    cJSON* jlimit = cJSON_GetObjectItem(params, "limit");
    size_t limit = cJSON_IsNumber(jlimit) ? (size_t)jlimit->valuedouble : 20;

    agent_info_t** agents = NULL;
    size_t count = 0;
    int ret = market_service_search_agents(g_service, keyword, offset, limit, &agents, &count);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Search failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
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

    char* success = build_success_response(arr, id);
    cJSON_Delete(arr);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/**
 * @brief 处理 install_agent 方法
 */
static void handle_install_agent(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jid = cJSON_GetObjectItem(params, "agent_id");
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing agent_id", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    cJSON* jver = cJSON_GetObjectItem(params, "version");
    const char* version = cJSON_IsString(jver) ? jver->valuestring : "latest";

    int ret = market_service_install_agent(g_service, jid->valuestring, version);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Install failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to install agent: %s@%s (error=%d)", jid->valuestring, version, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "installed");
        cJSON_AddStringToObject(result, "agent_id", jid->valuestring);
        cJSON_AddStringToObject(result, "installed_version", version);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_INFO("Agent installed: %s@%s", jid->valuestring, version);
    }
}

/**
 * @brief 处理 register_skill 方法
 */
static void handle_register_skill(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* skill_json = cJSON_GetObjectItem(params, "skill");
    if (!cJSON_IsObject(skill_json)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing skill object", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }

    skill_info_t info = {0};
    cJSON* jid = cJSON_GetObjectItem(skill_json, "skill_id");
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing skill_id", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        return;
    }
    info.skill_id = jid->valuestring;

    cJSON* jname = cJSON_GetObjectItem(skill_json, "name");
    if (cJSON_IsString(jname)) info.name = jname->valuestring;

    cJSON* jver = cJSON_GetObjectItem(skill_json, "version");
    if (cJSON_IsString(jver)) info.version = jver->valuestring;

    int ret = market_service_register_skill(g_service, &info);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Register failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
        SVC_LOG_ERROR("Failed to register skill: %s (error=%d)", jid->valuestring, ret);
    } else {
        cJSON* result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "registered");
        cJSON_AddStringToObject(result, "skill_id", jid->valuestring);
        char* success = build_success_response(result, id);
        cJSON_Delete(result);
        if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
        SVC_LOG_INFO("Skill registered: %s", jid->valuestring);
    }
}

/**
 * @brief 处理 search_skills 方法
 */
static void handle_search_skills(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jkeyword = cJSON_GetObjectItem(params, "keyword");
    const char* keyword = cJSON_IsString(jkeyword) ? jkeyword->valuestring : "";

    skill_info_t** skills = NULL;
    size_t count = 0;
    int ret = market_service_search_skills(g_service, keyword, 0, 20, &skills, &count);

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Search failed", id);
        if (err) { agentos_socket_send(client_fd, err, strlen(err)); free(err); }
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

    char* success = build_success_response(arr, id);
    cJSON_Delete(arr);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/**
 * @brief 处理 health_check 方法
 */
static void handle_health_check(int id, agentos_socket_t client_fd) {
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "service", "market_d");
    cJSON_AddBoolToObject(result, "healthy", true);
    cJSON_AddNumberToObject(result, "timestamp", (double)(uint64_t)time(NULL) * 1000);

    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    if (success) { agentos_socket_send(client_fd, success, strlen(success)); free(success); }
}

/* ==================== 客户端连接处理 ==================== */

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

    if (strcmp(method->valuestring, "register_agent") == 0) {
        handle_register_agent(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "search_agents") == 0) {
        handle_search_agents(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "install_agent") == 0) {
        handle_install_agent(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "register_skill") == 0) {
        handle_register_skill(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "search_skills") == 0) {
        handle_search_skills(params, req_id, client_fd);
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
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Market service stopped");
    return 0;
}
