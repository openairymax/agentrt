/**
 * @file mempool.c
 * @brief P3.16: mempool 最小保证分配器实现 — 紧急预留 + 对象池 + 最低保证分配
 *
 * 在系统 OOM 时为关键路径（IPC 消息传递）提供最低保证分配能力。
 *
 * 两级架构：
 *   普通池 — 日常使用，通过标准分配器
 *   紧急池 — OOM 专用，预分配 32MB 内存块
 *
 * 分配路径：
 *   正常模式：普通池 → 紧急池（降级）
 *   紧急模式：仅紧急池
 *
 * 内存块管理：
 *   紧急池使用固定大小内存块池（power-of-2 分配），
 *   最小 64B，最大 64KB，共 10 个大小类。
 *
 * 验证目标：OOM 场景下 IPC 不死锁
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "mempool.h"
#include "mem.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** 默认紧急预留大小 (32MB) */
#define MEMPOOL_DEFAULT_EMERGENCY_RESERVE \
    (AGENTRT_MEMPOOL_IPC_EMERGENCY_RESERVE_MB * 1024ULL * 1024ULL)

/** 紧急池大小类数量（2^6=64 到 2^15=32768，共 10 类） */
#define MEMPOOL_SIZE_CLASSES    10

/** Cache line 对齐 */
#define MEMPOOL_ALIGN           64

/** 每个大小类的块数（紧急池总大小 / 每类平均分配） */
#define MEMPOOL_BLOCKS_PER_CLASS  128

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief 内存块头
 *
 * 每个分配的块前有一个头部，记录块大小和所属池。
 * 紧急池使用预分配的内存块数组。
 */
typedef struct mempool_block {
    struct mempool_block *next;    /**< 空闲链表中的下一个块 */
    size_t                size;    /**< 块大小（含头部） */
    uint8_t               pool;    /**< 0=普通池, 1=紧急池 */
    uint8_t               magic;   /**< 魔术字 (0xA3) 防野指针 */
    uint8_t               data[];  /**< 用户数据区 */
} mempool_block_t;

/** 魔术字 */
#define MEMPOOL_MAGIC  0xA3

/** 块头部大小 */
#define MEMPOOL_BLOCK_HEADER  offsetof(mempool_block_t, data)

/**
 * @brief 紧急池大小类
 *
 * 每个大小类维护一个空闲链表，使用预分配的内存块。
 * 大小类索引：class 0 = 64B, class 1 = 128B, ..., class 9 = 32768B
 */
typedef struct {
    mempool_block_t *freelist;     /**< 空闲链表头 */
    size_t           block_size;   /**< 每个块的用户数据大小 */
    size_t           total_size;   /**< 块总大小（含头部） */
    uint32_t         total_blocks; /**< 总块数 */
    uint32_t         free_blocks;  /**< 空闲块数 */
    uint32_t         peak_used;    /**< 峰值使用块数 */
    uint8_t         *block_memory; /**< 预分配的内存块数组 */
} mempool_class_t;

/**
 * @brief mempool 主结构
 */
struct agentrt_mempool {
    char                  name[AGENTRT_MEMPOOL_NAME_MAX]; /**< 名称 */
    size_t                emergency_reserve;              /**< 紧急池总大小 */
    size_t                max_obj_size;                   /**< 最大对象大小 */
    bool                  enable_stats;                   /**< 统计开关 */

    /* 紧急池大小类 */
    mempool_class_t       classes[MEMPOOL_SIZE_CLASSES];

    /* 普通池统计 */
    atomic_uint_fast64_t  normal_allocs;
    atomic_uint_fast64_t  normal_frees;
    atomic_uint_fast64_t  normal_fail_count;
    atomic_size_t         normal_allocated;
    atomic_size_t         normal_peak;

    /* 紧急池统计 */
    atomic_uint_fast64_t  emergency_allocs;
    atomic_uint_fast64_t  emergency_frees;
    atomic_uint_fast64_t  emergency_fail_count;
    atomic_size_t         emergency_allocated;
    atomic_size_t         emergency_peak;

    /* OOM 统计 */
    atomic_uint_fast64_t  oom_trigger_count;
    atomic_uint_fast64_t  oom_rescue_count;

    /* 状态 */
    atomic_bool           emergency_mode;
    bool                  initialized;
    bool                  destroyed;

    /* 线程安全 */
    pthread_mutex_t       lock;
};

/* ================================================================
 * 辅助函数
 * ================================================================ */

/**
 * @brief 对齐到 MEMPOOL_ALIGN
 */
