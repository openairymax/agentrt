/**
 * @file stdio_gateway.h
 * @brief Stdio 网关接口
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_STDIO_GATEWAY_H
#define DYNAMIC_STDIO_GATEWAY_H

#include "gateway.h"

/**
 * @brief 创建 Stdio 网关
 * 
 * @param server 服务器引用
 * @return 网关实例，失败返回 NULL
 * 
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* stdio_gateway_create(dynamic_server_t* server);

#endif /* DYNAMIC_STDIO_GATEWAY_H */
