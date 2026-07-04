#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file main.c
 * @brief Tool 服务守护进程主入口（遵循 daemon 模块统一规范）
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
#include "daemon_cupolas_bootstrap.h"
#include "daemon_event_driver.h"
#include "jsonrpc_helpers.h"
#include "logging.h"
#include "method_dispatcher.h"
#include "param_validator.h"
#include "platform.h"
#include "svc_logger.h"
#include "thread_pool.h"
#include "tool_service.h"

#include <cjson/cJSON.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 配置常量 ==================== */

static void handle_client(agentos_socket_t client_fd);

#define DEFAULT_SOCKET_PATH_UNIX AGENTOS_RUNTIME_DIR "/tool.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_tool"
#define DEFAULT_TCP_PORT 8081
#define MAX_BUFFER 65536
#define MAX_CLIENTS 64

/* ==================== 全局状态 ==================== */

static tool_service_t *g_service = NULL;
static atomic_int g_running = 1;
static agentos_mutex_t g_running_lock;
static method_dispatcher_t *g_dispatcher = NULL;
static daemon_event_driver_t *g_event_driver = NULL;
static daemon_bootstrap_sd_t *g_bsd = NULL;
static daemon_bootstrap_ipc_t *g_bipc = NULL;

/* 服务配置 */
typedef struct {
    char *socket_path;
    char *tcp_host;
    uint16_t tcp_port;
    int use_tcp;
    int max_clients;
} tool_daemon_config_t;

static tool_daemon_config_t g_config = {0};

/* ==================== 信号处理 ==================== */

/**
 * @brief 信号处理函数
 * @param sig 信号值
 */
static void signal_handler(int sig __attribute__((unused)))
{
    agentos_mutex_lock(&g_running_lock);
    atomic_store_explicit(&g_running, 0, memory_order_seq_cst);
    agentos_mutex_unlock(&g_running_lock);
    if (g_event_driver)
        daemon_event_driver_stop(g_event_driver);
}

#ifdef _WIN32
/**
 * @brief Windows控制台处理函数
 * @param fdwCtrlType 控制信号类型
 * @return TRUE 已处理
 */
