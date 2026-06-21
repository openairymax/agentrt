/**
 * @file agentos_hook.h
 * @brief P2.1.5: Hook 系统公共 API — agentos_hook_register/unregister/trigger
 *
 * 提供一套简洁的 'agentos_hook_*' 命名空间 API，供智能体核心
 * 和其他 daemon 调用以注册和触发 Hook。
 *
 * 使用示例：
 *   // 注册 Hook
 *   agentos_hook_register("my_audit_hook", HOOK_TYPE_PRE_TOOL,
 *                          my_audit_callback, NULL, 100, true);
 *
 *   // 触发 Hook
 *   hook_context_t ctx = { .type = HOOK_TYPE_PRE_TOOL, ... };
 *   hook_decision_t decision = agentos_hook_trigger(&ctx);
 *
 *   // 注销 Hook
 *   agentos_hook_unregister("my_audit_hook");
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_HOOK_H
#define AGENTOS_HOOK_H

#include "hook_service.h"
#include "hook_registry.h"
#include "hook_executor.h"
#include "hook_timeout.h"
#include "memory_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 公共 API ==================== */

/**
 * @brief 注册 Hook
 *
 * 封装 hook_registry_register，提供更简洁的注册接口。
 *
 * @param name      Hook 名称（全局唯一）
 * @param type      Hook 类型（事件分类）
 * @param callback  C 回调函数（可为 NULL 用于 Shell/Python/Webhook）
 * @param user_data 用户数据（透传给回调）
 * @param priority  优先级（数值越大越先执行）
 * @param enabled   是否启用
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_register(const char *name,
                                         hook_type_t type,
                                         hook_callback_t callback,
                                         void *user_data,
                                         int priority,
                                         bool enabled)
{
    hook_entry_t entry;
    AGENTOS_MEMSET(&entry, 0, sizeof(entry));

    AGENTOS_STRNCPY_TERM(entry.name, name, sizeof(entry.name));
    entry.type       = type;
    entry.impl_type  = HOOK_IMPL_CALLBACK;
    entry.callback   = callback;
    entry.user_data  = user_data;
    entry.priority   = priority;
    entry.enabled    = enabled;

    return hook_registry_register(&entry);
}

/**
 * @brief 注册 Shell 脚本 Hook
 *
 * @param name        Hook 名称
 * @param type        Hook 类型
 * @param script_path Shell 脚本路径
 * @param priority    优先级
 * @param enabled     是否启用
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_register_shell(const char *name,
                                               hook_type_t type,
                                               const char *script_path,
                                               int priority,
                                               bool enabled)
{
    hook_entry_t entry;
    AGENTOS_MEMSET(&entry, 0, sizeof(entry));

    AGENTOS_STRNCPY_TERM(entry.name, name, sizeof(entry.name));
    AGENTOS_STRNCPY_TERM(entry.script_path, script_path,
                         sizeof(entry.script_path));
    entry.type       = type;
    entry.impl_type  = HOOK_IMPL_SHELL;
    entry.priority   = priority;
    entry.enabled    = enabled;

    return hook_registry_register(&entry);
}

/**
 * @brief 注册 Webhook Hook
 *
 * @param name      Hook 名称
 * @param type      Hook 类型
 * @param url       Webhook URL
 * @param priority  优先级
 * @param enabled   是否启用
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_register_webhook(const char *name,
                                                 hook_type_t type,
                                                 const char *url,
                                                 int priority,
                                                 bool enabled)
{
    hook_entry_t entry;
    AGENTOS_MEMSET(&entry, 0, sizeof(entry));

    AGENTOS_STRNCPY_TERM(entry.name, name, sizeof(entry.name));
    AGENTOS_STRNCPY_TERM(entry.script_path, url, sizeof(entry.script_path));
    entry.type       = type;
    entry.impl_type  = HOOK_IMPL_WEBHOOK;
    entry.priority   = priority;
    entry.enabled    = enabled;

    return hook_registry_register(&entry);
}

/**
 * @brief 注销 Hook
 *
 * @param name Hook 名称
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_unregister(const char *name)
{
    return hook_registry_unregister(name);
}

/**
 * @brief 触发 Hook 链
 *
 * 执行指定类型的所有已启用 Hook，按优先级降序执行。
 * 返回聚合后的决策。
 *
 * @param ctx Hook 上下文
 * @return 聚合决策
 */
static inline hook_decision_t agentos_hook_trigger(hook_context_t *ctx)
{
    return hook_executor_run(ctx, HOOK_EXEC_MODE_SEQUENTIAL);
}

/**
 * @brief 启用/禁用 Hook
 *
 * @param name    Hook 名称
 * @param enabled 是否启用
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_set_enabled(const char *name, bool enabled)
{
    return hook_registry_set_enabled(name, enabled);
}

/**
 * @brief 获取 Hook 统计
 *
 * @param name  Hook 名称
 * @param stats 输出统计
 * @return 0 成功，-1 失败
 */
static inline int agentos_hook_get_stats(const char *name, hook_stats_t *stats)
{
    return hook_registry_get_stats(name, stats);
}

/**
 * @brief 获取 Hook 条目
 *
 * @param name Hook 名称
 * @return Hook 条目指针（只读），未找到返回 NULL
 */
static inline const hook_entry_t *agentos_hook_get(const char *name)
{
    return hook_registry_find(name);
}

/**
 * @brief 获取注册的 Hook 总数
 */
static inline size_t agentos_hook_count(void)
{
    return hook_registry_count();
}

/**
 * @brief 获取指定类型的 Hook 数量
 */
static inline size_t agentos_hook_count_by_type(hook_type_t type)
{
    return hook_registry_count_by_type(type);
}

/**
 * @brief 初始化 Hook 系统
 *
 * 包括注册表、超时管理器、拦截器。
 *
 * @return 0 成功，非0 失败
 */
static inline int agentos_hook_init(void)
{
    if (hook_registry_init() != 0) return -1;
    if (hook_timeout_manager_init(0) != 0) return -1;
    return 0;
}

/**
 * @brief 销毁 Hook 系统
 */
static inline void agentos_hook_shutdown(void)
{
    hook_timeout_manager_destroy();
    hook_registry_destroy();
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HOOK_H */