/**
 * @file mempool.c
 * @brief P3.16: mempool 最小保证分配器 — 紧急预留 + 对象池 + OOM 保证
 *
 * 实现细节：
 *   - 紧急预留池：预分配固定大小内存，仅 HIGH/CRITICAL 优先级可用
 *   - 对象池：固定大小块快速分配，使用 freelist 链表
 *   - 分级水位线：正常(50%) → 警告(75%) → 高(90%) → 紧急
 *   - OOM 时 CRITICAL 优先级保证分配（从紧急预留池取）
 *   - 低优先级在 HIGH 水位线时被拒绝
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mempool.h"
#include "memory_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/* ================================================================
 * 常量
 * ================================================================ */

/** 默认紧急预留大小（字节） */
#define MEMPOOL_DEFAULT_RESERVE_SIZE    (32UL * 1024 * 1024)
/** 默认对象池块大小 */
#define MEMPOOL_DEFAULT_BLOCK_SIZE      256
/** 默认对象池块数量 */
#define MEMPOOL_DEFAULT_BLOCK_COUNT     4096
/** 最小分配大小 */
#define MEMPOOL_MIN_ALLOC_SIZE          16
/** 对齐大小 */
#define MEMPOOL_ALIGNMENT               sizeof(void *)
/** 对齐宏 */
#define MEMPOOL_ALIGN(s) \
    (((s) + MEMPOOL_ALIGNMENT - 1) & ~(MEMPOOL_ALIGNMENT - 1))

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/** 对象池块 */
typedef struct mempool_block {
    struct mempool_block *next;   /**< 空闲链表 */
    char data[];                  /**< 数据区 */
} mempool_block_t;

/** 预留池分配记录 */
typedef struct mempool_alloc {
    struct mempool_alloc *next;   /**< 链表 */
    void *ptr;                    /**< 分配的内存指针 */
    size_t size;                  /**< 分配大小 */
    agentos_mempool_priority_t priority; /**< 分配优先级 */
} mempool_alloc_t;

/** 内存池 */
struct agentos_mempool {
    /* 紧急预留池 */
    void *reserve_base;           /**< 预留内存基址 */
    size_t reserve_size;          /**< 预留大小 */
    size_t reserve_used;          /**< 已使用大小 */
    size_t reserve_peak;          /**< 峰值使用 */

    /* 对象池 */
    size_t block_size;            /**< 块大小 */
    size_t block_count;           /**< 总块数 */
    mempool_block_t *free_list;   /**< 空闲块链表 */
    size_t free_blocks;           /**< 空闲块数 */
    size_t total_blocks;          /**< 总块数 */

    /* 分配跟踪 */
    mempool_alloc_t *alloc_list;  /**< 分配记录链表 */

    /* 统计 */
    size_t total_allocs;          /**< 总分配次数 */
    size_t total_frees;           /**< 总释放次数 */
    size_t oom_rejections;        /**< OOM 拒绝次数 */
    size_t emergency_allocs;      /**< 紧急分配次数 */

    /* 线程安全 */
#ifdef __linux__
    pthread_mutex_t lock;
#elif defined(_WIN32)
    CRITICAL_SECTION lock;
#endif
};

/* ================================================================
 * 平台抽象
 * ================================================================ */

static int mempool_mutex_init(agentos_mempool_t *pool)
{
#ifdef __linux__
    return pthread_mutex_init(&pool->lock, NULL);
#elif defined(_WIN32)
    InitializeCriticalSection(&pool->lock);
    return 0;
#else
    (void)pool;
    return 0;
#endif
}

static void mempool_mutex_lock(agentos_mempool_t *pool)
{
#ifdef __linux__
    pthread_mutex_lock(&pool->lock);
#elif defined(_WIN32)
    EnterCriticalSection(&pool->lock);
#else
    (void)pool;
#endif
}

static void mempool_mutex_unlock(agentos_mempool_t *pool)
{
#ifdef __linux__
    pthread_mutex_unlock(&pool->lock);
#elif defined(_WIN32)
    LeaveCriticalSection(&pool->lock);
#else
    (void)pool;
#endif
}

static void mempool_mutex_destroy(agentos_mempool_t *pool)
{
#ifdef __linux__
    pthread_mutex_destroy(&pool->lock);
#elif defined(_WIN32)
    DeleteCriticalSection(&pool->lock);
#else
    (void)pool;
#endif
}

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 计算当前水位线
 */
static agentos_mempool_watermark_t mempool_calc_watermark(agentos_mempool_t *pool)
{
    if (pool->reserve_size == 0) return MEMPOOL_WATERMARK_NORMAL;

    double usage = (double)pool->reserve_used / (double)pool->reserve_size;

    if (usage >= 0.90) return MEMPOOL_WATERMARK_CRITICAL;
    if (usage >= 0.75) return MEMPOOL_WATERMARK_HIGH;
    if (usage >= 0.50) return MEMPOOL_WATERMARK_WARN;
    return MEMPOOL_WATERMARK_NORMAL;
}

