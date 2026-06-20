/**
 * @file arena.c
 * @brief P1.19: Arena 线性分配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 基于区块链的线性分配器。
 * 每个区块在内部线性分配，用完自动创建新区块。
 * arena_reset() 将所有区块指针重置到起始位置。
 *
 * 区块管理：
 *   arena_t → block_1 → block_2 → ... → block_N
 *   block: [header][...data...]
 *   header 包含 offset 和 next 指针
 */

#include "arena.h"
#include "logger.h"
#include "memory_compat.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_BLOCK_SIZE (64 * 1024)   /* 64KB */
#define DEFAULT_ALIGNMENT  16
#define BLOCK_HEADER_SIZE  64           /* 区块头大小 */

/* ==================== 区块结构 ==================== */

/**
 * @brief Arena 区块
 *
 * 每个区块在独立分配的内存块中，
 * 区块头紧接在数据区前面。
 */
typedef struct arena_block_s {
    struct arena_block_s *next;       /**< 下一个区块 */
    size_t capacity;                  /**< 数据区总容量 */
    size_t offset;                    /**< 当前分配偏移 */
    size_t alignment;                 /**< 对齐要求 */
    /* 数据区紧跟区块头 */
} arena_block_t;

/* ==================== Arena 结构 ==================== */

struct arena_s {
    arena_block_t *head;              /**< 第一个区块 */
    arena_block_t *current;           /**< 当前分配区块 */
    arena_config_t config;