static BOOL WINAPI console_handler(DWORD fdwCtrlType)
{
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

static void svc_log_toggle_handler(int sig)
{
    (void)sig;
    static int debug_mode = 0;
    debug_mode = !debug_mode;
    log_set_module_level("*", debug_mode ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
}

/* ==================== JSON-RPC 错误码 ==================== */


/* ==================== 请求处理方法 ==================== */

/**
 * @brief 处理 register 方法
 */
static void handle_register(cJSON *params, int id, agentos_socket_t fd);

/**
 * @brief 处理 list_tools 方法
 */
static void handle_list(int id, agentos_socket_t fd);

/**
 * @brief 处理 get_tool 方法
 */
static void handle_get(cJSON *params, int id, agentos_socket_t fd);

/**
 * @brief 处理 execute_tool 方法
 */
static void handle_execute(cJSON *params, int id, agentos_socket_t fd);

/**
 * @brief 方法处理器包装函数
 */

/**
 * @brief register 方法的包装器
 */
static void on_register_method(cJSON *params, int id, void *user_data)
{
    handle_register(params, id, *(agentos_socket_t *)user_data);
}

/**
 * @brief list 方法的包装器
 */
static void on_list_method(cJSON *params, int id, void *user_data)
{
    handle_list(id, *(agentos_socket_t *)user_data);
}

/**
 * @brief get 方法的包装器
 */
static void on_get_method(cJSON *params, int id, void *user_data)
{
    handle_get(params, id, *(agentos_socket_t *)user_data);
}

/**
 * @brief execute 方法的包装器
 */
static void on_execute_method(cJSON *params, int id, void *user_data)
{
    handle_execute(params, id, *(agentos_socket_t *)user_data);
}

static int tool_on_client(void *service_ctx, agentos_socket_t client_fd)
{
    (void)service_ctx;
    handle_client(client_fd);
    return 0;
}

/**
 * @brief 处理 register 方法
 * @param params 参数对象
 * @param id 请求 ID
 * @param client_fd 客户端描述符
 */
static void handle_register(cJSON *params, int id, agentos_socket_t client_fd)
{
    cJSON *tool = jsonrpc_get_object_param(params, "tool");
    if (!tool) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS, "Missing tool object", id);
        return;
    }

    tool_metadata_t meta = {0};
    const char *tid = get_string_field(tool, "id", NULL);
    const char *tname = get_string_field(tool, "name", NULL);
    const char *texec = get_string_field(tool, "executable", NULL);

    if (!tid || !tname || !texec) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS,
                           "Invalid tool fields: id, name, executable required", id);
        return;
    }

    meta.id = (char *)tid;
    meta.name = (char *)tname;
    meta.executable = (char *)texec;

    meta.description = (char *)get_string_field(tool, "description", NULL);
    meta.timeout_sec = get_int_field(tool, "timeout_sec", 0);
    meta.cacheable = get_bool_field(tool, "cacheable", false);
    meta.permission_rule = (char *)get_string_field(tool, "permission_rule", NULL);

    /* 参数列表 */
    cJSON *params_arr = cJSON_GetObjectItem(tool, "params");
    if (cJSON_IsArray(params_arr)) {
        size_t cnt = cJSON_GetArraySize(params_arr);
        tool_param_t *p = (tool_param_t *)AGENTOS_CALLOC(cnt, sizeof(tool_param_t));
        if (p) {
            for (size_t i = 0; i < cnt; ++i) {
                cJSON *item = cJSON_GetArrayItem(params_arr, i);
                cJSON *pname = cJSON_GetObjectItem(item, "name");
                cJSON *pschema = cJSON_GetObjectItem(item, "schema");
                if (cJSON_IsString(pname))
                    p[i].name = pname->valuestring;
                if (cJSON_IsString(pschema))
                    p[i].schema = pschema->valuestring;
            }
            meta.params = p;
            meta.param_count = cnt;
        }
    }

    int ret = tool_service_register(g_service, &meta);
    AGENTOS_FREE((void *)meta.params);

    if (ret != AGENTOS_SUCCESS) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Register failed", id);
        SVC_LOG_ERROR("Failed to register tool: %s (error=%d)", meta.id, ret);
    } else {
        JSONRPC_SEND_SUCCESS(client_fd, NULL, id);
        SVC_LOG_INFO("Tool registered successfully: %s", meta.id);
    }
}

