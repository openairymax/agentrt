/**
 * @file tcache.h
 * @brief P1.20: per-Thread 缓存层 — 减少 pool.c 全局锁竞争
 *
 * 每个线程维护本地缓存（tcache），批量从全局内存池获取/归还内存块，
 * 减少对 pool.c 全局 agentrt_mutex_t 的锁竞争。
 *
 * 设计：
 *   - _Thread_local 存储每个线程的缓存
 *   - 批量获取（batch_fill）：一次锁操作获取多个块
 *   - 批量归还（batch_flush）：缓存满时一次归还多个块
 *   - 限流上限：每个 tcache 最多缓存 TCACHE_MAX_CACHED 个块
 *
 * 性能目标：单线程分配延迟降低 > 30%
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_TCACHE_H
#define AGENTRT_TCACHE_H

#include "memory_pool.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 默认配置 ==================== */

#define TCACHE_DEFAULT_BATCH_SIZE 16     /**< 默认批量获取/归还数量 */
#define TCACHE_MAX_CACHED         64     /**< 最大缓存块数（防内存积压） */
#define TCACHE_FLUSH_THRESHOLD    48     /**< 达到此阈值强制 flush */

/* ==================== tcache 句柄 ==================== */

typedef struct agentrt_tcache agentrt_tcache_t;

/* ==================== tcache 统计 ==================== */

typedef struct {
    uint64_t alloc_count;       /**< 分配次数（含命中和未命中） */
    uint64_t free_count;        /**< 释放次数 */
    uint64_t hit_count;         /**< tcache 命中次数 */
    uint64_t miss_count;        /**< tcache 未命中（需访问全局池） */
    uint64_t batch_fill_count;  /**< 批量填充次数 */
    uint64_t batch_flush_count; /**< 批量归还次数 */
    uint64_t bypass_count;      /**< 绕过 tcache 直接访问池的次数 */
    double   hit_rate;          /**< 命中率 (hit / alloc * 100) */
} tcache_stats_t;

/* ==================== 生命周期 API ==================== */

/**
 * @brief P1.20.1: 创建 tcache（通常每个线程一个）
 *
 * @ownership alloc — 返回的 tcache 句柄由调用者持有，需通过 tcache_destroy 释放
 *
 * @param pool       关联的内存池
 * @param batch_size 批量获取大小（0 使用默认）
 * @param max_cached 最大缓存块数（0 使用默认）
 * @return tcache 句柄，失败返回 NULL
 */
agentrt_tcache_t *tcache_create(memory_pool_t *pool, size_t batch_size, size_t max_cached);

/**
 * @brief P1.20.1: 销毁 tcache（归还所有缓存块到池）
 *
 * @ownership release — 释放 tc 句柄的所有权，销毁后 tc 失效
 *
 * @param tc tcache 句柄
 */
void tcache_destroy(agentrt_tcache_t *tc);

/* ==================== 分配 / 释放 API ==================== */

/**
 * @brief P1.20.2: 从 tcache 快速分配
 *
 * @ownership alloc — 返回的内存块由调用者持有，需通过 tcache_free 归还
 *
 * 优先从线程本地缓存获取，未命中时才访问全局池。
 *
 * @param tc tcache 句柄
 * @return 内存块指针，失败返回 NULL
 */
void *tcache_alloc(agentrt_tcache_t *tc);

/**
 * @brief P1.20.2: 归还内存块到 tcache
 *
 * @ownership release — 释放 ptr 的所有权，调用后 ptr 失效
 *
 * 优先归还到线程本地缓存，缓存满时批量归还到全局池。
 *
 * @param tc  tcache 句柄
 * @param ptr 内存块指针（可为 NULL）
 */
void tcache_free(agentrt_tcache_t *tc, void *ptr);

/* ==================== 批量操作 ==================== */

/**
 * @brief P1.20.1: 从全局池批量填充 tcache
 * @param tc tcache 句柄
 * @return 填充的块数
 */
size_t tcache_batch_fill(agentrt_tcache_t *tc);

/**
 * @brief P1.20.1: 将 tcache 缓存批量归还到全局池
 *
 * 将超过 TCACHE_FLUSH_THRESHOLD 的缓存块归还。
 *
 * @param tc tcache 句柄
 * @return 归还的块数
 */
size_t tcache_batch_flush(agentrt_tcache_t *tc);

/**
 * @brief 立即归还所有缓存块到全局池
 * @param tc tcache 句柄
 */
void tcache_flush_all(agentrt_tcache_t *tc);

/* ==================== 查询 API ==================== */

/**
 * @brief 获取 tcache 统计信息
 * @param tc    tcache 句柄
 * @param stats 输出统计信息
 * @return true 成功
 */
bool tcache_get_stats(agentrt_tcache_t *tc, tcache_stats_t *stats);

/**
 * @brief 获取 tcache 当前缓存块数
 * @param tc tcache 句柄
 * @return 缓存块数
 */
size_t tcache_cached_count(agentrt_tcache_t *tc);

/**
 * @brief 检查 tcache 是否已满（达到 max_cached 上限）
 * @param tc tcache 句柄
 * @return true 已满
 */
bool tcache_is_full(agentrt_tcache_t *tc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_TCACHE_H */