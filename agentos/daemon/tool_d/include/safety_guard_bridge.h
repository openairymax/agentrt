/**
 * @file safety_guard_bridge.h
 * @brief C-L05: Cupolas SafetyGuard → tool_d 桥接层
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将 Cupolas 安全穹顶的 safety_guard_check_chain() API
 * 桥接到 tool_d 的工具审批流程中，实现 6 种守卫类型的
 * 权限检查、速率限制、内容过滤等安全控制。
 *
 * 守卫类型映射：
 *   SAFETY_GUARD_PERMISSION   → RBAC 权限检查
 *   SAFETY_GUARD_RATE_LIMIT   → 工具调用频率限制
 *   SAFETY_GUARD_CONTENT_FILTER → 输入内容过滤
 *   SAFETY_GUARD_INPUT        → 参数净化
 *   SAFETY_GUARD_RESOURCE     → 资源配额检查
 *   SAFETY_GUARD_AUDIT        → 审计日志记录
 */

#ifndef AGENTOS_SAFETY_GUARD_BRIDGE_H
#define AGENTOS_SAFETY_GUARD_BRIDGE_H

#include "tool_approval.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 桥接层句柄 ==================== */

typedef struct safety_guard_bridge_s safety_guard_bridge_t;

/* ==================== 桥接层配置 ==================== */

typedef struct {
    bool enable_permission_guard;    /**< 启用权限守卫 */
    bool enable_rate_limit_guard;    /**< 启用速率限制守卫 */
    bool enable_content_filter;      /**< 启用内容过滤守卫 */
    bool enable_input_sanitization;  /**< 启用输入净化守卫 */
    bool enable_resource_quota;      /**< 启用资源配额守卫 */
    bool enable_audit_guard;         /**< 启用审计守卫 */
    uint32_t rate_limit_per_minute;  /**< 每分钟最大调用次数，0=无限制 */
    uint32_t max_params_size;        /**< 最大参数大小（字节），0=无限制 */
    const char *denied_patterns;     /**< 禁止的参数模式（逗号分隔），NULL=无 */
    const char *agent_id;            /**< Agent ID */
} safety_guard_bridge_config_t;

/* ==================== 守卫检查结果 ==================== */

typedef struct {
    int permission_passed;           /**< 权限检查是否通过 */
    int rate_limit_passed;           /**< 速率限制是否通过 */
    int content_filter_passed;       /**< 内容过滤是否通过 */
    int input_sanitized;             /**< 输入是否被净化 */
    int resource_quota_passed;       /**< 资源配额是否通过 */
    int audit_recorded;              /**< 审计日志是否已记录 */
    char denial_reason[256];         /**< 拒绝原因 */
    char sanitized_params[4096];     /**< 净化后的参数 */
    int guard_chain_length;          /**< 守卫链长度 */
    int guards_executed;             /**< 实际执行的守卫数 */
} safety_guard_bridge_result_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建 SafetyGuard 桥接层
 * @param config 桥接配置（NULL 使用默认：所有守卫启用）
 * @return 桥接句柄，失败返回 NULL
 * @ownership return: OWNER
 */
safety_guard_bridge_t *safety_guard_bridge_create(const safety_guard_bridge_config_t *config);

/**
 * @brief 销毁 SafetyGuard 桥接层
 * @param bridge 桥接句柄
 * @ownership bridge: TRANSFER
 */
void safety_guard_bridge_destroy(safety_guard_bridge_t *bridge);

/* ==================== 守卫检查 ==================== */

/**
 * @brief C-L05: 执行完整的 SafetyGuard 守卫链检查
 *
 * 依次执行 6 种守卫类型：
 *   1. SAFETY_GUARD_PERMISSION   → RBAC 权限检查
 *   2. SAFETY_GUARD_RATE_LIMIT   → 频率限制
 *   3. SAFETY_GUARD_CONTENT_FILTER → 内容过滤
 *   4. SAFETY_GUARD_INPUT        → 参数净化
 *   5. SAFETY_GUARD_RESOURCE     → 资源配额
 *   6. SAFETY_GUARD_AUDIT        → 审计日志
 *
 * 任一守卫返回 DENY → 立即终止并返回拒绝
 *
 * @param bridge 桥接句柄
 * @param meta 工具元数据
 * @param params_json 原始参数 JSON
 * @param result 输出检查结果
 * @return 0 全部通过，非0 被拒绝
 * @ownership bridge: BORROW, meta: BORROW, params_json: BORROW, result: BORROW
 */
int safety_guard_bridge_check(safety_guard_bridge_t *bridge,
                              const tool_metadata_t *meta,
                              const char *params_json,
                              safety_guard_bridge_result_t *result);

/**
 * @brief 仅执行权限守卫检查
 * @param bridge 桥接句柄
 * @param agent_id Agent ID
 * @param tool_name 工具名称
 * @param action 操作（"execute"/"register"/"list"）
 * @return 0 通过，非0 拒绝
 */
int safety_guard_bridge_check_permission(safety_guard_bridge_t *bridge,
                                         const char *agent_id,
                                         const char *tool_name,
                                         const char *action);

/**
 * @brief 仅执行速率限制检查
 * @param bridge 桥接句柄
 * @param tool_name 工具名称
 * @return 0 通过，非0 超出限制
 */
int safety_guard_bridge_check_rate_limit(safety_guard_bridge_t *bridge,
                                         const char *tool_name);

/**
 * @brief 仅执行内容过滤检查
 * @param bridge 桥接句柄
 * @param params_json 参数 JSON
 * @param sanitized_params 输出净化后参数
 * @param sanitized_size 输出缓冲区大小
 * @return 0 通过，非0 被过滤
 */
int safety_guard_bridge_filter_content(safety_guard_bridge_t *bridge,
                                       const char *params_json,
                                       char *sanitized_params,
                                       size_t sanitized_size);

/* ==================== 审计日志 ==================== */

/**
 * @brief 记录审计日志事件
 * @param bridge 桥接句柄
 * @param event_type 事件类型
 * @param tool_name 工具名称
 * @param decision 决策结果
 * @param reason 原因
 * @param agent_id Agent ID
 * @return 0 成功
 */
int safety_guard_bridge_audit_log(safety_guard_bridge_t *bridge,
                                  const char *event_type,
                                  const char *tool_name,
                                  int decision,
                                  const char *reason,
                                  const char *agent_id);

/**
 * @brief 获取桥接层统计信息
 * @param bridge 桥接句柄
 * @param out_total_checks 输出总检查次数
 * @param out_denied_count 输出拒绝次数
 * @param out_rate_limited 输出速率限制次数
 */
void safety_guard_bridge_get_stats(safety_guard_bridge_t *bridge,
                                   uint64_t *out_total_checks,
                                   uint64_t *out_denied_count,
                                   uint64_t *out_rate_limited);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SAFETY_GUARD_BRIDGE_H */