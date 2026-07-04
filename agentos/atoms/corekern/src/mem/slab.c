/**
 * @file slab.c
 * @brief P3.15: Slab 分配器实现 — per-CPU freelist + 全局 partial 链 + 构造/析构回调
 *
 * 实现固定大小对象的高效分配/释放。
 * 每个 CPU 维护一个本地 freelist（无锁），
 * 全局 partial 链在 CPU 间共享（持锁访问）。
 *
 * 分配路径（热路径）：
 *   1. per-CPU freelist 弹出 → 命中（无锁 O(1)）
 *   2. 全局 partial 链弹出 → 命中（持锁 O(1)）
 *   3. 分配新页 → 冷路径（持锁）
 *
 * 释放路径（热路径）：
 *   1. 检查 per-CPU freelist 是否已满 → 未满则推入（无锁 O(1)）
 *   2. 已满则批量归还全局 partial 链（持锁）
 *
 * 性能目标：高频分配场景性能提升 > 20%
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "slab.h"
#include "mem.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** 每个 per-CPU freelist 的最大对象数（超过则归还全局链） */
#define SLAB_CPU_FREELIST_MAX    64

/** 每次批量归还到全局链的对象数 */
#define SLAB_BATCH_RETURN_SIZE    16

/** 页面对齐 */
#define SLAB_PAGE_ALIGN          64

/** Cache line 对齐 */
#define SLAB_CACHE_LINE_ALIGN    64

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief Slab 页 — 一个连续内存块，包含固定数量的对象
 *
 * 每个页维护一个位图表示哪些对象已分配。
 * 已分配到 freelist 的对象也在位图中标记。
 */
typedef struct slab_page {
    struct slab_page *next;        /**< 全局链中的下一页 */
    struct slab_page *prev;        /**< 全局链中的上一页 */
    uint8_t          *bitmap;      /**< 分配位图 (1=已分配, 0=空闲) */
    uint32_t          obj_count;   /**< 本页总对象数 */
    uint32_t          free_count;  /**< 本页空闲对象数 */
    uint32_t          page_id;     /**< 页 ID（调试用） */
    uint8_t           data[];      /**< 对齐后的对象数据区 */
} slab_page_t;

/**
 * @brief Per-CPU 空闲链表节点
 */
typedef struct slab_freelist_node {
    struct slab_freelist_node *next;
} slab_freelist_node_t;

/**
 * @brief Per-CPU 缓存
 *
 * 使用 _Thread_local 存储，每个线程独立。
 * 分配/释放优先在此完成，无锁操作。
 */
typedef struct {
    slab_freelist_node_t *head;    /**< 空闲链表头 */
    uint32_t              count;   /**< 当前空闲对象数 */
    int                   cpu_id;  /**< CPU ID */
} slab_cpu_cache_t;

/**
 * @brief Slab 分配器主结构
 */
struct agentos_slab {
    char                      name[AGENTOS_SLAB_NAME_MAX]; /**< 名称 */
    size_t                    obj_size;                    /**< 对象大小 */
    size_t                    obj_aligned;                 /**< 对齐后对象大小 */
    uint32_t                  objs_per_page;               /**< 每页对象数 */
    uint32_t                  page_size;                   /**< 页总大小 */
    agentos_slab_ctor_fn      ctor;                        /**< 构造回调 */
    agentos_slab_dtor_fn      dtor;                        /**< 析构回调 */
    void                     *ctor_arg;                    /**< 构造参数 */
    void                     *dtor_arg;                    /**< 析构参数 */
    bool                      enable_stats;                /**< 统计开关 */

    /* 全局 partial 链 — 持锁访问 */
    slab_page_t              *partial_head;                /**< partial 链头 */
    slab_page_t              *partial_tail;                /**< partial 链尾 */
    pthread_mutex_t           partial_lock;                /**< 全局链锁 */