    /* 统计 */
    size_t total_allocated;           /**< 总分配量 */
    size_t total_blocks;              /**< 总区块数 */
    size_t current_usage;             /**< 当前使用量 */
    size_t peak_usage;                /**< 峰值使用量 */
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 对齐到指定边界
 */
static inline size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief 获取区块的数据区起始地址
 */
static inline uint8_t *block_data(arena_block_t *block)
{
    return ((uint8_t *)block) + BLOCK_HEADER_SIZE;
}

/**
 * @brief 创建新区块并链入当前区块之后
 */
static arena_block_t *arena_block_create(size_t capacity, size_t alignment)
{
    size_t total_size = BLOCK_HEADER_SIZE + capacity;
    AGENTOS_LOG_DEBUG("Arena: allocating new block (capacity=%zu, total=%zu, "
                      "alignment=%zu)",
                      capacity, total_size, alignment);

    arena_block_t *block = (arena_block_t *)AGENTOS_MALLOC(total_size);
    if (!block) {
        AGENTOS_LOG_ERROR("Arena: failed to allocate block of %zu bytes "
                          "(capacity=%zu, alignment=%zu)",
                          total_size, capacity, alignment);
        return NULL;
    }

    AGENTOS_LOG_DEBUG("Arena: block allocated at %p (data_start=%p, "
                      "data_end=%p)",
                      (void *)block,
                      (void *)((uint8_t *)block + BLOCK_HEADER_SIZE),
                      (void *)((uint8_t *)block + total_size));

    block->next = NULL;
    block->capacity = capacity;
    block->offset = 0;
    block->alignment = alignment;

    return block;
}

/**
 * @brief 在区块中分配内存
 */
static void *arena_block_alloc(arena_block_t *block, size_t size,
                               size_t alignment)
{
    size_t effective_alignment = (alignment > block->alignment)
                                     ? alignment : block->alignment;
    size_t aligned_offset = align_up(block->offset, effective_alignment);

    if (aligned_offset + size > block->capacity) {
        /* 空间不足 — 记录详细诊断信息 */
        AGENTOS_LOG_DEBUG("Arena: block %p full (requested=%zu, capacity=%zu, "
                          "offset=%zu, aligned_offset=%zu, remaining=%zu)",
                          (void *)block, size, block->capacity,
                          block->offset, aligned_offset,
                          block->capacity - block->offset);
        return NULL;
    }

    void *ptr = block_data(block) + aligned_offset;
    size_t prev_offset = block->offset;
    block->offset = aligned_offset + size;

    AGENTOS_LOG_TRACE("Arena: block_alloc %p (size=%zu, align=%zu, "
                      "offset %zu→%zu, ptr=%p, remaining=%zu)",
                      (void *)block, size, effective_alignment,
                      prev_offset, block->offset, ptr,
                      block->capacity - block->offset);

    return ptr;
}

/* ==================== 生命周期实现 ==================== */

arena_t *arena_create(const arena_config_t *config)
{
    arena_t *arena = (arena_t *)AGENTOS_CALLOC(1, sizeof(arena_t));
    if (!arena) {
        AGENTOS_LOG_ERROR("Arena: OOM creating arena");
        return NULL;
    }

    arena->config.block_size = (config && config->block_size > 0)
                                   ? config->block_size : DEFAULT_BLOCK_SIZE;
    arena->config.alignment = (config && config->alignment > 0)
                                  ? config->alignment : DEFAULT_ALIGNMENT;
    arena->config.use_huge_pages = config ? config->use_huge_pages : false;

    /* 创建第一个区块 */
    arena->head = arena_block_create(arena->config.block_size,
                                     arena->config.alignment);
    if (!arena->head) {
        AGENTOS_FREE(arena);
        return NULL;
    }

    arena->current = arena->head;
    arena->total_allocated = 0;
    arena->total_blocks = 1;
    arena->current_usage = 0;
    arena->peak_usage = 0;

    AGENTOS_LOG_DEBUG("Arena: created (block_size=%zu, alignment=%zu)",
                      arena->config.block_size, arena->config.alignment);
    return arena;
}

void arena_destroy(arena_t *arena)
{
    if (!arena) return;

    /* 释放所有区块 */
    arena_block_t *block = arena->head;
    size_t block_count = 0;
    size_t total_bytes = 0;
    while (block) {
        arena_block_t *next = block->next;
        size_t block_bytes = BLOCK_HEADER_SIZE + block->capacity;
        total_bytes += block_bytes;
        AGENTOS_LOG_TRACE("Arena: destroy freeing block %p (capacity=%zu, "
                          "used=%zu/%zu)",
                          (void *)block, block->capacity,
                          block->offset, block->capacity);
        AGENTOS_FREE(block);
        block = next;
        block_count++;
    }

    AGENTOS_LOG_INFO("Arena: destroyed (blocks=%zu, total_bytes=%zu, "
                     "peak=%zu, total_allocated=%zu)",
                     block_count, total_bytes,
                     arena->peak_usage, arena->total_allocated);
    AGENTOS_FREE(arena);
}

/* ==================== 分配操作实现 ==================== */

void *arena_alloc(arena_t *arena, size_t size)
{
    return arena_alloc_aligned(arena, size, arena->config.alignment);
}

void *arena_alloc_aligned(arena_t *arena, size_t size, size_t alignment)
{
    if (!arena || size == 0) {
        if (!arena) {
            AGENTOS_LOG_ERROR("Arena: arena_alloc_aligned called with NULL arena");
        }
        return NULL;
    }

    /* 尝试在当前区块中分配 */
    void *ptr = arena_block_alloc(arena->current, size, alignment);
    if (ptr) {
        arena->current_usage += size;
        arena->total_allocated += size;
        if (arena->current_usage > arena->peak_usage) {
            arena->peak_usage = arena->current_usage;
            AGENTOS_LOG_DEBUG("Arena: new peak usage %zu bytes (blocks=%zu, "
                              "alloc_count=%zu)",
                              arena->peak_usage, arena->total_blocks,
                              arena->total_allocated);
        }
        return ptr;
    }

    /* 当前区块空间不足，创建新区块 */
    size_t new_block_size = arena->config.block_size;
    if (size > new_block_size) {
        AGENTOS_LOG_INFO("Arena: request %zu exceeds default block_size %zu, "
                         "creating oversized block",
                         size, new_block_size);
        new_block_size = size + BLOCK_HEADER_SIZE + alignment;
    }

    AGENTOS_LOG_DEBUG("Arena: extending chain (current_block=%zu/%zu used, "
                      "new_block_size=%zu, total_blocks_before=%zu)",
                      arena->current->offset, arena->current->capacity,
                      new_block_size, arena->total_blocks);

    arena_block_t *new_block = arena_block_create(new_block_size,
                                                  arena->config.alignment);
    if (!new_block) {
        AGENTOS_LOG_ERROR("Arena: failed to extend, requested %zu bytes "
                          "(arena_total=%zu, blocks=%zu, peak=%zu)",
                          size, arena->total_allocated,
                          arena->total_blocks, arena->peak_usage);
        return NULL;
    }

    /* 链入区块列表 */
    arena->current->next = new_block;
    arena->current = new_block;
    arena->total_blocks++;

    AGENTOS_LOG_DEBUG("Arena: chain extended → block #%zu (capacity=%zu, "
                      "total_blocks=%zu)",
                      arena->total_blocks, new_block_size,
                      arena->total_blocks);

    /* 在新区块中分配 */
    ptr = arena_block_alloc(arena->current, size, alignment);
    if (ptr) {
        arena->current_usage += size;
        arena->total_allocated += size;
        if (arena->current_usage > arena->peak_usage) {
            arena->peak_usage = arena->current_usage;
        }
    } else {
        AGENTOS_LOG_ERROR("Arena: BUG — block_alloc failed in new block "
                          "(size=%zu, capacity=%zu)",
                          size, new_block_size);
    }

    return ptr;
}

void *arena_calloc(arena_t *arena, size_t size)
{
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        __builtin_memset(ptr, 0, size);
    }
    return ptr;
}