static inline size_t align_to_mempool(size_t size)
{
    return (size + MEMPOOL_ALIGN - 1) & ~((size_t)(MEMPOOL_ALIGN - 1));
}

/**
 * @brief 根据用户请求大小选择大小类索引
 *
 * 返回最小的满足 size 需求的大小类。
 * 返回 -1 表示请求大小超出最大大小类。
 */
static int mempool_class_index(size_t size)
{
    /* 大小类：64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 */
    static const size_t class_sizes[MEMPOOL_SIZE_CLASSES] = {
        64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
    };

    for (int i = 0; i < MEMPOOL_SIZE_CLASSES; i++) {
        if (size <= class_sizes[i]) {
            return i;
        }
    }
    return -1; /* 超出最大大小类 */
}

/**
 * @brief 初始化一个大小类
 *
 * 预分配所有内存块，连接成空闲链表。
 */
static int mempool_class_init(mempool_class_t *cls, size_t block_data_size,
                               uint32_t block_count)
{
    cls->block_size  = block_data_size;
    cls->total_size  = block_data_size + MEMPOOL_BLOCK_HEADER;
    cls->total_blocks = block_count;
    cls->free_blocks  = block_count;
    cls->peak_used    = 0;
    cls->freelist     = NULL;

    /* 分配连续内存块数组 */
    size_t alloc_size = cls->total_size * block_count;
    cls->block_memory = (uint8_t *)AGENTRT_CALLOC(1, alloc_size);
    if (!cls->block_memory) {
        return -1;
    }

    /* 连接成空闲链表 */
    for (uint32_t i = 0; i < block_count; i++) {
        mempool_block_t *block = (mempool_block_t *)(cls->block_memory +
                                                      (size_t)i * cls->total_size);
        block->next  = cls->freelist;
        block->size  = cls->total_size;
        block->pool  = 1; /* 紧急池 */
        block->magic = MEMPOOL_MAGIC;
        cls->freelist = block;
    }

    return 0;
}

/**
 * @brief 销毁一个大小类
 */
static void mempool_class_destroy(mempool_class_t *cls)
{
    if (cls->block_memory) {
        AGENTRT_FREE(cls->block_memory);
        cls->block_memory = NULL;
    }
    cls->freelist    = NULL;
    cls->free_blocks  = 0;
    cls->total_blocks = 0;
}

/* ================================================================
 * 公共 API
 * ================================================================ */

agentrt_mempool_t *agentrt_mempool_create(const agentrt_mempool_config_t *config)
{
    agentrt_mempool_t *mp = (agentrt_mempool_t *)AGENTRT_CALLOC(1, sizeof(*mp));
    if (!mp) {
        AGENTRT_LOG_ERROR("Mempool: P3.16 CREATE-FAIL — OOM allocating struct");
        return NULL;
    }

    /* 名称 */
    if (config && config->name && config->name[0]) {
        AGENTRT_STRNCPY_TERM(mp->name, config->name, sizeof(mp->name));
    } else {
        snprintf(mp->name, sizeof(mp->name), "mempool_%p", (void *)mp);
    }

    /* 紧急预留大小 */
    mp->emergency_reserve = (config && config->emergency_reserve > 0)
                            ? config->emergency_reserve
                            : MEMPOOL_DEFAULT_EMERGENCY_RESERVE;

    mp->max_obj_size = (config && config->max_obj_size > 0)
                       ? config->max_obj_size
                       : AGENTRT_MEMPOOL_DEFAULT_MAX_OBJ_SIZE;

    mp->enable_stats = config ? config->enable_stats : true;

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 CREATE emergency_reserve=%zuMB "
                     "max_obj_size=%zuKB",
                     mp->name,
                     mp->emergency_reserve / (1024 * 1024),
                     mp->max_obj_size / 1024);

    /* 初始化大小类 */
    static const size_t class_sizes[MEMPOOL_SIZE_CLASSES] = {
        64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
    };

    size_t remaining = mp->emergency_reserve;
    size_t total_allocated = 0;

    for (int i = 0; i < MEMPOOL_SIZE_CLASSES; i++) {
        /* 计算每个大小类的块数，按百分比分配紧急池 */
        /* 小类块数多，大类块数少 */
        size_t class_ratio = (size_t)(MEMPOOL_SIZE_CLASSES - i);
        size_t class_total = sizeof(mempool_block_t) + class_sizes[i];
        size_t class_budget = (remaining * class_ratio) /
                               (size_t)((MEMPOOL_SIZE_CLASSES * (MEMPOOL_SIZE_CLASSES + 1)) / 2);
        uint32_t block_count = class_budget > class_total
                               ? (uint32_t)(class_budget / class_total)
                               : 4; /* 最少 4 块 */

        if (block_count > 1024) block_count = 1024; /* 上限 */

        AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 CLASS-INIT class[%d] "
                          "block_size=%zu total=%zu count=%u budget=%zu",
                          mp->name, i, class_sizes[i],
                          class_total, block_count, class_budget);

        if (mempool_class_init(&mp->classes[i], class_sizes[i], block_count) != 0) {
            AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 CREATE-FAIL — "
                              "class[%d] init failed", mp->name, i);

            /* 回滚已初始化的类 */
            for (int j = 0; j < i; j++) {
                mempool_class_destroy(&mp->classes[j]);
            }
            AGENTRT_FREE(mp);
            return NULL;
        }

        size_t used = mp->classes[i].total_size * block_count;
        total_allocated += used;
        if (remaining > used) {
            remaining -= used;
        } else {
            remaining = 0;
        }
    }

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 CREATE — emergency pool initialized "
                     "(%zu bytes = %.2fMB, %d classes)",
                     mp->name, total_allocated,
                     (double)total_allocated / (1024.0 * 1024.0),
                     MEMPOOL_SIZE_CLASSES);

    /* 初始化锁 */
    if (pthread_mutex_init(&mp->lock, NULL) != 0) {
        AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 CREATE-FAIL — mutex_init failed",
                          mp->name);
        for (int i = 0; i < MEMPOOL_SIZE_CLASSES; i++) {
            mempool_class_destroy(&mp->classes[i]);
        }
        AGENTRT_FREE(mp);
        return NULL;
    }

    mp->initialized = true;
    mp->destroyed   = false;
    atomic_store(&mp->emergency_mode, false);

    return mp;
}