    /* 统计 */
    atomic_uint_fast64_t      total_allocs;
    atomic_uint_fast64_t      total_frees;
    atomic_uint_fast64_t      cpu_hit_count;
    atomic_uint_fast64_t      cpu_miss_count;
    atomic_uint_fast64_t      partial_hit_count;
    atomic_uint_fast64_t      page_alloc_count;
    atomic_uint_fast64_t      page_free_count;
    atomic_uint_fast64_t      ctor_fail_count;
    atomic_uint               active_objs;
    atomic_uint               peak_objs;
    atomic_uint               page_id_counter;

    bool                      initialized;
    bool                      destroyed;
    pthread_mutex_t           init_lock;
};

/* ================================================================
 * Per-CPU 缓存 (Thread-Local Storage)
 * ================================================================ */

/**
 * @brief Per-CPU 缓存（每个线程独立）
 *
 * 使用 _Thread_local 确保每个线程有独立的 freelist。
 * 初始化时惰性创建，无需预先分配。
 */
static _Thread_local slab_cpu_cache_t g_cpu_cache;

/* ================================================================
 * 辅助函数
 * ================================================================ */

/**
 * @brief 对齐到 cache line 边界
 */
static inline size_t align_to_cache_line(size_t size)
{
    return (size + SLAB_CACHE_LINE_ALIGN - 1) & ~((size_t)(SLAB_CACHE_LINE_ALIGN - 1));
}

/**
 * @brief 计算位图大小（字节）
 */
static inline size_t bitmap_size(uint32_t obj_count)
{
    return (obj_count + 7) / 8;
}

/**
 * @brief 设置位图中第 index 位为 1
 */
static inline void bitmap_set(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] |= (uint8_t)(1u << (index % 8));
}

/**
 * @brief 清除位图中第 index 位为 0
 */
static inline void bitmap_clear(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] &= (uint8_t)(~(1u << (index % 8)));
}

/**
 * @brief 检查位图中第 index 位
 */
static inline bool bitmap_test(const uint8_t *bitmap, uint32_t index)
{
    return (bitmap[index / 8] & (1u << (index % 8))) != 0;
}

/**
 * @brief 在位图中查找第一个空闲位
 */
static int32_t bitmap_find_free(const uint8_t *bitmap, uint32_t obj_count)
{
    size_t bytes = bitmap_size(obj_count);
    for (size_t i = 0; i < bytes; i++) {
        if (bitmap[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                uint32_t idx = (uint32_t)(i * 8 + bit);
                if (idx >= obj_count) return -1;
                if (!(bitmap[i] & (1u << bit))) {
                    return (int32_t)idx;
                }
            }
        }
    }
    return -1;
}

/**
 * @brief 分配新 slab 页
 */
static slab_page_t *slab_page_alloc(agentos_slab_t *slab, uint32_t page_id)
{
    size_t total = sizeof(slab_page_t) + slab->page_size;
    slab_page_t *page = (slab_page_t *)agentos_mem_alloc(total);
    if (!page) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 PAGE-ALLOC-FAIL size=%zu page_id=%u",
                          slab->name, total, page_id);
        return NULL;
    }

    page->next       = NULL;
    page->prev       = NULL;
    page->bitmap     = page->data;
    page->obj_count  = slab->objs_per_page;
    page->free_count = slab->objs_per_page;
    page->page_id    = page_id;

    /* 位图清零 */
    memset(page->bitmap, 0, bitmap_size(slab->objs_per_page));

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 PAGE-ALLOC id=%u objs=%u "
                      "page_size=%u total=%zu",
                      slab->name, page_id, page->obj_count,
                      slab->page_size, total);

    return page;
}

/**
 * @brief 释放 slab 页
 */
static void slab_page_free(agentos_slab_t *slab, slab_page_t *page)
{
    if (!page) return;

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 PAGE-FREE id=%u free_count=%u/%u",
                      slab->name, page->page_id,
                      page->free_count, page->obj_count);

    agentos_mem_free(page);
}

/**
 * @brief 从全局 partial 链获取一个空闲对象
 *
 * 遍历 partial 链，找到第一个有空闲对象的页。
 * 如果所有页都已满，返回 NULL。
 */
