/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway.h
 * @brief AgentOS 网关统一接口
 *
 * 网关层只负责协议转换，将外部请求转换为系统调用。
 * 所有业务逻辑通过 atoms/syscall 接口调用。
 *
 * 架构定位：
 *   daemon/gateway_d/ → gateway/ → atoms/syscall/
 *                      ↑
 *                 协议转换层
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_H
#define AGENTOS_GATEWAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 错误码 ========== */

/**
 * @brief 网关错误码
 */
typedef enum {
    GATEWAY_SUCCESS = 0,        /**< 成功 */
    GATEWAY_ERROR_INVALID = -1, /**< 无效参数 */
    GATEWAY_ERROR_MEMORY = -2,  /**< 内存不足 */
    GATEWAY_ERROR_IO = -3,      /**< I/O 错误 */
    GATEWAY_ERROR_TIMEOUT = -4, /**< 超时 */
    GATEWAY_ERROR_CLOSED = -5,  /**< 连接已关闭 */
    GATEWAY_ERROR_PROTOCOL = -6 /**< 协议错误 */
} gateway_error_t;

/* ========== 网关类型 ========== */

/**
 * @brief 网关类型枚举
 */
typedef enum {
    GATEWAY_TYPE_HTTP = 0,   /**< HTTP 网关 */
    GATEWAY_TYPE_WS,         /**< WebSocket 网关 */
    GATEWAY_TYPE_STDIO       /**< Stdio 网关 */
} gateway_type_t;

/* ========== 网关句柄 ========== */

/**
 * @brief 网关不透明句柄
 */
typedef struct gateway gateway_t;

/* ========== 回调类型 ========== */

/**
 * @brief 请求处理回调
 *
 * @param request_json 请求 JSON 字符串
 * @param response_json 输出响应 JSON（需调用者释放）
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
typedef int (*gateway_request_handler_t)(
    const char* request_json,
    char** response_json,
    void* user_data
);

/* ========== 通用接口 ========== */

/**
 * @brief 创建 HTTP 网关
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关句柄，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* gateway_http_create(const char* host, uint16_t port);

/**
 * @brief 创建 WebSocket 网关
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关句柄，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* gateway_ws_create(const char* host, uint16_t port);

/**
 * @brief 创建 Stdio 网关
 *
 * @return 网关句柄，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_destroy() 释放
 */
gateway_t* gateway_stdio_create(void);

/**
 * @brief 销毁网关
 * @param gw 网关句柄
 */
void gateway_destroy(gateway_t* gw);

/**
 * @brief 启动网关
 *
 * @param gw 网关句柄
 * @return GATEWAY_SUCCESS 成功
 */
gateway_error_t gateway_start(gateway_t* gw);

/**
 * @brief 停止网关
 *
 * @param gw 网关句柄
 * @return GATEWAY_SUCCESS 成功
 */
gateway_error_t gateway_stop(gateway_t* gw);

/**
 * @brief 设置请求处理回调
 *
 * @param gw 网关句柄
 * @param handler 回调函数
 * @param user_data 用户数据
 * @return GATEWAY_SUCCESS 成功
 */
gateway_error_t gateway_set_handler(
    gateway_t* gw,
    gateway_request_handler_t handler,
    void* user_data
);

/**
 * @brief 获取网关类型
 * @param gw 网关句柄
 * @return 网关类型
 */
gateway_type_t gateway_get_type(gateway_t* gw);

/**
 * @brief 检查网关是否运行中
 * @param gw 网关句柄
 * @return true 运行中，false 已停止
 */
bool gateway_is_running(gateway_t* gw);

/**
 * @brief 获取网关统计信息
 *
 * @param gw 网关句柄
 * @param out_json 输出 JSON 字符串（需调用者释放）
 * @return GATEWAY_SUCCESS 成功
 */
gateway_error_t gateway_get_stats(gateway_t* gw, char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_H */