/**
 * @brief 处理 list_tools 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_list(int id, agentos_socket_t client_fd)
{
    char *list_json = tool_service_list(g_service);
    if (!list_json) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "List failed", id);
        return;
    }

    cJSON *result = cJSON_Parse(list_json);
    AGENTOS_FREE(list_json);

    if (!result) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Invalid JSON from list", id);
        return;
    }

    JSONRPC_SEND_SUCCESS(client_fd, result, id);
}

/**
 * @brief 处理 get_tool 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get(cJSON *params, int id, agentos_socket_t client_fd)
{
    const char *tid = get_string_field(params, "tool_id", NULL);
    if (!tid) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS, "Missing tool_id", id);
        return;
    }

    tool_metadata_t *meta = tool_service_get(g_service, tid);
    if (!meta) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_METHOD_NOT_FOUND, "Tool not found", id);
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", meta->id);
    cJSON_AddStringToObject(obj, "name", meta->name);
    cJSON_AddStringToObject(obj, "executable", meta->executable);
    if (meta->description)
        cJSON_AddStringToObject(obj, "description", meta->description);
    cJSON_AddNumberToObject(obj, "timeout_sec", meta->timeout_sec);
    cJSON_AddBoolToObject(obj, "cacheable", meta->cacheable);
    if (meta->permission_rule)
        cJSON_AddStringToObject(obj, "permission_rule", meta->permission_rule);

    if (meta->param_count > 0) {
        cJSON *params_arr = cJSON_CreateArray();
        for (size_t i = 0; i < meta->param_count; ++i) {
            cJSON *pobj = cJSON_CreateObject();
            cJSON_AddStringToObject(pobj, "name", meta->params[i].name);
            cJSON_AddStringToObject(pobj, "schema", meta->params[i].schema);
            cJSON_AddItemToArray(params_arr, pobj);
        }
        cJSON_AddItemToObject(obj, "params", params_arr);
    }

    JSONRPC_SEND_SUCCESS(client_fd, obj, id);
    tool_metadata_free(meta);
}

/**
 * @brief 处理 execute_tool 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_execute(cJSON *params, int id, agentos_socket_t client_fd)
{
    const char *tid = get_string_field(params, "tool_id", NULL);
    cJSON *jparams = jsonrpc_get_object_param(params, "params");

    if (!tid || !jparams) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_PARAMS,
                           "Invalid execute params: tool_id and params required", id);
        return;
    }

    char *params_json = cJSON_PrintUnformatted(jparams);
    if (!params_json) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "JSON serialization failed", id);
        return;
    }

    tool_execute_request_t req = {.tool_id = tid, .params_json = params_json, .stream = 0};

    tool_result_t *res = NULL;
    int ret = tool_service_execute(g_service, &req, &res);
    AGENTOS_FREE((void *)params_json);

    if (ret != AGENTOS_SUCCESS || !res) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INTERNAL_ERROR, "Execution failed", id);
        SVC_LOG_ERROR("Tool execution failed: %s (error=%d)", tid, ret);
        return;
    }

    /* 结果转 JSON */
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "success", res->success);
    if (res->output)
        cJSON_AddStringToObject(result, "output", res->output);
    if (res->error)
        cJSON_AddStringToObject(result, "error", res->error);
    cJSON_AddNumberToObject(result, "exit_code", res->exit_code);

    JSONRPC_SEND_SUCCESS(client_fd, result, id);
    tool_result_free(res);
}

/* ==================== 客户端连接处理 ==================== */

/**
 * @brief 处理单个客户端连接
 * @param client_fd 客户端描述符
 */
static void handle_client(agentos_socket_t client_fd)
{
    char *buffer = (char *)AGENTOS_MALLOC(MAX_BUFFER);
    if (!buffer) {
        agentos_socket_close(client_fd);
        return;
    }
    ssize_t n = agentos_socket_recv(client_fd, buffer, MAX_BUFFER - 1);

    if (n <= 0) {
        AGENTOS_FREE(buffer);
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    if ((size_t)n >= (size_t)(MAX_BUFFER - 1)) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "Request too large", -1);
        AGENTOS_FREE(buffer);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON *req = cJSON_Parse(buffer);
    if (!req) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_PARSE_ERROR, "Parse error: invalid JSON", -1);
        AGENTOS_FREE(buffer);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON *jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON *method = cJSON_GetObjectItem(req, "method");
    (void)cJSON_GetObjectItem(req, "params");
    cJSON *id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        JSONRPC_SEND_ERROR(client_fd, JSONRPC_INVALID_REQUEST, "Invalid Request: missing jsonrpc/method/id",
                           -1);
        cJSON_Delete(req);
        AGENTOS_FREE(buffer);
        agentos_socket_close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;

    SVC_LOG_DEBUG("Processing request: method=%s, id=%d", method->valuestring, req_id);

    method_dispatcher_dispatch(g_dispatcher, req, jsonrpc_build_error, &client_fd);

    cJSON_Delete(req);
    AGENTOS_FREE(buffer);
    agentos_socket_close(client_fd);
}

/* ==================== 配置加载 ==================== */

/**
 * @brief 加载守护进程配置
 * @param config_path 配置文件路径
 * @return 0 成功，非0 失败
 */