static void *partial_list_pop(agentos_slab_t *slab)
{
    slab_page_t *page = slab->partial_head;
    while (page) {
        if (page->free_count > 0) {
            int32_t idx = bitmap_find_free(page->bitmap, page->obj_count);
            if (idx >= 0) {
                bitmap_set(page->bitmap, (uint32_t)idx);
                page->free_count--;

                /* 如果该页已满，从 partial 链移除 */
                if (page->free_count == 0) {
                    if (page->prev) page->prev->next = page->next;
                    else slab->partial_head = page->next;
                    if (page->next) page->next->prev = page->prev;
                    else slab->partial_tail = page->prev;
                    page->next = NULL;
                    page->prev = NULL;
                }

                void *obj = (uint8_t *)page->data +
                            bitmap_size(page->obj_count) +
                            (size_t)idx * slab->obj_aligned;

                AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 PARTIAL-POP page=%u idx=%d "
                                  "free=%u/%u",
                                  slab->name, page->page_id, idx,
                                  page->free_count, page->obj_count);

                atomic_fetch_add(&slab->partial_hit_count, 1);
                return obj;
            }
        }
        page = page->next;
    }

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 PARTIAL-POP-MISS (no free objects in partial list)",
                      slab->name);

    return NULL;
}

/**
 * @brief 将对象归还到全局 partial 链
 *
 * 查找对象所属的页，归还对象。
 * 如果页从满变空，如果只有这一页则保留，否则释放。
 */
