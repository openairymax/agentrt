// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file daemon_degradation.h
 * @brief Daemon 层优雅降级处理器（SEC-14 合规）
 *
 * 为所有 daemon 服务提供预构建的 OOM 降级处理器。
 * 每个服务启动时调用 daemon_degradation_register_*() 注册对应的降级回调。
 *
 * 降级策略映射:
 *   WARNING  → 减小缓存 50%、降低日志级别、驱逐 LRU
 *   HIGH     → 异步 IO 降级为同步、减小批次大小
 *   CRITICAL → 拒绝新连接、暂停非关键功能
 *
 * @see oom_handler.h  OOM 分级响应框架
 * @see svc_cache.h    缓存服务接口
 * @see svc_logger.h   日志服务接口
 */

#ifndef DAEMON_DEGRADATION_H
#define DAEMON_DEGRADATION_H

#include "agentos.h"
#include "oom_handler.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 降级上下文类型
 * ============================================================================ */

/**
 * @brief 缓存降级上下文
 *
 * 保存原始缓存容量，以便恢复时还原。
 */
typedef struct {
    void *cache_handle;      /**< 缓存句柄（svc_cache_t *） */
    size_t original_capacity; /**< 原始缓存容量 */
    size_t reduced_capacity;  /**< 降级后缓存容量 */
} degrade_cache_ctx_t;

/**
 * @brief 日志降级上下文
 *
 * 保存原始日志级别，以便恢复时还原。
 */
typedef struct {
    int original_log_level;  /**< 原始日志级别 */
    int degraded_log_level;  /**< 降级后日志级别 */
} degrade_log_ctx_t;

/**
 * @brief 批处理降级上下文
 *
 * 保存原始批次大小，以便恢复时还原。
 */
typedef struct {
    size_t *batch_size_ptr;      /**< 指向当前批次大小的指针 */
    size_t original_batch_size;  /**< 原始批次大小 */
    size_t reduced_batch_size;   /**< 降级后批次大小 */
} degrade_batch_ctx_t;

/**
 * @brief 连接控制降级上下文
 *
 * 控制新连接是否被接受。
 */
typedef struct {
    bool *reject_new_flag;  /**< 指向拒绝新连接标志的指针 */
} degrade_conn_ctx_t;

/* ============================================================================
 * 预构建降级处理器注册
 * ============================================================================ */

/**
 * @brief 注册缓存容量降级处理器
 *
 * 当水位达到 WATERMARK_WARNING 时，将缓存容量减半。
 * 当水位回落时，恢复原始缓存容量。
 *
 * @param cache_handle 缓存句柄（svc_cache_t * 或兼容类型）
 * @param original_capacity 原始缓存容量
 * @return 降级处理器指针（静态分配，调用者不持有所有权），失败返回 NULL
 */
degradation_handler_t *daemon_degradation_register_cache(
    void *cache_handle, size_t original_capacity);

/**
 * @brief 注册日志级别降级处理器
 *
 * 当水位达到 WATERMARK_WARNING 时，将日志级别提升到 ERROR。
 * 当水位回落时，恢复原始日志级别。
 *
 * @param original_log_level 原始日志级别
 * @return 降级处理器指针（静态分配），失败返回 NULL
 */
degradation_handler_t *daemon_degradation_register_log_level(
    int original_log_level);

/**
 * @brief 注册批处理大小降级处理器
 *
 * 当水位达到 WATERMARK_HIGH 时，将批次大小减半。
 * 当水位回落时，恢复原始批次大小。
 *
 * @param batch_size_ptr 指向当前批次大小的指针（运行时可变）
 * @param original_batch_size 原始批次大小
 * @return 降级处理器指针（静态分配），失败返回 NULL
 */
degradation_handler_t *daemon_degradation_register_batch(
    size_t *batch_size_ptr, size_t original_batch_size);

/**
 * @brief 注册新连接拒绝降级处理器
 *
 * 当水位达到 WATERMARK_CRITICAL 时，设置拒绝新连接标志。
 * 当水位回落时，清除拒绝标志。
 *
 * @param reject_flag 指向拒绝标志的指针（运行时可变）
 * @return 降级处理器指针（静态分配），失败返回 NULL
 */
degradation_handler_t *daemon_degradation_register_reject_conn(
    bool *reject_flag);

/**
 * @brief 注册自定义降级处理器
 *
 * 允许 daemon 服务注册自定义的降级和恢复回调。
 *
 * @param feature_name 功能名称（用于日志）
 * @param trigger_level 触发降级的水位级别
 * @param on_degrade 降级回调
 * @param on_restore 恢复回调
 * @param context 用户上下文
 * @return 降级处理器指针（静态分配），失败返回 NULL
 */
degradation_handler_t *daemon_degradation_register_custom(
    const char *feature_name,
    watermark_level_t trigger_level,
    int (*on_degrade)(degradation_handler_t *, watermark_level_t, watermark_level_t),
    int (*on_restore)(degradation_handler_t *, watermark_level_t, watermark_level_t),
    void *context);

/**
 * @brief 注销所有 daemon 降级处理器
 *
 * 在 daemon 服务关闭时调用，清理所有注册的降级处理器。
 */
void daemon_degradation_unregister_all(void);

#ifdef __cplusplus
}
#endif

#endif /* DAEMON_DEGRADATION_H */