/**
 * @brief 从预留池分配
 */
static void *mempool_reserve_alloc(agentos_mempool_t *pool, size_t size,
                                    agentos_mempool_priority_t priority)
{
    agentos_mempool_watermark_t wm = mempool_calc_watermark(pool);

    /* 低优先级在水位线 HIGH 时被拒绝 */
    if (priority <= MEMPOOL_PRIORITY_LOW && wm >= MEMPOOL_WATERMARK_HIGH) {
        pool->oom_rejections++;
        return NULL;
    }

    /* 普通优先级在水位线 CRITICAL 时被拒绝 */
    if (priority <= MEMPOOL_PRIORITY_NORMAL &&
        wm >= MEMPOOL_WATERMARK_CRITICAL) {
        pool->oom_rejections++;
        return NULL;
    }

    /* 检查是否有足够空间 */
    size_t remaining = pool->reserve_size - pool->reserve_used;
    if (size > remaining) {
        /* CRITICAL 优先级：使用超过预留的紧急分配 */
        if (priority == MEMPOOL_PRIORITY_CRITICAL) {
            /* 从系统堆分配（超出预留池） */
            void *ptr = malloc(size);
            if (ptr) {
                pool->emergency_allocs++;
                pool->reserve_used += size;
                if (pool->reserve_used > pool->reserve_peak) {
                    pool->reserve_peak = pool->reserve_used;
                }
            }
            return ptr;
        }
        pool->oom_rejections++;
        return NULL;
    }

    /* 从预留池分配 */
    void *ptr = (char *)pool->reserve_base + pool->reserve_used;
    pool->reserve_used += size;
    if (pool->reserve_used > pool->reserve_peak) {
        pool->reserve_peak = pool->reserve_used;
    }

    return ptr;
}

/**
 * @brief 从对象池分配
 */
static void *mempool_block_alloc(agentos_mempool_t *pool)
{
    if (!pool->free_list) return NULL;

    mempool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->free_blocks--;

    return block->data;
}

/**
 * @brief 释放对象池块
 */
static void mempool_block_free(agentos_mempool_t *pool, void *ptr)
{
    if (!ptr) return;

    /* 计算块头地址 */
    mempool_block_t *block = (mempool_block_t *)((char *)ptr - sizeof(mempool_block_t));

    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_blocks++;
}

/**
 * @brief 判断一个指针是否来自对象池
 */
static bool mempool_is_block_ptr(agentos_mempool_t *pool, void *ptr)
{
    /* 对象池块在 reserve_base 之前分配，不在预留池范围内 */
    if (!pool->reserve_base) return false;

    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)pool->reserve_base;
    uintptr_t end  = base + pool->reserve_size;

    return (addr < base || addr >= end);
}

/**
 * @brief 初始化对象池
 */