static void partial_list_push(agentos_slab_t *slab, void *obj)
{
    if (!obj) {
        AGENTOS_LOG_WARN("Slab[%s]: P3.15 PARTIAL-PUSH-NULL ignored", slab->name);
        return;
    }

    /* 遍历 partial 链查找对象所属的页 */
    slab_page_t *page = slab->partial_head;
    bool found = false;

    while (page) {
        uint8_t *page_start = page->data + bitmap_size(page->obj_count);
        uint8_t *page_end   = page_start + (size_t)page->obj_count * slab->obj_aligned;
        if ((uint8_t *)obj >= page_start && (uint8_t *)obj < page_end) {
            found = true;
            break;
        }
        page = page->next;
    }

    if (!found) {
        AGENTOS_LOG_WARN("Slab[%s]: P3.15 PARTIAL-PUSH unknown obj=%p", slab->name, obj);
        return;
    }

    size_t offset = (uint8_t *)obj - (page->data + bitmap_size(page->obj_count));
    uint32_t idx = (uint32_t)(offset / slab->obj_aligned);

    if (idx >= page->obj_count) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 PARTIAL-PUSH bad index %u (max=%u)",
                          slab->name, idx, page->obj_count);
        return;
    }

    bool was_full = (page->free_count == 0);
    bitmap_clear(page->bitmap, idx);
    page->free_count++;

    /* 如果之前是满的，重新加入 partial 链 */
    if (was_full) {
        page->next = NULL;
        page->prev = slab->partial_tail;
        if (slab->partial_tail) {
            slab->partial_tail->next = page;
        } else {
            slab->partial_head = page;
        }
        slab->partial_tail = page;
    }

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 PARTIAL-PUSH page=%u idx=%u "
                      "free=%u/%u was_full=%d",
                      slab->name, page->page_id, idx,
                      page->free_count, page->obj_count, was_full);

    /* 如果页完全空闲且不止一页，考虑回收 */
    /* 保留至少一页在 partial 链中作为缓存 */
    if (page->free_count == page->obj_count &&
        (slab->partial_head != slab->partial_tail)) {
        /* 从 partial 链移除 */
        if (page->prev) page->prev->next = page->next;
        else slab->partial_head = page->next;
        if (page->next) page->next->prev = page->prev;
        else slab->partial_tail = page->prev;

        AGENTOS_LOG_INFO("Slab[%s]: P3.15 PAGE-RECLAIM id=%u free=%u/%u "
                         "(all objects returned)",
                         slab->name, page->page_id,
                         page->free_count, page->obj_count);

        atomic_fetch_add(&slab->page_free_count, 1);
        slab_page_free(slab, page);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

agentos_slab_t *agentos_slab_create(const agentos_slab_config_t *config)
{
    if (!config) {
        AGENTOS_LOG_ERROR("Slab: P3.15 CREATE-FAIL — NULL config");
        return NULL;
    }
    if (config->obj_size == 0) {
        AGENTOS_LOG_ERROR("Slab: P3.15 CREATE-FAIL — obj_size must be > 0");
        return NULL;
    }

    agentos_slab_t *slab = (agentos_slab_t *)AGENTOS_CALLOC(1, sizeof(*slab));
    if (!slab) {
        AGENTOS_LOG_ERROR("Slab: P3.15 CREATE-FAIL — OOM allocating slab struct");
        return NULL;
    }

    /* 复制名称 */
    if (config->name && config->name[0]) {
        AGENTOS_STRNCPY_TERM(slab->name, config->name, sizeof(slab->name));
    } else {
        snprintf(slab->name, sizeof(slab->name), "slab_%p", (void *)slab);
    }

    /* 对象大小对齐到 cache line */
    slab->obj_size    = config->obj_size;
    slab->obj_aligned = align_to_cache_line(config->obj_size);

    /* 每页对象数 */
    slab->objs_per_page = config->objs_per_page > 0
                          ? config->objs_per_page
                          : AGENTOS_SLAB_DEFAULT_OBJS_PER_PAGE;

    /* 计算页大小：位图 + 对齐后的对象数组 */
    size_t bm_size = bitmap_size(slab->objs_per_page);
    slab->page_size = align_to_cache_line(bm_size + slab->objs_per_page * slab->obj_aligned);

    slab->ctor         = config->ctor;
    slab->dtor         = config->dtor;
    slab->ctor_arg     = config->ctor_arg;
    slab->dtor_arg     = config->dtor_arg;
    slab->enable_stats = config->enable_stats;

    /* 初始化全局 partial 链 */
    slab->partial_head = NULL;
    slab->partial_tail = NULL;

    if (pthread_mutex_init(&slab->partial_lock, NULL) != 0) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 CREATE-FAIL — mutex_init failed",
                          slab->name);
        AGENTOS_FREE(slab);
        return NULL;
    }

    if (pthread_mutex_init(&slab->init_lock, NULL) != 0) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 CREATE-FAIL — init_lock failed",
                          slab->name);
        pthread_mutex_destroy(&slab->partial_lock);
        AGENTOS_FREE(slab);
        return NULL;
    }

    slab->initialized = true;
    slab->destroyed   = false;

    AGENTOS_LOG_INFO("Slab[%s]: P3.15 CREATE obj_size=%zu aligned=%zu "
                     "objs_per_page=%u page_size=%u ctor=%s dtor=%s",
                     slab->name, slab->obj_size, slab->obj_aligned,
                     slab->objs_per_page, slab->page_size,
                     slab->ctor ? "yes" : "no",
                     slab->dtor ? "yes" : "no");

    return slab;
}

