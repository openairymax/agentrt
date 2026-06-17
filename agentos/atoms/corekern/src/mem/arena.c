/**
 * @file arena.c
 * @brief Arena 分配器实现 — 线性分配 + 链式扩展 + 整体 reset
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * P1.19: Arena 分配器用于 ALLOC_SHORT_LIVED 场景。
 */

#include "arena.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

/* 线程局部存储：当前线程的 Arena */
#ifdef _WIN32
static __declspec(thread) agentos_arena_t *tls_current_arena = NULL;
#else
static __thread agentos_arena_t *tls_current_arena = NULL;
#endif

agentos_arena_t *agentos_arena_get_current(void)
{
    return tls_current_arena;
}

void agentos_arena_set_current(agentos_arena_t *arena)
{
    tls_current_arena = arena;
}

/* 默认块大小 64KB */
#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)

/* 最小块大小 4KB */
#define ARENA_MIN_BLOCK_SIZE (4 * 1024)

/* 对齐粒度 */
#define ARENA_ALIGN_SIZE sizeof(void *)

/* 块头部魔数 */
#define ARENA_BLOCK_MAGIC 0xA7E4A000

/* ==================== 内部结构 ==================== */

/**
 * @brief Arena 内存块
 *
 * 每个块是一段连续内存，Arena 通过链表管理多个块。
 */
typedef struct agentos_arena_block {
    uint32_t magic;                   /**< 魔数校验 */
    size_t capacity;                  /**< 块容量（data 区域大小） */
    size_t used;                      /**< 已使用字节数 */
    struct agentos_arena_block *next; /**< 下一个块 */
    /* data 区域紧跟在此结构之后 */
} agentos_arena_block_t;

/**
 * @brief 获取块的数据区域起始地址
 */
static inline void *block_data(agentos_arena_block_t *block)
{
    return (void *)((char *)block + sizeof(agentos_arena_block_t));
}

/**
 * @brief Arena 结构体
 */
struct agentos_arena {
    agentos_arena_block_t *first;     /**< 第一个块（保留块） */
    agentos_arena_block_t *current;   /**< 当前分配块 */
    size_t block_size;                /**< 每个块的 data 区域大小 */
    size_t total_used;                /**< 所有块已使用总字节数 */
    size_t total_capacity;            /**< 所有块总容量 */
    size_t block_count;               /**< 块数量 */
};

/* ==================== 块操作 ==================== */

static agentos_arena_block_t *arena_block_create(size_t data_size)
{
    size_t total = sizeof(agentos_arena_block_t) + data_size;
    agentos_arena_block_t *block = (agentos_arena_block_t *)malloc(total);
    if (!block)
        return NULL;

    block->magic = ARENA_BLOCK_MAGIC;
    block->capacity = data_size;
    block->used = 0;
    block->next = NULL;

    /* 清零数据区域 */
    memset(block_data(block), 0, data_size);

    return block;
}

static void arena_block_destroy(agentos_arena_block_t *block)
{
    if (!block || block->magic != ARENA_BLOCK_MAGIC)
        return;
    block->magic = 0;
    free(block);
}

/* ==================== 对齐辅助 ==================== */

static inline size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

/* ==================== 公共 API ==================== */

agentos_arena_t *agentos_arena_create(size_t block_size)
{
    agentos_arena_t *arena = (agentos_arena_t *)calloc(1, sizeof(agentos_arena_t));
    if (!arena)
        return NULL;

    arena->block_size = block_size > 0 ? block_size : ARENA_DEFAULT_BLOCK_SIZE;
    if (arena->block_size < ARENA_MIN_BLOCK_SIZE)
        arena->block_size = ARENA_MIN_BLOCK_SIZE;

    /* 创建第一个块 */
    arena->first = arena_block_create(arena->block_size);
    if (!arena->first) {
        free(arena);
        return NULL;
    }

    arena->current = arena->first;
    arena->total_capacity = arena->block_size;
    arena->block_count = 1;

    return arena;
}

void agentos_arena_destroy(agentos_arena_t *arena)
{
    if (!arena)
        return;

    agentos_arena_block_t *block = arena->first;
    while (block) {
        agentos_arena_block_t *next = block->next;
        arena_block_destroy(block);
        block = next;
    }

    free(arena);
}

void *agentos_arena_alloc(agentos_arena_t *arena, size_t size)
{
    return agentos_arena_alloc_aligned(arena, size, ARENA_ALIGN_SIZE);
}

