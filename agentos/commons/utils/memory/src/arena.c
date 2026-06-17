/**
 * @file arena.c
 * @brief P1.19: Arena 线性分配器实现
 *
 * 实现 arena_create/destroy/alloc/calloc/reset/mark/release API。
 * 使用链表串联多个 chunk，bump 指针线性分配，O(1) 整体释放。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "arena.h"

#include "memory_compat.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== Chunk 结构 ==================== */

typedef struct arena_chunk {
    struct arena_chunk *next;    /**< 下一个 chunk */
    uint8_t            *start;   /**< chunk 内存起始地址 */
    uint8_t            *bump;    /**< 当前 bump 指针 */
    uint8_t            *end;     /**< chunk 内存结束地址 */
    size_t              size;    /**< chunk 总大小 */
} arena_chunk_t;

/* ==================== Arena 结构 ==================== */

struct agentos_arena {
    arena_chunk_t *first_chunk;  /**< 第一个 chunk（链表头） */
    arena_chunk_t *current;      /**< 当前活跃 chunk */
    size_t         chunk_size;   /**< 默认 chunk 大小 */
    size_t         max_chunks;   /**< 最大 chunk 数量（0=无限制） */
    size_t         num_chunks;   /**< 当前 chunk 数量 */

    /* 统计 */
    size_t         total_allocated;
    uint64_t       alloc_count;
    uint64_t       reset_count;
    uint64_t       fallback_count;

    /* 线程同步 */
    agentos_mutex_t lock;
    bool            thread_safe;
};

/* ==================== 辅助函数 ==================== */

static arena_chunk_t *chunk_create(size_t size)
{
    arena_chunk_t *chunk = (arena_chunk_t *)malloc(sizeof(arena_chunk_t));
    if (!chunk) return NULL;

    chunk->start = (uint8_t *)malloc(size);
    if (!chunk->start) {
        free(chunk);
        return NULL;
    }

    chunk->bump = chunk->start;
    chunk->end  = chunk->start + size;
    chunk->size = size;
    chunk->next = NULL;
    return chunk;
}

static void chunk_destroy(arena_chunk_t *chunk)
{
    while (chunk) {
        arena_chunk_t *next = chunk->next;
        free(chunk->start);
        free(chunk);
        chunk = next;
    }
}

/* ==================== 公共 API 实现 ==================== */

agentos_arena_t *arena_create(size_t chunk_size, size_t max_chunks)
{
    if (chunk_size == 0) {
        chunk_size = ARENA_DEFAULT_CHUNK_SIZE;
    }
    if (chunk_size > ARENA_MAX_CHUNK_SIZE) {
        chunk_size = ARENA_MAX_CHUNK_SIZE;
    }

    agentos_arena_t *arena = (agentos_arena_t *)AGENTOS_CALLOC(1, sizeof(agentos_arena_t));
    if (!arena) return NULL;

    arena->chunk_size = chunk_size;
    arena->max_chunks = max_chunks;
    arena->thread_safe = true;

    /* 创建第一个 chunk */
    arena->first_chunk = chunk_create(chunk_size);
    if (!arena->first_chunk) {
        AGENTOS_FREE(arena);
        return NULL;
    }
    arena->current = arena->first_chunk;
    arena->num_chunks = 1;

    agentos_mutex_init(&arena->lock);

    return arena;
}

void arena_destroy(agentos_arena_t *arena)
{
    if (!arena) return;

    agentos_mutex_destroy(&arena->lock);
    chunk_destroy(arena->first_chunk);
    AGENTOS_FREE(arena);
}

void *arena_alloc(agentos_arena_t *arena, size_t size)
{
    if (!arena || size == 0) return NULL;

    /* 对齐到 ARENA_ALIGNMENT */
    size_t aligned = (size + ARENA_ALIGNMENT - 1) & ~((size_t)ARENA_ALIGNMENT - 1);

    agentos_mutex_lock(&arena->lock);

    /* 超大分配直接回退到 malloc */
    if (aligned > arena->chunk_size / 2) {
        arena->fallback_count++;
        arena->total_allocated += aligned;
        arena->alloc_count++;
        agentos_mutex_unlock(&arena->lock);
        return AGENTOS_MALLOC(aligned);
    }

    /* 当前 chunk 空间不足，分配新 chunk */
    if (arena->current->bump + aligned > arena->current->end) {
        /* 检查 chunk 数量限制 */
        if (arena->max_chunks > 0 && arena->num_chunks >= arena->max_chunks) {
            agentos_mutex_unlock(&arena->lock);
            return NULL;  /* 达到 chunk 上限 */
        }

        /* 分配新 chunk（每次翻倍，直到最大） */
        size_t new_size = arena->chunk_size;
        if (arena->num_chunks > 0 && new_size < ARENA_MAX_CHUNK_SIZE) {
            new_size = arena->chunk_size * (1 << (arena->num_chunks > 4 ? 4 : arena->num_chunks));
            if (new_size > ARENA_MAX_CHUNK_SIZE) new_size = ARENA_MAX_CHUNK_SIZE;
        }

        arena_chunk_t *new_chunk = chunk_create(new_size);
        if (!new_chunk) {
            agentos_mutex_unlock(&arena->lock);
            return NULL;
        }

        /* 追加到链表尾部 */
        arena->current->next = new_chunk;
        arena->current = new_chunk;
        arena->num_chunks++;
    }

    /* Bump 分配 */
    void *ptr = arena->current->bump;
    arena->current->bump += aligned;

    arena->total_allocated += aligned;
    arena->alloc_count++;

    agentos_mutex_unlock(&arena->lock);

    return ptr;
}

