/**
 * @file stdio_gateway.c
 * @brief Stdio 网关实现（JSON-RPC 2.0）
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "stdio_gateway.h"
#include "../server.h"
#include "../logger.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 缓冲区大小 */
#define STDIO_BUFFER_SIZE   65536
#define STDIO_MAX_LINE      1048576

/* Stdio 网关实现结构 */
typedef struct stdio_gateway_impl {
    dynamic_server_t*   server;
    
    pthread_t           reader_thread;
    atomic_bool         running;
    
    FILE*               stdin_file;
    FILE*               stdout_file;
    
    atomic_uint_fast64_t requests_total;
    atomic_uint_fast64_t requests_failed;
    atomic_uint_fast64_t bytes_received;
    atomic_uint_fast64_t bytes_sent;
} stdio_gateway_impl_t;

/* ========== JSON-RPC 处理 ========== */

/**
 * @brief 处理 JSON-RPC 请求
 */
static char* handle_jsonrpc(stdio_gateway_impl_t* gw, const char* line) {
    /* 简化的 JSON-RPC 解析 */
    if (!line || line[0] != '{') {
        return strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"},\"id\":null}");
    }
    
    /* 提取 method */
    const char* method_start = strstr(line, "\"method\"");
    if (!method_start) {
        return strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}");
    }
    
    method_start = strchr(method_start + 8, ':');
    if (!method_start) {
        return strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}");
    }
    
    method_start = strchr(method_start + 1, '"');
    if (!method_start) {
        return strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}");
    }
    
    const char* method_end = strchr(method_start + 1, '"');
    if (!method_end) {
        return strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}");
    }
    
    char method[64] = {0};
    size_t method_len = method_end - method_start - 1;
    if (method_len >= sizeof(method)) method_len = sizeof(method) - 1;
    memcpy(method, method_start + 1, method_len);
    
    /* 路由到对应处理器 */
    char* result = NULL;
    
    if (strcmp(method, "server.status") == 0) {
        dynamic_server_get_stats(gw->server, &result);
    } else if (strcmp(method, "server.health") == 0) {
        dynamic_server_get_health(gw->server, &result);
    } else if (strcmp(method, "server.metrics") == 0) {
        dynamic_server_get_metrics(gw->server, &result);
    } else if (strcmp(method, "session.create") == 0) {
        char* session_id = NULL;
        if (session_manager_create_session(gw->server->session_mgr, NULL, &session_id) 
                == AGENTOS_SUCCESS) {
            asprintf(&result, "{\"session_id\":\"%s\"}", session_id);
            free(session_id);
        }
    } else if (strcmp(method, "shutdown") == 0) {
        /* 特殊方法：关闭服务器 */
        dynamic_server_stop(gw->server);
        result = strdup("null");
    } else {
        char* error_json = NULL;
        asprintf(&error_json, 
            "{\"code\":-32601,\"message\":\"Method not found: %s\"}",
            method);
        char* response = NULL;
        asprintf(&response, 
            "{\"jsonrpc\":\"2.0\",\"error\":%s,\"id\":null}",
            error_json);
        free(error_json);
        return response;
    }
    
    /* 构建成功响应 */
    char* response = NULL;
    if (result) {
        asprintf(&response, "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":1}", result);
        free(result);
    } else {
        response = strdup("{\"jsonrpc\":\"2.0\",\"result\":null,\"id\":1}");
    }
    
    return response;
}

/* ========== 读取线程 ========== */

