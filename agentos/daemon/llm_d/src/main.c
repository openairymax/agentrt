/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief LLM 服务守护进程主入口（遵循 daemon 模块统一规范）
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性(成对管理)
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性(platform.h)
 * - ARCHITECTURAL_PRINCIPLES.md E-5 命名语义化(SVC_LOG_*)
 * - ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯(AGENTOS_ERR_*)
 */

#include "llm_service.h"
#include "platform.h"
#include "error.h"
#include "response.h"
#include "jsonrpc_helpers.h"
#include "method_dispatcher.h"
#include "param_validator.h"
#include "svc_logger.h"
#include "daemon_errors.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <time.h>

/* ==================== 配置常量 ==================== */

#define DEFAULT_SOCKET_PATH_UNIX "/var/run/agentos/llm.sock"
#define DEFAULT_SOCKET_PATH_WIN "\\\\.\\pipe\\agentos_llm"
#define DEFAULT_TCP_PORT 8080
#define MAX_BUFFER 65536
#define MAX_CLIENTS 64
#define MAX_THREADS 8
#define MAX_MESSAGES_PER_REQUEST 128

/* ==================== 全局状态 ==================== */

static llm_service_t* g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;
static method_dispatcher_t* g_dispatcher = NULL;  /* 方法分发器 */

/* 服务配置 */
typedef struct {
    char* socket_path;
    char* tcp_host;
    uint16_t tcp_port;
    int use_tcp;
    int max_threads;
    int max_clients;
} llm_daemon_config_t;

static llm_daemon_config_t g_config = {0};

/* ==================== 信号处理 ==================== */

static void signal_handler(int sig) {
    (void)sig;
    agentos_mutex_lock(&g_running_lock);
    g_running = 0;
    agentos_mutex_unlock(&g_running_lock);
}

/* ==================== JSON-RPC 错误码 ==================== */

#define PARSE_ERROR     -32700
#define INVALID_REQUEST -32600
#define METHOD_NOT_FOUND -32601
#define INVALID_PARAMS  -32602
#define INTERNAL_ERROR  -32000

/* ==================== 请求上下文（线程安全） ==================== */

typedef struct {
    llm_message_t messages[MAX_MESSAGES_PER_REQUEST];
    size_t message_count;
    char* response_buffer;
    size_t response_size;
    size_t response_capacity;
} request_context_t;

/**
 * @brief 创建请求上下文
 */
static request_context_t* request_context_create(void) {
    request_context_t* ctx = (request_context_t*)calloc(1, sizeof(request_context_t));
    if (!ctx) return NULL;
    
    ctx->response_capacity = MAX_BUFFER;
    ctx->response_buffer = (char*)malloc(ctx->response_capacity);
    if (!ctx->response_buffer) {
        free(ctx);
        return NULL;
    }
    ctx->response_buffer[0] = '\0';
    ctx->response_size = 0;
    
    return ctx;
}

/**
 * @brief 销毁请求上下文
 */
static void request_context_destroy(request_context_t* ctx) {
    if (!ctx) return;
    
    for (size_t i = 0; i < ctx->message_count; i++) {
        free((void*)ctx->messages[i].role);
        free((void*)ctx->messages[i].content);
    }
    
    free(ctx->response_buffer);
    free(ctx);
}

/* ==================== 参数解析（线程安全） ==================== */

/**
 * @brief 解析请求参数为 llm_request_config_t
 * @param params JSON 参数对象
 * @param ctx 请求上下文（用于存储消息）
 * @param cfg 输出配置
 * @return 0 成功，非0 失败
 */
