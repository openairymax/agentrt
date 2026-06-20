/**
 * @file tcache.h
 * @brief P1.20: per-Thread 缓存层 — 批量获取/归还 + 限流上限
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 在每个线程的 TLS 中维护一个小的分配缓存。
 * 小对象分配直接从 tcache 获取，避免频繁访问全局分配器。
 * 缓存耗尽时批量从 Arena 或全局分配器获取。
 *
 * 设计目标：
 *   - 单线程分配延迟降低 > 30%
 *   - 批量获取/归还减少锁竞争
 *   - 限流上限防止单个线程过度缓存
 *
 * @see arena.h  Arena 线性分配器
 */

#ifndef AGENTOS_TCACHE_H
#define AGENTOS_TCACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 句柄 ==================== */

typedef struct tcache_s tcache_t;

/* ==================== 配置 ==================== */

typedef struct {
    size_t max_cache_entries;      /**< 每个大小类最大缓存条目，默认 64 */
    size_t batch_fill_count;       /**< 批量填充数量，默认 32 */
    uint32_t max_cache_size_class; /**< 最大缓存大小类，超过此大小直接分配 */
    uint32_t max_total_cached_bytes;/**< 线程最大缓存总字节数，0 无限制 */
    bool enable_stats;             /**< 是否启用统计 */
} tcache_config_t;

/* ==================== 统计 ==================== */

typedef struct {
    uint64_t alloc_count;          /**< 总分配次数 */
    uint64_t free_count;           /**< 总释放次数 */
    uint64_t cache_hit_count;      /**< 缓存命中次数 */
    uint64_t cache_miss_count;     /**< 缓存未命中次数 */
    uint64_t batch_fill_count;     /**< 批量填充次数 */
    uint64_t batch_flush_count;    /**< 批量归还次数 */
    uint64_t oversized_alloc;      /**< 超大对象分配次数 */
    double hit_rate;               /**< 缓存命中率 */
} tcache_stats_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建 tcache 实例
 *
 * 每个线程应创建自己的 tcache 实例（通常存储在 _Thread_local）。
 *
 * @param config 配置（NULL 使用默认）
 * @return tcache 句柄，失败返回 NULL
 */
tcache_t *tcache_create(const tcache_config_t *config);

/**
 * @brief 销毁 tcache 实例
 *
 * 将所有缓存的对象归还给全局分配器。
 *
 * @param tc tcache 句柄
 */
void tcache_destroy(tcache_t *tc);

/* ==================== 分配操作 ==================== */

/**
 * @brief 从 tcache 分配内存
 *
 * 优先从线程本地缓存获取，缓存未命中时批量从全局分配器获取。
 * 超过 max_cache_size_class 的对象直接分配。
 *
 * @param tc tcache 句柄
 * @param size 分配大小
 * @return 内存指针，失败返回 NULL
 */
void *tcache_alloc(tcache_t *tc, size_t size);

/**
 * @brief 释放内存到 tcache
 *
 * 小对象缓存到线程本地，大对象直接释放。
 * 如果缓存已满，批量归还到全局分配器。
 *
 * @param tc tcache 句柄
 * @param ptr 内存指针
 * @param size 分配时的大小
 */
void tcache_free(tcache_t *tc, void *ptr, size_t size);

/* ==================== 批量操作 ==================== */

/**
 * @brief 批量归还所有缓存对象到全局分配器
 *
 * 在线程退出前调用，确保所有缓存对象被正确释放。
 *
 * @param tc tcache 句柄
 */
void tcache_flush(tcache_t *tc);

/**
 * @brief 清空缓存（不归还到全局分配器）
 *
 * 用于 Arena 模式：缓存对象属于 Arena，不需要单独释放。
 *
 * @param tc tcache 句柄
 */
void tcache_purge(tcache_t *tc);

/* ==================== 查询 ==================== */

/**
 * @brief 获取 tcache 统计信息
 *
 * @param tc tcache 句柄
 * @param out_stats 输出统计
 */
void tcache_get_stats(tcache_t *tc, tcache_stats_t *out_stats);

/**
 * @brief 获取当前缓存的对象数
 *
 * @param tc tcache 句柄
 * @return 缓存对象数
 */
size_t tcache_cached_count(tcache_t *tc);

/**
 * @brief 获取当前缓存的总字节数
 *
 * @param tc tcache 句柄
 * @return 缓存字节数
 */
size_t tcache_cached_bytes(tcache_t *tc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TCACHE_H */