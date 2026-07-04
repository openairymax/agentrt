/**
 * @file hook_interceptor.h
 * @brief P2.1.3 / P2.1a: 拦截型 Hook — SafetyGuard 安全链集成
 *
 * 在 PRE_TOOL / PRE_EXEC 等 Hook 点触发 SafetyGuard 检查链。
 * 将 Hook 上下文转换为 SafetyGuard 事件，通过 safety_guard_check_chain
 * 执行安全检查，将结果映射回 Hook 决策。
 *
 * 集成模式：
 *   Hook 触发 → hook_interceptor_check() → SafetyGuard → 决策映射
 *
 * 安全决策映射：
 *   ALLOW       → CONTINUE
 *   DENY        → ABORT
 *   CONDITIONAL → MODIFY（附带修改后的上下文）
 *   DEFER       → SKIP
 *   ABORT       → ABORT
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTOS_HOOK_INTERCEPTOR_H
#define AGENTOS_HOOK_INTERCEPTOR_H

#include "hook_service.h"
#include "safety_guard.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 拦截器配置 ==================== */

/**
 * @brief 拦截器配置
 */
typedef struct {
    bool enable_safety_guard;        /**< 是否启用 SafetyGuard 检查 */
    bool enable_permission_check;    /**< 是否启用权限检查 */
    bool enable_audit_log;           /**< 是否启用审计日志 */
    bool enable_parameter_sanitize;  /**< 是否启用参数净化 */
    uint32_t max_guard_timeout_ms;   /**< SafetyGuard 最大超时（毫秒） */
} hook_interceptor_config_t;

/* ==================== 拦截器 API ==================== */

/**
 * @brief 初始化拦截器
 *
 * @param config 配置（NULL 使用默认配置）
 * @return 0 成功，非0 失败
 */
int hook_interceptor_init(const hook_interceptor_config_t *config);

/**
 * @brief 销毁拦截器
 */
void hook_interceptor_destroy(void);

/**
 * @brief 执行拦截检查
 *
 * 将 Hook 上下文作为安全检查事件，通过 SafetyGuard 链执行检查。
 * 仅在 enable_safety_guard 为 true 时生效。
 *
 * @param ctx Hook 上下文
 * @return Hook 决策
 *
 * @note 此函数应在 PRE_TOOL/PRE_EXEC Hook 触发时调用
 */
hook_decision_t hook_interceptor_check(hook_context_t *ctx);

/**
 * @brief 将 Hook 上下文映射为 SafetyGuard 事件
 *
 * @param ctx   Hook 上下文
 * @param event 输出 SafetyGuard 事件
 * @return 0 成功，-1 失败
 */
int hook_interceptor_ctx_to_safety_event(const hook_context_t *ctx,
                                          safety_event_t *event);

/**
 * @brief 将 SafetyGuard 决策映射为 Hook 决策
 *
 * @param sg_decision SafetyGuard 决策
 * @return Hook 决策
 */
hook_decision_t hook_interceptor_map_decision(safety_decision_t sg_decision);

/**
 * @brief 获取拦截器配置
 *
 * @param config 输出配置
 * @return 0 成功，-1 未初始化
 */
int hook_interceptor_get_config(hook_interceptor_config_t *config);

/**
 * @brief 更新拦截器配置
 *
 * @param config 新配置
 * @return 0 成功，-1 未初始化
 */
int hook_interceptor_set_config(const hook_interceptor_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HOOK_INTERCEPTOR_H */