void *agentos_arena_alloc_aligned(agentos_arena_t *arena, size_t size, size_t alignment)
{
    if (!arena || size == 0)
        return NULL;

    /* 确保对齐是 2 的幂 */
    if (alignment < ARENA_ALIGN_SIZE)
        alignment = ARENA_ALIGN_SIZE;
    if ((alignment & (alignment - 1)) != 0)
        return NULL; /* 非法对齐 */

    /* 在当前块中尝试分配 */
    agentos_arena_block_t *block = arena->current;
    size_t aligned_used = align_up(block->used, alignment);
    size_t new_used = aligned_used + size;

    if (new_used <= block->capacity) {
        /* 当前块有足够空间 */
        void *ptr = (char *)block_data(block) + aligned_used;
        block->used = new_used;
        arena->total_used += size;
        return ptr;
    }

    /* 当前块空间不足，尝试后续已有块 */
    if (block->next) {
        block = block->next;
        aligned_used = align_up(block->used, alignment);
        new_used = aligned_used + size;

        if (new_used <= block->capacity) {
            void *ptr = (char *)block_data(block) + aligned_used;
            block->used = new_used;
            arena->current = block;
            arena->total_used += size;
            return ptr;
        }
    }

    /* 需要分配新块 */
    size_t new_block_size = arena->block_size;
    /* 如果请求大小超过默认块大小，分配专用大块 */
    if (size + alignment > new_block_size) {
        new_block_size = size + alignment;
    }

    agentos_arena_block_t *new_block = arena_block_create(new_block_size);
    if (!new_block)
        return NULL;

    /* 将新块链接到当前块之后 */
    new_block->next = block->next;
    block->next = new_block;
    arena->current = new_block;
    arena->total_capacity += new_block_size;
    arena->block_count++;

    /* 在新块中分配 */
    aligned_used = align_up(new_block->used, alignment);
    new_used = aligned_used + size;

    if (new_used <= new_block->capacity) {
        void *ptr = (char *)block_data(new_block) + aligned_used;
        new_block->used = new_used;
        arena->total_used += size;
        return ptr;
    }

    /* 不应该到达这里（新块应该足够大） */
    return NULL;
}

void agentos_arena_reset(agentos_arena_t *arena)
{
    if (!arena)
        return;

    /* 保留第一个块，释放其余 */
    agentos_arena_block_t *block = arena->first->next;
    while (block) {
        agentos_arena_block_t *next = block->next;
        arena->total_capacity -= block->capacity;
        arena->block_count--;
        arena_block_destroy(block);
        block = next;
    }

    /* 重置第一个块 */
    arena->first->used = 0;
    arena->first->next = NULL;
    arena->current = arena->first;
    arena->total_used = 0;
}

agentos_arena_mark_t agentos_arena_mark(agentos_arena_t *arena)
{
    agentos_arena_mark_t mark = {0, 0};
    if (!arena)
        return mark;

    /* 找到当前块在链表中的索引 */
    size_t idx = 0;
    agentos_arena_block_t *block = arena->first;
    while (block && block != arena->current) {
        idx++;
        block = block->next;
    }

    mark.block_idx = idx;
    mark.offset = arena->current ? arena->current->used : 0;
    return mark;
}

void agentos_arena_reset_to(agentos_arena_t *arena, agentos_arena_mark_t mark)
{
    if (!arena)
        return;

    /* 找到标记点对应的块 */
    size_t idx = 0;
    agentos_arena_block_t *block = arena->first;

    while (block && idx < mark.block_idx) {
        idx++;
        block = block->next;
    }

    if (!block)
        return;

    /* 释放标记点之后的所有块 */
    agentos_arena_block_t *extra = block->next;
    while (extra) {
        agentos_arena_block_t *next = extra->next;
        arena->total_capacity -= extra->capacity;
        arena->block_count--;
        arena_block_destroy(extra);
        extra = next;
    }

    /* 回滚当前块到标记偏移 */
    block->used = mark.offset;
    block->next = NULL;
    arena->current = block;

    /* 重新计算 total_used */
    arena->total_used = 0;
    agentos_arena_block_t *b = arena->first;
    while (b && b != block) {
        arena->total_used += b->used;
        b = b->next;
    }
    arena->total_used += mark.offset;
}

size_t agentos_arena_used(agentos_arena_t *arena)
{
    return arena ? arena->total_used : 0;
}

size_t agentos_arena_capacity(agentos_arena_t *arena)
{
    return arena ? arena->total_capacity : 0;
}
