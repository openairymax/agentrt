/**
 * @file http_gateway.h
 * @brief HTTP 网关接口
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_HTTP_GATEWAY_H
#define DYNAMIC_HTTP_GATEWAY_H

#include "gateway.h"
#include <stdint.h>

/**
 * @brief 创建 HTTP 网关
 * 
 * @param host 监听地址
 * @param port 监听端口
 * @param server 服务器引用
 * @return 网关实例，失败返回 NULL
 * 
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* http_gateway_create(
    const char* host,
    uint16_t port,
    dynamic_server_t* server);

#endif /* DYNAMIC_HTTP_GATEWAY_H */