static int load_daemon_config(const char *config_path)
{
    /* 默认配置 */
    g_config.use_tcp = 0;
    g_config.max_clients = MAX_CLIENTS;

#if defined(AGENTOS_PLATFORM_WINDOWS)
    g_config.socket_path = AGENTOS_STRDUP(DEFAULT_SOCKET_PATH_WIN);
    g_config.tcp_host = AGENTOS_STRDUP("127.0.0.1");
#else
    g_config.socket_path = AGENTOS_STRDUP(DEFAULT_SOCKET_PATH_UNIX);
    g_config.tcp_host = AGENTOS_STRDUP("127.0.0.1");
#endif
    g_config.tcp_port = DEFAULT_TCP_PORT;

    /* 如果提供了配置文件，尝试加载 */
    if (config_path) {
        FILE *f = fopen(config_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (len > 0 && len < 1024 * 1024) { /* 限制配置文件大小为 1MB */
                char *content = (char *)AGENTOS_MALLOC((size_t)len + 1);
                if (content) {
                    size_t read_len = fread(content, 1, (size_t)len, f);
                    if (read_len == (size_t)len) {
                        content[read_len] = '\0';

                        cJSON *root = cJSON_Parse(content);
                        if (root) {
                            cJSON *daemon_cfg = cJSON_GetObjectItem(root, "daemon");
                            if (daemon_cfg) {
                                cJSON *socket_path = cJSON_GetObjectItem(daemon_cfg, "socket_path");
                                if (cJSON_IsString(socket_path)) {
                                    AGENTOS_FREE(g_config.socket_path);
                                    g_config.socket_path = AGENTOS_STRDUP(socket_path->valuestring);
                                }

                                cJSON *tcp_port = cJSON_GetObjectItem(daemon_cfg, "tcp_port");
                                if (cJSON_IsNumber(tcp_port)) {
                                    g_config.tcp_port = (uint16_t)tcp_port->valueint;
                                    g_config.use_tcp = 1;
                                }

                                cJSON *max_clients = cJSON_GetObjectItem(daemon_cfg, "max_clients");
                                if (cJSON_IsNumber(max_clients)) {
                                    g_config.max_clients = max_clients->valueint;
                                }
                            }
                            cJSON_Delete(root);
                        }
                    }
                    AGENTOS_FREE(content);
                }
            }
            fclose(f);
        }
    }

    return 0;
}

/**
 * @brief 释放配置资源
 */
static void free_daemon_config(void)
{
    AGENTOS_FREE(g_config.socket_path);
    AGENTOS_FREE(g_config.tcp_host);
    __builtin_memset(&g_config, 0, sizeof(g_config));
}

