/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file http_gateway_routes.c
 * @brief HTTP 网关路由处理函数实现
 *
 * 将 handle_http_request 的复杂逻辑拆分为独立的路由处理函数，
 * 降低圈复杂度，提高可维护性。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "http_gateway.h"
#include "jsonrpc.h"
#include "syscall_router.h"
#include "gateway_utils.h"
#include "gateway_rate_limiter.h"
#include "syscalls.h"

#include <microhttpd.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/* ========== 路由处理函数实现 ========== */

/**
 * @brief 处理 JSON-RPC POST 请求 (CC=3)
 */
int handle_post_jsonrpc(http_gateway_t* gateway,
                                struct MHD_Connection* connection,
                                http_request_context_t* context) {
    (void)connection;
    
    char* json_response = handle_jsonrpc_request(gateway, context);
    if (!json_response) {
        const char* err_msg = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}";
        struct MHD_Response* response = create_http_response(500, err_msg, strlen(err_msg));
        int ret = MHD_queue_response(connection, 500, response);
        MHD_destroy_response(response);
        return ret;
    }
    struct MHD_Response* response = create_http_response(200, json_response, strlen(json_response));
    
    uint64_t response_time_ns = gateway_time_ns() - context->start_time_ns;
    (void)response_time_ns;  /* 可用于日志记录 */
    
    atomic_fetch_add(&gateway->requests_total, 1);
    atomic_fetch_add(&gateway->bytes_received, context->upload_data_size);
    atomic_fetch_add(&gateway->bytes_sent, strlen(json_response));
    
    int ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    free(json_response);
    cJSON_Delete(context->json_request);
    free(context);
    
    return ret;
}

/**
 * @brief 处理 OPTIONS 请求（CORS 预检）(CC=2)
 */
