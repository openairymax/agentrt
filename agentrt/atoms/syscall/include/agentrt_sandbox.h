/**
 * @file agentrt_sandbox.h
 * @brief 安全沙箱公共 API（P3.18 / ACC-DT27）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 安全沙箱提供系统调用的隔离执行环境，所有 syscall 经 permission_check →
 * quota_check → audit_log 三层拦截后才实际执行。
 *
 * 典型用法（tool_d 执行工具）：
 * ```c
 * agentrt_sandbox_manager_init();           // 幂等，进程级初始化
 * agentrt_sandbox_t *sb = NULL;
 * agentrt_sandbox_create_default("tool_d", &sb);  // 默认配置
 * agentrt_sandbox_add_rule(sb, SYS_TOOL_EXECUTE, PERM_ALLOW, NULL);
 * void *args[1] = { &tool_args };
 * void *out = NULL;
 * agentrt_error_t rc = agentrt_sandbox_invoke(sb, SYS_TOOL_EXECUTE, args, 1, &out);
 * if (rc != AGENTRT_SUCCESS) { // fail-closed: 拒绝执行
 * }
 * agentrt_sandbox_destroy(sb);
 * agentrt_sandbox_manager_destroy();
 * ```
 *
 * 注意：上方代码块仅作示意，实际 fail-closed 逻辑由调用方实现。
 * 线程安全：所有 API 均线程安全（内部加锁）。sandbox 句柄可在多线程间
 * 共享，但同一 sandbox 的并发 invoke 会串行化。
 */

#ifndef AGENTRT_SANDBOX_H
#define AGENTRT_SANDBOX_H

#include "agentrt.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */

/**
 * @brief 沙箱不透明句柄
 * @details 完整定义在 sandbox_internal.h（仅供 sandbox.c / sandbox_permission.c /
 * sandbox_quota.c / sandbox_utils.c 等内部模块使用）。外部调用方通过此不透明
 * 指针操作沙箱，无法直接访问内部字段，保证封装安全性。
 */
typedef struct agentrt_sandbox agentrt_sandbox_t;

/**
 * @brief 权限类型枚举
 * @details 控制 syscall 在沙箱中的执行许可。sandbox_permission_check 遍历
 * 规则链表时，PERM_DENY 优先级最高（立即拒绝），PERM_ASK 次之，
 * 无匹配规则时默认 PERM_ALLOW。
 *
 * @note 此枚举原定义在 sandbox_permission.h（内部头），P3.18 提升为公共 API
 * 以便外部调用方（如 tool_d）通过 agentrt_sandbox_add_rule 配置权限。
 * sandbox_permission.h 通过包含本头获取此类型定义。
 */
typedef enum {
    PERM_ALLOW = 0, /**< 允许执行 */
    PERM_DENY,      /**< 拒绝执行（fail-closed） */
    PERM_ASK        /**< 需用户确认（当前实现等同 ALLOW，预留扩展） */
} permission_type_t;

/* ==================== 管理器生命周期 ==================== */

/**
 * @brief 初始化沙箱管理器（进程级，幂等）
 * @return AGENTRT_SUCCESS 成功（含重复初始化）；AGENTRT_ENOMEM 内存不足
 * @note 必须在 agentrt_sandbox_create_default 之前调用。重复调用安全。
 */
AGENTRT_API agentrt_error_t agentrt_sandbox_manager_init(void);

/**
 * @brief 销毁沙箱管理器，释放所有受管沙箱
 * @note 销毁后所有未释放的 sandbox 句柄变为无效，不得再使用。
 */
AGENTRT_API void agentrt_sandbox_manager_destroy(void);

/* ==================== 沙箱生命周期 ==================== */

/**
 * @brief 使用默认配置创建沙箱
 * @param name 沙箱名称（可为 NULL，内部会 strdup；用于审计日志标识）
 * @param owner_id 所有者 ID（可为 NULL，如 "tool_d"）
 * @param out_sandbox 输出沙箱句柄
 * @return AGENTRT_SUCCESS 成功；AGENTRT_ENOTINIT 管理器未初始化；
 *         AGENTRT_EBUSY 无可用槽位；AGENTRT_ENOMEM 内存不足
 *
 * @details 默认配置：
 * - priority: 0（普通）
 * - timeout_ms: 30000（30 秒，与 tool_d 默认一致）
 * - flags: 0
 * - quota: sandbox_quota_init 的默认值
 * - 启用输入净化和资源监控
 *
 * @note 此函数是 agentrt_sandbox_create(config, out) 的便捷封装，
 *       内部构造 sandbox_config_t 后调用真实 create。非桩函数，功能完整。
 */
AGENTRT_API agentrt_error_t agentrt_sandbox_create_default(const char *name,
                                                            const char *owner_id,
                                                            agentrt_sandbox_t **out_sandbox);

/**
 * @brief 销毁沙箱，释放所有资源
 * @param sandbox 沙箱句柄（可为 NULL，NULL 时安全无操作）
 * @note 从管理器注销并释放权限规则、审计日志、锁等内部资源。
 */
AGENTRT_API void agentrt_sandbox_destroy(agentrt_sandbox_t *sandbox);

/* ==================== 权限规则 ==================== */

/**
 * @brief 添加权限规则
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号（如 SYS_TOOL_EXECUTE，见 syscalls.h）
 * @param perm_type 权限类型（PERM_ALLOW / PERM_DENY / PERM_ASK）
 * @param condition 条件表达式（JSON 格式，可为 NULL；当前实现未解析，预留扩展）
 * @return AGENTRT_SUCCESS 成功；AGENTRT_EINVAL sandbox 为 NULL；AGENTRT_ENOMEM 内存不足
 *
 * @details 规则按 syscall_num 匹配。DENY 规则优先于 ALLOW/ASK。
 * 同一 syscall 可添加多条规则，DENY 任一即拒绝。
 */
AGENTRT_API agentrt_error_t agentrt_sandbox_add_rule(agentrt_sandbox_t *sandbox, int syscall_num,
                                                     permission_type_t perm_type,
                                                     const char *condition);

/* ==================== 系统调用执行 ==================== */

/**
 * @brief 在沙箱中执行系统调用
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号（1 ~ SYS_MAX-1）
 * @param args 参数数组（每个元素为 void*，具体含义由 syscall 定义）
 * @param argc 参数数量
 * @param out_result 输出 syscall 处理函数的返回值（可为 NULL）
 * @return AGENTRT_SUCCESS 执行成功；
 *         AGENTRT_EPERM 沙箱已终止；
 *         AGENTRT_EBUSY 沙箱已挂起；
 *         AGENTRT_EACCES 权限拒绝；
 *         AGENTRT_EQUOTA 配额超限；
 *         AGENTRT_EINVAL 参数无效
 *
 * @details 执行流程：
 * 1. 状态检查（TERMINATED → EPERM, SUSPENDED → EBUSY）
 * 2. permission_check（DENY → EACCES，记录违规）
 * 3. quota_check（CPU 配额不足 → EQUOTA）
 * 4. agentrt_syscall_invoke 分发到具体 syscall 处理函数
 * 5. 审计日志记录（成功/失败均记录）
 * 6. 性能统计更新（耗时、调用次数）
 *
 * @note fail-closed 原则：sandbox 为 NULL 时调用方必须拒绝执行（由调用方检查）。
 */
AGENTRT_API agentrt_error_t agentrt_sandbox_invoke(agentrt_sandbox_t *sandbox, int syscall_num,
                                                   void **args, int argc, void **out_result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_SANDBOX_H */
