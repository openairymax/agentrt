/**
 * @file ipc_backpressure.h
 * @brief IPC Bus 背压控制 — 三级策略
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * P1.24: 三级背压策略防止 IPC Bus 过载：
 *   - Queue > 80%: 生产者降速
 *   - Queue > 90%: Droppable 消息丢弃
 *   - Queue > 95%: 拒绝新连接 + 告警
 *   - Queue < 60%: 恢复正常速率
 */

#ifndef AGENTOS_IPC_BACKPRESSURE_H
#define AGENTOS_IPC_BACKPRESSURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 背压级别
 */
typedef enum {
    IPC_BP_NORMAL = 0,      /**< < 60% 正常 */
    IPC_BP_SLOW   = 1,      /**< 60-80% 降速 */
    IPC_BP_DROP   = 2,      /**< 80-90% 丢弃低优先级消息 */
    IPC_BP_REJECT = 3       /**< > 90% 拒绝新连接 */
} ipc_bp_level_t;

/**
 * @brief 背压配置
 */
typedef struct {
    size_t queue_capacity;       /**< 队列容量（消息数） */
    uint32_t slow_threshold_pct; /**< 降速阈值百分比（默认 80） */
    uint32_t drop_threshold_pct; /**< 丢弃阈值百分比（默认 90） */
    uint32_t reject_threshold_pct;/**< 拒绝阈值百分比（默认 95） */
    uint32_t recover_threshold_pct;/**< 恢复阈值百分比（默认 60） */
    uint32_t sample_interval_ms; /**< 采样间隔（默认 5000） */
} ipc_bp_config_t;

/**
 * @brief 背压统计
 */
typedef struct {
    ipc_bp_level_t current_level; /**< 当前背压级别 */
    size_t queue_depth;           /**< 当前队列深度 */
    size_t queue_capacity;        /**< 队列容量 */
    uint64_t total_sent;          /**< 总发送数 */
    uint64_t total_dropped;       /**< 总丢弃数 */
    uint64_t total_rejected;      /**< 总拒绝数 */
    uint64_t slow_down_events;    /**< 降速事件数 */
    uint64_t recover_events;      /**< 恢复事件数 */
} ipc_bp_stats_t;

/**
 * @brief 背压控制器句柄
 */
typedef struct ipc_bp_controller ipc_bp_controller_t;

/**
 * @brief 创建背压控制器
 *
 * @param config 配置（NULL 使用默认）
 * @return 控制器句柄，失败返回 NULL
 */
ipc_bp_controller_t *ipc_bp_create(const ipc_bp_config_t *config);

/**
 * @brief 销毁背压控制器
 */
void ipc_bp_destroy(ipc_bp_controller_t *ctrl);

/**
 * @brief 更新队列深度并评估背压级别
 *
 * 每 5s 采样一次，根据队列深度计算背压级别。
 *
 * @param ctrl 控制器
 * @param current_depth 当前队列深度
 * @return 当前背压级别
 */
ipc_bp_level_t ipc_bp_update(ipc_bp_controller_t *ctrl, size_t current_depth);

/**
 * @brief 检查消息是否应被发送
 *
 * @param ctrl 控制器
 * @param is_droppable 消息是否可丢弃（日志/指标等低优先级）
 * @return true 允许发送，false 应丢弃/拒绝
 */
bool ipc_bp_should_send(ipc_bp_controller_t *ctrl, bool is_droppable);

/**
 * @brief 检查是否应接受新连接
 *
 * @param ctrl 控制器
 * @return true 接受，false 拒绝
 */
bool ipc_bp_should_accept_connection(ipc_bp_controller_t *ctrl);

/**
 * @brief 获取背压统计
 */
void ipc_bp_get_stats(ipc_bp_controller_t *ctrl, ipc_bp_stats_t *out_stats);

/**
 * @brief 获取当前背压级别
 */
ipc_bp_level_t ipc_bp_get_level(ipc_bp_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_BACKPRESSURE_H */
