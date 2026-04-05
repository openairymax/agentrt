/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file ws_gateway.h
 * @brief WebSocket网关接口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_WS_H
#define AGENTOS_GATEWAY_WS_H

#include "gateway.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建WebSocket网关
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关实例，失败返回NULL
 *
 * @ownership 调用者需通过gateway_destroy()释放
 */
gateway_t* ws_gateway_create(const char* host, uint16_t port);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_WS_H */
