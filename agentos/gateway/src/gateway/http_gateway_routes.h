/**
 * @file http_gateway_routes.h
 * @brief HTTP 网关路由表定义
 * 
 * 使用路由表模式降低 handle_http_request 的圈复杂度，
 * 将每个路由处理逻辑拆分为独立函数。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef HTTP_GATEWAY_ROUTES_H
#define HTTP_GATEWAY_ROUTES_H

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <cJSON.h>

#include "http_gateway.h"
#include "jsonrpc.h"
#include "syscall_router.h"
#include "agentos.h"

/* ========== 路由处理函数声明 ========== */

/**
 * @brief 处理 JSON-RPC POST 请求
 */
static int handle_post_jsonrpc(http_gateway_t* gateway, 
                                struct MHD_Connection* connection,
                                http_request_context_t* context);

/**
 * @brief 处理 OPTIONS 请求（CORS 预检）
 */
static int handle_options_preflight(http_gateway_t* gateway,
                                     struct MHD_Connection* connection,
                                     http_request_context_t* context);

/**
 * @brief 处理 GET /health 健康检查
 */
static int handle_health_check(http_gateway_t* gateway,
                                struct MHD_Connection* connection,
                                http_request_context_t* context);

/**
 * @brief 处理 GET /metrics 指标导出
 */
static int handle_metrics_export(http_gateway_t* gateway,
                                  struct MHD_Connection* connection,
                                  http_request_context_t* context);

/**
 * @brief 处理 404 Not Found
 */
static int handle_not_found(http_gateway_t* gateway,
                             struct MHD_Connection* connection,
                             http_request_context_t* context);

/**
 * @brief 处理请求大小超限错误
 */
static int handle_request_too_large(http_gateway_t* gateway,
                                     struct MHD_Connection* connection,
                                     http_request_context_t* context,
                                     size_t data_size);

/**
 * @brief 处理 JSON 解析错误
 */
static int handle_parse_error(http_gateway_t* gateway,
                               struct MHD_Connection* connection,
                               http_request_context_t* context,
                               size_t data_size);

/* ========== 路由表结构 ========== */

/**
 * @brief HTTP 路由条目
 */
typedef struct {
    const char* method;           /**< HTTP 方法 */
    const char* path;             /**< URL 路径 */
    int (*handler)(http_gateway_t*, struct MHD_Connection*, http_request_context_t*);
} http_route_t;

/* ========== 路由表 ========== */

static const http_route_t http_routes[] = {
    {"POST", "/", handle_post_jsonrpc},
    {"OPTIONS", "*", handle_options_preflight},
    {"GET", "/health", handle_health_check},
    {"GET", "/metrics", handle_metrics_export},
    {NULL, NULL, handle_not_found}  /* 默认路由 */
};

/**
 * @brief 查找匹配的路由
 * 
 * @param method HTTP 方法
 * @param path URL 路径
 * @return 匹配的路由处理函数，未找到返回 NULL
 */
static inline http_route_handler_t find_http_route(const char* method, const char* path) {
    for (const http_route_t* route = http_routes; route->method != NULL; route++) {
        if (strcmp(method, route->method) == 0) {
            /* 通配符路由或精确路径匹配 */
            if (strcmp(route->path, "*") == 0 || strcmp(path, route->path) == 0) {
                return route->handler;
            }
        }
    }
    return handle_not_found;
}

#endif /* HTTP_GATEWAY_ROUTES_H */
