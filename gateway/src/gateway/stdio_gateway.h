/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file stdio_gateway.h
 * @brief Stdio 网关接口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_STDIO_GATEWAY_H
#define gateway_STDIO_GATEWAY_H

#include "gateway.h"

/**
 * @brief 创建 Stdio 网关
 *
 * @param server 网关服务器
 * @return 网关实例，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* stdio_gateway_create(gateway_server_t* server);

#endif /* gateway_STDIO_GATEWAY_H */
