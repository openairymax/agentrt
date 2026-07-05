/**
 * @file tool_approval.h
 * @brief C-L05: Cupolas SafetyGuard → tool_d 工具审批适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 在工具执行前通过 Cupolas 安全穹顶进行权限检查和参数净化。
 * 集成点位于 executor.c 的 tool_executor_run() 中。
 *
 * 审批流程：
 *   1. 参数净化 (daemon_sanitize_tool_params)
 *   2. 权限检查 (daemon_check_tool_permission)
 *   3. SafetyGuard 链式检查 (safety_guard_check_chain, 可选)
 *   4. 审计记录 (daemon_audit_log_event)
 */

#ifndef AGENTRT_TOOL_APPROVAL_H
#define AGENTRT_TOOL_APPROVAL_H

#include "tool_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 前向声明 */
typedef struct safety_guard_bridge_s safety_guard_bridge_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 工具审批结果
 */
typedef enum {
    TOOL_APPROVAL_ALLOWED = 0,     /**< 批准执行 */
    TOOL_APPROVAL_DENIED,          /**< 拒绝执行 */
    TOOL_APPROVAL_SANITIZED,       /**< 已净化参数，批准执行 */
    TOOL_APPROVAL_PENDING_AUDIT    /**< 需人工审核 */
} tool_approval_result_t;

/**
 * @brief 工具审批上下文
 */
typedef struct tool_approval_ctx tool_approval_ctx_t;

/**
 * @brief 工具审批配置
 */
typedef struct {
    const char *agent_id;            /**< 请求 Agent ID */
    bool enable_safety_guard_chain;  /**< 是否启用 SafetyGuard 链式检查 */
    bool enable_audit_logging;       /**< 是否记录审计日志 */
    const char *permission_rules;    /**< 权限规则 JSON（NULL 使用默认） */
} tool_approval_config_t;

/**
 * @brief 审批详细结果
 */
typedef struct {
    tool_approval_result_t decision;        /**< 审批决定 */
    char reason[256];                       /**< 审批原因 */
    char sanitized_params[4096];            /**< 净化后的参数 */
    int permission_check_passed;            /**< 权限检查是否通过 */
    int safety_guard_passed;                /**< SafetyGuard 是否通过 */
    int params_were_sanitized;              /**< 参数是否被修改 */
} tool_approval_detail_t;

/* ── 生命周期 ── */

/**
 * @brief 创建工具审批上下文
 * @param cfg 审批配置（NULL 使用默认）
 * @return 审批上下文，失败返回 NULL
 *
 * @ownership return: OWNER
 */
tool_approval_ctx_t *tool_approval_create(const tool_approval_config_t *cfg);

/**
 * @brief 销毁工具审批上下文
 * @param ctx 审批上下文
 *
 * @ownership ctx: TRANSFER
 */
void tool_approval_destroy(tool_approval_ctx_t *ctx);

/* ── 审批接口 ── */

/**
 * @brief C-L05: 审批工具执行请求
 *
 * 在工具执行前调用此函数，依次执行：
 *   1. 参数净化检查
 *   2. 权限检查（调用 daemon_check_tool_permission）
 *   3. SafetyGuard 链式检查（可选，调用 safety_guard_check_chain）
 *   4. 审计日志记录
 *
 * @param ctx 审批上下文
 * @param meta 工具元数据
 * @param params_json 原始参数 JSON
 * @param detail 输出审批详情
 * @return 0 成功（批准），非0 拒绝
 *
 * @ownership ctx: BORROW, meta: BORROW, params_json: BORROW, detail: BORROW
 */
int tool_approval_check(tool_approval_ctx_t *ctx, const tool_metadata_t *meta,
                        const char *params_json, tool_approval_detail_t *detail);

/**
 * @brief 仅执行参数净化（不检查权限）
 *
 * @param ctx 审批上下文
 * @param tool_name 工具名称
 * @param params_json 原始参数
 * @param sanitized_params 输出净化后参数
 * @param sanitized_size 输出缓冲区大小
 * @return 0 成功，非0 失败
 *
 * @ownership ctx: BORROW, tool_name: BORROW, params_json: BORROW
 * @ownership sanitized_params: caller provides buffer
 */
int tool_approval_sanitize_params(tool_approval_ctx_t *ctx, const char *tool_name,
                                  const char *params_json, char *sanitized_params,
                                  size_t sanitized_size);

/**
 * @brief 获取最后一次审批的统计信息
 *
 * @param ctx 审批上下文
 * @param out_total_checks 输出总检查次数
 * @param out_denied_count 输出拒绝次数
 * @param out_sanitized_count 输出净化次数
 *
 * @ownership ctx: BORROW
 */
void tool_approval_get_stats(tool_approval_ctx_t *ctx, uint64_t *out_total_checks,
                             uint64_t *out_denied_count, uint64_t *out_sanitized_count);

/**
 * @brief C-L05: 设置 SafetyGuard 桥接层
 *
 * 将 safety_guard_bridge 注入到审批上下文中，使 tool_approval_check()
 * 能够通过完整的 6 种守卫链进行安全检查。
 *
 * @param ctx 审批上下文
 * @param bridge SafetyGuard 桥接句柄（NULL 禁用桥接）
 *
 * @ownership ctx: BORROW, bridge: BORROW (caller retains ownership)
 */
void tool_approval_set_safety_guard_bridge(tool_approval_ctx_t *ctx,
                                           safety_guard_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_TOOL_APPROVAL_H */