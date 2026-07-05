/**
 * @file arena.h
 * @brief P1.19: Arena 短生命周期线性分配器
 *
 * Arena 分配器提供 O(1) 分配和 O(1) 整体释放（reset），
 * 适用于请求处理路径中的短生命周期内存分配。
 *
 * 架构：
 *   arena_create() -> 创建 Arena（初始 64KB 块）
 *   arena_alloc()  -> O(1) 线性 bump 分配
 *   arena_reset()  -> O(1) 整体释放（回退 bump 指针）
 *   arena_destroy() -> 销毁 Arena
 *
 * 特性：
 *   - 链式扩展：大块分配自动分配新 chunk（链表串联）
 *   - 线程局部存储：Per-thread Arena 减少锁竞争
 *   - mark/release：支持临时回退点（bump 指针快照）
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_ARENA_H
#define AGENTRT_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 默认配置 ==================== */

#define ARENA_DEFAULT_CHUNK_SIZE (64 * 1024)   /**< 默认 chunk 大小 64KB */
#define ARENA_MAX_CHUNK_SIZE     (1024 * 1024) /**< 最大 chunk 大小 1MB */
#define ARENA_ALIGNMENT          16            /**< 默认对齐 16 字节 */

/* ==================== Arena 句柄 ==================== */

typedef struct agentrt_arena agentrt_arena_t;

/* ==================== Arena 统计 ==================== */

typedef struct {
    size_t total_allocated;    /**< 累计分配字节数 */
    size_t current_used;       /**< 当前已使用字节数 */
    size_t chunk_count;        /**< chunk 数量 */
    size_t total_chunk_bytes;  /**< 所有 chunk 总字节数 */
    uint64_t alloc_count;      /**< 分配次数 */
    uint64_t reset_count;      /**< reset 次数 */
    uint64_t fallback_count;   /**< 回退到 malloc 的次数（超大分配） */
} arena_stats_t;

/* ==================== Arena 标记（回退点） ==================== */

typedef struct {
    agentrt_arena_t *arena;  /**< 所属 Arena */
    void            *bump;   /**< bump 指针快照 */
    agentrt_arena_t *chunk;  /**< 当前 chunk 快照 */
} arena_mark_t;

/* ==================== 生命周期 API ==================== */

/**
 * @brief P1.19.2: 创建 Arena 分配器
 *
 * @ownership alloc — 返回的 Arena 句柄由调用者持有，需通过 arena_destroy 释放
 *
 * @param chunk_size  初始 chunk 大小（0 使用默认 64KB）
 * @param max_chunks  最大 chunk 数量（0 无限制）
 * @return Arena 句柄，失败返回 NULL
 */
agentrt_arena_t *arena_create(size_t chunk_size, size_t max_chunks);

/**
 * @brief P1.19.2: 销毁 Arena 并释放所有内存
 *
 * @ownership release — 释放 arena 句柄所有权，销毁后所有通过此 Arena 分配的指针失效
 *
 * @param arena Arena 句柄
 */
void arena_destroy(agentrt_arena_t *arena);

/* ==================== 分配 API ==================== */

/**
 * @brief P1.19.2: 从 Arena 线性分配内存（bump 指针前进）
 *
 * @ownership borrow — 返回的指针生命周期受 Arena 管理，arena_reset/destroy 后即失效
 *
 * 分配速度 O(1)，无碎片。当当前 chunk 不足时自动分配新 chunk。
 * 超大分配（> chunk_size / 2）直接回退到 malloc，不占用 Arena 空间。
 *
 * @param arena Arena 句柄
 * @param size  分配大小（字节）
 * @return 分配的内存指针（16 字节对齐），失败返回 NULL
 */
void *arena_alloc(agentrt_arena_t *arena, size_t size);

/**
 * @brief P1.19.2: 从 Arena 分配并清零内存
 *
 * @ownership borrow — 返回的指针生命周期受 Arena 管理，arena_reset/destroy 后即失效
 *
 * @param arena Arena 句柄
 * @param size  分配大小（字节）
 * @return 清零的内存指针，失败返回 NULL
 */
void *arena_calloc(agentrt_arena_t *arena, size_t size);

/* ==================== 释放 / 重置 API ==================== */

/**
 * @brief P1.19.2: 整体重置 Arena（O(1)）
 *
 * 将所有 chunk 的 bump 指针回退到起始位置，所有先前分配的内存失效。
 * 调用者必须确保所有通过 Arena 分配的指针不再使用。
 *
 * @param arena Arena 句柄
 */
void arena_reset(agentrt_arena_t *arena);

/* ==================== 标记 / 回退 API ==================== */

/**
 * @brief P1.19.2: 创建回退标记（保存当前 bump 指针位置）
 *
 * 用于请求处理中间阶段需要临时分配后回退的场景。
 *
 * @param arena Arena 句柄
 * @param mark  输出标记（调用者分配）
 */
void arena_mark(agentrt_arena_t *arena, arena_mark_t *mark);

/**
 * @brief P1.19.2: 回退到标记位置
 *
 * 所有在 mark 之后通过 arena_alloc 分配的内存失效。
 *
 * @param mark 之前通过 arena_mark 创建的标记
 */
void arena_release(arena_mark_t *mark);

/* ==================== 查询 API ==================== */

/**
 * @brief 获取 Arena 统计信息
 * @param arena Arena 句柄
 * @param stats 输出统计信息
 * @return true 成功
 */
bool arena_get_stats(agentrt_arena_t *arena, arena_stats_t *stats);

/**
 * @brief 检查指针是否在当前 Arena 的某个 chunk 范围内
 * @param arena Arena 句柄
 * @param ptr   待检查指针
 * @return true 属于此 Arena
 */
bool arena_contains(agentrt_arena_t *arena, const void *ptr);

/**
 * @brief 获取 Arena 当前总容量
 * @param arena Arena 句柄
 * @return 总容量（字节）
 */
size_t arena_capacity(agentrt_arena_t *arena);

/**
 * @brief 获取 Arena 当前已使用量
 * @param arena Arena 句柄
 * @return 已使用字节数
 */
size_t arena_used(agentrt_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ARENA_H */