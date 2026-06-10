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

/* agentos_mutex_t 和 agentos_mutex_* API — 来自 platform.h（通过 agentos_types.h 间接包含） */

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
 * 内存压力分级系统（SEC-12 OOM 分级响应框架）
 * ============================================================================ */

/**
 * @brief 五级内存压力级别（SEC-12 OOM 分级响应）
 *
 * 基于内存使用率的五级压力评估，阈值可配置。
 */
typedef enum {
    AGENTOS_MEM_PRESSURE_NORMAL    = 0,  /**< <70% usage - normal operation */
    AGENTOS_MEM_PRESSURE_WARNING   = 1,  /**< 70-80% usage - start cleanup */
    AGENTOS_MEM_PRESSURE_DEGRADED  = 2,  /**< 80-90% usage - aggressive cleanup */
    AGENTOS_MEM_PRESSURE_CRITICAL  = 3,  /**< 90-95% usage - deny non-essential allocations */
    AGENTOS_MEM_PRESSURE_FATAL     = 4   /**< >95% usage - emergency shutdown */
} agentos_mem_pressure_level_t;

/** 每个压力级别最大回调注册数 */
#define AGENTOS_PRESSURE_MAX_CALLBACKS 8

/**
 * @brief 压力变化回调函数类型
 *
 * @param level     当前压力级别
 * @param user_data 注册时传入的用户数据
 */
typedef void (*agentos_pressure_callback_t)(
    agentos_mem_pressure_level_t level, void *user_data);

/**
 * @brief 压力回调注册槽位
 */
typedef struct {
    agentos_pressure_callback_t callback;  /**< 回调函数指针 */
    void                       *user_data; /**< 用户数据 */
    bool                        active;    /**< 是否激活 */
} agentos_pressure_callback_slot_t;

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

    /* 压力分级系统（SEC-12 OOM 分级响应框架） */
    agentos_mem_pressure_level_t current_pressure;  /**< 当前压力级别 */
    agentos_mutex_t pressure_mutex;                  /**< 压力系统互斥锁 */
    agentos_pressure_callback_slot_t
        pressure_callbacks[5][AGENTOS_PRESSURE_MAX_CALLBACKS]; /**< 每级回调槽位 */
    size_t pressure_denied_count;                    /**< 被拒绝的分配计数 */
    size_t total_allocated_bytes;                    /**< 累计分配字节数 */
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
AGENTOS_API oom_response_level_t agentos_oom_determine_response(watermark_level_t level);

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

/* ============================================================================
 * 内存压力分级 API（SEC-12 OOM 分级响应框架）
 * ============================================================================ */

/**
 * @brief 获取当前内存压力级别（SEC-12）
 *
 * 基于当前内存使用率计算压力级别：
 *   <70%  → NORMAL
 *   70-80% → WARNING
 *   80-90% → DEGRADED
 *   90-95% → CRITICAL
 *   >95%  → FATAL
 *
 * 如果压力级别发生变化，自动触发已注册的回调。
 *
 * @return 当前压力级别
 */
AGENTOS_API agentos_mem_pressure_level_t agentos_oom_get_pressure(void);

/**
 * @brief 注册压力变化回调（SEC-12）
 *
 * 当压力级别达到或超过指定级别时，回调被触发。
 * 每个级别最多支持 AGENTOS_PRESSURE_MAX_CALLBACKS (8) 个回调。
 *
 * @param level     触发回调的压力级别
 * @param callback  回调函数指针
 * @param user_data 传递给回调的用户数据
 * @return 0 成功，AGENTOS_EINVAL 参数无效，AGENTOS_EBUSY 槽位已满
 */
AGENTOS_API int agentos_oom_register_callback(
    agentos_mem_pressure_level_t level,
    void (*callback)(agentos_mem_pressure_level_t, void *),
    void *user_data);

/**
 * @brief 检查分配是否应被允许（SEC-12）
 *
 * 根据当前压力级别决定是否允许内存分配：
 *   NORMAL   → 允许所有分配
 *   WARNING  → 允许所有分配（记录警告）
 *   DEGRADED → 允许分配，但建议减少非必要分配
 *   CRITICAL → 拒绝非必要分配
 *   FATAL    → 拒绝所有分配
 *
 * @param requested_size 请求分配的字节数
 * @return 0 允许分配，非0 拒绝分配
 */
AGENTOS_API int agentos_oom_check_allocation(size_t requested_size);

/**
 * @brief 报告内存压力统计信息（SEC-12）
 *
 * 通过 AGENTOS_LOG_INFO 输出当前内存压力统计：
 *   - 当前压力级别和使用率
 *   - 系统总内存和已分配内存
 *   - 被拒绝的分配计数
 *   - 各级别已注册的回调数量
 */
AGENTOS_API void agentos_oom_report_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_OOM_HANDLER_H */