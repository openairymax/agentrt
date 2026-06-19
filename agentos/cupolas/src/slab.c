/**
 * @file slab.c
 * @brief P3.15: Slab 分配器实现 — per-CPU freelist + 全局 partial 链
 *
 * 实现细节：
 *   - 每个 Slab 包含多个 slab 页（page），每页包含固定数量对象
 *   - 每页维护一个 freelist（空闲对象链表）
 *   - 页按状态分为：FULL（无空闲对象）、PARTIAL（部分空闲）、EMPTY（全空闲）
 *   - 全局 partial 链表用于 CPU 间负载均衡（steal 机制）
 *   - 分配时优先从当前 CPU 的 partial 页获取，失败时从全局 partial 链 steal
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE
#include "slab.h"
#include "memory_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/* ================================================================
 * 常量
 * ================================================================ */

/** 默认每页对象数（根据对象大小计算） */
#define SLAB_DEFAULT_OBJS_PER_PAGE(obj_size) \
    ((4096 - sizeof(struct slab_page)) / (obj_size))

/** 最大 CPU 数 */
#define SLAB_MAX_CPUS 128

/** 对齐到指针大小 */
#define SLAB_ALIGN(s) (((s) + sizeof(void *) - 1) & ~(sizeof(void *) - 1))

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/** slab 页状态 */
typedef enum {
    SLAB_PAGE_EMPTY   = 0,
    SLAB_PAGE_PARTIAL = 1,
    SLAB_PAGE_FULL    = 2,
} slab_page_state_t;

/** slab 页 */
typedef struct slab_page {
    struct slab_page *next;       /**< 链表指针 */
    void *freelist;               /**< 空闲对象链表头 */
    size_t free_count;            /**< 空闲对象数 */
    size_t total_count;           /**< 总对象数 */
    slab_page_state_t state;      /**< 页状态 */
    char data[];                  /**< 对象数据区 */
} slab_page_t;

/** per-CPU 缓存 */
typedef struct {
    slab_page_t *partial;         /**< 当前 partial 页 */
    size_t alloc_count;           /**< 本 CPU 分配计数 */
    size_t free_count;            /**< 本 CPU 释放计数 */
} slab_cpu_cache_t;

/** Slab 分配器 */
struct agentos_slab {
    size_t obj_size;              /**< 对象大小（对齐后） */
    size_t objs_per_page;         /**< 每页对象数 */
    size_t page_size;             /**< 每页总大小 */

    agentos_slab_ctor_t ctor;     /**< 构造回调 */
    agentos_slab_dtor_t dtor;     /**< 析构回调 */
    void *ctor_user_data;         /**< 构造/析构回调用户数据 */

    /* 全局 partial 链 */
    slab_page_t *partial_list;    /**< 全局 partial 链表头 */
    size_t partial_count;         /**< 全局 partial 页数 */

    /* 统计 */
    size_t total_pages;           /**< 总页数 */
    size_t total_allocs;          /**< 总分配次数 */
    size_t total_frees;           /**< 总释放次数 */
    size_t cpu_steals;            /**< CPU 间 steal 次数 */

    /* 线程安全 */
#ifdef __linux__
    pthread_mutex_t lock;
#elif defined(_WIN32)
    CRITICAL_SECTION lock;
#endif

    /* per-CPU 缓存 */
    slab_cpu_cache_t cpu_caches[SLAB_MAX_CPUS];
    size_t cpu_count;
};

/* ================================================================
 * 平台抽象
 * ================================================================ */

static int slab_mutex_init(struct agentos_slab *slab)
{
#ifdef __linux__
    return pthread_mutex_init(&slab->lock, NULL);
#elif defined(_WIN32)
    InitializeCriticalSection(&slab->lock);
    return 0;
#else
    (void)slab;
    return 0;
#endif
}

static void slab_mutex_lock(struct agentos_slab *slab)
{
#ifdef __linux__
    pthread_mutex_lock(&slab->lock);
#elif defined(_WIN32)
    EnterCriticalSection(&slab->lock);
#else
    (void)slab;
#endif
}

static void slab_mutex_unlock(struct agentos_slab *slab)
{
#ifdef __linux__
    pthread_mutex_unlock(&slab->lock);
#elif defined(_WIN32)
    LeaveCriticalSection(&slab->lock);
#else
    (void)slab;
#endif
}

static void slab_mutex_destroy(struct agentos_slab *slab)
{
#ifdef __linux__
    pthread_mutex_destroy(&slab->lock);
#elif defined(_WIN32)
    DeleteCriticalSection(&slab->lock);
#else
    (void)slab;
#endif
}

