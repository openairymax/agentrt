/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file http_gateway.h
 * @brief HTTP网关接口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_HTTP_H
#define AGENTOS_GATEWAY_HTTP_H

#include "gateway.h"
#include <stdint.h>
#include <stdatomic.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MHD_Connection;
struct MHD_Response;

typedef struct http_request_context {
    const char* method;              /**< HTTP方法 */
    const char* url;                 /**< 请求URL */
    const char* upload_data;         /**< 上传数据 */
    size_t upload_data_size;         /**< 上传数据大小 */
    
    cJSON* json_request;             /**< JSON请求对象 */
    uint64_t start_time_ns;          /**< 请求开始时间 */
} http_request_context_t;

typedef struct http_gateway {
    struct MHD_Daemon* daemon;       /**< MHD守护进程 */
    uint16_t port;                   /**< 监听端口 */
    char* host;                      /**< 监听地址 */

    void* handler_adapter;           /**< 公共回调适配器（动态分配） */
    gateway_request_handler_t handler; /**< 内部请求处理回调 */
    void* handler_data;              /**< 回调用户数据 */
    
    atomic_bool running;             /**< 运行标志 */
    
    atomic_uint_fast64_t requests_total;    /**< 总请求数 */
    atomic_uint_fast64_t requests_failed;   /**< 失败请求数 */
    atomic_uint_fast64_t bytes_received;    /**< 接收字节数 */
    atomic_uint_fast64_t bytes_sent;        /**< 发送字节数 */
    
    size_t max_request_size;         /**< 最大请求大小 */
} http_gateway_t;

/**
 * @brief 创建HTTP网关
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关实例，失败返回NULL
 *
 * @ownership 调用者需通过gateway_destroy()释放
 */
gateway_t* http_gateway_create(const char* host, uint16_t port);

/**
 * @brief 处理JSON-RPC请求
 */
char* handle_jsonrpc_request(http_gateway_t* gateway, http_request_context_t* context);

/**
 * @brief 创建HTTP响应
 */
struct MHD_Response* create_http_response(int status_code, const char* content, size_t content_len);

/**
 * @brief HTTP请求处理回调函数类型
 */
typedef int (*http_request_handler_t)(void* cls, struct MHD_Connection* connection,
                                       const char* url, const char* method,
                                       const char* version, const char* upload_data,
                                       size_t* upload_data_size, void** con_cls);

/**
 * @brief HTTP请求处理函数
 */
int handle_http_request(void* cls, struct MHD_Connection* connection,
                        const char* url, const char* method,
                        const char* version, const char* upload_data,
                        size_t* upload_data_size, void** con_cls);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_HTTP_H */
