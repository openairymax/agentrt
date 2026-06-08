/**
 * @file oom_handler.h
 * @brief OOM 分级响应框架 - 公共接口
 *
 * 实现五级内存压力框架（NORMAL → WARNING → DEGRADED → CRITICAL → FATAL），
 * 提供水位监控、OOM 事件记录、优雅降级回调注册和触发机制。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_OOM_HANDLER_H
#define AGENTOS_OOM_HANDLER_H

#include "error.h"
#include "export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 前向声明 — 类型定义在 memory_compat.h 中 */
#include "../../../commons/utils/memory/include/memory_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 五级压力级别枚举（扩展 WATERMARK_CRITICAL 之上新增 FATAL）
 * ============================================================================ */

/**
 * @brief 五级内存压力级别（SEC-12 合规）
 *
 * 从 memory_compat.h 的 4 级扩展为 5 级，新增 FATAL 级别。
 * 与 watermark_level_t 兼容：前 4 级值相同。
 */
#define WATERMARK_FATAL 4  /**< > 95% 致命，立即终止进程 */

/**
 * @brief 五级 OOM 响应级别（SEC-12 合规）
 *
 * 对应五级压力级别的响应动作。
 */
#define OOM_RESPONSE_FATAL_TERMINATE 4  /**< 立即中止进程（abort） */

/* ============================================================================
 * 优雅降级处理器类型（SEC-14 合规）
 * ============================================================================ */

/**
 * @brief 降级动作枚举
 */
typedef enum {
    DEGRADE_REDUCE_CACHE       = 0,  /**< 减小缓存容量 */
    DEGRADE_REDUCE_LOG_LEVEL   = 1,  /**< 降低日志级别 */
    DEGRADE_SYNC_IO            = 2,  /**< 异步IO降级为同步 */
    DEGRADE_REDUCE_BATCH       = 3,  /**< 减小批次大小 */
    DEGRADE_REJECT_NEW_CONN    = 4,  /**< 拒绝新连接 */
    DEGRADE_SUSPEND_NONCRITICAL = 5, /**< 暂停非关键功能 */
    DEGRADE_EVICT_LRU          = 6,  /**< 驱逐LRU缓存 */
    DEGRADE_CUSTOM             = 99, /**< 自定义降级动作 */
} degradation_action_t;

/**
 * @brief 优雅降级处理器结构体（SEC-14 合规）
 *
 * 每个 daemon 服务注册一个降级处理器，包含降级和恢复回调。
 */
typedef struct degradation_handler {
    const char *feature_name;                    /**< 功能名称（用于日志） */
    watermark_level_t trigger_level;             /**< 触发降级的水位级别 */
    degradation_action_t action;                 /**< 降级动作类型 */

    /**
     * @brief 降级回调
     * @param handler    本处理器
     * @param old_level  之前的水位级别
     * @param new_level  当前的水位级别
     * @return 0 成功，非0 失败
     */
    int (*on_degrade)(struct degradation_handler *handler,
                      watermark_level_t old_level,
                      watermark_level_t new_level);

    /**
     * @brief 恢复回调（水位回落后调用）
     * @param handler    本处理器
     * @param old_level  之前的水位级别
     * @param new_level  当前的水位级别
     * @return 0 成功，非0 失败
     */
    int (*on_restore)(struct degradation_handler *handler,
                      watermark_level_t old_level,
                      watermark_level_t new_level);

    void *context;                               /**< 用户上下文（透传） */
    bool is_degraded;                            /**< 当前是否处于降级状态 */
    struct degradation_handler *next;            /**< 链表下一节点 */
} degradation_handler_t;

/* ============================================================================
 * OOM 处理核心结构体（SEC-12 合规）
 * ============================================================================ */

/**
 * @brief OOM 处理器结构体
 *
 * 管理五级压力框架的状态、事件记录和降级处理器链表。
 * 全局单例，由 agentos_oom_init() 初始化。
 */
typedef struct oom_handler {
    /* 统计信息 */
    uint64_t oom_event_count;           /**< OOM 事件总数 */
    uint64_t last_oom_time;             /**< 上次 OOM 时间（毫秒时间戳） */
    size_t   last_oom_requested;        /**< 上次 OOM 请求大小 */
    size_t   last_oom_available;        /**< 上次 OOM 可用内存 */

    /* 水位状态 */
    watermark_level_t current_watermark; /**< 当前水位级别 */
    size_t total_system_memory;          /**< 系统总内存（字节） */

    /* 降级处理器链表 */
    degradation_handler_t *degradation_handlers;

    /* 扩展统计（指向 memory_stats_extended_t） */
    memory_stats_extended_t *ext_stats;
} oom_handler_t;

