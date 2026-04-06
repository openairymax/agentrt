/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_rate_limiter.h
 * @brief Gateway 内置速率限制器
 *
 * 提供基于令牌桶算法的速率限制功能，防止 DoS 攻击和资源滥用。
 * 支持按客户端 IP 或 API Key 进行限制。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_RATE_LIMITER_H
#define AGENTOS_GATEWAY_RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 速率限制器配置
 */
typedef struct {
    bool enabled;                          /**< 是否启用速率限制 */
    uint32_t requests_per_second;          /**< 每秒请求数限制 */
    uint32_t requests_per_minute;          /**< 每分钟请求数限制 */
    uint32_t requests_per_hour;            /**< 每小时请求数限制 */
    uint32_t burst_size;                   /**< 突发容量（令牌桶大小） */
    uint32_t cleanup_interval_sec;         /**< 清理过期客户端的间隔（秒） */
} gateway_rate_limit_config_t;

/**
 * @brief 速率限制器实例（不透明指针）
 */
typedef struct gateway_rate_limiter gateway_rate_limiter_t;

/**
 * @brief 创建速率限制器
 *
 * @param config 配置参数
 * @return 速率限制器实例，失败返回 NULL
 *
 * @ownership 调用者需通过 gateway_rate_limiter_destroy() 释放
 * @threadsafe 安全
 * @since 1.0.0
 */
gateway_rate_limiter_t* gateway_rate_limiter_create(const gateway_rate_limit_config_t* config);

/**
 * @brief 销毁速率限制器
 *
 * @param limiter 速率限制器实例
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
void gateway_rate_limiter_destroy(gateway_rate_limiter_t* limiter);

/**
 * @brief 检查请求是否允许
 *
 * @param limiter 速率限制器实例
 * @param client_key 客户端标识（IP地址或 API Key）
 * @return true 允许请求，false 拒绝请求
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
bool gateway_rate_limiter_allow(gateway_rate_limiter_t* limiter, const char* client_key);

/**
 * @brief 获取当前限制状态（用于监控）
 *
 * @param limiter 速率限制器实例
 * @param total_allowed 输出：总允许请求数
 * @param total_rejected 输出：总拒绝请求数
 * @param active_clients 输出：活跃客户端数
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
void gateway_rate_limiter_get_stats(
    const gateway_rate_limiter_t* limiter,
    uint64_t* total_allowed,
    uint64_t* total_rejected,
    uint32_t* active_clients
);

/**
 * @brief 重置指定客户端的计数器
 *
 * @param limiter 速率限制器实例
 * @param client_key 客户端标识
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
void gateway_rate_limiter_reset_client(gateway_rate_limiter_t* limiter, const char* client_key);

/**
 * @brief 获取默认配置
 *
 * @param config 输出配置结构
 *
 * @since 1.0.0
 */
void gateway_rate_limiter_get_default_config(gateway_rate_limit_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_RATE_LIMITER_H */
