/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file http_gateway.h
 * @brief HTTP 网关接口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_HTTP_GATEWAY_H
#define gateway_HTTP_GATEWAY_H

#include "gateway.h"
#include <stdint.h>

/**
 * @brief 创建 HTTP 网关
 *
 * @param host 监听地址
 * @param port 监听端口
 * @param server 网关服务器
 * @return 网关实例，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* http_gateway_create(
    const char* host,
    uint16_t port,
    gateway_server_t* server);

#endif /* gateway_HTTP_GATEWAY_H */
