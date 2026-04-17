/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_api.c
 * @brief 公共 API 包装函数实现
 *
 * 实现 include/gateway.h 中声明的公共 API 函数，
 * 将调用委托给各网关的内部创建函数。
 *
 * 架构说明：
 *   公共 API (gateway_http_create) --> 内部实现 (http_gateway_create)
 *   这样 agentos/daemon/gateway_d 等外部模块只需依赖公共头文件即可。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway.h"
#include "http_gateway.h"
#include "ws_gateway.h"
#include "stdio_gateway.h"

/**
 * @brief 创建 HTTP 网关（公共 API 入口）
 *
 * 委托给 http_gateway_create() 内部实现。
 * 对外暴露统一的 gateway_http_create 符号。
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关句柄，失败返回 NULL
 */
gateway_t* gateway_http_create(const char* host, uint16_t port) {
    return http_gateway_create(host, port);
}

/**
 * @brief 创建 WebSocket 网关（公共 API 入口）
 *
 * 委托给 ws_gateway_create() 内部实现。
 */
gateway_t* gateway_ws_create(const char* host, uint16_t port) {
    return ws_gateway_create(host, port);
}

/**
 * @brief 创建 Stdio 网关（公共 API 入口）
 *
 * 委托给 stdio_gateway_create() 内部实现。
 */
gateway_t* gateway_stdio_create(void) {
    return stdio_gateway_create();
}

void gateway_destroy(gateway_t* gw) {
    if (!gw) return;
    free(gw);
}

agentos_error_t gateway_start(gateway_t* gw) {
    (void)gw;
    return AGENTOS_OK;
}