static int parse_params(cJSON* params, request_context_t* ctx, llm_request_config_t* cfg) {
    memset(cfg, 0, sizeof(llm_request_config_t));
    
    /* 解析模型 */
    cJSON* model = cJSON_GetObjectItem(params, "model");
    if (!cJSON_IsString(model)) {
        return -1;
    }
    cfg->model = strdup(model->valuestring);
    if (!cfg->model) return -1;
    
    /* 解析消息 */
    cJSON* messages = cJSON_GetObjectItem(params, "messages");
    if (cJSON_IsArray(messages)) {
        size_t count = cJSON_GetArraySize(messages);
        if (count > MAX_MESSAGES_PER_REQUEST) {
            return -1;
        }
        
        ctx->message_count = count;
        cfg->message_count = count;
        cfg->messages = ctx->messages;
        
        for (size_t i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(messages, i);
            cJSON* role = cJSON_GetObjectItem(item, "role");
            cJSON* content = cJSON_GetObjectItem(item, "content");
            
            if (!cJSON_IsString(role) || !cJSON_IsString(content)) {
                return -1;
            }
            
            /* 复制字符串到上下文 */
            ctx->messages[i].role = strdup(role->valuestring);
            ctx->messages[i].content = strdup(content->valuestring);
            
            if (!ctx->messages[i].role || !ctx->messages[i].content) {
                return -1;
            }
        }
    }
    
    /* 解析可选参数 */
    cJSON* temp = cJSON_GetObjectItem(params, "temperature");
    if (cJSON_IsNumber(temp)) {
        cfg->temperature = (float)temp->valuedouble;
    }
    
    cJSON* top_p = cJSON_GetObjectItem(params, "top_p");
    if (cJSON_IsNumber(top_p)) {
        cfg->top_p = (float)top_p->valuedouble;
    }
    
    cJSON* max_tokens = cJSON_GetObjectItem(params, "max_tokens");
    if (cJSON_IsNumber(max_tokens)) {
        cfg->max_tokens = max_tokens->valueint;
    }
    
    cJSON* stream = cJSON_GetObjectItem(params, "stream");
    if (cJSON_IsBool(stream)) {
        cfg->stream = cJSON_IsTrue(stream) ? 1 : 0;
    }
    
    cJSON* presence_penalty = cJSON_GetObjectItem(params, "presence_penalty");
    if (cJSON_IsNumber(presence_penalty)) {
        cfg->presence_penalty = presence_penalty->valuedouble;
    }
    
    cJSON* frequency_penalty = cJSON_GetObjectItem(params, "frequency_penalty");
    if (cJSON_IsNumber(frequency_penalty)) {
        cfg->frequency_penalty = frequency_penalty->valuedouble;
    }
    
    return 0;
}

/* ==================== 方法处理器包装函数 ==================== */

/* 前向声明 */
static char* handle_complete(cJSON* params, int id);
static char* handle_complete_stream(cJSON* params, int id, agentos_socket_t client_fd);

/**
 * @brief complete 方法的包装器（适配 method_dispatcher 接口）
 */
static void on_complete_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    char* response = handle_complete(params, id);
    if (response) {
        agentos_socket_t client_fd = *(agentos_socket_t*)user_data;
        agentos_socket_send(client_fd, response, strlen(response));
        free(response);
    }
}

/**
 * @brief complete_stream 方法的包装器
 */
static void on_complete_stream_method(cJSON* params, int id, void* user_data) {
    (void)user_data;
    agentos_socket_t client_fd = *(agentos_socket_t*)user_data;
    char* response = handle_complete_stream(params, id, client_fd);
    if (response) {
        agentos_socket_send(client_fd, response, strlen(response));
        free(response);
    }
}

/**
 * @brief 注册所有 JSON-RPC 方法
 */
static int register_rpc_methods(void) {
    g_dispatcher = method_dispatcher_create(16);  /* 预留 16 个方法 */
    if (!g_dispatcher) {
        SVC_LOG_ERROR("Failed to create method dispatcher");
        return -1;
    }
    
    method_dispatcher_register(g_dispatcher, "complete", on_complete_method, NULL);
    method_dispatcher_register(g_dispatcher, "complete_stream", on_complete_stream_method, NULL);
    
    SVC_LOG_INFO("Registered %d RPC methods", 2);
    return 0;
}

/* ==================== 请求处理 ==================== */

/**
 * @brief 处理 complete 方法
 */
