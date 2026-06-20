/**
 * @file hook_timeout.h
 * @brief P2.1.4: Hook 执行超时保护
 *
 * 为 Hook 执行提供超时保护机制，防止恶意或异常的 Hook
 * 回调阻塞整个 Hook 链。
 *
 * 机制：
 *   - 每个 Hook 回调设置最大执行时间（默认 500ms）
 *   - 超时后强制终止 Hook 线程，返回 HOOK_DECISION_ABORT
 *   - 支持为单个 Hook 设置自定义超时
 *   - 超时记录到统计信息
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_HOOK_TIMEOUT_H
#define AGENTOS_HOOK_TIMEOUT_H

#include "hook_registry.h"
#include "hook_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define HOOK_TIMEOUT_DEFAULT_MS      500    /**< 默认超时（毫秒） */
#define HOOK_TIMEOUT_MIN_MS          10     /**< 最小超时 */
#define HOOK_TIMEOUT_MAX_MS          30000  /**< 最大超时（30秒） */
#define HOOK_TIMEOUT_MAX_NAME_LEN    128    /**< 超时条目名称最大长度 */

/* ==================== 超时配置 ==================== */

/**
 * @brief 单个 Hook 超时配置
 */
typedef struct {
    char hook_name[HOOK_TIMEOUT_MAX_NAME_LEN]; /**< Hook 名称 */
    uint32_t timeout_ms;                       /**< 超时时间（毫秒） */
    uint64_t timeout_count;                    /**< 超时次数（统计） */
    bool enabled;                              /**< 是否启用超时 */
} hook_timeout_entry_t;

/* ==================== 超时管理器 API ==================== */

/**
 * @brief 初始化超时管理器
 * @param default_timeout_ms 默认超时（毫秒，0 使用默认 500ms）
 * @return 0 成功，非0 失败
 */
int hook_timeout_manager_init(uint32_t default_timeout_ms);

/**
 * @brief 销毁超时管理器
 */
void hook_timeout_manager_destroy(void);

/**
 * @brief 设置指定 Hook 的超时时间
 *
 * @param hook_name  Hook 名称
 * @param timeout_ms 超时时间（毫秒，0 使用默认值）
 * @return 0 成功，-1 失败
 */
int hook_timeout_set(const char *hook_name, uint32_t timeout_ms);

/**
 * @brief 获取指定 Hook 的超时时间
 *
 * @param hook_name Hook 名称
 * @return 超时时间（毫秒），未找到返回默认超时
 */
uint32_t hook_timeout_get(const char *hook_name);

/**
 * @brief 执行带超时保护的 Hook 回调
 *
 * 在独立线程中运行回调，超时后强制终止。
 *
 * @param entry      Hook 条目
 * @param ctx        Hook 上下文
 * @param timeout_ms 超时时间（毫秒，0 使用 Hook 配置的超时）
 * @param out_duration_ns 输出实际执行耗时（纳秒）
 * @return Hook 决策（超时时返回 HOOK_DECISION_ABORT）
 */
hook_decision_t hook_timeout_run(const hook_entry_t *entry,
                                  hook_context_t *ctx,
                                  uint32_t timeout_ms,
                                  uint64_t *out_duration_ns);

/**
 * @brief 获取超时发生的次数
 *
 * @param hook_name Hook 名称
 * @return 超时次数，-1 表示未找到
 */
int hook_timeout_get_count(const char *hook_name);

/**
 * @brief 重置指定 Hook 的超时计数器
 *
 * @param hook_name Hook 名称
 * @return 0 成功，-1 未找到
 */
int hook_timeout_reset_count(const char *hook_name);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HOOK_TIMEOUT_H */