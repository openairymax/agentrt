/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file ratelimit.h
 * @brief 速率限制器模块接口
 *
 * 提供基于滑动窗口算法的请求速率限制功能，
 * 防止 DoS 攻击和资源滥用。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_RATELIMIT_H
#define gateway_RATELIMIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "agentos.h"

/**
 * @brief 速率限制器不透明句柄
 */
typedef struct ratelimiter ratelimiter_t;

/**
 * @brief 速率限制结果枚举
 */
typedef enum {
    RATELIMIT_ALLOWED = 0,    /**< 请求被允许 */
    RATELIMIT_DENIED,         /**< 请求被拒绝（超出限制） */
    RATELIMIT_DISABLED        /**< 速率限制已被禁用 */
} ratelimit_result_t;

/**
 * @brief 速率限制器配置
 */
typedef struct {
    uint32_t    requests_per_second;   /**< 每秒允许请求数 */
    uint32_t    burst_size;            /**< 突发大小（允许的额外请求数） */
    uint32_t    window_ms;             /**< 时间窗口大小（毫秒） */
    bool        enabled;               /**< 是否启用 */
} ratelimit_config_t;

/**
 * @brief 创建速率限制器
 *
 * @param manager 配置指针，为NULL时使用默认值
 * @return 速率限制器句柄，失败返回NULL
 *
 * @ownership 调用者需通过 ratelimiter_destroy() 释放
 */
ratelimiter_t* ratelimiter_create(const ratelimit_config_t* manager);

/**
 * @brief 创建速率限制器简化版本
 *
 * @param max_requests 最大请求数
 * @param window_seconds 时间窗口（秒）
 * @return 速率限制器句柄，失败返回NULL
 *
 * @ownership 调用者需通过 ratelimiter_destroy() 释放
 */
ratelimiter_t* ratelimiter_create_simple(uint32_t max_requests, uint32_t window_seconds);

/**
 * @brief 销毁速率限制器
 * @param limiter 速率限制器句柄
 */
void ratelimiter_destroy(ratelimiter_t* limiter);

/**
 * @brief 检查请求是否允许
 *
 * @param limiter 速率限制器句柄
 * @param client_id 客户端标识（如IP地址）
 * @return 限制结果
 *
 * @threadsafe 是
 */
ratelimit_result_t ratelimiter_check(ratelimiter_t* limiter, const char* client_id);

/**
 * @brief 重置客户端的计数
 *
 * @param limiter 速率限制器句柄
 * @param client_id 客户端标识
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t ratelimiter_reset(ratelimiter_t* limiter, const char* client_id);

/**
 * @brief 获取客户端当前计数
 *
 * @param limiter 速率限制器句柄
 * @param client_id 客户端标识
 * @return 当前计数，失败返回0
 */
uint32_t ratelimiter_get_count(ratelimiter_t* limiter, const char* client_id);

/**
 * @brief 清理过期条目
 *
 * @param limiter 速率限制器句柄
 * @return 清理的条目数
 */
size_t ratelimiter_cleanup(ratelimiter_t* limiter);

/**
 * @brief 获取统计信息（JSON格式）
 *
 * @param limiter 速率限制器句柄
 * @param out_json 输出JSON字符串，需调用free释放
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t ratelimiter_get_stats(
    ratelimiter_t* limiter,
    char** out_json);

#endif /* gateway_RATELIMIT_H */
