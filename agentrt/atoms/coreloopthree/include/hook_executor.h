/**
 * @file hook_executor.h
 * @brief P2.1.2: Hook 执行器 — 顺序/并行执行 Hook 链
 *
 * 支持：
 *   - 顺序执行：按优先级递减顺序逐个执行 Hook
 *   - 并行执行：同优先级 Hook 并发执行，不同优先级顺序执行
 *   - 回调执行：C 函数回调
 *   - Shell 执行：fork + exec shell 脚本
 *   - 决策聚合：ABORT > RETRY > MODIFY > SKIP > CONTINUE
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTRT_HOOK_EXECUTOR_H
#define AGENTRT_HOOK_EXECUTOR_H

#include "hook_registry.h"
#include "hook_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 执行模式 ==================== */

typedef enum {
    HOOK_EXEC_MODE_SEQUENTIAL = 0,  /**< 顺序执行（默认） */
    HOOK_EXEC_MODE_PARALLEL   = 1,  /**< 并行执行（同优先级并发） */
} hook_exec_mode_t;

/* ==================== 执行器 API ==================== */

/**
 * @brief 执行指定类型的 Hook 链
 *
 * 从注册表中获取指定类型的所有已启用 Hook，按优先级降序执行。
 * 决策聚合规则：最严格的决策优先。
 *
 * @param ctx  Hook 上下文
 * @param mode 执行模式
 * @return 聚合决策
 */
hook_decision_t hook_executor_run(hook_context_t *ctx, hook_exec_mode_t mode);

/**
 * @brief 执行单个 Hook 条目
 *
 * 根据 Hook 实现类型（CALLBACK/SHELL/PYTHON/WEBHOOK）分发执行。
 *
 * @param entry Hook 条目
 * @param ctx   Hook 上下文
 * @param out_duration_ns 输出执行耗时（纳秒）
 * @return Hook 决策
 */
hook_decision_t hook_executor_run_one(const hook_entry_t *entry,
                                       hook_context_t *ctx,
                                       uint64_t *out_duration_ns);

/**
 * @brief 执行 Shell 脚本 Hook
 *
 * @param script_path 脚本路径
 * @param ctx         Hook 上下文（序列化为 JSON 环境变量传入）
 * @return 脚本退出码，0=CONTINUE, 1=SKIP, 2=RETRY, 3=ABORT, 4=MODIFY
 */
int hook_executor_run_shell(const char *script_path, hook_context_t *ctx);

/**
 * @brief 执行 Webhook（HTTP POST）
 *
 * @param url Webhook URL
 * @param ctx Hook 上下文（JSON 序列化到 POST body）
 * @return 0 成功，-1 失败
 */
int hook_executor_run_webhook(const char *url, hook_context_t *ctx);

/**
 * @brief 将 Hook 上下文序列化为 JSON 字符串
 *
 * @param ctx    Hook 上下文
 * @param buf    输出缓冲区
 * @param bufsize 缓冲区大小
 * @return 写入的字节数（不含 null 终止符），-1 表示截断
 */
int hook_executor_ctx_to_json(const hook_context_t *ctx, char *buf, size_t bufsize);

/**
 * @brief 聚合 Hook 决策
 *
 * 当前决策 vs 新决策，返回更严格的。
 *
 * @param current 当前决策
 * @param new_dec 新决策
 * @return 聚合后的决策
 */
hook_decision_t hook_executor_merge_decision(hook_decision_t current,
                                              hook_decision_t new_dec);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_HOOK_EXECUTOR_H */