static int slab_get_cpu_id(void)
{
#ifdef __linux__
    return sched_getcpu();
#elif defined(_WIN32)
    return (int)GetCurrentProcessorNumber();
#else
    return 0;
#endif
}

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 分配一个新的 slab 页
 */
static slab_page_t *slab_page_create(agentos_slab_t *slab)
{
    size_t data_size = slab->obj_size * slab->objs_per_page;
    size_t total_size = sizeof(slab_page_t) + data_size;

    slab_page_t *page = (slab_page_t *)AGENTOS_CALLOC(1, total_size);
    if (!page) {
        AGENTOS_LOG_ERROR("Slab: OOM creating page (obj_size=%zu, objs_per_page=%zu, "
                          "total_size=%zu)",
                          slab->obj_size, slab->objs_per_page, total_size);
        return NULL;
    }

    page->total_count = slab->objs_per_page;
    page->free_count = slab->objs_per_page;
    page->state = SLAB_PAGE_EMPTY;

    /* 初始化 freelist：将所有对象链接为单链表 */
    page->freelist = page->data;
    char *obj = (char *)page->data;
    for (size_t i = 0; i < slab->objs_per_page - 1; i++) {
        void **next_ptr = (void **)(obj + i * slab->obj_size);
        *next_ptr = (void *)(obj + (i + 1) * slab->obj_size);
    }
    /* 最后一个对象的 next 为 NULL */
    void **last_ptr = (void **)(obj + (slab->objs_per_page - 1) * slab->obj_size);
    *last_ptr = NULL;

    AGENTOS_LOG_DEBUG("Slab: created page (obj_size=%zu, objs=%zu, total_pages=%zu)",
                      slab->obj_size, slab->objs_per_page, slab->total_pages + 1);

    return page;
}

/**
 * @brief 释放一个 slab 页
 */
static void slab_page_destroy(slab_page_t *page)
{
    AGENTOS_FREE(page);
}

/**
 * @brief 从页的 freelist 弹出一个对象
 */
static void *slab_page_pop(slab_page_t *page)
{
    if (!page->freelist || page->free_count == 0) return NULL;

    void *obj = page->freelist;
    page->freelist = *(void **)obj;
    page->free_count--;

    /* 更新状态 */
    if (page->free_count == 0) {
        page->state = SLAB_PAGE_FULL;
    } else {
        page->state = SLAB_PAGE_PARTIAL;
    }

    return obj;
}

/**
 * @brief 将对象推回页的 freelist
 */
static void slab_page_push(slab_page_t *page, void *obj)
{
    if (!obj) return;

    *(void **)obj = page->freelist;
    page->freelist = obj;
    page->free_count++;

    /* 更新状态 */
    if (page->free_count == page->total_count) {
        page->state = SLAB_PAGE_EMPTY;
    } else {
        page->state = SLAB_PAGE_PARTIAL;
    }
}

/**
 * @brief 从全局 partial 链获取一个页（或 steal）
 */
static slab_page_t *slab_get_partial_page(agentos_slab_t *slab)
{
    slab_mutex_lock(slab);

    if (slab->partial_list) {
        slab_page_t *page = slab->partial_list;
        slab->partial_list = page->next;
        page->next = NULL;
        slab->partial_count--;
        slab->cpu_steals++;
        slab_mutex_unlock(slab);
        return page;
    }

    slab_mutex_unlock(slab);
    return NULL;
}

/**
 * @brief 将页归还到全局 partial 链
 */
static void slab_return_partial_page(agentos_slab_t *slab, slab_page_t *page)
{
    if (!page) return;

    slab_mutex_lock(slab);
    page->next = slab->partial_list;
    slab->partial_list = page;
    slab->partial_count++;
    slab_mutex_unlock(slab);
}

/* ================================================================
 * 公共 API
 * ================================================================ */