void agentos_slab_destroy(agentos_slab_t *slab)
{
    if (!slab) {
        AGENTOS_LOG_WARN("Slab: P3.15 DESTROY — NULL slab ignored");
        return;
    }
    if (slab->destroyed) {
        AGENTOS_LOG_WARN("Slab[%s]: P3.15 DESTROY — already destroyed", slab->name);
        return;
    }

    uint32_t active = atomic_load(&slab->active_objs);
    uint32_t peak   = atomic_load(&slab->peak_objs);
    uint64_t total_allocs = atomic_load(&slab->total_allocs);
    uint64_t total_frees  = atomic_load(&slab->total_frees);
    uint64_t cpu_hits     = atomic_load(&slab->cpu_hit_count);
    uint64_t cpu_misses   = atomic_load(&slab->cpu_miss_count);
    uint64_t total_hits   = cpu_hits + cpu_misses;
    double hit_rate = total_hits > 0
                      ? (double)cpu_hits / (double)total_hits * 100.0
                      : 0.0;

    AGENTOS_LOG_INFO("Slab[%s]: P3.15 DESTROY allocs=%llu frees=%llu "
                     "active=%u peak=%u cpu_hits=%llu cpu_misses=%llu "
                     "hit_rate=%.1f%% pages=%llu/%llu",
                     slab->name,
                     (unsigned long long)total_allocs,
                     (unsigned long long)total_frees,
                     active, peak,
                     (unsigned long long)cpu_hits,
                     (unsigned long long)cpu_misses,
                     hit_rate,
                     (unsigned long long)atomic_load(&slab->page_alloc_count),
                     (unsigned long long)atomic_load(&slab->page_free_count));

    if (active > 0) {
        AGENTOS_LOG_WARN("Slab[%s]: P3.15 DESTROY — %u objects still active "
                         "(potential leak)", slab->name, active);
    }

    /* 释放全局 partial 链中的所有页 */
    pthread_mutex_lock(&slab->partial_lock);
    slab_page_t *page = slab->partial_head;
    size_t freed_pages = 0;
    while (page) {
        slab_page_t *next = page->next;
        slab_page_free(slab, page);
        freed_pages++;
        page = next;
    }
    slab->partial_head = NULL;
    slab->partial_tail = NULL;
    pthread_mutex_unlock(&slab->partial_lock);

    AGENTOS_LOG_INFO("Slab[%s]: P3.15 DESTROY — freed %zu pages from partial list",
                     slab->name, freed_pages);

    slab->initialized = false;
    slab->destroyed   = true;
    pthread_mutex_destroy(&slab->partial_lock);
    pthread_mutex_destroy(&slab->init_lock);
    AGENTOS_FREE(slab);
}

void *agentos_slab_alloc(agentos_slab_t *slab)
{
    if (!slab) {
        AGENTOS_LOG_ERROR("Slab: P3.15 ALLOC-FAIL — NULL slab");
        return NULL;
    }
    if (!slab->initialized || slab->destroyed) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 ALLOC-FAIL — not initialized or destroyed",
                          slab->name);
        return NULL;
    }

    void *obj = NULL;

    /* 步骤 1: 尝试 per-CPU freelist（无锁） */
    if (g_cpu_cache.head && g_cpu_cache.count > 0) {
        obj = g_cpu_cache.head;
        g_cpu_cache.head = g_cpu_cache.head->next;
        g_cpu_cache.count--;
        atomic_fetch_add(&slab->cpu_hit_count, 1);
        goto alloc_success;
    }

    atomic_fetch_add(&slab->cpu_miss_count, 1);

    /* 步骤 2: 尝试全局 partial 链（持锁） */
    pthread_mutex_lock(&slab->partial_lock);
    obj = partial_list_pop(slab);
    pthread_mutex_unlock(&slab->partial_lock);

    if (obj) {
        goto alloc_success;
    }

    /* 步骤 3: 分配新页 */
    uint32_t page_id = atomic_fetch_add(&slab->page_id_counter, 1);
    slab_page_t *page = slab_page_alloc(slab, page_id);
    if (!page) {
        goto alloc_fail;
    }

    atomic_fetch_add(&slab->page_alloc_count, 1);

    /* 从新页取第一个对象 */
    int32_t idx = bitmap_find_free(page->bitmap, page->obj_count);
    if (idx < 0) {
        AGENTOS_LOG_ERROR("Slab[%s]: P3.15 ALLOC-FAIL — new page has no free obj",
                          slab->name);
        slab_page_free(slab, page);
        goto alloc_fail;
    }

    bitmap_set(page->bitmap, (uint32_t)idx);
    page->free_count--;

    /* 如果页还有空闲对象，加入全局 partial 链 */
    if (page->free_count > 0) {
        pthread_mutex_lock(&slab->partial_lock);
        page->next = NULL;
        page->prev = slab->partial_tail;
        if (slab->partial_tail) {
            slab->partial_tail->next = page;
        } else {
            slab->partial_head = page;
        }
        slab->partial_tail = page;
        pthread_mutex_unlock(&slab->partial_lock);
    }

    obj = (uint8_t *)page->data + bitmap_size(page->obj_count) +
          (size_t)idx * slab->obj_aligned;

