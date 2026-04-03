/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway.h
 * @brief 网关抽象接口 - 内部使用
 *
 * 定义网关操作表(ops)模式和内部数据结构。
 * 同时提供公共 API 的 inline 包装函数实现。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_INTERNAL_H
#define AGENTOS_GATEWAY_INTERNAL_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 网关类型 ========== */

/**
 * @brief 网关类型枚举（与公共头文件保持一致）
 */
typedef enum {
    GATEWAY_TYPE_HTTP = 0,   /**< HTTP 网关 */
    GATEWAY_TYPE_WS,         /**< WebSocket 网关 */
    GATEWAY_TYPE_STDIO       /**< Stdio 网关 */
} gateway_type_t;

/* ========== 回调类型（内部使用） ========== */

/**
 * @brief 内部请求处理回调函数类型
 *
 * @param request 请求对象指针
 * @param user_data 用户数据
 * @return JSON 响应字符串（需调用者 free），或 NULL
 */
typedef char* (*gateway_request_handler_t)(void* request, void* user_data);

/* ========== 操作表 ========== */

/**
 * @brief 网关操作表（策略模式/多态分发）
 *
 * 每种网关类型实现一套操作函数，通过函数指针统一调用。
 */
typedef struct gateway_ops {
    /**
     * @brief 启动网关
     * @param[in] impl 具体实现实例指针
     * @return AGENTOS_SUCCESS 成功
     */
    agentos_error_t (*start)(void* impl);

    /**
     * @brief 停止网关
     * @param[in] impl 具体实现实例指针
     */
    void (*stop)(void* impl);

    /**
     * @brief 销毁网关（释放 impl 资源，不含 gateway_t 自身）
     * @param[in] impl 具体实现实例指针
     */
    void (*destroy)(void* impl);

    /**
     * @brief 获取网关名称
     * @param[in] impl 具体实现实例指针
     * @return 名称字符串
     */
    const char* (*get_name)(void* impl);

    /**
     * @brief 获取统计信息
     * @param[in] impl 具体实现实例指针
     * @param[out] out_json 输出 JSON 字符串（调用者 free）
     * @return AGENTOS_SUCCESS 成功
     */
    agentos_error_t (*get_stats)(void* impl, char** out_json);

    /**
     * @brief 检查是否运行中
     * @param[in] impl 具体实现实例指针
     * @return true 运行中
     */
    bool (*is_running)(void* impl);

    /**
     * @brief 设置请求处理回调
     * @param[in] impl 具体实现实例指针
     * @param[in] handler 回调函数
     * @param[in] user_data 用户数据
     * @return AGENTOS_SUCCESS 成功
     */
    agentos_error_t (*set_handler)(void* impl, gateway_request_handler_t handler, void* user_data);
} gateway_ops_t;

/* ========== 网关抽象结构 ========== */

/**
 * @brief 网关抽象结构（不透明句柄的具体定义）
 *
 * 组合模式：ops 操作表 + impl 具体实现 + type 类型标记。
 * 所有公共 API 通过此结构的 ops 分发到具体实现。
 */
typedef struct gateway {
    const gateway_ops_t* ops;       /**< 操作表（不可变，生命周期同 gateway） */
    void* impl;                     /**< 具体实现数据（由各网关 create 分配，destroy 释放） */
    gateway_type_t type;            /**< 网关类型标记 */
} gateway_t;

/* ========== 内联操作函数（供内部模块使用） ========== */

/**
 * @brief 启动网关（内部内联版本）
 * @param gateway 网关实例
 * @return AGENTOS_SUCCESS 成功，AGENTOS_EINVAL 参数无效
 */
static inline agentos_error_t gateway_start(gateway_t* gateway) {
    if (!gateway || !gateway->ops || !gateway->ops->start) {
        return AGENTOS_EINVAL;
    }
    return gateway->ops->start(gateway->impl);
}

/**
 * @brief 停止网关（内部内联版本）
 * @param gateway 网关实例
 */
static inline void gateway_stop(gateway_t* gateway) {
    if (gateway && gateway->ops && gateway->ops->stop) {
        gateway->ops->stop(gateway->impl);
    }
}

/**
 * @brief 销毁网关（内部内联版本，同时释放 gateway_t 自身）
 * @param gateway 网关实例
 */
static inline void gateway_destroy(gateway_t* gateway) {
    if (gateway && gateway->ops && gateway->ops->destroy) {
        gateway->ops->destroy(gateway->impl);
        free(gateway);
    }
}

/**
 * @brief 获取网关名称
 * @param gateway 网关实例
 * @return 名称字符串，NULL 返回 "unknown"
 */
static inline const char* gateway_get_name(gateway_t* gateway) {
    if (!gateway || !gateway->ops || !gateway->ops->get_name) {
        return "unknown";
    }
    return gateway->ops->get_name(gateway->impl);
}

/**
 * @brief 获取统计信息
 * @param gateway 网关实例
 * @param[out] out_json 输出 JSON 字符串
 * @return AGENTOS_SUCCESS 成功
 */
static inline agentos_error_t gateway_get_stats(gateway_t* gateway, char** out_json) {
    if (!gateway || !gateway->ops || !gateway->ops->get_stats) {
        return AGENTOS_EINVAL;
    }
    return gateway->ops->get_stats(gateway->impl, out_json);
}

/**
 * @brief 检查是否运行中
 * @param gateway 网关实例
 * @return true 运行中，false 已停止或无效
 */
static inline bool gateway_is_running(gateway_t* gateway) {
    if (!gateway || !gateway->ops || !gateway->ops->is_running) {
        return false;
    }
    return gateway->ops->is_running(gateway->impl);
}

/**
 * @brief 获取网关类型
 * @param gateway 网关实例
 * @return 网关类型，NULL 返回 GATEWAY_TYPE_HTTP（默认值）
 */
static inline gateway_type_t gateway_get_type(gateway_t* gateway) {
    if (!gateway) {
        return GATEWAY_TYPE_HTTP;
    }
    return gateway->type;
}

/**
 * @brief 设置处理回调（内部版本）
 * @param gateway 网关实例
 * @param handler 回调函数
 * @param user_data 用户数据
 * @return AGENTOS_SUCCESS 成功
 */
static inline agentos_error_t gateway_set_handler(
    gateway_t* gateway,
    gateway_request_handler_t handler,
    void* user_data
) {
    if (!gateway || !gateway->ops || !gateway->ops->set_handler) {
        return AGENTOS_EINVAL;
    }
    return gateway->ops->set_handler(gateway->impl, handler, user_data);
}

/* ========== 公共 API 包装函数 ==========
 *
 * 以下函数是公共 API (include/gateway.h) 声明的函数的实现。
 * 它们委托给各网关的 xxx_gateway_create() 函数。
 * 这些函数在 gateway 库编译时提供符号解析，
 * 使 daemon/gateway_d 可以链接使用。
 *
 * 注意：公共头文件中的 gateway_request_handler_t 签名与此处的
 *       内部版本不同。公共版本使用 (const char*, char**, void*) -> int，
 *       此处内部版本使用 (void*, void*) -> char*。
 *       在具体网关实现的 set_handler 中需要做适配层转换。
 */

/**
 * @brief 创建 HTTP 网关（公共 API 实现）
 */
gateway_t* gateway_http_create(const char* host, uint16_t port);

/**
 * @brief 创建 WebSocket 网关（公共 API 实现）
 */
gateway_t* gateway_ws_create(const char* host, uint16_t port);

/**
 * @brief 创建 Stdio 网关（公共 API 实现）
 */
gateway_t* gateway_stdio_create(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_INTERNAL_H */