agentos_slab_t *agentos_slab_create(size_t obj_size,
                                     size_t objs_per_slab,
                                     agentos_slab_ctor_t ctor,
                                     agentos_slab_dtor_t dtor,
                                     void *user_data)
{
    if (obj_size == 0) {
        AGENTOS_LOG_ERROR("Slab: create called with obj_size=0");
        return NULL;
    }

    agentos_slab_t *slab = (agentos_slab_t *)AGENTOS_CALLOC(1, sizeof(*slab));
    if (!slab) {
        AGENTOS_LOG_ERROR("Slab: OOM allocating slab struct");
        return NULL;
    }

    slab->obj_size = SLAB_ALIGN(obj_size);

    if (objs_per_slab == 0) {
        slab->objs_per_page = SLAB_DEFAULT_OBJS_PER_PAGE(slab->obj_size);
        if (slab->objs_per_page < 4) slab->objs_per_page = 4;
    } else {
        slab->objs_per_page = objs_per_slab;
    }

    slab->page_size = sizeof(slab_page_t) + slab->obj_size * slab->objs_per_page;
    slab->ctor = ctor;
    slab->dtor = dtor;
    slab->ctor_user_data = user_data;
    slab->cpu_count = SLAB_MAX_CPUS;

    if (slab_mutex_init(slab) != 0) {
        AGENTOS_LOG_ERROR("Slab: mutex init failed");
        AGENTOS_FREE(slab);
        return NULL;
    }

    AGENTOS_LOG_INFO("Slab: created (obj_size=%zu, aligned=%zu, objs_per_page=%zu, "
                     "page_size=%zu, ctor=%s, dtor=%s)",
                     obj_size, slab->obj_size, slab->objs_per_page,
                     slab->page_size,
                     ctor ? "yes" : "no", dtor ? "yes" : "no");

    return slab;
}

void agentos_slab_destroy(agentos_slab_t *slab)
{
    if (!slab) return;

    AGENTOS_LOG_INFO("Slab: destroying (total_pages=%zu, total_allocs=%zu, "
                     "total_frees=%zu, active=%zu, steals=%zu)",
                     slab->total_pages, slab->total_allocs, slab->total_frees,
                     slab->total_allocs - slab->total_frees, slab->cpu_steals);

    /* 释放全局 partial 链中的所有页 */
    slab_page_t *page = slab->partial_list;
    size_t freed_pages = 0;
    while (page) {
        slab_page_t *next = page->next;
        slab_page_destroy(page);
        page = next;
        freed_pages++;
    }

    /* 释放 per-CPU partial 页 */
    for (size_t i = 0; i < slab->cpu_count; i++) {
        if (slab->cpu_caches[i].partial) {
            slab_page_destroy(slab->cpu_caches[i].partial);
            freed_pages++;
        }
    }

    slab_mutex_destroy(slab);
    AGENTOS_FREE(slab);

    AGENTOS_LOG_INFO("Slab: destroyed (%zu pages freed)", freed_pages);
}

void *agentos_slab_alloc(agentos_slab_t *slab)
{
    if (!slab) return NULL;

    int cpu = slab_get_cpu_id();
    if (cpu < 0 || (size_t)cpu >= slab->cpu_count) cpu = 0;

    slab_cpu_cache_t *cache = &slab->cpu_caches[cpu];

    /* 1. 尝试从当前 CPU 的 partial 页分配 */
    if (cache->partial) {
        void *obj = slab_page_pop(cache->partial);
        if (obj) {
            cache->alloc_count++;
            slab->total_allocs++;
            if (slab->ctor) slab->ctor(obj, slab->ctor_user_data);
            AGENTOS_LOG_DEBUG("Slab: alloc from CPU%d cache (total_allocs=%zu)",
                              cpu, slab->total_allocs);
            return obj;
        }
        /* 当前 partial 页已满，归还到全局链 */
        AGENTOS_LOG_DEBUG("Slab: CPU%d partial page full, returning to global chain", cpu);
        slab_return_partial_page(slab, cache->partial);
        cache->partial = NULL;
    }

    /* 2. 尝试从全局 partial 链 steal */
    cache->partial = slab_get_partial_page(slab);
    if (cache->partial) {
        void *obj = slab_page_pop(cache->partial);
        if (obj) {
            cache->alloc_count++;
            slab->total_allocs++;
            if (slab->ctor) slab->ctor(obj, slab->ctor_user_data);
            AGENTOS_LOG_DEBUG("Slab: alloc from stolen page CPU%d (steals=%zu)",
                              cpu, slab->cpu_steals);
            return obj;
        }
    }

    /* 3. 分配新页 */
    slab_page_t *new_page = slab_page_create(slab);
    if (!new_page) {
        AGENTOS_LOG_WARN("Slab: page allocation failed, OOM (total_pages=%zu)",
                         slab->total_pages);
        return NULL;
    }

    slab->total_pages++;

    /* 从新页分配 */
    void *obj = slab_page_pop(new_page);
    if (obj) {
        /* 新页变为 partial，保存为当前 CPU 缓存 */
        cache->partial = new_page;
        cache->alloc_count++;
        slab->total_allocs++;
        if (slab->ctor) slab->ctor(obj, slab->ctor_user_data);
        AGENTOS_LOG_DEBUG("Slab: alloc from new page CPU%d (total_pages=%zu)",
                          cpu, slab->total_pages);
        return obj;
    }

    /* 不应该到达这里 */
    AGENTOS_LOG_ERROR("Slab: new page created but pop returned NULL");
    slab_page_destroy(new_page);
    slab->total_pages--;
    return NULL;
}

