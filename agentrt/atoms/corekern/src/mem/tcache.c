/**
 * @file tcache.c
 * @brief per-Thread 缓存层实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.20: 每个线程维护本地空闲链表，减少全局池锁竞争。
 */

#include "../../include/tcache.h"

#include <stdlib.h>
#include <string.h>

/* 默认配置 */
#define TCACHE_DEFAULT_SLOT_SIZE    256     /* 256 字节 */
#define TCACHE_DEFAULT_MAX_SLOTS    256     /* 最多 256 个槽位 */
#define TCACHE_DEFAULT_BATCH_SIZE   32      /* 批量 32 个 */

/* ==================== 内部结构 ==================== */

/**
 * @brief 空闲槽位链表节点
 *
 * 嵌入在空闲内存块中，不额外分配内存。
 */
typedef struct tcache_slot {
    struct tcache_slot *next;
} tcache_slot_t;

/**
 * @brief per-Thread 缓存结构
 */
struct agentrt_tcache {
    tcache_slot_t *free_list;          /**< 空闲链表头 */
    uint32_t free_count;               /**< 空闲槽位数 */
    size_t slot_size;                  /**< 槽位大小 */
    uint32_t max_slots;                /**< 最大槽位数 */
    uint32_t batch_size;               /**< 批量大小 */
    tcache_global_alloc_fn global_alloc; /**< 全局分配回调 */
    tcache_global_free_fn global_free;   /**< 全局释放回调 */
    agentrt_tcache_stats_t stats;      /**< 统计信息 */
};

/* ==================== 公共 API ==================== */

agentrt_tcache_t *agentrt_tcache_create(
    const agentrt_tcache_config_t *config,
    tcache_global_alloc_fn global_alloc,
    tcache_global_free_fn global_free)
{
    if (!global_alloc || !global_free)
        return NULL;

    agentrt_tcache_t *tc = (agentrt_tcache_t *)calloc(1, sizeof(agentrt_tcache_t));
    if (!tc)
        return NULL;

    if (config) {
        tc->slot_size = config->slot_size > 0 ? config->slot_size : TCACHE_DEFAULT_SLOT_SIZE;
        tc->max_slots = config->max_slots > 0 ? config->max_slots : TCACHE_DEFAULT_MAX_SLOTS;
        tc->batch_size = config->batch_size > 0 ? config->batch_size : TCACHE_DEFAULT_BATCH_SIZE;
    } else {
        tc->slot_size = TCACHE_DEFAULT_SLOT_SIZE;
        tc->max_slots = TCACHE_DEFAULT_MAX_SLOTS;
        tc->batch_size = TCACHE_DEFAULT_BATCH_SIZE;
    }

    /* 确保 slot_size 足够存放链表节点 */
    if (tc->slot_size < sizeof(tcache_slot_t))
        tc->slot_size = sizeof(tcache_slot_t);

    /* 对齐到 sizeof(void*) */
    tc->slot_size = (tc->slot_size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    tc->global_alloc = global_alloc;
    tc->global_free = global_free;

    return tc;
}

void agentrt_tcache_destroy(agentrt_tcache_t *tcache)
{
    if (!tcache)
        return;

    /* 将所有空闲槽位归还到全局池 */
    tcache_slot_t *slot = tcache->free_list;
    while (slot) {
        tcache_slot_t *next = slot->next;
        tcache->global_free(slot);
        slot = next;
    }

    free(tcache);
}

void *agentrt_tcache_alloc(agentrt_tcache_t *tcache)
{
    if (!tcache)
        return NULL;

    tcache->stats.alloc_count++;

    /* 从本地空闲链表获取 */
    if (tcache->free_list) {
        tcache_slot_t *slot = tcache->free_list;
        tcache->free_list = slot->next;
        tcache->free_count--;
        tcache->stats.current_slots = tcache->free_count;
        return (void *)slot;
    }

    /* 本地缓存不足，从全局池批量获取 */
    tcache->stats.miss_count++;

    /* 先获取一个直接返回 */
    void *ptr = tcache->global_alloc(tcache->slot_size);
    if (!ptr)
        return NULL;

    /* 批量预取额外 (batch_size - 1) 个到本地缓存 */
    uint32_t to_prefetch = tcache->batch_size - 1;
    if (to_prefetch > tcache->max_slots - tcache->free_count)
        to_prefetch = tcache->max_slots - tcache->free_count;

    for (uint32_t i = 0; i < to_prefetch; i++) {
        void *extra = tcache->global_alloc(tcache->slot_size);
        if (!extra)
            break;

        tcache_slot_t *slot = (tcache_slot_t *)extra;
        slot->next = tcache->free_list;
        tcache->free_list = slot;
        tcache->free_count++;
    }

    tcache->stats.current_slots = tcache->free_count;
    return ptr;
}

void agentrt_tcache_free(agentrt_tcache_t *tcache, void *ptr)
{
    if (!tcache || !ptr)
        return;

    tcache->stats.free_count++;

    /* 本地缓存未满，放入本地空闲链表 */
    if (tcache->free_count < tcache->max_slots) {
        tcache_slot_t *slot = (tcache_slot_t *)ptr;
        slot->next = tcache->free_list;
        tcache->free_list = slot;
        tcache->free_count++;
        tcache->stats.current_slots = tcache->free_count;
        return;
    }

    /* 本地缓存已满，批量归还到全局池 */
    tcache->stats.overflow_count++;

    uint32_t to_evict = tcache->batch_size;
    if (to_evict > tcache->free_count)
        to_evict = tcache->free_count;

    /* 归还 batch_size 个槽位到全局池 */
    for (uint32_t i = 0; i < to_evict; i++) {
        tcache_slot_t *slot = tcache->free_list;
        if (!slot)
            break;
        tcache->free_list = slot->next;
        tcache->free_count--;
        tcache->global_free(slot);
    }

    /* 将当前 ptr 放入本地缓存 */
    tcache_slot_t *slot = (tcache_slot_t *)ptr;
    slot->next = tcache->free_list;
    tcache->free_list = slot;
    tcache->free_count++;
    tcache->stats.current_slots = tcache->free_count;
}

void agentrt_tcache_get_stats(agentrt_tcache_t *tcache, agentrt_tcache_stats_t *out_stats)
{
    if (!tcache || !out_stats)
        return;
    *out_stats = tcache->stats;
}

void agentrt_tcache_reset_stats(agentrt_tcache_t *tcache)
{
    if (!tcache)
        return;
    memset(&tcache->stats, 0, sizeof(tcache->stats));
    tcache->stats.current_slots = tcache->free_count;
}