alloc_success:
    /* 步骤 4: 调用构造回调 */
    if (slab->ctor) {
        void *constructed = slab->ctor(obj, slab->ctor_arg);
        if (!constructed) {
            /* 构造失败，回收对象 */
            AGENTOS_LOG_WARN("Slab[%s]: P3.15 ALLOC — ctor failed for obj=%p",
                             slab->name, obj);
            atomic_fetch_add(&slab->ctor_fail_count, 1);

            /* 将对象归还到 per-CPU freelist */
            slab_freelist_node_t *node = (slab_freelist_node_t *)obj;
            node->next = g_cpu_cache.head;
            g_cpu_cache.head = node;
            g_cpu_cache.count++;
            return NULL;
        }
        obj = constructed;
    }

    /* 更新统计 */
    atomic_fetch_add(&slab->total_allocs, 1);
    uint32_t active = atomic_fetch_add(&slab->active_objs, 1) + 1;
    uint32_t peak = atomic_load(&slab->peak_objs);
    while (active > peak) {
        if (atomic_compare_exchange_weak(&slab->peak_objs, &peak, active)) {
            break;
        }
        peak = atomic_load(&slab->peak_objs);
    }

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 ALLOC obj=%p active=%u peak=%u",
                      slab->name, obj, active, peak);

    return obj;

alloc_fail:
    AGENTOS_LOG_ERROR("Slab[%s]: P3.15 ALLOC-FAIL — no memory available", slab->name);
    return NULL;
}

void agentos_slab_free(agentos_slab_t *slab, void *obj)
{
    if (!slab) {
        AGENTOS_LOG_ERROR("Slab: P3.15 FREE-FAIL — NULL slab");
        return;
    }
    if (!obj) {
        AGENTOS_LOG_WARN("Slab[%s]: P3.15 FREE — NULL obj ignored", slab->name);
        return;
    }

    /* 步骤 1: 调用析构回调 */
    if (slab->dtor) {
        slab->dtor(obj, slab->dtor_arg);
    }

    /* 步骤 2: 加入 per-CPU freelist（无锁） */
    if (g_cpu_cache.count < SLAB_CPU_FREELIST_MAX) {
        slab_freelist_node_t *node = (slab_freelist_node_t *)obj;
        node->next = g_cpu_cache.head;
        g_cpu_cache.head = node;
        g_cpu_cache.count++;

        atomic_fetch_add(&slab->total_frees, 1);
        atomic_fetch_sub(&slab->active_objs, 1);

        AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 FREE obj=%p cpu_freelist=%u",
                          slab->name, obj, g_cpu_cache.count);
        return;
    }

    /* 步骤 3: per-CPU freelist 已满，批量归还到全局 partial 链 */
    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 FREE — cpu_freelist full (%u), "
                      "batch returning %u objects",
                      slab->name, g_cpu_cache.count, SLAB_BATCH_RETURN_SIZE);

    pthread_mutex_lock(&slab->partial_lock);

    /* 批量归还 SLAB_BATCH_RETURN_SIZE 个对象 */
    uint32_t returned = 0;
    slab_freelist_node_t *batch_head = NULL;
    slab_freelist_node_t *batch_tail = NULL;

    for (uint32_t i = 0; i < SLAB_BATCH_RETURN_SIZE && g_cpu_cache.head; i++) {
        slab_freelist_node_t *node = g_cpu_cache.head;
        g_cpu_cache.head = node->next;
        g_cpu_cache.count--;

        node->next = NULL;
        if (!batch_head) {
            batch_head = node;
            batch_tail = node;
        } else {
            batch_tail->next = node;
            batch_tail = node;
        }
        returned++;
    }

    pthread_mutex_unlock(&slab->partial_lock);

    /* 将批量的对象归还到 partial 链 */
    slab_freelist_node_t *node = batch_head;
    while (node) {
        slab_freelist_node_t *next = node->next;
        pthread_mutex_lock(&slab->partial_lock);
        partial_list_push(slab, node);
        pthread_mutex_unlock(&slab->partial_lock);
        atomic_fetch_add(&slab->total_frees, 1);
        atomic_fetch_sub(&slab->active_objs, 1);
        node = next;
    }

    /* 将当前对象也加入 per-CPU freelist（刚才已经清空了足够空间） */
    slab_freelist_node_t *curr_node = (slab_freelist_node_t *)obj;
    curr_node->next = g_cpu_cache.head;
    g_cpu_cache.head = curr_node;
    g_cpu_cache.count++;
    atomic_fetch_add(&slab->total_frees, 1);
    atomic_fetch_sub(&slab->active_objs, 1);

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 FREE obj=%p batch_returned=%u "
                      "active=%u cpu_freelist=%u",
                      slab->name, obj, returned,
                      atomic_load(&slab->active_objs),
                      g_cpu_cache.count);
}