static char* handle_complete(cJSON* params, int id) {
    request_context_t* ctx = request_context_create();
    if (!ctx) {
        return jsonrpc_build_error(INTERNAL_ERROR, "Out of memory", id);
    }

    llm_request_config_t cfg;
    if (parse_params(params, ctx, &cfg) != 0) {
        request_context_destroy(ctx);
        return jsonrpc_build_error(INVALID_PARAMS, "Invalid params", id);
    }

    uint64_t start_time = agentos_time_ms();

#define LLM_MAX_RETRIES 3
#define LLM_BASE_DELAY_MS 100

    llm_response_t* resp = NULL;
    int ret = -1;

    for (int attempt = 0; attempt <= LLM_MAX_RETRIES; attempt++) {
        ret = llm_service_complete(g_service, &cfg, &resp);

        if (ret == 0) break;

        if (attempt < LLM_MAX_RETRIES) {
            unsigned delay_ms = LLM_BASE_DELAY_MS * (1U << (attempt > 15 ? 15 : attempt));
            SVC_LOG_WARN("LLM complete attempt %d/%d failed (err=%d), retrying in %ums",
                         attempt + 1, LLM_MAX_RETRIES + 1, ret, delay_ms);
            agentos_sleep_ms(delay_ms);
        }
    }

    uint64_t end_time = agentos_time_ms();
    (void)start_time; (void)end_time;

    if (ret != 0) {
        SVC_LOG_ERROR("LLM complete failed after %d attempts (total %llums)",
                      LLM_MAX_RETRIES + 1,
                      (unsigned long long)(end_time - start_time));
        free((void*)cfg.model);
        request_context_destroy(ctx);
        return jsonrpc_build_error(INTERNAL_ERROR, "LLM service unavailable after retries", id);
    }
    
    char* resp_json = response_to_json(resp);
    llm_response_free(resp);
    free((void*)cfg.model);
    
    if (!resp_json) {
        request_context_destroy(ctx);
        return jsonrpc_build_error(INTERNAL_ERROR, "Failed to serialize response", id);
    }
    
    cJSON* result = cJSON_Parse(resp_json);
    free(resp_json);
    
    if (!result) {
        request_context_destroy(ctx);
        return jsonrpc_build_error(INTERNAL_ERROR, "Invalid response format", id);
    }
    
    char* success = jsonrpc_build_success(result, id);
    cJSON_Delete(result);
    
    request_context_destroy(ctx);
    return success;
}

/**
 * @brief 处理 complete_stream 方法
 */
typedef struct {
    agentos_socket_t fd;
} llm_stream_ctx_t;

static void llm_stream_callback(const char* chunk, void* user_data) {
    llm_stream_ctx_t* sctx = (llm_stream_ctx_t*)user_data;
    agentos_socket_send(sctx->fd, chunk, strlen(chunk));
}

static char* handle_complete_stream(cJSON* params, int id, agentos_socket_t client_fd) {
    request_context_t* ctx = request_context_create();
    if (!ctx) {
        return jsonrpc_build_error(INTERNAL_ERROR, "Out of memory", id);
    }
    
    llm_request_config_t cfg;
    if (parse_params(params, ctx, &cfg) != 0) {
        request_context_destroy(ctx);
        return jsonrpc_build_error(INVALID_PARAMS, "Invalid params", id);
    }
    
    cfg.stream = 1;
    
    llm_stream_ctx_t stream_ctx = { .fd = client_fd };
    
    llm_response_t* resp = NULL;
    int ret = llm_service_complete_stream(g_service, &cfg, llm_stream_callback, &stream_ctx, &resp);
    
    if (ret != 0) {
        free((void*)cfg.model);
        request_context_destroy(ctx);
        return jsonrpc_build_error(INTERNAL_ERROR, "Service error", id);
    }
    
    if (resp) {
        llm_response_free(resp);
    }
    
    free((void*)cfg.model);
    request_context_destroy(ctx);
    return NULL;
}

/* ==================== 客户端处理 ==================== */

/**
 * @brief 处理单个客户端连接
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
        JSONRPC_SEND_ERROR(client_fd, PARSE_ERROR, "Parse error", -1);
        agentos_socket_close(client_fd);
        return;
    }

    cJSON* jsonrpc = cJSON_GetObjectItem(req, "jsonrpc");
    cJSON* method = cJSON_GetObjectItem(req, "method");
    cJSON* params = cJSON_GetObjectItem(req, "params");
    cJSON* id = cJSON_GetObjectItem(req, "id");

    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0 ||
        !cJSON_IsString(method) || !params || !id) {
        JSONRPC_SEND_ERROR(client_fd, INVALID_REQUEST, "Invalid Request", -1);
        cJSON_Delete(req);
        agentos_socket_close(client_fd);
        return;
    }

    method_dispatcher_dispatch(g_dispatcher, req, jsonrpc_build_error, &client_fd);

    cJSON_Delete(req);
    agentos_socket_close(client_fd);
}

/* ==================== 配置加载 ==================== */

/**
 * @brief 加载守护进程配置
 */
