/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway.h
 * @brief 网关抽象接口 - 内部使用
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_INTERNAL_H
#define AGENTOS_GATEWAY_INTERNAL_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 请求处理回调函数类型
 * @param request JSON请求对象
 * @param user_data 用户数据
 * @return JSON响应字符串
 */
typedef char* (*gateway_request_handler_t)(void* request, void* user_data);

/**
 * @brief 网关操作表
 */
typedef struct gateway_ops {
    /**
     * @brief 启动网关
     * @param gateway 网关实例
     * @return AGENTOS_SUCCESS 成功
     */
    agentos_error_t (*start)(void* gateway);

    /**
     * @brief 停止网关
     * @param gateway 网关实例
     */
    void (*stop)(void* gateway);

    /**
     * @brief 销毁网关
     * @param gateway 网关实例
     */
    void (*destroy)(void* gateway);

    /**
     * @brief 获取网关名称
     * @param gateway 网关实例
     * @return 网关名称字符串
     */
    const char* (*get_name)(void* gateway);

    /**
     * @brief 获取网关统计信息
     * @param gateway 网关实例
     * @param[out] out_json 输出JSON字符串（需调用者free）
     * @return AGENTOS_SUCCESS 成功
     */
    agentos_error_t (*get_stats)(void* gateway, char** out_json);
} gateway_ops_t;

/**
 * @brief 网关抽象结构
 */
typedef struct gateway {
    const gateway_ops_t* ops;       /**< 操作表 */
    void* impl;                     /**< 具体实现数据 */
} gateway_t;

/**
 * @brief 启动网关
 * @param gateway 网关实例
 * @return AGENTOS_SUCCESS 成功
 */
static inline agentos_error_t gateway_start(gateway_t* gateway) {
    if (!gateway || !gateway->ops || !gateway->ops->start) {
        return AGENTOS_EINVAL;
    }
    return gateway->ops->start(gateway->impl);
}

/**
 * @brief 停止网关
 * @param gateway 网关实例
 */
static inline void gateway_stop(gateway_t* gateway) {
    if (gateway && gateway->ops && gateway->ops->stop) {
        gateway->ops->stop(gateway->impl);
    }
}

/**
 * @brief 销毁网关
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
 * @return 网关名称字符串
 */
static inline const char* gateway_get_name(gateway_t* gateway) {
    if (!gateway || !gateway->ops || !gateway->ops->get_name) {
        return "unknown";
    }
    return gateway->ops->get_name(gateway->impl);
}

/**
 * @brief 获取网关统计信息
 * @param gateway 网关实例
 * @param[out] out_json 输出JSON字符串
 * @return AGENTOS_SUCCESS 成功
 */
static inline agentos_error_t gateway_get_stats(gateway_t* gateway, char** out_json) {
    if (!gateway || !gateway->ops || !gateway->ops->get_stats) {
        return AGENTOS_EINVAL;
    }
    return gateway->ops->get_stats(gateway->impl, out_json);
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_INTERNAL_H */