static void* reader_thread_func(void* arg) {
    stdio_gateway_impl_t* gw = (stdio_gateway_impl_t*)arg;
    
    char* line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    
    AGENTOS_LOG_INFO("Stdio gateway started");
    
    while (atomic_load(&gw->running)) {
        line_len = getline(&line, &line_cap, gw->stdin_file);
        
        if (line_len < 0) {
            /* EOF 或错误 */
            break;
        }
        
        /* 移除换行符 */
        while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line[--line_len] = '\0';
        }
        
        if (line_len == 0) continue;
        
        atomic_fetch_add(&gw->bytes_received, line_len);
        atomic_fetch_add(&gw->requests_total, 1);
        
        /* 处理请求 */
        char* response = handle_jsonrpc(gw, line);
        
        if (response) {
            /* 输出响应 */
            fprintf(gw->stdout_file, "%s\n", response);
            fflush(gw->stdout_file);
            
            atomic_fetch_add(&gw->bytes_sent, strlen(response));
            free(response);
        } else {
            atomic_fetch_add(&gw->requests_failed, 1);
        }
    }
    
    free(line);
    AGENTOS_LOG_INFO("Stdio gateway stopped");
    return NULL;
}

/* ========== 网关操作实现 ========== */

static agentos_error_t stdio_start(void* impl) {
    stdio_gateway_impl_t* gw = (stdio_gateway_impl_t*)impl;
    if (!gw) return AGENTOS_EINVAL;
    
    atomic_store(&gw->running, true);
    
    if (pthread_create(&gw->reader_thread, NULL, reader_thread_func, gw) != 0) {
        return AGENTOS_ERROR;
    }
    
    return AGENTOS_SUCCESS;
}

static void stdio_stop(void* impl) {
    stdio_gateway_impl_t* gw = (stdio_gateway_impl_t*)impl;
    if (!gw) return;
    
    atomic_store(&gw->running, false);
    
    /* 关闭 stdin 以唤醒 getline */
    if (gw->stdin_file) {
        fclose(gw->stdin_file);
        gw->stdin_file = NULL;
    }
    
    pthread_join(gw->reader_thread, NULL);
}

static void stdio_destroy(void* impl) {
    stdio_gateway_impl_t* gw = (stdio_gateway_impl_t*)impl;
    if (!gw) return;
    
    stdio_stop(impl);
    free(gw);
}

static const char* stdio_get_name(void* impl) {
    (void)impl;
    return "stdio";
}

static agentos_error_t stdio_get_stats(void* impl, char** out_json) {
    stdio_gateway_impl_t* gw = (stdio_gateway_impl_t*)impl;
    if (!gw || !out_json) return AGENTOS_EINVAL;
    
    char* json = NULL;
    int len = asprintf(&json,
        "{\"name\":\"stdio\","
        "\"requests_total\":%llu,\"requests_failed\":%llu,"
        "\"bytes_received\":%llu,\"bytes_sent\":%llu}",
        (unsigned long long)atomic_load(&gw->requests_total),
        (unsigned long long)atomic_load(&gw->requests_failed),
        (unsigned long long)atomic_load(&gw->bytes_received),
        (unsigned long long)atomic_load(&gw->bytes_sent));
    
    if (len < 0 || !json) return AGENTOS_ENOMEM;
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

static const gateway_ops_t stdio_ops = {
    .start = stdio_start,
    .stop = stdio_stop,
    .destroy = stdio_destroy,
    .get_name = stdio_get_name,
    .get_stats = stdio_get_stats
};

/* ========== 公共 API ========== */

gateway_t* stdio_gateway_create(dynamic_server_t* server) {
    if (!server) return NULL;
    
    stdio_gateway_impl_t* impl = (stdio_gateway_impl_t*)calloc(1, sizeof(stdio_gateway_impl_t));
    if (!impl) return NULL;
    
    impl->server = server;
    impl->stdin_file = stdin;
    impl->stdout_file = stdout;
    
    atomic_init(&impl->requests_total, 0);
    atomic_init(&impl->requests_failed, 0);
    atomic_init(&impl->bytes_received, 0);
    atomic_init(&impl->bytes_sent, 0);
    
    gateway_t* gateway = (gateway_t*)malloc(sizeof(gateway_t));
    if (!gateway) {
        free(impl);
        return NULL;
    }
    
    gateway->ops = &stdio_ops;
    gateway->server = server;
    gateway->impl = impl;
    
    return gateway;
}
