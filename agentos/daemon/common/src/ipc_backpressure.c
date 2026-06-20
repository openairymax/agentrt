/**
 * @file ipc_backpressure.c
 * @brief IPC Bus 背压控制实现 — 三级策略
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * P1.24: 三级背压策略防止 IPC Bus 过载。
 */

#include "ipc_backpressure.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 默认配置 */
#define BP_DEFAULT_QUEUE_CAPACITY    10000
#define BP_DEFAULT_SLOW_PCT          80
#define BP_DEFAULT_DROP_PCT           90
#define BP_DEFAULT_REJECT_PCT         95
#define BP_DEFAULT_RECOVER_PCT        60
#define BP_DEFAULT_SAMPLE_MS          5000

/* ==================== 内部结构 ==================== */

struct ipc_bp_controller {
    ipc_bp_config_t config;
    ipc_bp_stats_t stats;
    uint64_t last_sample_time_ms;   /**< 上次采样时间 */
};

/* ==================== 辅助函数 ==================== */

static uint64_t current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static ipc_bp_level_t compute_level(size_t depth, size_t capacity,
                                     const ipc_bp_config_t *cfg)
{
    if (capacity == 0)
        return IPC_BP_NORMAL;

    uint32_t pct = (uint32_t)((depth * 100) / capacity);

    if (pct >= cfg->reject_threshold_pct)
        return IPC_BP_REJECT;
    if (pct >= cfg->drop_threshold_pct)
        return IPC_BP_DROP;
    if (pct >= cfg->slow_threshold_pct)
        return IPC_BP_SLOW;
    return IPC_BP_NORMAL;
}

/* ==================== 公共 API ==================== */

ipc_bp_controller_t *ipc_bp_create(const ipc_bp_config_t *config)
{
    ipc_bp_controller_t *ctrl = (ipc_bp_controller_t *)AGENTOS_CALLOC(1, sizeof(*ctrl));
    if (!ctrl) {
        SVC_LOG_ERROR("C-L02: BP: CREATE-FAIL reason=oom");
        return NULL;
    }

    if (config) {
        ctrl->config = *config;
    } else {
        ctrl->config.queue_capacity = BP_DEFAULT_QUEUE_CAPACITY;
        ctrl->config.slow_threshold_pct = BP_DEFAULT_SLOW_PCT;
        ctrl->config.drop_threshold_pct = BP_DEFAULT_DROP_PCT;
        ctrl->config.reject_threshold_pct = BP_DEFAULT_REJECT_PCT;
        ctrl->config.recover_threshold_pct = BP_DEFAULT_RECOVER_PCT;
        ctrl->config.sample_interval_ms = BP_DEFAULT_SAMPLE_MS;
    }

    if (ctrl->config.queue_capacity == 0)
        ctrl->config.queue_capacity = BP_DEFAULT_QUEUE_CAPACITY;

    ctrl->stats.current_level = IPC_BP_NORMAL;
    ctrl->stats.queue_capacity = ctrl->config.queue_capacity;
    ctrl->last_sample_time_ms = current_time_ms();

    SVC_LOG_INFO("P1.24: IPC backpressure created (capacity=%zu, slow=%u%%, drop=%u%%, reject=%u%%, recover=%u%%)",
                 ctrl->config.queue_capacity,
                 ctrl->config.slow_threshold_pct,
                 ctrl->config.drop_threshold_pct,
                 ctrl->config.reject_threshold_pct,
                 ctrl->config.recover_threshold_pct);

    return ctrl;
}

void ipc_bp_destroy(ipc_bp_controller_t *ctrl)
{
    if (!ctrl)
        return;

    SVC_LOG_INFO("P1.24: IPC backpressure destroyed (sent=%zu, dropped=%zu, rejected=%zu)",
                 (size_t)ctrl->stats.total_sent,
                 (size_t)ctrl->stats.total_dropped,
                 (size_t)ctrl->stats.total_rejected);
    AGENTOS_FREE(ctrl);
}

