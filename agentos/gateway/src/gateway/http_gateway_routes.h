/**
 * @file http_gateway_routes.h
 * @brief HTTP 网关路由表声明
 * 
 * 使用路由表模式降低 handle_http_request 的圈复杂度，
 * 将每个路由处理逻辑拆分为独立函数。
 * 
 * 设计原则：
 *   E-8 可测试性：每个路由处理函数可独立测试
 *   K-1 内核极简：路由函数只做分发，不含业务逻辑
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef HTTP_GATEWAY_ROUTES_H
#define HTTP_GATEWAY_ROUTES_H

#include <string.h>
#include <stdlib.h>
#include <cJSON.h>

/* 跨平台原子操作支持 */
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include "../../../commons/utils/include/atomic_compat.h"
#else
#include <stdatomic.h>
#endif

#include "http_gateway.h"
#include "jsonrpc.h"
#include "syscall_router.h"
#include "syscalls.h"

/* ========== 路由处理函数声明 ========== */

/**
 * @brief 处理 JSON-RPC POST 请求
 */
int handle_post_jsonrpc(http_gateway_t* gateway,
                        struct MHD_Connection* connection,
                        http_request_context_t* context);

/**
 * @brief 处理 OPTIONS 请求（CORS 预检）
 */
int handle_options_preflight(http_gateway_t* gateway,
                             struct MHD_Connection* connection,
                             http_request_context_t* context);

/**
 * @brief 处理 GET /health 健康检查
 */
int handle_health_check(http_gateway_t* gateway,
                        struct MHD_Connection* connection,
                        http_request_context_t* context);

/**
 * @brief 处理 GET /metrics 指标导出
 */
int handle_metrics_export(http_gateway_t* gateway,
                          struct MHD_Connection* connection,
                          http_request_context_t* context);

/**
 * @brief 处理 404 Not Found
 */
int handle_not_found(http_gateway_t* gateway,
                     struct MHD_Connection* connection,
                     http_request_context_t* context);

/**
 * @brief 处理请求大小超限错误
 */
int handle_request_too_large(http_gateway_t* gateway,
                             struct MHD_Connection* connection,
                             http_request_context_t* context,
                             size_t data_size);

/**
 * @brief 处理 JSON 解析错误
 */
int handle_parse_error(http_gateway_t* gateway,
                        struct MHD_Connection* connection,
                        http_request_context_t* context,
                        size_t data_size);

/* ========== 路由条目类型 ========== */

/**
 * @brief HTTP 路由条目
 */
typedef struct {
    const char* method;           /**< HTTP 方法 */
    const char* path;             /**< URL 路径 */
    int (*handler)(http_gateway_t*, struct MHD_Connection*, http_request_context_t*);
} http_route_t;

typedef int (*http_route_handler_t)(http_gateway_t*, struct MHD_Connection*, http_request_context_t*);

/* ========== 主入口函数声明 ========== */

/**
 * @brief HTTP 请求处理主函数
 * 
 * 圈复杂度从~25 降至~8，通过路由表模式实现。
 * 
 * @param cls 网关实例指针 (http_gateway_t*)
 * @param connection MHD 连接对象
 * @param url 请求 URL
 * @param method HTTP 方法
 * @param version HTTP 版本
 * @param upload_data 上传数据
 * @param upload_data_size 数据大小
 * @param con_cls 连接上下文
 * @return MHD_YES/MHD_NO
 */
int handle_http_request(void* cls, struct MHD_Connection* connection,
                        const char* url, const char* method,
                        const char* version, const char* upload_data,
                        size_t* upload_data_size, void** con_cls);

#endif /* HTTP_GATEWAY_ROUTES_H */