int handle_options_preflight(http_gateway_t* gateway,
                                     struct MHD_Connection* connection,
                                     http_request_context_t* context) {
    (void)gateway;
    
    /* 安全清理：如果之前有 POST 数据部分处理，释放残留的 json_request */
    if (context->json_request) {
        cJSON_Delete(context->json_request);
        context->json_request = NULL;
    }

    struct MHD_Response* response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    int ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/**
 * @brief 处理 GET /health 健康检查 (CC=2)
 */
int handle_health_check(http_gateway_t* gateway,
                                struct MHD_Connection* connection,
                                http_request_context_t* context) {
    (void)context;
    
    const char* health_json = "{\"status\":\"healthy\",\"service\":\"gateway\"}";
    struct MHD_Response* response = create_http_response(200, health_json, strlen(health_json));
    
    atomic_fetch_add(&gateway->requests_total, 1);
    
    int ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/**
 * @brief 处理 GET /metrics 指标导出 (CC=3)
 */
int handle_metrics_export(http_gateway_t* gateway,
                                  struct MHD_Connection* connection,
                                  http_request_context_t* context) {
    (void)context;
    
    char* metrics_json = NULL;
    agentos_error_t err = agentos_sys_telemetry_metrics(&metrics_json);
    
    if (err != AGENTOS_SUCCESS || !metrics_json) {
        metrics_json = strdup("{\"error\":\"failed to get metrics\"}");
    }
    
    struct MHD_Response* response = create_http_response(200, metrics_json, strlen(metrics_json));
    free(metrics_json);
    
    atomic_fetch_add(&gateway->requests_total, 1);
    
    int ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/**
 * @brief 处理 404 Not Found (CC=2)
 */
int handle_not_found(http_gateway_t* gateway,
                             struct MHD_Connection* connection,
                             http_request_context_t* context) {
    (void)gateway;
    
    char* error_response = jsonrpc_create_error_response(NULL, -32601, "Not Found", NULL);
    struct MHD_Response* response = create_http_response(404, error_response, strlen(error_response));
    free(error_response);
    
    atomic_fetch_add(&gateway->requests_failed, 1);
    
    int ret = MHD_queue_response(connection, 404, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/**
 * @brief 处理请求大小超限错误 (CC=2)
 */
int handle_request_too_large(http_gateway_t* gateway,
                                     struct MHD_Connection* connection,
                                     http_request_context_t* context,
                                     size_t data_size) {
    (void)context;
    
    char* error_response = jsonrpc_create_error_response(NULL, -413, "Request too large", NULL);
    struct MHD_Response* response = create_http_response(413, error_response, strlen(error_response));
    free(error_response);
    
    atomic_fetch_add(&gateway->requests_failed, 1);
    atomic_fetch_add(&gateway->bytes_received, data_size);
    
    int ret = MHD_queue_response(connection, 413, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/**
 * @brief 处理 JSON 解析错误 (CC=2)
 */
int handle_parse_error(http_gateway_t* gateway,
                               struct MHD_Connection* connection,
                               http_request_context_t* context,
                               size_t data_size) {
    (void)context;
    
    char* error_response = jsonrpc_create_error_response(NULL, -32700, "Parse error", NULL);
    struct MHD_Response* response = create_http_response(400, error_response, strlen(error_response));
    free(error_response);
    
    atomic_fetch_add(&gateway->requests_failed, 1);
    atomic_fetch_add(&gateway->bytes_received, data_size);
    
    int ret = MHD_queue_response(connection, 400, response);
    MHD_destroy_response(response);
    free(context);
    
    return ret;
}

/* ========== 路由表定义（唯一实现） ========== */

/**
 * @brief HTTP 路由条目
 */
typedef struct {
    const char* method;           /**< HTTP 方法 */
    const char* path;             /**< URL 路径 */
    int (*handler)(http_gateway_t*, struct MHD_Connection*, http_request_context_t*);
} http_route_t;

/**
 * @brief HTTP 路由表（按优先级排序）
 *
 * 路由匹配规则：
 * 1. 先匹配 HTTP 方法
 * 2. 再匹配路径（支持通配符 "*"）
 * 3. 未匹配则走默认路由 (handle_not_found)
 */
static const http_route_t http_routes[] = {
    {"POST", "/", handle_post_jsonrpc},
    {"OPTIONS", "*", handle_options_preflight},
    {"GET", "/health", handle_health_check},
    {"GET", "/metrics", handle_metrics_export},
    {NULL, NULL, handle_not_found}  /* 默认路由（必须最后） */
};

/**
 * @brief 查找匹配的路由处理函数 (CC=2)
 *
 * @param method HTTP 方法（如 "POST", "GET"）
 * @param path URL 路径（如 "/", "/health"）
 * @return 匹配的路由处理函数
 */
static http_route_handler_t find_http_route(const char* method, const char* path) {
    for (const http_route_t* route = http_routes; route->method != NULL; route++) {
        if (strcmp(method, route->method) == 0) {
            if (strcmp(route->path, "*") == 0 || strcmp(path, route->path) == 0) {
                return route->handler;
            }
        }
    }
    return handle_not_found;
}

/* ========== 重构后的主请求处理函数 (CC=8) ========== */

/**
 * @brief HTTP 请求处理主函数
 *
 * 处理流程（4个阶段）：
 * 阶段1: 初始化请求上下文（首次调用）
 * 阶段2: 接收 POST 数据体
 * 阶段3: 处理完整 JSON-RPC 请求
 * 阶段4: 路由到其他端点（OPTIONS/GET等）
 */
int handle_http_request(void* cls, struct MHD_Connection* connection,
                        const char* url, const char* method,
                        const char* version, const char* upload_data,
                        size_t* upload_data_size, void** con_cls) {
    http_gateway_t* gateway = (http_gateway_t*)cls;
    http_request_context_t* context = (http_request_context_t*)*con_cls;
    
    (void)version;
    
    /* 速率限制检查（在早期阶段进行） */
    if (gateway->rate_limiter) {
        /* 获取客户端 IP 地址 */
        const char* client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Forwarded-For");
        if (!client_ip) {
            client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Real-IP");
        }
        if (!client_ip) {
            /* 尝试从连接获取 IP（简化实现） */
            client_ip = "unknown";
        }
        
        if (!gateway_rate_limiter_allow(gateway->rate_limiter, client_ip)) {
            /* 返回 429 Too Many Requests */
            const char* error_response = "{\"error\":{\"code\":-32004,\"message\":\"Rate limit exceeded\"}}";
            struct MHD_Response* response = MHD_create_response_from_buffer(
                strlen(error_response), (void*)error_response, MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header(response, "Content-Type", "application/json");
            MHD_add_response_header(response, "Server", "AgentOS-gateway/1.0");
            int ret = MHD_queue_response(connection, 429, response);
            MHD_destroy_response(response);
            return ret;
        }
    }
    
    /* 阶段 1: 初始化请求上下文 */
    if (!context) {
        context = calloc(1, sizeof(http_request_context_t));
        if (!context) {
            return MHD_NO;
        }
        context->method = method;
        context->url = url;
        context->start_time_ns = gateway_time_ns();
        *con_cls = context;
        
        MHD_get_connection_values(connection, MHD_HEADER_KIND, parse_headers, context);
        
        return MHD_YES;
    }
    
    /* 阶段 2: 处理 POST 数据 */
    if (strcmp(method, "POST") == 0 && upload_data && *upload_data_size > 0) {
        if (*upload_data_size > gateway->max_request_size) {
            return handle_request_too_large(gateway, connection, context, *upload_data_size);
        }
        
        context->upload_data = upload_data;
        context->upload_data_size = *upload_data_size;
        
        if (parse_json_request(gateway, context, upload_data, *upload_data_size) != 0) {
            return handle_parse_error(gateway, connection, context, *upload_data_size);
        }
        
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    /* 阶段 3: 处理完整请求（路由分发） */
    if (strcmp(method, "POST") == 0 && context->json_request) {
        return handle_post_jsonrpc(gateway, connection, context);
    }
    
    /* 阶段 4: 路由到其他处理函数 */
    int (*route_handler)(http_gateway_t*, struct MHD_Connection*, http_request_context_t*) = 
        find_http_route(method, url);
    
    return route_handler(gateway, connection, context);
}