ipc_bp_level_t ipc_bp_update(ipc_bp_controller_t *ctrl, size_t current_depth)
{
    if (!ctrl)
        return IPC_BP_NORMAL;

    ctrl->stats.queue_depth = current_depth;

    ipc_bp_level_t old_level = ctrl->stats.current_level;
    ipc_bp_level_t new_level = compute_level(current_depth,
                                               ctrl->config.queue_capacity,
                                               &ctrl->config);

    /* 恢复逻辑：只有低于恢复阈值才降级 */
    if (new_level < old_level) {
        uint32_t pct = (uint32_t)((current_depth * 100) / ctrl->config.queue_capacity);
        if (pct > ctrl->config.recover_threshold_pct) {
            /* 尚未达到恢复阈值，保持当前级别 */
            new_level = old_level;
        }
    }

    if (new_level != old_level) {
        ctrl->stats.current_level = new_level;

        static const char *level_names[] = {"NORMAL", "SLOW", "DROP", "REJECT"};

        if (new_level > old_level) {
            ctrl->stats.slow_down_events++;
            SVC_LOG_WARN("P1.24: IPC backpressure %s → %s (depth=%zu/%zu, %.1f%%)",
                         level_names[old_level], level_names[new_level],
                         current_depth, ctrl->config.queue_capacity,
                         ctrl->config.queue_capacity > 0 ?
                         (double)current_depth * 100.0 / ctrl->config.queue_capacity : 0.0);

            if (new_level == IPC_BP_REJECT) {
                SVC_LOG_ERROR("C-L02: BP: UPDATE level=REJECT depth=%zu/%zu sent=%zu dropped=%zu rejected=%zu",
                              current_depth, ctrl->config.queue_capacity,
                              (size_t)ctrl->stats.total_sent,
                              (size_t)ctrl->stats.total_dropped,
                              (size_t)ctrl->stats.total_rejected);
            }
        } else {
            ctrl->stats.recover_events++;
            SVC_LOG_INFO("P1.24: IPC backpressure %s → %s (depth=%zu/%zu, %.1f%%)",
                         level_names[old_level], level_names[new_level],
                         current_depth, ctrl->config.queue_capacity,
                         ctrl->config.queue_capacity > 0 ?
                         (double)current_depth * 100.0 / ctrl->config.queue_capacity : 0.0);
        }
    }

    return new_level;
}

bool ipc_bp_should_send(ipc_bp_controller_t *ctrl, bool is_droppable)
{
    if (!ctrl)
        return true;

    switch (ctrl->stats.current_level) {
    case IPC_BP_NORMAL:
        ctrl->stats.total_sent++;
        return true;

    case IPC_BP_SLOW:
        /* 降速：允许发送但建议减少频率 */
        ctrl->stats.total_sent++;
        return true;

    case IPC_BP_DROP:
        /* 丢弃可丢弃消息 */
        if (is_droppable) {
            ctrl->stats.total_dropped++;
            SVC_LOG_WARN("C-L02: BP: DROP level=DROP depth=%zu/%zu sent=%zu dropped=%zu",
                         (size_t)ctrl->stats.queue_depth, ctrl->stats.queue_capacity,
                         (size_t)ctrl->stats.total_sent,
                         (size_t)ctrl->stats.total_dropped);
            return false;
        }
        ctrl->stats.total_sent++;
        return true;

    case IPC_BP_REJECT:
        /* 拒绝所有非关键消息 */
        if (is_droppable) {
            ctrl->stats.total_dropped++;
            SVC_LOG_WARN("C-L02: BP: DROP level=REJECT depth=%zu/%zu sent=%zu dropped=%zu rejected=%zu",
                         (size_t)ctrl->stats.queue_depth, ctrl->stats.queue_capacity,
                         (size_t)ctrl->stats.total_sent,
                         (size_t)ctrl->stats.total_dropped,
                         (size_t)ctrl->stats.total_rejected);
            return false;
        }
        /* 关键消息仍然允许，但记录警告 */
        ctrl->stats.total_sent++;
        return true;
    }

    return true;
}

bool ipc_bp_should_accept_connection(ipc_bp_controller_t *ctrl)
{
    if (!ctrl)
        return true;

    if (ctrl->stats.current_level >= IPC_BP_REJECT) {
        ctrl->stats.total_rejected++;
        SVC_LOG_WARN("C-L02: BP: REJECT-ACCEPT level=%d depth=%zu/%zu rejected=%zu",
                     (int)ctrl->stats.current_level,
                     (size_t)ctrl->stats.queue_depth, ctrl->stats.queue_capacity,
                     (size_t)ctrl->stats.total_rejected);
        return false;
    }

    return true;
}

void ipc_bp_get_stats(ipc_bp_controller_t *ctrl, ipc_bp_stats_t *out_stats)
{
    if (!ctrl || !out_stats)
        return;
    *out_stats = ctrl->stats;
}

ipc_bp_level_t ipc_bp_get_level(ipc_bp_controller_t *ctrl)
{
    if (!ctrl)
        return IPC_BP_NORMAL;
    return ctrl->stats.current_level;
}