/* ================================================================
 * 统计与辅助
 * ================================================================ */

void agentos_slab_get_stats(agentos_slab_t *slab,
                             agentos_slab_stats_t *out_stats)
{
    if (!out_stats) {
        AGENTOS_LOG_WARN("Slab: P3.15 STATS — NULL out_stats ignored");
        return;
    }

    if (!slab) {
        memset(out_stats, 0, sizeof(*out_stats));
        return;
    }

    out_stats->total_allocs     = atomic_load(&slab->total_allocs);
    out_stats->total_frees      = atomic_load(&slab->total_frees);
    out_stats->cpu_hit_count    = atomic_load(&slab->cpu_hit_count);
    out_stats->cpu_miss_count   = atomic_load(&slab->cpu_miss_count);
    out_stats->partial_hit_count = atomic_load(&slab->partial_hit_count);
    out_stats->page_alloc_count  = atomic_load(&slab->page_alloc_count);
    out_stats->page_free_count   = atomic_load(&slab->page_free_count);
    out_stats->ctor_fail_count   = atomic_load(&slab->ctor_fail_count);
    out_stats->active_objs       = atomic_load(&slab->active_objs);
    out_stats->peak_objs         = atomic_load(&slab->peak_objs);
    out_stats->cpu_freelist_len  = g_cpu_cache.count;

    uint64_t total_hits = out_stats->cpu_hit_count + out_stats->cpu_miss_count;
    out_stats->cpu_hit_rate = total_hits > 0
                              ? (double)out_stats->cpu_hit_count / (double)total_hits
                              : 0.0;

    AGENTOS_LOG_DEBUG("Slab[%s]: P3.15 STATS allocs=%llu frees=%llu "
                      "cpu_hits=%llu cpu_misses=%llu hit_rate=%.1f%% "
                      "active=%u peak=%u freelist=%u pages=%llu/%llu",
                      slab->name,
                      (unsigned long long)out_stats->total_allocs,
                      (unsigned long long)out_stats->total_frees,
                      (unsigned long long)out_stats->cpu_hit_count,
                      (unsigned long long)out_stats->cpu_miss_count,
                      out_stats->cpu_hit_rate * 100.0,
                      out_stats->active_objs, out_stats->peak_objs,
                      out_stats->cpu_freelist_len,
                      (unsigned long long)out_stats->page_alloc_count,
                      (unsigned long long)out_stats->page_free_count);
}

void agentos_slab_reset_stats(agentos_slab_t *slab)
{
    if (!slab) return;

    AGENTOS_LOG_INFO("Slab[%s]: P3.15 STATS-RESET", slab->name);

    atomic_store(&slab->total_allocs, 0);
    atomic_store(&slab->total_frees, 0);
    atomic_store(&slab->cpu_hit_count, 0);
    atomic_store(&slab->cpu_miss_count, 0);
    atomic_store(&slab->partial_hit_count, 0);
    atomic_store(&slab->page_alloc_count, 0);
    atomic_store(&slab->page_free_count, 0);
    atomic_store(&slab->ctor_fail_count, 0);
    atomic_store(&slab->peak_objs, atomic_load(&slab->active_objs));
}

size_t agentos_slab_obj_size(agentos_slab_t *slab)
{
    return slab ? slab->obj_size : 0;
}

const char *agentos_slab_name(agentos_slab_t *slab)
{
    return slab ? slab->name : "null";
}