/* ============================================================================
 * 公共 API
 * ============================================================================ */

/**
 * @brief 初始化 OOM 处理器（SEC-12.1）
 *
 * 在使用任何 OOM API 前调用。创建全局 OOM 处理器单例。
 *
 * @param total_system_memory 系统总内存（字节），0 表示自动检测
 * @return AGENTOS_SUCCESS 或 AGENTOS_ENOMEM
 */
AGENTOS_API agentos_error_t agentos_oom_init(size_t total_system_memory);

/**
 * @brief 销毁 OOM 处理器
 *
 * 释放所有资源，包括降级处理器链表。
 */
AGENTOS_API void agentos_oom_destroy(void);

/**
 * @brief 获取全局 OOM 处理器（SEC-12.1）
 *
 * @return OOM 处理器指针，未初始化返回 NULL
 */
AGENTOS_API oom_handler_t *agentos_oom_get_handler(void);

/**
 * @brief 确定 OOM 响应级别（SEC-12.1）
 *
 * 根据五级水位级别映射到对应的 OOM 响应级别。
 * 水位映射:
 *   NORMAL   (≤60%)  → OOM_RESPONSE_WARNING
 *   WARNING  (60-75%) → OOM_RESPONSE_DEGRADED
 *   HIGH     (75-90%) → OOM_RESPONSE_CRITICAL
 *   CRITICAL (>90%)   → OOM_RESPONSE_FATAL
 *   FATAL    (>95%)   → OOM_RESPONSE_FATAL_TERMINATE
 *
 * @param level 当前水位级别
 * @return 对应的 OOM 响应级别（int 值）
 */
AGENTOS_API int agentos_oom_determine_response(watermark_level_t level);

/**
 * @brief 处理 OOM 事件（SEC-12.3）
 *
 * 记录 OOM 事件，根据当前水位确定响应级别，执行对应动作：
 *   WARNING  → 记录日志
 *   DEGRADED → 触发降级回调
 *   CRITICAL → 拒绝新请求，完成现有请求
 *   FATAL    → 立即终止进程
 *
 * @param requested 请求分配的大小（字节）
 * @param available 当前可用内存（字节），0 表示自动计算
 * @return OOM 响应级别代码
 */
AGENTOS_API int agentos_oom_handle(size_t requested, size_t available);

/**
 * @brief 注册优雅降级处理器（SEC-14.1）
 *
 * 每个 daemon 服务启动时注册自己的降级处理器。
 * 当水位达到触发级别时，自动调用 on_degrade 回调。
 * 当水位回落到触发级别以下时，自动调用 on_restore 回调。
 *
 * @param handler 降级处理器（调用者分配，OOM 模块持有引用）
 * @return AGENTOS_SUCCESS 或 AGENTOS_EINVAL
 */
AGENTOS_API agentos_error_t agentos_register_degradation(
    degradation_handler_t *handler);

/**
 * @brief 注销优雅降级处理器
 *
 * @param handler 要注销的降级处理器
 */
AGENTOS_API void agentos_unregister_degradation(
    degradation_handler_t *handler);

/**
 * @brief 执行优雅降级（SEC-14.3）
 *
 * 遍历所有已注册的降级处理器，对达到触发级别的处理器执行 on_degrade 回调。
 * 对水位已回落的处理器执行 on_restore 回调。
 *
 * @param old_level 之前的水位级别
 * @param new_level 当前的水位级别
 */
AGENTOS_API void agentos_oom_degrade(watermark_level_t old_level,
                                     watermark_level_t new_level);

/**
 * @brief 更新内存水位（SEC-12）
 *
 * 基于当前内存使用量重新计算水位级别。
 * 如果水位发生变化，触发降级/恢复回调。
 *
 * @param current_allocated 当前已分配内存（字节）
 */
AGENTOS_API void agentos_oom_update_watermark(size_t current_allocated);

/**
 * @brief 获取当前水位级别
 *
 * @return 当前水位级别
 */
AGENTOS_API watermark_level_t agentos_oom_get_watermark(void);

/**
 * @brief 检查是否处于 OOM 降级状态
 *
 * @return true 如果水位 >= WATERMARK_WARNING
 */
AGENTOS_API bool agentos_oom_is_degraded(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_OOM_HANDLER_H */