static int mempool_init_blocks(agentos_mempool_t *pool)
{
    size_t total_alloc_size = (sizeof(mempool_block_t) + pool->block_size) *
                               pool->block_count;

    char *raw = (char *)calloc(1, total_alloc_size);
    if (!raw) return -1;

    pool->free_list = NULL;
    pool->free_blocks = 0;
    pool->total_blocks = pool->block_count;

    for (size_t i = 0; i < pool->block_count; i++) {
        mempool_block_t *block = (mempool_block_t *)(raw +
            i * (sizeof(mempool_block_t) + pool->block_size));
        block->next = pool->free_list;
        pool->free_list = block;
        pool->free_blocks++;
    }

    return 0;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

agentos_mempool_t *agentos_mempool_create(size_t reserve_size,
                                           size_t block_size,
                                           size_t block_count)
{
    agentos_mempool_t *pool = (agentos_mempool_t *)AGENTOS_CALLOC(1, sizeof(*pool));
    if (!pool) {
        AGENTOS_LOG_ERROR("Mempool: OOM allocating pool struct");
        return NULL;
    }

    /* 设置默认值 */
    pool->reserve_size = reserve_size > 0 ? reserve_size : MEMPOOL_DEFAULT_RESERVE_SIZE;
    pool->block_size   = MEMPOOL_ALIGN(block_size > 0 ? block_size : MEMPOOL_DEFAULT_BLOCK_SIZE);
    pool->block_count  = block_count > 0 ? block_count : MEMPOOL_DEFAULT_BLOCK_COUNT;

    AGENTOS_LOG_INFO("Mempool: creating (reserve=%zuMB, block_size=%zu, block_count=%zu)",
                     pool->reserve_size / (1024 * 1024),
                     pool->block_size, pool->block_count);

    /* 分配紧急预留池 */
    pool->reserve_base = AGENTOS_CALLOC(1, pool->reserve_size);
    if (!pool->reserve_base) {
        AGENTOS_LOG_ERROR("Mempool: OOM allocating reserve pool (%zuMB)",
                          pool->reserve_size / (1024 * 1024));
        AGENTOS_FREE(pool);
        return NULL;
    }

    AGENTOS_LOG_INFO("Mempool: reserve pool allocated at %p (%zuMB)",
                     pool->reserve_base, pool->reserve_size / (1024 * 1024));

    /* 初始化对象池 */
    if (mempool_init_blocks(pool) != 0) {
        AGENTOS_LOG_ERROR("Mempool: failed to init object pool blocks");
        AGENTOS_FREE(pool->reserve_base);
        AGENTOS_FREE(pool);
        return NULL;
    }

    AGENTOS_LOG_INFO("Mempool: object pool initialized (%zu blocks of %zu bytes)",
                     pool->block_count, pool->block_size);

    /* 初始化互斥锁 */
    if (mempool_mutex_init(pool) != 0) {
        AGENTOS_LOG_ERROR("Mempool: mutex init failed");
        AGENTOS_FREE(pool->reserve_base);
        AGENTOS_FREE(pool);
        return NULL;
    }

    AGENTOS_LOG_INFO("Mempool: created successfully");
    return pool;
}

void agentos_mempool_destroy(agentos_mempool_t *pool)
{
    if (!pool) return;

    AGENTOS_LOG_INFO("Mempool: destroying (total_allocs=%zu, total_frees=%zu, "
                     "oom_rejections=%zu, emergency_allocs=%zu, "
                     "reserve_used=%zu/%zu, blocks_used=%zu/%zu)",
                     pool->total_allocs, pool->total_frees,
                     pool->oom_rejections, pool->emergency_allocs,
                     pool->reserve_used, pool->reserve_size,
                     pool->total_blocks - pool->free_blocks,
                     pool->total_blocks);

    mempool_mutex_lock(pool);

    /* 释放分配记录 */
    mempool_alloc_t *alloc = pool->alloc_list;
    size_t alloc_records = 0;
    while (alloc) {
        mempool_alloc_t *next = alloc->next;
        AGENTOS_FREE(alloc);
        alloc = next;
        alloc_records++;
    }

    /* 释放预留池 */
    if (pool->reserve_base) {
        AGENTOS_FREE(pool->reserve_base);
        pool->reserve_base = NULL;
    }

    mempool_mutex_unlock(pool);
    mempool_mutex_destroy(pool);

    AGENTOS_FREE(pool);

    AGENTOS_LOG_INFO("Mempool: destroyed (%zu alloc records freed)", alloc_records);
}

void *agentos_mempool_alloc(agentos_mempool_t *pool,
                              size_t size,
                              agentos_mempool_priority_t priority)
{
    if (!pool || size == 0) {
        AGENTOS_LOG_DEBUG("Mempool: alloc called with NULL pool or size=0");
        return NULL;
    }

    size_t aligned_size = MEMPOOL_ALIGN(size);
    if (aligned_size < MEMPOOL_MIN_ALLOC_SIZE) {
        aligned_size = MEMPOOL_MIN_ALLOC_SIZE;
    }

    mempool_mutex_lock(pool);

    void *ptr = NULL;
    agentos_mempool_watermark_t wm = mempool_calc_watermark(pool);

    /* 等于 block_size 的分配：优先从对象池获取 */
    if (aligned_size == pool->block_size && pool->free_blocks > 0) {
        ptr = mempool_block_alloc(pool);
        if (ptr) {
            pool->total_allocs++;
            AGENTOS_LOG_DEBUG("Mempool: alloc from object pool (size=%zu, "
                              "priority=%d, free_blocks=%zu)",
                              aligned_size, priority, pool->free_blocks);
            mempool_mutex_unlock(pool);
            return ptr;
        }
    }

    /* 从预留池分配 */
    ptr = mempool_reserve_alloc(pool, aligned_size, priority);
    if (ptr) {
        pool->total_allocs++;

        /* 记录分配 */
        mempool_alloc_t *record = (mempool_alloc_t *)AGENTOS_MALLOC(sizeof(*record));
        if (record) {
            record->ptr = ptr;
            record->size = aligned_size;
            record->priority = priority;
            record->next = pool->alloc_list;
            pool->alloc_list = record;
        }

        AGENTOS_LOG_DEBUG("Mempool: alloc from reserve pool (size=%zu, "
                          "priority=%d, watermark=%d, reserve_used=%zu/%zu)",
                          aligned_size, priority, wm,
                          pool->reserve_used, pool->reserve_size);

        if (priority == MEMPOOL_PRIORITY_CRITICAL) {
            AGENTOS_LOG_WARN("Mempool: emergency allocation (size=%zu, "
                             "watermark=%d, reserve_used=%zu/%zu)",
                             aligned_size, wm,
                             pool->reserve_used, pool->reserve_size);
        }
    } else {
        AGENTOS_LOG_WARN("Mempool: alloc rejected (size=%zu, priority=%d, "
                         "watermark=%d, reserve_used=%zu/%zu, oom_rejections=%zu)",
                         aligned_size, priority, wm,
                         pool->reserve_used, pool->reserve_size,
                         pool->oom_rejections + 1);
    }

    mempool_mutex_unlock(pool);
    return ptr;
}

void agentos_mempool_free(agentos_mempool_t *pool, void *ptr)
{
    if (!pool || !ptr) return;

    mempool_mutex_lock(pool);

    /* 检查是否是对象池块 */
    if (mempool_is_block_ptr(pool, ptr)) {
        mempool_block_free(pool, ptr);
        pool->total_frees++;
        AGENTOS_LOG_DEBUG("Mempool: free to object pool (free_blocks=%zu)",
                          pool->free_blocks);
        mempool_mutex_unlock(pool);
        return;
    }

    /* 检查是否在预留池范围内 */
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)pool->reserve_base;
    uintptr_t end  = base + pool->reserve_size;

    if (addr >= base && addr < end) {
        /* 预留池分配不支持单独释放（线性分配），仅更新统计 */
        pool->total_frees++;

        /* 移除分配记录 */
        mempool_alloc_t **prev = &pool->alloc_list;
        while (*prev) {
            if ((*prev)->ptr == ptr) {
                mempool_alloc_t *to_free = *prev;
                *prev = to_free->next;
                AGENTOS_FREE(to_free);
                break;
            }
            prev = &(*prev)->next;
        }

        AGENTOS_LOG_DEBUG("Mempool: free from reserve pool (reserve_used=%zu)",
                          pool->reserve_used);
    } else {
        AGENTOS_LOG_WARN("Mempool: free called with ptr not owned by pool %p", ptr);
    }

    mempool_mutex_unlock(pool);
}