void agentrt_mempool_destroy(agentrt_mempool_t *mp)
{
    if (!mp) {
        AGENTRT_LOG_WARN("Mempool: P3.16 DESTROY — NULL ignored");
        return;
    }
    if (mp->destroyed) {
        AGENTRT_LOG_WARN("Mempool[%s]: P3.16 DESTROY — already destroyed", mp->name);
        return;
    }

    agentrt_mempool_stats_t stats;
    agentrt_mempool_get_stats(mp, &stats);

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 DESTROY normal=%zu/%zu emergency=%zu/%zu "
                     "oom=%llu rescue=%llu",
                     mp->name,
                     stats.normal_allocated, stats.normal_peak,
                     stats.emergency_allocated, stats.emergency_peak,
                     (unsigned long long)stats.oom_trigger_count,
                     (unsigned long long)stats.oom_rescue_count);

    /* 检查是否有未归还的分配 */
    if (stats.normal_allocated > 0 || stats.emergency_allocated > 0) {
        AGENTRT_LOG_WARN("Mempool[%s]: P3.16 DESTROY — %zu normal + %zu emergency "
                         "bytes still allocated (potential leak)",
                         mp->name,
                         stats.normal_allocated,
                         stats.emergency_allocated);
    }

    /* 销毁所有大小类 */
    for (int i = 0; i < MEMPOOL_SIZE_CLASSES; i++) {
        mempool_class_destroy(&mp->classes[i]);
    }

    mp->initialized = false;
    mp->destroyed   = true;
    pthread_mutex_destroy(&mp->lock);
    AGENTRT_FREE(mp);
}

