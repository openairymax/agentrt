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
#include <cJSON.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/* 前向声明 */
struct gateway_rate_limiter;
typedef struct gateway_rate_limiter gateway_rate_limiter_t;
struct gateway_protocol_handler_s;
typedef struct gateway_protocol_handler_s* gateway_protocol_handler_t;

#ifdef __cplusplus
extern "C" {
#endif

struct MHD_Connection;
struct MHD_Response;

/**
 * @brief CORS 配置结构
 *
 * 用于控制跨源资源共享(CORS)的安全设置。
 * 生产环境应配置白名单，开发环境可允许所有来源。
 */
typedef struct {
    bool allow_all_origins;           /**< 开发模式：允许所有来源（默认false） */
    char** allowed_origins;           /**< 生产模式：允许的来源白名单 */
    size_t allowed_origins_count;     /**< 白名单数量 */
    char* allowed_methods;            /**< 允许的HTTP方法（如"POST, GET, OPTIONS"） */
    char* allowed_headers;            /**< 允许的请求头（如"Content-Type, Authorization"） */
    int max_age;                      /**< 预检请求缓存时间（秒） */
} cors_config_t;

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
    unsigned int connection_limit;   /**< 最大并发连接数(0=默认1000) */
    unsigned int connection_timeout; /**< 连接超时秒数(0=默认30) */
    cors_config_t cors;              /**< CORS配置 */
    gateway_rate_limiter_t* rate_limiter; /**< 速率限制器 */
    
    gateway_protocol_handler_t protocol_handler; /**< 多协议处理器 */
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