void arena_reset(arena_t *arena)
{
    if (!arena) return;

    size_t blocks_before = arena->total_blocks;
    size_t usage_before = arena->current_usage;
    size_t peak_before = arena->peak_usage;

    /* 重置所有区块的偏移 */
    arena_block_t *block = arena->head;
    while (block) {
        block->offset = 0;
        block = block->next;
    }

    /* 回到第一个区块 */
    arena->current = arena->head;
    arena->current_usage = 0;

    AGENTOS_LOG_INFO("Arena: reset (blocks=%zu, usage_before=%zu, "
                     "peak=%zu, reclaimed=%zu bytes)",
                     blocks_before, usage_before, peak_before, usage_before);
}

/* ==================== 标记与回滚实现 ==================== */

arena_mark_t arena_mark(arena_t *arena)
{
    arena_mark_t mark;
    __builtin_memset(&mark, 0, sizeof(mark));

    if (!arena) return mark;

    mark.block_start = arena->current;
    mark.offset = arena->current ? arena->current->offset : 0;
    mark.arena_internal = arena;

    return mark;
}

void arena_rollback(arena_t *arena, arena_mark_t mark)
{
    if (!arena || !mark.block_start) {
        if (!arena) {
            AGENTOS_LOG_WARN("Arena: rollback called with NULL arena");
        }
        return;
    }

    arena_block_t *target_block = (arena_block_t *)mark.block_start;
    size_t blocks_before = arena->total_blocks;
    size_t usage_before = arena->current_usage;
    size_t target_offset = mark.offset;

    /* 释放标记之后的所有区块 */
    arena_block_t *block = target_block->next;
    size_t freed_blocks = 0;
    size_t freed_bytes = 0;
    while (block) {
        arena_block_t *next = block->next;
        freed_bytes += BLOCK_HEADER_SIZE + block->capacity;
        AGENTOS_LOG_DEBUG("Arena: rollback freeing block %p (capacity=%zu)",
                          (void *)block, block->capacity);
        AGENTOS_FREE(block);
        block = next;
        arena->total_blocks--;
        freed_blocks++;
    }

    /* 恢复目标区块的状态 */
    target_block->next = NULL;
    target_block->offset = target_offset;
    arena->current = target_block;
    arena->current_usage = target_offset;

    /* 重新计算总使用量 */
    size_t usage = 0;
    block = arena->head;
    while (block) {
        usage += block->offset;
        block = block->next;
    }
    arena->current_usage = usage;

    AGENTOS_LOG_INFO("Arena: rollback → block #%zu (offset=%zu) "
                     "(freed_blocks=%zu, freed_bytes=%zu, "
                     "usage_before=%zu → usage_after=%zu, "
                     "blocks_before=%zu → blocks_after=%zu)",
                     arena->total_blocks, target_offset,
                     freed_blocks, freed_bytes,
                     usage_before, arena->current_usage,
                     blocks_before, arena->total_blocks);
}

/* ==================== 查询实现 ==================== */

void arena_get_stats(arena_t *arena,
                     size_t *out_total_allocated,
                     size_t *out_total_blocks,
                     size_t *out_current_usage,
                     size_t *out_peak_usage)
{
    if (!arena) {
        if (out_total_allocated) *out_total_allocated = 0;
        if (out_total_blocks) *out_total_blocks = 0;
        if (out_current_usage) *out_current_usage = 0;
        if (out_peak_usage) *out_peak_usage = 0;
        return;
    }

    if (out_total_allocated) *out_total_allocated = arena->total_allocated;
    if (out_total_blocks) *out_total_blocks = arena->total_blocks;
    if (out_current_usage) *out_current_usage = arena->current_usage;
    if (out_peak_usage) *out_peak_usage = arena->peak_usage;
}

size_t arena_available(arena_t *arena)
{
    if (!arena || !arena->current) return 0;
    return arena->current->capacity - arena->current->offset;
}