void *agentrt_mempool_alloc(agentrt_mempool_t *mp, size_t size)
{
    if (!mp) {
        AGENTRT_LOG_ERROR("Mempool: P3.16 ALLOC-FAIL — NULL mempool");
        return NULL;
    }
    if (!mp->initialized || mp->destroyed) {
        AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 ALLOC-FAIL — not initialized or destroyed",
                          mp->name);
        return NULL;
    }
    if (size == 0) {
        AGENTRT_LOG_WARN("Mempool[%s]: P3.16 ALLOC — zero size requested", mp->name);
        return NULL;
    }

    bool is_emergency = atomic_load(&mp->emergency_mode);

    /* 正常模式：优先从普通池分配 */
    if (!is_emergency) {
        void *ptr = agentrt_mem_alloc(size);
        if (ptr) {
            atomic_fetch_add(&mp->normal_allocs, 1);
            size_t allocated = atomic_fetch_add(&mp->normal_allocated, size) + size;
            size_t peak = atomic_load(&mp->normal_peak);
            while (allocated > peak) {
                if (atomic_compare_exchange_weak(&mp->normal_peak, &peak, allocated)) {
                    break;
                }
                peak = atomic_load(&mp->normal_peak);
            }

            AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 ALLOC-NORMAL ptr=%p size=%zu "
                              "allocated=%zu peak=%zu",
                              mp->name, ptr, size, allocated, peak);
            return ptr;
        }

        /* 普通池分配失败，降级到紧急池 */
        atomic_fetch_add(&mp->normal_fail_count, 1);
        atomic_fetch_add(&mp->oom_trigger_count, 1);

        AGENTRT_LOG_WARN("Mempool[%s]: P3.16 ALLOC-FALLBACK — normal pool exhausted, "
                         "falling back to emergency pool (size=%zu)",
                         mp->name, size);
    }

    /* 紧急模式或降级：从紧急池分配 */
    int class_idx = mempool_class_index(size);
    if (class_idx < 0) {
        atomic_fetch_add(&mp->emergency_fail_count, 1);
        AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 ALLOC-FAIL — size=%zu exceeds "
                          "max emergency class size %zu",
                          mp->name, size, mp->max_obj_size);
        return NULL;
    }

    pthread_mutex_lock(&mp->lock);

    mempool_class_t *cls = &mp->classes[class_idx];
    if (!cls->freelist || cls->free_blocks == 0) {
        pthread_mutex_unlock(&mp->lock);
        atomic_fetch_add(&mp->emergency_fail_count, 1);

        AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 ALLOC-FAIL — emergency class[%d] "
                          "exhausted (block_size=%zu, free=%u/%u)",
                          mp->name, class_idx, cls->block_size,
                          cls->free_blocks, cls->total_blocks);
        return NULL;
    }

    /* 从空闲链表取一个块 */
    mempool_block_t *block = cls->freelist;
    cls->freelist = block->next;
    cls->free_blocks--;

    uint32_t used = cls->total_blocks - cls->free_blocks;
    if (used > cls->peak_used) {
        cls->peak_used = used;
    }

    pthread_mutex_unlock(&mp->lock);

    block->next  = NULL;
    block->magic = MEMPOOL_MAGIC;

    atomic_fetch_add(&mp->emergency_allocs, 1);
    size_t em_allocated = atomic_fetch_add(&mp->emergency_allocated, cls->block_size) + cls->block_size;
    size_t em_peak = atomic_load(&mp->emergency_peak);
    while (em_allocated > em_peak) {
        if (atomic_compare_exchange_weak(&mp->emergency_peak, &em_peak, em_allocated)) {
            break;
        }
        em_peak = atomic_load(&mp->emergency_peak);
    }

    if (is_emergency) {
        atomic_fetch_add(&mp->oom_rescue_count, 1);
    }

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 ALLOC-EMERGENCY ptr=%p class[%d] "
                     "block_size=%zu free=%u/%u used=%u mode=%s",
                     mp->name, block->data, class_idx,
                     cls->block_size, cls->free_blocks, cls->total_blocks, used,
                     is_emergency ? "EMERGENCY" : "FALLBACK");

    return block->data;
}

void agentrt_mempool_free(agentrt_mempool_t *mp, void *ptr)
{
    if (!mp) {
        AGENTRT_LOG_ERROR("Mempool: P3.16 FREE-FAIL — NULL mempool");
        return;
    }
    if (!ptr) {
        AGENTRT_LOG_WARN("Mempool[%s]: P3.16 FREE — NULL ptr ignored", mp->name);
        return;
    }

    /* 检查是否是紧急池的块（通过魔术字） */
    mempool_block_t *block = (mempool_block_t *)((uint8_t *)ptr - MEMPOOL_BLOCK_HEADER);

    if (block->magic == MEMPOOL_MAGIC && block->pool == 1) {
        /* 归还到紧急池 */
        size_t block_size = block->size - MEMPOOL_BLOCK_HEADER;

        /* 找到对应的大小类 */
        int class_idx = mempool_class_index(block_size);
        if (class_idx < 0) {
            AGENTRT_LOG_ERROR("Mempool[%s]: P3.16 FREE-FAIL — invalid block size %zu",
                              mp->name, block_size);
            return;
        }

        pthread_mutex_lock(&mp->lock);

        mempool_class_t *cls = &mp->classes[class_idx];
        block->next = cls->freelist;
        cls->freelist = block;
        cls->free_blocks++;

        pthread_mutex_unlock(&mp->lock);

        atomic_fetch_add(&mp->emergency_frees, 1);
        atomic_fetch_sub(&mp->emergency_allocated, block_size);

        AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 FREE-EMERGENCY ptr=%p class[%d] "
                          "block_size=%zu free=%u/%u",
                          mp->name, ptr, class_idx,
                          block_size, cls->free_blocks, cls->total_blocks);
    } else {
        /* 归还到普通池 */
        agentrt_mem_free(ptr);

        atomic_fetch_add(&mp->normal_frees, 1);
        /* 注意：此处无法精确获取 size，使用近似统计 */
        /* 普通池的 allocated 统计可能不精确 */

        AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 FREE-NORMAL ptr=%p", mp->name, ptr);
    }
}