void *arena_calloc(agentos_arena_t *arena, size_t size)
{
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void arena_reset(agentos_arena_t *arena)
{
    if (!arena) return;

    agentos_mutex_lock(&arena->lock);

    /* 将所有 chunk 的 bump 指针回退到起始位置 */
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        c->bump = c->start;
        /* 清零内存以辅助调试（检测 use-after-reset） */
        memset(c->start, 0, c->size);
    }

    /* 重置当前 chunk 为第一个 */
    arena->current = arena->first_chunk;

    arena->reset_count++;

    agentos_mutex_unlock(&arena->lock);
}

/* ==================== 标记 / 回退 API ==================== */

void arena_mark(agentos_arena_t *arena, arena_mark_t *mark)
{
    if (!arena || !mark) return;

    agentos_mutex_lock(&arena->lock);

    mark->arena = arena;
    mark->bump  = arena->current->bump;
    mark->chunk = (agentos_arena_t *)arena->current;

    agentos_mutex_unlock(&arena->lock);
}

void arena_release(arena_mark_t *mark)
{
    if (!mark || !mark->arena || !mark->chunk) return;

    agentos_arena_t *arena = mark->arena;
    arena_chunk_t *target = (arena_chunk_t *)mark->chunk;

    agentos_mutex_lock(&arena->lock);

    /* 从目标 chunk 开始，回退所有后续 chunk */
    arena_chunk_t *c = arena->first_chunk;
    while (c && c != target) {
        c->bump = c->start;  /* 回退较早的 chunk */
        memset(c->start, 0, c->size);
        c = c->next;
    }

    if (c == target) {
        c->bump = (uint8_t *)mark->bump;  /* 回退到标记位置 */
        if (c->bump < c->start) c->bump = c->start;

        /* 清零已释放区域 */
        if (c->bump < c->end) {
            memset(c->bump, 0, (size_t)(c->end - c->bump));
        }

        /* 后续 chunk 也回退 */
        for (arena_chunk_t *nc = c->next; nc; nc = nc->next) {
            nc->bump = nc->start;
            memset(nc->start, 0, nc->size);
        }
    }

    arena->current = target;

    agentos_mutex_unlock(&arena->lock);
}

/* ==================== 查询 API ==================== */

bool arena_get_stats(agentos_arena_t *arena, arena_stats_t *stats)
{
    if (!arena || !stats) return false;

    agentos_mutex_lock(&arena->lock);

    stats->total_allocated = arena->total_allocated;
    stats->alloc_count     = arena->alloc_count;
    stats->reset_count     = arena->reset_count;
    stats->fallback_count  = arena->fallback_count;
    stats->chunk_count     = arena->num_chunks;

    /* 计算当前使用量 */
    stats->current_used = 0;
    stats->total_chunk_bytes = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        stats->current_used += (size_t)(c->bump - c->start);
        stats->total_chunk_bytes += c->size;
    }

    agentos_mutex_unlock(&arena->lock);
    return true;
}

bool arena_contains(agentos_arena_t *arena, const void *ptr)
{
    if (!arena || !ptr) return false;

    agentos_mutex_lock(&arena->lock);

    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        if ((const uint8_t *)ptr >= c->start && (const uint8_t *)ptr < c->end) {
            agentos_mutex_unlock(&arena->lock);
            return true;
        }
    }

    agentos_mutex_unlock(&arena->lock);
    return false;
}

size_t arena_capacity(agentos_arena_t *arena)
{
    if (!arena) return 0;

    agentos_mutex_lock(&arena->lock);
    size_t cap = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        cap += c->size;
    }
    agentos_mutex_unlock(&arena->lock);
    return cap;
}

size_t arena_used(agentos_arena_t *arena)
{
    if (!arena) return 0;

    agentos_mutex_lock(&arena->lock);
    size_t used = 0;
    for (arena_chunk_t *c = arena->first_chunk; c; c = c->next) {
        used += (size_t)(c->bump - c->start);
    }
    agentos_mutex_unlock(&arena->lock);
    return used;
}

/* ==================== 线程局部 Arena (TLS) ==================== */

static _Thread_local agentos_arena_t *g_tls_arena = NULL;

agentos_arena_t *agentos_arena_get_current(void)
{
    return g_tls_arena;
}

void agentos_arena_set_current(agentos_arena_t *arena)
{
    g_tls_arena = arena;
}