void agentos_slab_free(agentos_slab_t *slab, void *obj)
{
    if (!slab || !obj) return;

    int cpu = slab_get_cpu_id();
    if (cpu < 0 || (size_t)cpu >= slab->cpu_count) cpu = 0;

    slab_cpu_cache_t *cache = &slab->cpu_caches[cpu];

    /* 析构回调 */
    if (slab->dtor) slab->dtor(obj, slab->ctor_user_data);

    /* 尝试归还到当前 CPU 的 partial 页 */
    if (cache->partial) {
        slab_page_push(cache->partial, obj);
        cache->free_count++;
        slab->total_frees++;

        /* 如果页变为全空，归还到全局链 */
        if (cache->partial->state == SLAB_PAGE_EMPTY) {
            AGENTOS_LOG_DEBUG("Slab: CPU%d partial page empty, returning to global chain",
                              cpu);
            slab_return_partial_page(slab, cache->partial);
            cache->partial = NULL;
        }
        return;
    }

    /* 没有当前 partial 页，从全局链获取一个 */
    slab_page_t *page = slab_get_partial_page(slab);
    if (page) {
        slab_page_push(page, obj);
        cache->partial = page;
        cache->free_count++;
        slab->total_frees++;
        return;
    }

    /* 分配新页存放释放的对象 */
    slab_page_t *new_page = slab_page_create(slab);
    if (new_page) {
        slab->total_pages++;
        slab_page_push(new_page, obj);
        cache->partial = new_page;
        cache->free_count++;
        slab->total_frees++;
        return;
    }

    /* 无法分配新页，对象丢失（内存泄漏） */
    AGENTOS_LOG_ERROR("Slab: cannot allocate new page for free, object leaked! "
                      "(total_frees=%zu, total_allocs=%zu)",
                      slab->total_frees, slab->total_allocs);
}

/* ================================================================
 * 统计与诊断
 * ================================================================ */

int agentos_slab_get_stats(agentos_slab_t *slab, agentos_slab_stats_t *stats)
{
    if (!slab || !stats) return -1;

    memset(stats, 0, sizeof(*stats));

    stats->obj_size = slab->obj_size;
    stats->objs_per_slab = slab->objs_per_page;
    stats->total_slabs = slab->total_pages;
    stats->total_allocs = slab->total_allocs;
    stats->total_frees = slab->total_frees;
    stats->active_objects = slab->total_allocs - slab->total_frees;
    stats->cpu_steals = slab->cpu_steals;

    /* 统计页状态 */
    slab_mutex_lock(slab);
    slab_page_t *page = slab->partial_list;
    while (page) {
        stats->partial_slabs++;
        if (page->state == SLAB_PAGE_EMPTY) {
            stats->empty_slabs++;
        } else if (page->state == SLAB_PAGE_FULL) {
            stats->full_slabs++;
        }
        page = page->next;
    }
    slab_mutex_unlock(slab);

    return 0;
}

size_t agentos_slab_shrink(agentos_slab_t *slab)
{
    if (!slab) return 0;

    size_t freed = 0;

    slab_mutex_lock(slab);

    /* 遍历全局 partial 链，释放空页 */
    slab_page_t **prev = &slab->partial_list;
    while (*prev) {
        slab_page_t *page = *prev;
        if (page->state == SLAB_PAGE_EMPTY) {
            *prev = page->next;
            slab_page_destroy(page);
            slab->partial_count--;
            slab->total_pages--;
            freed++;
        } else {
            prev = &page->next;
        }
    }

    slab_mutex_unlock(slab);

    if (freed > 0) {
        AGENTOS_LOG_INFO("Slab: shrink freed %zu empty pages (total_pages=%zu)",
                         freed, slab->total_pages);
    } else {
        AGENTOS_LOG_DEBUG("Slab: shrink found no empty pages (total_pages=%zu)",
                          slab->total_pages);
    }

    return freed;
}

bool agentos_slab_validate(agentos_slab_t *slab)
{
    if (!slab) return false;

    /* 基本检查 */
    if (slab->obj_size == 0) return false;
    if (slab->objs_per_page == 0) return false;

    /* 检查全局 partial 链 */
    slab_mutex_lock(slab);

    slab_page_t *page = slab->partial_list;
    size_t visited = 0;
    while (page && visited < slab->partial_count * 2) {
        if (page->total_count != slab->objs_per_page) {
            slab_mutex_unlock(slab);
            return false;
        }
        if (page->free_count > page->total_count) {
            slab_mutex_unlock(slab);
            return false;
        }
        page = page->next;
        visited++;
    }

    slab_mutex_unlock(slab);
    return true;
}