static int load_daemon_config(const char* config_path) {
    /* 默认配置 */
    g_config.use_tcp = 0;
    g_config.max_threads = MAX_THREADS;
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
            
            char* content = (char*)malloc(len + 1);
            if (content) {
                fread(content, 1, len, f);
                content[len] = '\0';
                
                cJSON* root = cJSON_Parse(content);
                if (root) {
                    cJSON* daemon = cJSON_GetObjectItem(root, "daemon");
                    if (daemon) {
                        cJSON* socket_path = cJSON_GetObjectItem(daemon, "socket_path");
                        if (cJSON_IsString(socket_path)) {
                            free(g_config.socket_path);
                            g_config.socket_path = strdup(socket_path->valuestring);
                        }
                        
                        cJSON* tcp_port = cJSON_GetObjectItem(daemon, "tcp_port");
                        if (cJSON_IsNumber(tcp_port)) {
                            g_config.tcp_port = (uint16_t)tcp_port->valueint;
                            g_config.use_tcp = 1;
                        }
                        
                        cJSON* max_threads = cJSON_GetObjectItem(daemon, "max_threads");
                        if (cJSON_IsNumber(max_threads)) {
                            g_config.max_threads = max_threads->valueint;
                        }
                    }
                    cJSON_Delete(root);
                }
                free(content);
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

/* ==================== 主函数 ==================== */

int main(int argc, char** argv) {
    const char* config_path = NULL;
    
    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--manager") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--manager <path>] [--tcp]\n", argv[0]);
            printf("  --manager  Configuration file path\n");
            printf("  --tcp     Use TCP instead of Unix socket\n");
            return 0;
        } else if (strcmp(argv[i], "--tcp") == 0) {
            g_config.use_tcp = 1;
        }
    }
    
    /* 初始化平台层 */
    agentos_socket_init();
    agentos_mutex_init(&g_running_lock);
    
    /* 设置信号处理 */
#if !defined(AGENTOS_PLATFORM_WINDOWS)
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
#else
    /* Windows 使用控制台控制处理 */
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)signal_handler, TRUE);
#endif
    
    /* 加载配置 */
    load_daemon_config(config_path);
    
    printf("LLM service starting, manager=%s\n", config_path ? config_path : "default");
    
    /* 创建 LLM 服务 */
    g_service = llm_service_create(config_path);
    if (!g_service) {
        fprintf(stderr, "Failed to create service\n");
        free_daemon_config();
        agentos_socket_cleanup();
        return 1;
    }
    
    /* 注册 RPC 方法 */
    if (register_rpc_methods() != 0) {
        fprintf(stderr, "Failed to register RPC methods\n");
        llm_service_destroy(g_service);
        free_daemon_config();
        agentos_socket_cleanup();
        return 1;
    }
    
    /* 创建服务器 Socket */
    agentos_socket_t server_fd;
    
    if (g_config.use_tcp) {
        server_fd = agentos_socket_create_tcp_server(g_config.tcp_host, g_config.tcp_port);
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            fprintf(stderr, "Failed to create TCP server on %s:%d\n", 
                    g_config.tcp_host, g_config.tcp_port);
            llm_service_destroy(g_service);
            free_daemon_config();
            agentos_socket_cleanup();
            return 1;
        }
        printf("Listening on TCP %s:%d\n", g_config.tcp_host, g_config.tcp_port);
    } else {
#if defined(AGENTOS_PLATFORM_WINDOWS)
        server_fd = agentos_socket_create_named_pipe_server(g_config.socket_path);
#else
        server_fd = agentos_socket_create_unix_server(g_config.socket_path);
#endif
        if (server_fd == AGENTOS_INVALID_SOCKET) {
            fprintf(stderr, "Failed to create socket at %s\n", g_config.socket_path);
            llm_service_destroy(g_service);
            free_daemon_config();
            agentos_socket_cleanup();
            return 1;
        }
        printf("Listening on %s\n", g_config.socket_path);
    }
    
    /* 主事件循环 */
    while (g_running) {
        agentos_socket_t client_fd = agentos_socket_accept(server_fd, 5000);
        
        if (client_fd == AGENTOS_INVALID_SOCKET) {
            continue;
        }
        
        /* 处理客户端请求 */
        handle_client(client_fd);
    }
    
    /* 清理 */
    printf("LLM service stopping...\n");
    agentos_socket_close(server_fd);
    llm_service_destroy(g_service);
    if (g_dispatcher) method_dispatcher_destroy(g_dispatcher);
    free_daemon_config();
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();
    
    printf("LLM service stopped\n");
    return 0;
}
