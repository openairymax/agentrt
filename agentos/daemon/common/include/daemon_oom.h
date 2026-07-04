/**
 * @file daemon_oom.h
 * @brief Daemon OOM 降级回调注册辅助
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.22: 为每个 daemon 提供标准化的 OOM 降级回调。
 * WARNING 级丢弃缓存，CRITICAL 级拒绝请求。
 */

#ifndef AGENTOS_DAEMON_OOM_H
#define AGENTOS_DAEMON_OOM_H

#include "oom_handler.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Daemon OOM 降级配置
 */
typedef struct {
    const char *daemon_name;         /**< Daemon 名称 */
    bool drop_cache_on_warning;      /**< WARNING 级是否丢弃缓存 */
    bool reject_requests_on_critical;/**< CRITICAL 级是否拒绝请求 */
    void *user_context;              /**< 用户上下文（传给回调） */
} daemon_oom_config_t;

/**
 * @brief 为 daemon 注册标准 OOM 降级回调
 *
 * 注册 WARNING 和 CRITICAL 两个级别的回调：
 *   - WARNING:  丢弃缓存、降低日志级别
 *   - CRITICAL: 拒绝新请求、暂停非关键功能
 *
 * @param config 配置
 * @return 0 成功，非0 失败
 */
int daemon_oom_register(const daemon_oom_config_t *config);

/**
 * @brief 注销 daemon 的 OOM 降级回调
 *
 * @param daemon_name Daemon 名称
 */
void daemon_oom_unregister(const char *daemon_name);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_OOM_H */
