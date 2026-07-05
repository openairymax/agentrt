/**
 * @file arena.h
 * @brief Arena 分配器 — 线性分配 + 链式扩展 + 整体 reset
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.19: Arena 分配器用于短生命周期内存分配（ALLOC_SHORT_LIVED），
 * 核心优势：
 *   - O(1) 分配（bump pointer）
 *   - O(1) 整体释放（arena_reset）
 *   - 零碎片（线性分配）
 *   - 链式扩展（单块用完自动分配新块）
 *
 * 典型用法：
 *   agentrt_arena_t *arena = agentrt_arena_create(64 * 1024);  // 64KB 初始块
 *   void *p1 = agentrt_arena_alloc(arena, 128);
 *   void *p2 = agentrt_arena_alloc(arena, 256);
 *   // ... 使用 p1, p2 ...
 *   agentrt_arena_reset(arena);   // 整体释放，arena 可复用
 *   agentrt_arena_destroy(arena);
 */

#ifndef AGENTRT_COREKERN_ARENA_H
#define AGENTRT_COREKERN_ARENA_H

#include "export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Arena 句柄（不透明指针）
 */
typedef struct agentrt_arena agentrt_arena_t;

/**
 * @brief Arena 标记点，用于部分回滚
 *
 * 由 agentrt_arena_mark() 生成，传给 agentrt_arena_reset_to() 可回滚到标记点。
 */
typedef struct {
    size_t offset;     /**< 块内偏移 */
    size_t block_idx;  /**< 块索引 */
} agentrt_arena_mark_t;

/**
 * @brief 创建 Arena 分配器
 *
 * @param block_size 每个内存块的大小（字节），0 使用默认 64KB
 * @return Arena 句柄，失败返回 NULL
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentrt_arena_destroy() 释放
 * @threadsafe 否
 */
AGENTRT_API agentrt_arena_t *agentrt_arena_create(size_t block_size);

/**
 * @brief 销毁 Arena 分配器
 *
 * 释放所有内存块和 Arena 结构本身。
 *
 * @param arena Arena 句柄
 *
 * @ownership arena: TRANSFER
 * @threadsafe 否
 */
AGENTRT_API void agentrt_arena_destroy(agentrt_arena_t *arena);

/**
 * @brief 从 Arena 分配内存
 *
 * O(1) 线性分配。当前块空间不足时自动扩展新块。
 * 分配的内存对齐到 sizeof(void*) 字节。
 *
 * @param arena Arena 句柄
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回 NULL
 *
 * @ownership 返回的指针生命周期由 Arena 管理，arena_reset/destroy 后失效
 * @threadsafe 否
 */
AGENTRT_API void *agentrt_arena_alloc(agentrt_arena_t *arena, size_t size);

/**
 * @brief 从 Arena 分配对齐内存
 *
 * @param arena Arena 句柄
 * @param size 分配大小（字节）
 * @param alignment 对齐要求（必须是 2 的幂）
 * @return 分配的内存指针，失败返回 NULL
 *
 * @ownership 返回的指针生命周期由 Arena 管理
 * @threadsafe 否
 */
AGENTRT_API void *agentrt_arena_alloc_aligned(agentrt_arena_t *arena, size_t size,
                                               size_t alignment);

/**
 * @brief 整体重置 Arena
 *
 * 将 Arena 恢复到初始状态，所有已分配内存失效。
 * 保留第一个内存块供后续使用，释放多余的扩展块。
 *
 * @param arena Arena 句柄
 *
 * @threadsafe 否
 */
AGENTRT_API void agentrt_arena_reset(agentrt_arena_t *arena);

/**
 * @brief 获取当前 Arena 标记点
 *
 * 记录当前分配位置，用于后续部分回滚。
 *
 * @param arena Arena 句柄
 * @return 标记点
 *
 * @threadsafe 否
 */
AGENTRT_API agentrt_arena_mark_t agentrt_arena_mark(agentrt_arena_t *arena);

/**
 * @brief 回滚 Arena 到指定标记点
 *
 * 释放标记点之后的所有分配，Arena 恢复到标记时的状态。
 *
 * @param arena Arena 句柄
 * @param mark 标记点（由 agentrt_arena_mark() 生成）
 *
 * @threadsafe 否
 */
AGENTRT_API void agentrt_arena_reset_to(agentrt_arena_t *arena, agentrt_arena_mark_t mark);

/**
 * @brief 获取 Arena 已使用的总字节数
 *
 * @param arena Arena 句柄
 * @return 已使用字节数
 */
AGENTRT_API size_t agentrt_arena_used(agentrt_arena_t *arena);

/**
 * @brief 获取 Arena 已分配的总字节数（含块内碎片）
 *
 * @param arena Arena 句柄
 * @return 已分配字节数
 */
AGENTRT_API size_t agentrt_arena_capacity(agentrt_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_COREKERN_ARENA_H */
