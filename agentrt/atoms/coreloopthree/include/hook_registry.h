/**
 * @file hook_registry.h
 * @brief P2.1.1: Hook 注册表 — 按事件分类，优先级排序
 *
 * 线程安全的 Hook 注册表，支持：
 *   - 按 Hook 类型（8 种）分类存储
 *   - 按优先级排序（数值越大越先执行）
 *   - Shell/Python/Webhook 三种 Hook 类型
 *   - 运行时启用/禁用
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTRT_HOOK_REGISTRY_H
#define AGENTRT_HOOK_REGISTRY_H

#include "hook_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define HOOK_REGISTRY_MAX       256     /**< 最大注册 Hook 数 */
#define HOOK_NAME_MAX_LEN       128     /**< Hook 名称最大长度 */
#define HOOK_HOOK_PATH_MAX_LEN  512     /**< Hook 脚本/Webhook URL 最大长度 */

/* ==================== Hook 实现类型 ==================== */

/**
 * @brief Hook 实现类型
 */
typedef enum {
    HOOK_IMPL_CALLBACK = 0,    /**< C 回调函数 */
    HOOK_IMPL_SHELL    = 1,    /**< Shell 脚本 */
    HOOK_IMPL_PYTHON   = 2,    /**< Python 脚本 */
    HOOK_IMPL_WEBHOOK  = 3     /**< Webhook HTTP 调用 */
} hook_impl_type_t;

/* ==================== Hook 条目 ==================== */

/**
 * @brief Hook 注册条目（内部结构）
 *
 * 扩展 hook_registration_t，增加实现类型和脚本路径。
 */
typedef struct hook_entry {
    char name[HOOK_NAME_MAX_LEN];       /**< Hook 名称（全局唯一） */
    hook_type_t type;                   /**< Hook 类型（事件分类） */
    hook_impl_type_t impl_type;         /**< 实现类型 */
    hook_callback_t callback;           /**< C 回调函数（impl_type=CALLBACK 时有效） */
    char script_path[HOOK_HOOK_PATH_MAX_LEN]; /**< 脚本路径/Webhook URL */
    void *user_data;                    /**< 用户数据 */
    int priority;                       /**< 优先级（数值越大越先执行） */
    bool enabled;                       /**< 是否启用 */

    /* 统计信息 */
    uint64_t invoke_count;              /**< 调用次数 */
    uint64_t skip_count;                /**< 跳过次数 */
    uint64_t abort_count;               /**< 中止次数 */
    uint64_t retry_count;               /**< 重试次数 */
    uint64_t modify_count;              /**< 修改次数 */
    uint64_t total_duration_ns;         /**< 总耗时（纳秒） */
    uint64_t max_duration_ns;           /**< 最大耗时（纳秒） */
} hook_entry_t;

/* ==================== 注册表 API ==================== */

/**
 * @brief 初始化 Hook 注册表
 * @return 0 成功，非0 失败
 */
int hook_registry_init(void);

/**
 * @brief 销毁 Hook 注册表，释放资源
 */
void hook_registry_destroy(void);

/**
 * @brief 注册 Hook 到注册表
 *
 * 按 Hook 类型分类存储，同类型内按优先级降序排列。
 *
 * @param entry Hook 条目
 * @return 0 成功，-1 参数无效，-2 已满，-3 重名
 */
int hook_registry_register(const hook_entry_t *entry);

/**
 * @brief 注销 Hook
 * @param name Hook 名称
 * @return 0 成功，-1 未找到
 */
int hook_registry_unregister(const char *name);

/**
 * @brief 获取指定类型的所有 Hook（按优先级排序）
 *
 * @param type Hook 类型
 * @param out_entries 输出条目数组
 * @param max_count 数组最大容量
 * @param out_count 实际返回数量
 * @return 0 成功
 */
int hook_registry_get_by_type(hook_type_t type,
                               hook_entry_t **out_entries,
                               size_t max_count, size_t *out_count);

/**
 * @brief 按名称查找 Hook
 * @param name Hook 名称
 * @return Hook 条目指针（只读），未找到返回 NULL
 */
const hook_entry_t *hook_registry_find(const char *name);

/**
 * @brief 启用/禁用 Hook
 * @param name Hook 名称
 * @param enabled 是否启用
 * @return 0 成功，-1 未找到
 */
int hook_registry_set_enabled(const char *name, bool enabled);

/**
 * @brief 获取注册的 Hook 总数
 */
size_t hook_registry_count(void);

/**
 * @brief 获取指定类型的 Hook 数量
 */
size_t hook_registry_count_by_type(hook_type_t type);

/**
 * @brief 更新 Hook 统计信息
 * @param name Hook 名称
 * @param decision 执行决策
 * @param duration_ns 执行耗时（纳秒）
 * @return 0 成功
 */
int hook_registry_update_stats(const char *name, hook_decision_t decision,
                                uint64_t duration_ns);

/**
 * @brief 获取 Hook 统计信息
 * @param name Hook 名称
 * @param stats 输出统计
 * @return 0 成功，-1 未找到
 */
int hook_registry_get_stats(const char *name, hook_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_HOOK_REGISTRY_H */