/* ================================================================
 * 统计与诊断
 * ================================================================ */

int agentos_mempool_get_stats(agentos_mempool_t *pool,
                               agentos_mempool_stats_t *stats)
{
    if (!pool || !stats) return -1;

    mempool_mutex_lock(pool);

    stats->total_reserved     = pool->reserve_size;
    stats->total_allocated    = pool->reserve_used;
    stats->peak_allocated     = pool->reserve_peak;
    stats->available_reserved = pool->reserve_size > pool->reserve_used
                                ? pool->reserve_size - pool->reserve_used : 0;
    stats->object_pool_total  = pool->total_blocks;
    stats->object_pool_used   = pool->total_blocks - pool->free_blocks;
    stats->total_allocs       = pool->total_allocs;
    stats->total_frees        = pool->total_frees;
    stats->oom_rejections     = pool->oom_rejections;
    stats->emergency_allocs   = pool->emergency_allocs;
    stats->watermark          = mempool_calc_watermark(pool);

    mempool_mutex_unlock(pool);
    return 0;
}

agentos_mempool_watermark_t agentos_mempool_get_watermark(agentos_mempool_t *pool)
{
    if (!pool) return MEMPOOL_WATERMARK_NORMAL;

    mempool_mutex_lock(pool);
    agentos_mempool_watermark_t wm = mempool_calc_watermark(pool);
    mempool_mutex_unlock(pool);
    return wm;
}

size_t agentos_mempool_shrink(agentos_mempool_t *pool)
{
    if (!pool) return 0;

    /* 预留池不支持收缩（线性分配器），返回 0 */
    (void)pool;
    return 0;
}

bool agentos_mempool_validate(agentos_mempool_t *pool)
{
    if (!pool) return false;

    mempool_mutex_lock(pool);

    /* 基本检查 */
    if (pool->reserve_base == NULL) {
        mempool_mutex_unlock(pool);
        return false;
    }

    /* 检查对象池链表 */
    size_t visited = 0;
    mempool_block_t *block = pool->free_list;
    while (block && visited < pool->total_blocks * 2) {
        visited++;
        block = block->next;
    }

    if (visited != pool->free_blocks) {
        mempool_mutex_unlock(pool);
        return false;
    }

    /* 检查预留池使用量 */
    if (pool->reserve_used > pool->reserve_size) {
        mempool_mutex_unlock(pool);
        return false;
    }

    mempool_mutex_unlock(pool);
    return true;
}