/* ==================== 帮助信息 ==================== */

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
static void print_usage(const char *prog)
{
    char buf[256];
    fputs("AgentRT Tool Daemon\n", stdout);
    snprintf(buf, sizeof(buf), "Usage: %s [options]\n\n", prog);
    fputs(buf, stdout);
    fputs("Options:\n", stdout);
    fputs("  --manager <path>   Configuration file path\n", stdout);
    fputs("  --tcp             Use TCP instead of Unix socket\n", stdout);
    fputs("  --help             Show this help\n", stdout);
    fputs("\n", stdout);
    fputs("Examples:\n", stdout);
    snprintf(buf, sizeof(buf), "  %s --manager AGENTOS_CONFIG_DIR \"/tool.yaml\"\n", prog);
    fputs(buf, stdout);
    snprintf(buf, sizeof(buf), "  %s --tcp           # Use TCP mode on port 8081\n", prog);
    fputs(buf, stdout);
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv)
{
    const char *config_path = NULL;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--manager") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            g_config.use_tcp = 1;
        } else {
            SVC_LOG_ERROR("Unknown option: %s", argv[i]);
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
    signal(SIGUSR1, svc_log_toggle_handler);
#endif

    agentos_log_init(NULL);
    atexit(log_cleanup);

    /* P3.14 ACC-DT15: 初始化 cupolas 安全穹顶（permission_engine + sanitizer + audit_logger）*/
    daemon_cupolas_init("tool_d");

    /* 加载配置 */
    load_daemon_config(config_path);

    SVC_LOG_INFO("Tool service starting, manager=%s", config_path ? config_path : "default");

    /* 创建工具服务 */
    g_service =
        tool_service_create(config_path ? config_path : "agentos/manager/service/tool_d/tool.yaml");
    if (!g_service) {
        SVC_LOG_ERROR("Failed to create tool service");
        free_daemon_config();
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    /* 创建服务器 Socket */
    agentos_socket_t server_fd;

    if (g_config.use_tcp) {
        server_fd = agentos_socket_create_tcp_server(g_config.tcp_host, g_config.tcp_port);
        if (server_fd < 0) {
            SVC_LOG_ERROR("Failed to create TCP server on %s:%d", g_config.tcp_host,
                          g_config.tcp_port);
            tool_service_destroy(g_service);
            free_daemon_config();
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on TCP %s:%d", g_config.tcp_host, g_config.tcp_port);
        g_bsd = daemon_bootstrap_sd_start("tool_d", "tool", g_config.tcp_host,
                                          g_config.tcp_port, "tool,core", 0);
        g_bipc = daemon_bootstrap_ipc_start("tool_d", "tool", g_config.tcp_host,
                                            g_config.tcp_port, IPC_BUS_PROTO_JSON_RPC);
    } else {
#if defined(AGENTOS_PLATFORM_WINDOWS)
        server_fd = agentos_socket_create_named_pipe_server(g_config.socket_path);
#else
        server_fd = agentos_socket_create_unix_server(g_config.socket_path);
#endif
        if (server_fd < 0) {
            SVC_LOG_ERROR("Failed to create socket at %s", g_config.socket_path);
            tool_service_destroy(g_service);
            free_daemon_config();
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on %s", g_config.socket_path);
        g_bsd = daemon_bootstrap_sd_start("tool_d", "tool", g_config.socket_path,
                                          0, "tool,core", 0);
        g_bipc = daemon_bootstrap_ipc_start("tool_d", "tool", g_config.socket_path,
                                            0, IPC_BUS_PROTO_JSON_RPC);
    }

    /* 创建事件驱动框架 */
    daemon_event_config_t ev_config;
    __builtin_memset(&ev_config, 0, sizeof(ev_config));
    ev_config.max_events = 64;
    ev_config.thread_pool_min = 4;
    ev_config.thread_pool_max = 8;
    ev_config.thread_pool_queue_size = 256;
    ev_config.use_jsonrpc = true;
    ev_config.on_client = tool_on_client;
    ev_config.service_ctx = NULL;

    g_event_driver = daemon_event_driver_create(&ev_config);
    if (!g_event_driver) {
        SVC_LOG_ERROR("Failed to create event driver");
        agentos_socket_close(server_fd);
        tool_service_destroy(g_service);
        free_daemon_config();
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    g_dispatcher = daemon_event_driver_get_dispatcher(g_event_driver);
    method_dispatcher_register(g_dispatcher, "register", on_register_method, NULL);
    method_dispatcher_register(g_dispatcher, "list_tools", on_list_method, NULL);
    method_dispatcher_register(g_dispatcher, "get_tool", on_get_method, NULL);
    method_dispatcher_register(g_dispatcher, "execute_tool", on_execute_method, NULL);
    SVC_LOG_INFO("Registered %d RPC methods", 4);

    if (daemon_event_driver_add_server_fd(g_event_driver, (int)server_fd) != 0) {
        SVC_LOG_ERROR("Failed to add server fd to event driver");
        daemon_event_driver_destroy(g_event_driver);
        agentos_socket_close(server_fd);
        tool_service_destroy(g_service);
        free_daemon_config();
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Tool service running (event-driven mode)");
    daemon_event_driver_run(g_event_driver);

    /* 清理资源 */
    daemon_bootstrap_ipc_stop(g_bipc);
    daemon_bootstrap_sd_stop(g_bsd);
    SVC_LOG_INFO("Tool service stopping...");
    daemon_event_driver_destroy(g_event_driver);
    agentos_socket_close(server_fd);
    tool_service_destroy(g_service);
    free_daemon_config();
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Tool service stopped");
    daemon_cupolas_cleanup(); /* P3.14 ACC-DT15: 清理 cupolas 安全穹顶 */
    log_cleanup();
    return 0;
}
