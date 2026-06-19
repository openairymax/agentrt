/**
 * @file tcache.h
 * @brief per-Thread 缓存层 — _Thread_local 缓存 + 批量获取/归还
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * P1.20: per-Thread 缓存层减少多线程场景下的锁竞争。
 * 每个线程维护一个本地缓存，分配/释放优先在本地完成，
 * 仅在本地缓存不足/溢出时才与全局池交互。
 *
 * 性能目标：单线程分配延迟降低 > 30%
 */

#ifndef AGENTOS_COREKERN_TCACHE_H
#define AGENTOS_COREKERN_TCACHE_H

#include "export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief per-Thread 缓存配置
 */
typedef struct {
    size_t slot_size;         /**< 缓存槽大小（字节） */
    uint32_t max_slots;       /**< 最大槽位数（限流上限） */
    uint32_t batch_size;      /**< 批量获取/归还大小 */
} agentos_tcache_config_t;

/**
 * @brief per-Thread 缓存统计
 */
typedef struct {
    uint64_t alloc_count;     /**< 本地分配次数 */
    uint64_t free_count;      /**< 本地释放次数 */
    uint64_t miss_count;      /**< 本地未命中次数（需从全局池获取） */
    uint64_t overflow_count;  /**< 溢出次数（归还到全局池） */
    uint32_t current_slots;   /**< 当前已用槽位数 */
} agentos_tcache_stats_t;

/**
 * @brief per-Thread 缓存句柄（不透明指针）
 */
typedef struct agentos_tcache agentos_tcache_t;

/**
 * @brief 全局池回调函数类型
 *
 * tcache 在本地缓存不足/溢出时调用这些回调与全局池交互。
 */
typedef void *(*tcache_global_alloc_fn)(size_t size);
typedef void (*tcache_global_free_fn)(void *ptr);

/**
 * @brief 创建 per-Thread 缓存
 *
 * @param config 配置（NULL 使用默认）
 * @param global_alloc 全局池分配回调
 * @param global_free 全局池释放回调
 * @return 缓存句柄，失败返回 NULL
 */
AGENTOS_API agentos_tcache_t *agentos_tcache_create(
    const agentos_tcache_config_t *config,
    tcache_global_alloc_fn global_alloc,
    tcache_global_free_fn global_free);

/**
 * @brief 销毁 per-Thread 缓存
 *
 * 将所有缓存槽位归还到全局池。
 *
 * @param tcache 缓存句柄
 */
AGENTOS_API void agentos_tcache_destroy(agentos_tcache_t *tcache);

/**
 * @brief 从 per-Thread 缓存分配
 *
 * 优先从本地缓存获取，不足时从全局池批量获取。
 *
 * @param tcache 缓存句柄
 * @return 分配的内存指针，失败返回 NULL
 */
AGENTOS_API void *agentos_tcache_alloc(agentos_tcache_t *tcache);

/**
 * @brief 释放到 per-Thread 缓存
 *
 * 优先归还到本地缓存，满时批量归还到全局池。
 *
 * @param tcache 缓存句柄
 * @param ptr 要释放的指针
 */
AGENTOS_API void agentos_tcache_free(agentos_tcache_t *tcache, void *ptr);

/**
 * @brief 获取 per-Thread 缓存统计
 *
 * @param tcache 缓存句柄
 * @param out_stats 输出统计
 */
AGENTOS_API void agentos_tcache_get_stats(agentos_tcache_t *tcache,
                                           agentos_tcache_stats_t *out_stats);

/**
 * @brief 重置 per-Thread 缓存统计
 *
 * @param tcache 缓存句柄
 */
AGENTOS_API void agentos_tcache_reset_stats(agentos_tcache_t *tcache);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COREKERN_TCACHE_H */
