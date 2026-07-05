/**
 * @file arena.h
 * @brief P1.19: Arena 线性分配器 — 链式扩展 + 整体 reset
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 适用于短生命周期对象的批量分配。
 * 基于区块（block）的线性分配器，区块用尽后自动链式扩展。
 * 通过 arena_reset() 一次性释放所有内存。
 *
 * 设计目标：
 *   - O(1) 分配（仅移动指针）
 *   - O(1) 批量释放（reset 整个 arena）
 *   - 零碎片（线性分配，无 free 操作）
 *   - 线程不安全（由调用者加锁或使用 tcache）
 *
 * @see tcache.h  per-thread 缓存层
 */

#ifndef AGENTRT_ARENA_H
#define AGENTRT_ARENA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Arena 句柄 ==================== */

typedef struct arena_s arena_t;

/* ==================== 配置 ==================== */

typedef struct {
    size_t block_size;          /**< 单个区块大小，默认 64KB */
    size_t alignment;           /**< 对齐要求，默认 16 */
    bool use_huge_pages;        /**< 是否使用大页（Linux only） */
} arena_config_t;

/* ==================== 标记 ==================== */

/**
 * @brief Arena 位置标记
 *
 * 用于记录当前分配位置，以便后续回滚。
 */
typedef struct {
    void *block_start;          /**< 当前区块起始 */
    size_t offset;              /**< 当前区块内偏移 */
    void *arena_internal;       /**< 内部标记 */
} arena_mark_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建 Arena 分配器
 *
 * @param config 配置（NULL 使用默认）
 * @return arena 句柄，失败返回 NULL
 */
arena_t *arena_create(const arena_config_t *config);

/**
 * @brief 销毁 Arena 分配器
 *
 * 释放所有区块和元数据。
 *
 * @param arena arena 句柄
 */
void arena_destroy(arena_t *arena);

/* ==================== 分配操作 ==================== */

/**
 * @brief 从 Arena 分配内存
 *
 * 如果当前区块空间不足，自动分配新区块（链式扩展）。
 *
 * @param arena arena 句柄
 * @param size 分配大小
 * @return 内存指针，失败返回 NULL
 */
void *arena_alloc(arena_t *arena, size_t size);

/**
 * @brief 从 Arena 分配对齐内存
 *
 * @param arena arena 句柄
 * @param size 分配大小
 * @param alignment 对齐要求（必须是 2 的幂）
 * @return 内存指针，失败返回 NULL
 */
void *arena_alloc_aligned(arena_t *arena, size_t size, size_t alignment);

/**
 * @brief 从 Arena 分配并清零
 *
 * @param arena arena 句柄
 * @param size 分配大小
 * @return 内存指针，失败返回 NULL
 */
void *arena_calloc(arena_t *arena, size_t size);

/**
 * @brief 重置 Arena
 *
 * 将所有区块的分配指针重置到起始位置。
 * 不释放区块内存，后续分配复用已有区块。
 *
 * @param arena arena 句柄
 */
void arena_reset(arena_t *arena);

/* ==================== 标记与回滚 ==================== */

/**
 * @brief 记录当前 Arena 位置
 *
 * @param arena arena 句柄
 * @return 位置标记
 */
arena_mark_t arena_mark(arena_t *arena);

/**
 * @brief 回滚到标记位置
 *
 * 释放标记之后分配的所有区块，并将当前区块指针回退到标记位置。
 * 如果标记之后分配了新区块，这些区块被释放。
 *
 * @param arena arena 句柄
 * @param mark 位置标记
 */
void arena_rollback(arena_t *arena, arena_mark_t mark);

/* ==================== 查询 ==================== */

/**
 * @brief 获取 Arena 统计信息
 *
 * @param arena arena 句柄
 * @param out_total_allocated 输出总分配量
 * @param out_total_blocks 输出总区块数
 * @param out_current_usage 输出当前使用量
 * @param out_peak_usage 输出峰值使用量
 */
void arena_get_stats(arena_t *arena,
                     size_t *out_total_allocated,
                     size_t *out_total_blocks,
                     size_t *out_current_usage,
                     size_t *out_peak_usage);

/**
 * @brief 获取当前区块中的剩余空间
 *
 * @param arena arena 句柄
 * @return 剩余字节数
 */
size_t arena_available(arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ARENA_H */