/**
 * @file mempool.h
 * @brief P3.16: mempool 最小保证分配器 — 紧急预留 + 对象池 + 最低保证分配
 *
 * 在 OOM 场景下保证 IPC 消息等关键路径的内存分配。
 * 核心特性：
 *   - 紧急预留池（默认 32MB）
 *   - 对象池（固定大小对象快速分配）
 *   - 最低保证分配（OOM 时优先满足关键路径）
 *   - 分级水位线（正常/警告/紧急）
 *
 * 典型用法：
 *   agentos_mempool_t *pool = agentos_mempool_create(32 * 1024 * 1024, 256, 1024);
 *   void *buf = agentos_mempool_alloc(pool, 512, MEMPOOL_PRIORITY_CRITICAL);
 *   // ... 使用 buf ...
 *   agentos_mempool_free(pool, buf);
 *   agentos_mempool_destroy(pool);
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CUPOLAS_MEMPOOL_H
#define CUPOLAS_MEMPOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量定义
 * ================================================================ */

/** 默认紧急预留大小（32MB） */
#define MEMPOOL_DEFAULT_RESERVE_MB    32
/** 默认对象池块大小 */
#define MEMPOOL_DEFAULT_BLOCK_SIZE    256
/** 默认对象池块数量 */
#define MEMPOOL_DEFAULT_BLOCK_COUNT   4096

/* ================================================================
 * 类型定义
 * ================================================================ */

/** 内存池句柄（不透明） */
typedef struct agentos_mempool agentos_mempool_t;

/** 分配优先级 */
typedef enum {
    MEMPOOL_PRIORITY_LOW       = 0,  /**< 低优先级（常规分配，可能失败） */
    MEMPOOL_PRIORITY_NORMAL    = 1,  /**< 普通优先级 */
    MEMPOOL_PRIORITY_HIGH      = 2,  /**< 高优先级（关键路径） */
    MEMPOOL_PRIORITY_CRITICAL  = 3,  /**< 紧急优先级（OOM 时仍保证分配） */
} agentos_mempool_priority_t;

/** 内存池水位线状态 */
typedef enum {
    MEMPOOL_WATERMARK_NORMAL   = 0,  /**< 正常：使用率 < 50% */
    MEMPOOL_WATERMARK_WARN     = 1,  /**< 警告：使用率 50-75% */
    MEMPOOL_WATERMARK_HIGH     = 2,  /**< 高：使用率 75-90% */
    MEMPOOL_WATERMARK_CRITICAL = 3,  /**< 紧急：使用率 > 90% */
} agentos_mempool_watermark_t;

/** 内存池统计信息 */
typedef struct {
    size_t total_reserved;       /**< 总预留大小（字节） */
    size_t total_allocated;      /**< 当前已分配大小（字节） */
    size_t peak_allocated;       /**< 峰值分配大小（字节） */
    size_t available_reserved;   /**< 剩余可用预留大小（字节） */
    size_t object_pool_total;    /**< 对象池总块数 */
    size_t object_pool_used;     /**< 对象池已用块数 */
    size_t total_allocs;         /**< 总分配次数 */
    size_t total_frees;          /**< 总释放次数 */
    size_t oom_rejections;       /**< OOM 拒绝次数 */
    size_t emergency_allocs;     /**< 紧急分配次数 */
    agentos_mempool_watermark_t watermark; /**< 当前水位线 */
} agentos_mempool_stats_t;

/* ================================================================
 * 生命周期 API
 * ================================================================ */

/**
 * @brief 创建内存池
 *
 * @param reserve_size   紧急预留大小（字节），0 使用默认 32MB
 * @param block_size     对象池块大小（字节），0 使用默认 256B
 * @param block_count    对象池块数量，0 使用默认 4096
 * @return 内存池句柄，失败返回 NULL
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentos_mempool_destroy() 释放
 * @threadsafe 是
 */
agentos_mempool_t *agentos_mempool_create(size_t reserve_size,
                                           size_t block_size,
                                           size_t block_count);

/**
 * @brief 销毁内存池
 *
 * 释放所有预留内存和对象池。
 *
 * @param pool 内存池句柄
 *
 * @ownership pool: TRANSFER
 * @threadsafe 否
 */
void agentos_mempool_destroy(agentos_mempool_t *pool);

/* ================================================================
 * 分配/释放 API
 * ================================================================ */

/**
 * @brief 从内存池分配内存
 *
 * 分配策略：
 *   - 等于 block_size 的分配 → 优先从对象池获取
 *   - 其他大小 → 从预留池分配
 *   - 优先级 CRITICAL → OOM 时仍保证分配（从紧急预留取）
 *   - 水位线告警时 → 低优先级分配可能被拒绝
 *
 * @param pool     内存池句柄
 * @param size     分配大小（字节）
 * @param priority 分配优先级
 * @return 内存指针，失败返回 NULL
 *
 * @ownership 返回的内存由调用者管理，需通过 agentos_mempool_free() 归还
 * @threadsafe 是
 */
void *agentos_mempool_alloc(agentos_mempool_t *pool,
                              size_t size,
                              agentos_mempool_priority_t priority);

/**
 * @brief 释放内存回内存池
 *
 * @param pool 内存池句柄
 * @param ptr  内存指针（NULL 无操作）
 *
 * @ownership ptr: TRANSFER
 * @threadsafe 是
 */
void agentos_mempool_free(agentos_mempool_t *pool, void *ptr);

/* ================================================================
 * 统计与诊断 API
 * ================================================================ */

/**
 * @brief 获取内存池统计信息
 *
 * @param pool  内存池句柄
 * @param stats 输出统计信息
 * @return 0 成功，非0失败
 */
int agentos_mempool_get_stats(agentos_mempool_t *pool,
                               agentos_mempool_stats_t *stats);

/**
 * @brief 获取当前水位线
 *
 * @param pool 内存池句柄
 * @return 当前水位线状态
 */
agentos_mempool_watermark_t agentos_mempool_get_watermark(agentos_mempool_t *pool);

/**
 * @brief 收缩内存池：释放空闲对象池块
 *
 * @param pool 内存池句柄
 * @return 释放的块数
 */
size_t agentos_mempool_shrink(agentos_mempool_t *pool);

/**
 * @brief 验证内存池内部一致性
 *
 * @param pool 内存池句柄
 * @return true 一致，false 损坏
 */
bool agentos_mempool_validate(agentos_mempool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_MEMPOOL_H */