void agentrt_mempool_set_emergency(agentrt_mempool_t *mp, bool enable)
{
    if (!mp) {
        AGENTRT_LOG_WARN("Mempool: P3.16 EMERGENCY-MODE — NULL mempool ignored");
        return;
    }

    bool current = atomic_load(&mp->emergency_mode);
    if (current == enable) {
        AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 EMERGENCY-MODE — already %s",
                          mp->name, enable ? "ON" : "OFF");
        return;
    }

    atomic_store(&mp->emergency_mode, enable);

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 EMERGENCY-MODE %s %s",
                     mp->name,
                     enable ? "ENTERED" : "EXITED",
                     enable ? "(OOM — normal pool disabled)" : "(normal pool restored)");
}

void agentrt_mempool_get_stats(agentrt_mempool_t *mp,
                                agentrt_mempool_stats_t *out_stats)
{
    if (!out_stats) {
        AGENTRT_LOG_WARN("Mempool: P3.16 STATS — NULL out_stats ignored");
        return;
    }

    if (!mp) {
        memset(out_stats, 0, sizeof(*out_stats));
        return;
    }

    out_stats->normal_allocs    = atomic_load(&mp->normal_allocs);
    out_stats->normal_frees     = atomic_load(&mp->normal_frees);
    out_stats->normal_fail_count = atomic_load(&mp->normal_fail_count);
    out_stats->normal_allocated  = atomic_load(&mp->normal_allocated);
    out_stats->normal_peak       = atomic_load(&mp->normal_peak);

    out_stats->emergency_allocs    = atomic_load(&mp->emergency_allocs);
    out_stats->emergency_frees     = atomic_load(&mp->emergency_frees);
    out_stats->emergency_fail_count = atomic_load(&mp->emergency_fail_count);
    out_stats->emergency_allocated  = atomic_load(&mp->emergency_allocated);
    out_stats->emergency_peak       = atomic_load(&mp->emergency_peak);
    out_stats->emergency_reserved   = mp->emergency_reserve;

    out_stats->oom_trigger_count = atomic_load(&mp->oom_trigger_count);
    out_stats->oom_rescue_count  = atomic_load(&mp->oom_rescue_count);
    out_stats->emergency_mode    = atomic_load(&mp->emergency_mode);

    AGENTRT_LOG_DEBUG("Mempool[%s]: P3.16 STATS normal=%zu/%zu emergency=%zu/%zu "
                      "oom_trigger=%llu rescue=%llu mode=%s",
                      mp->name,
                      out_stats->normal_allocated, out_stats->normal_peak,
                      out_stats->emergency_allocated, out_stats->emergency_peak,
                      (unsigned long long)out_stats->oom_trigger_count,
                      (unsigned long long)out_stats->oom_rescue_count,
                      out_stats->emergency_mode ? "EMERGENCY" : "NORMAL");
}

void agentrt_mempool_reset_stats(agentrt_mempool_t *mp)
{
    if (!mp) return;

    AGENTRT_LOG_INFO("Mempool[%s]: P3.16 STATS-RESET", mp->name);

    atomic_store(&mp->normal_allocs, 0);
    atomic_store(&mp->normal_frees, 0);
    atomic_store(&mp->normal_fail_count, 0);
    atomic_store(&mp->normal_peak, atomic_load(&mp->normal_allocated));

    atomic_store(&mp->emergency_allocs, 0);
    atomic_store(&mp->emergency_frees, 0);
    atomic_store(&mp->emergency_fail_count, 0);
    atomic_store(&mp->emergency_peak, atomic_load(&mp->emergency_allocated));

    atomic_store(&mp->oom_trigger_count, 0);
    atomic_store(&mp->oom_rescue_count, 0);
}

size_t agentrt_mempool_emergency_available(agentrt_mempool_t *mp)
{
    if (!mp) return 0;

    size_t available = 0;
    pthread_mutex_lock(&mp->lock);
    for (int i = 0; i < MEMPOOL_SIZE_CLASSES; i++) {
        available += (size_t)mp->classes[i].free_blocks * mp->classes[i].block_size;
    }
    pthread_mutex_unlock(&mp->lock);

    return available;
}

const char *agentrt_mempool_name(agentrt_mempool_t *mp)
{
    return mp ? mp->name : "null";
}