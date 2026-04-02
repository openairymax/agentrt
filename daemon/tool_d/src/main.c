/**
 * @file main.c
 * @brief 工具服务守护进程入口（跨平台、线程安全版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改进说明：
 * 1. 跨平台支持（Linux/macOS/Windows）
 * 2. 线程安全的请求处理
 * 3. 配置文件驱动的 Socket 路径
 * 4. 完善的错误处理和日志
 */

#include "tool_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <cjson/cJSON.h>

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/tool.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_tool"
#define DEFAULT_TCP_PORT 8081
#define MAX_BUFFER 65536
#define MAX_CLIENTS 64

/* ==================== 全局状态 ==================== */

static tool_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;

/* 服务配置 */
typedef struct {
    char* socket_path;
    char* tcp_host;
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

/* ==================== 请求处理方法 ==================== */

/**
 * @brief 处理 register_tool 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_register(cJSON* params, int id, agentos_socket_t client_fd) {
    /* 解析参数：需要 tool 对象 */
    cJSON* tool = cJSON_GetObjectItem(params, "tool");
    if (!cJSON_IsObject(tool)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing tool object", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    tool_metadata_t meta = {0};
    cJSON* jid = cJSON_GetObjectItem(tool, "id");
    cJSON* jname = cJSON_GetObjectItem(tool, "name");
    cJSON* jexec = cJSON_GetObjectItem(tool, "executable");

    if (!cJSON_IsString(jid) || !cJSON_IsString(jname) || !cJSON_IsString(jexec)) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid tool fields: id, name, executable required", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    meta.id = jid->valuestring;
    meta.name = jname->valuestring;
    meta.executable = jexec->valuestring;

    /* 其他字段可选 */
    cJSON* desc = cJSON_GetObjectItem(tool, "description");
    if (cJSON_IsString(desc)) meta.description = desc->valuestring;

    cJSON* timeout = cJSON_GetObjectItem(tool, "timeout_sec");
    if (cJSON_IsNumber(timeout)) meta.timeout_sec = timeout->valueint;

    cJSON* cacheable = cJSON_GetObjectItem(tool, "cacheable");
    if (cJSON_IsBool(cacheable)) meta.cacheable = cacheable->valueint;

    cJSON* rule = cJSON_GetObjectItem(tool, "permission_rule");
    if (cJSON_IsString(rule)) meta.permission_rule = rule->valuestring;

    /* 参数列表 */
    cJSON* params_arr = cJSON_GetObjectItem(tool, "params");
    if (cJSON_IsArray(params_arr)) {
        size_t cnt = cJSON_GetArraySize(params_arr);
        tool_param_t* p = (tool_param_t*)calloc(cnt, sizeof(tool_param_t));
        if (p) {
            for (size_t i = 0; i < cnt; ++i) {
                cJSON* item = cJSON_GetArrayItem(params_arr, i);
                cJSON* pname = cJSON_GetObjectItem(item, "name");
                cJSON* pschema = cJSON_GetObjectItem(item, "schema");
                if (cJSON_IsString(pname)) p[i].name = pname->valuestring;
                if (cJSON_IsString(pschema)) p[i].schema = pschema->valuestring;
            }
            meta.params = p;
            meta.param_count = cnt;
        }
    }

    int ret = tool_service_register(g_service, &meta);
    free((void*)meta.params); /* params 是临时分配，注册内部会复制 */

    if (ret != AGENTOS_SUCCESS) {
        char* err = build_error_response(INTERNAL_ERROR, "Register failed", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        SVC_LOG_ERROR("Failed to register tool: %s (error=%d)", meta.id, ret);
    } else {
        char* success = build_success_response(NULL, id);
        if (success) {
            agentos_socket_send(client_fd, success, strlen(success));
            free(success);
        }
        SVC_LOG_INFO("Tool registered successfully: %s", meta.id);
    }
}

/**
 * @brief 处理 list_tools 方法
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_list(int id, agentos_socket_t client_fd) {
    char* list_json = tool_service_list(g_service);
    if (!list_json) {
        char* err = build_error_response(INTERNAL_ERROR, "List failed", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    cJSON* result = cJSON_Parse(list_json);
    free(list_json);

    if (!result) {
        char* err = build_error_response(INTERNAL_ERROR, "Invalid JSON from list", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    if (success) {
        agentos_socket_send(client_fd, success, strlen(success));
        free(success);
    }
}

/**
 * @brief 处理 get_tool 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_get(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jid = cJSON_GetObjectItem(params, "tool_id");
    if (!cJSON_IsString(jid)) {
        char* err = build_error_response(INVALID_PARAMS, "Missing tool_id", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    tool_metadata_t* meta = tool_service_get(g_service, jid->valuestring);
    if (!meta) {
        char* err = build_error_response(METHOD_NOT_FOUND, "Tool not found", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    /* 转换为 JSON */
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", meta->id);
    cJSON_AddStringToObject(obj, "name", meta->name);
    cJSON_AddStringToObject(obj, "executable", meta->executable);
    if (meta->description) cJSON_AddStringToObject(obj, "description", meta->description);
    cJSON_AddNumberToObject(obj, "timeout_sec", meta->timeout_sec);
    cJSON_AddBoolToObject(obj, "cacheable", meta->cacheable);
    if (meta->permission_rule) cJSON_AddStringToObject(obj, "permission_rule", meta->permission_rule);

    if (meta->param_count > 0) {
        cJSON* params_arr = cJSON_CreateArray();
        for (size_t i = 0; i < meta->param_count; ++i) {
            cJSON* pobj = cJSON_CreateObject();
            cJSON_AddStringToObject(pobj, "name", meta->params[i].name);
            cJSON_AddStringToObject(pobj, "schema", meta->params[i].schema);
            cJSON_AddItemToArray(params_arr, pobj);
        }
        cJSON_AddItemToObject(obj, "params", params_arr);
    }

    char* success = build_success_response(obj, id);
    cJSON_Delete(obj);
    if (success) {
        agentos_socket_send(client_fd, success, strlen(success));
        free(success);
    }
    tool_metadata_free(meta);
}

/**
 * @brief 处理 execute_tool 方法
 * @param params 参数对象
 * @param id 请求ID
 * @param client_fd 客户端描述符
 */
static void handle_execute(cJSON* params, int id, agentos_socket_t client_fd) {
    cJSON* jid = cJSON_GetObjectItem(params, "tool_id");
    cJSON* jparams = cJSON_GetObjectItem(params, "params");

    if (!cJSON_IsString(jid) || !cJSON_IsObject(jparams)) {
        char* err = build_error_response(INVALID_PARAMS, "Invalid execute params: tool_id and params required", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    char* params_json = cJSON_PrintUnformatted(jparams);
    if (!params_json) {
        char* err = build_error_response(INTERNAL_ERROR, "JSON serialization failed", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        return;
    }

    tool_execute_request_t req = {
        .tool_id = jid->valuestring,
        .params_json = params_json,
        .stream = 0
    };

    tool_result_t* res = NULL;
    int ret = tool_service_execute(g_service, &req, &res);
    free((void*)params_json);

    if (ret != AGENTOS_SUCCESS || !res) {
        char* err = build_error_response(INTERNAL_ERROR, "Execution failed", id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        SVC_LOG_ERROR("Tool execution failed: %s (error=%d)", jid->valuestring, ret);
        return;
    }

    /* 结果转 JSON */
    cJSON* result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "success", res->success);
    if (res->output) cJSON_AddStringToObject(result, "output", res->output);
    if (res->error) cJSON_AddStringToObject(result, "error", res->error);
    cJSON_AddNumberToObject(result, "exit_code", res->exit_code);

    char* success = build_success_response(result, id);
    cJSON_Delete(result);
    if (success) {
        agentos_socket_send(client_fd, success, strlen(success));
        free(success);
    }
    tool_result_free(res);
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
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* req = cJSON_Parse(buffer);
    if (!req) {
        char* err = build_error_response(PARSE_ERROR, "Parse error: invalid JSON", -1);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(req, "method");
    cJSON* params = cJSON_GetObjectItem(req, "params");
    cJSON* id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !id) {
        char* err = build_error_response(INVALID_REQUEST, "Invalid Request: missing jsonrpc/method/id", -1);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        cJSON_Delete(req);
        agentos_socket_close(client_fd);
        return;
    }

    int req_id = cJSON_IsNumber(id) ? id->valueint : 0;

    SVC_LOG_DEBUG("Processing request: method=%s, id=%d", method->valuestring, req_id);

    if (strcmp(method->valuestring, "register_tool") == 0) {
        handle_register(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "list_tools") == 0) {
        handle_list(req_id, client_fd);
    } else if (strcmp(method->valuestring, "get_tool") == 0) {
        handle_get(params, req_id, client_fd);
    } else if (strcmp(method->valuestring, "execute_tool") == 0) {
        handle_execute(params, req_id, client_fd);
    } else {
        char* err = build_error_response(METHOD_NOT_FOUND, "Method not found", req_id);
        if (err) {
            agentos_socket_send(client_fd, err, strlen(err));
            free(err);
        }
        SVC_LOG_WARN("Unknown method requested: %s", method->valuestring);
    }

    cJSON_Delete(req);
    agentos_socket_close(client_fd);
}

/* ==================== 配置加载 ==================== */

/**
 * @brief 加载守护进程配置
 * @param config_path 配置文件路径
 * @return 0 成功，非0 失败
 */
static int load_daemon_config(const char* config_path) {
    /* 默认配置 */
    g_config.use_tcp = 0;
    g_config.max_clients = MAX_CLIENTS;

#if defined(AGENTOS_PLATFORM_WINDOWS)
    g_config.socket_path = strdup(DEFAULT_SOCKET_PATH_WIN);
    g_config.tcp_host = strdup("127.0.0.1");
#else
    g_config.socket_path = strdup(DEFAULT_SOCKET_PATH_UNIX);
    g_config.tcp_host = strdup("127.0.0.1");
#endif
    g_config.tcp_port = DEFAULT_TCP_PORT;

    /* 如果提供了配置文件，尝试加载 */
    if (config_path) {
        FILE* f = fopen(config_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (len > 0 && len < 1024 * 1024) { /* 限制配置文件大小为 1MB */
                char* content = (char*)malloc((size_t)len + 1);
                if (content) {
                    size_t read_len = fread(content, 1, (size_t)len, f);
                    content[read_len] = '\0';

                    cJSON* root = cJSON_Parse(content);
                    if (root) {
                        cJSON* daemon_cfg = cJSON_GetObjectItem(root, "daemon");
                        if (daemon_cfg) {
                            cJSON* socket_path = cJSON_GetObjectItem(daemon_cfg, "socket_path");
                            if (cJSON_IsString(socket_path)) {
                                free(g_config.socket_path);
                                g_config.socket_path = strdup(socket_path->valuestring);
                            }

                            cJSON* tcp_port = cJSON_GetObjectItem(daemon_cfg, "tcp_port");
                            if (cJSON_IsNumber(tcp_port)) {
                                g_config.tcp_port = (uint16_t)tcp_port->valueint;
                                g_config.use_tcp = 1;
                            }

                            cJSON* max_clients = cJSON_GetObjectItem(daemon_cfg, "max_clients");
                            if (cJSON_IsNumber(max_clients)) {
                                g_config.max_clients = max_clients->valueint;
                            }
                        }
                        cJSON_Delete(root);
                    }
                    free(content);
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
static void free_daemon_config(void) {
    free(g_config.socket_path);
    free(g_config.tcp_host);
    memset(&g_config, 0, sizeof(g_config));
}

/* ==================== 帮助信息 ==================== */

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
static void print_usage(const char* prog) {
    printf("AgentOS Tool Daemon\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --manager <path>   Configuration file path\n");
    printf("  --tcp             Use TCP instead of Unix socket\n");
    printf("  --help             Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --manager /etc/agentos/tool.yaml\n", prog);
    printf("  %s --tcp           # Use TCP mode on port 8081\n", prog);
}

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    const char* config_path = NULL;

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

    /* 加载配置 */
    load_daemon_config(config_path);

    SVC_LOG_INFO("Tool service starting, manager=%s", config_path ? config_path : "default");

    /* 创建工具服务 */
    g_service = tool_service_create(config_path ? config_path : "manager/service/tool_d/tool.yaml");
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
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create TCP server on %s:%d",
                        g_config.tcp_host, g_config.tcp_port);
            tool_service_destroy(g_service);
            free_daemon_config();
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on TCP %s:%d", g_config.tcp_host, g_config.tcp_port);
    } else {
#if defined(AGENTOS_PLATFORM_WINDOWS)
        server_fd = agentos_socket_create_named_pipe_server(g_config.socket_path);
#else
        server_fd = agentos_socket_create_unix_server(g_config.socket_path);
#endif
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            SVC_LOG_ERROR("Failed to create socket at %s", g_config.socket_path);
            tool_service_destroy(g_service);
            free_daemon_config();
            agentos_mutex_destroy(&g_running_lock);
            agentos_socket_cleanup();
            return 1;
        }
        SVC_LOG_INFO("Listening on %s", g_config.socket_path);
    }

    SVC_LOG_INFO("Tool service started successfully");

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
    SVC_LOG_INFO("Tool service stopping...");
    agentos_socket_close(server_fd);
    tool_service_destroy(g_service);
    free_daemon_config();
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Tool service stopped");
    return 0;
}
