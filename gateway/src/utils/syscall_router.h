/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file syscall_router.h
 * @brief 系统调用路由器接口
 *
 * 统一处理 JSON-RPC 请求到系统调用的路由，
 * 被 HTTP/WebSocket/Stdio 网关共同使用。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_SYSCALL_ROUTER_H
#define GATEWAY_SYSCALL_ROUTER_H

#include <cJSON.h>
#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 路由系统调用请求
 *
 * 将 JSON-RPC 方法名和参数路由到对应的系统调用函数。
 *
 * @param[in] method 方法名（如 "agentos_sys_task_submit"）
 * @param[in] params 参数对象
 * @param[in] request_id 请求 ID（可为 NULL）
 * @return JSON 响应字符串，需调用者 free()
 */
char* gateway_syscall_route(const char* method, cJSON* params, cJSON* request_id);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_SYSCALL_ROUTER_H */
