/**
 * @file slab.h
 * @brief P3.15: Slab 分配器 — per-CPU freelist + 全局 partial 链 + 构造/析构回调
 *
 * Slab 分配器用于固定大小对象的高频分配/释放。
 * 核心优势：
 *   - O(1) 分配/释放（per-CPU freelist）
 *   - 零碎片（固定大小对象）
 *   - 构造/析构回调（C++ 风格 RAII 支持）
 *   - 全局 partial 链（CPU 间负载均衡）
 *
 * 典型用法：
 *   agentos_slab_t *slab = agentos_slab_create(sizeof(my_struct), 64, NULL, NULL);
 *   my_struct *obj = (my_struct *)agentos_slab_alloc(slab);
 *   // ... 使用 obj ...
 *   agentos_slab_free(slab, obj);
 *   agentos_slab_destroy(slab);
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef CUPOLAS_SLAB_H
#define CUPOLAS_SLAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型定义
 * ================================================================ */

/** Slab 句柄（不透明） */
typedef struct agentos_slab agentos_slab_t;

/** 构造回调：在对象分配后调用 */
typedef void (*agentos_slab_ctor_t)(void *obj, void *user_data);

/** 析构回调：在对象释放前调用 */
typedef void (*agentos_slab_dtor_t)(void *obj, void *user_data);

/** Slab 统计信息 */
typedef struct {
    size_t obj_size;            /**< 对象大小 */
    size_t objs_per_slab;       /**< 每个 slab 页的对象数 */
    size_t total_slabs;         /**< 总 slab 页数 */
    size_t full_slabs;          /**< 满页数 */
    size_t partial_slabs;       /**< 部分满页数 */
    size_t empty_slabs;         /**< 空页数 */
    size_t total_allocs;        /**< 总分配次数 */
    size_t total_frees;         /**< 总释放次数 */
    size_t active_objects;      /**< 当前活跃对象数 */
    size_t cpu_steals;          /**< CPU 间偷取次数 */
} agentos_slab_stats_t;

/* ================================================================
 * 生命周期 API
 * ================================================================ */

/**
 * @brief 创建 Slab 分配器
 *
 * @param obj_size        对象大小（字节），会自动对齐到 sizeof(void*)
 * @param objs_per_slab   每个 slab 页的对象数，0 使用默认值（根据 obj_size 计算）
 * @param ctor            构造回调（可为 NULL）
 * @param dtor            析构回调（可为 NULL）
 * @param user_data       传递给构造/析构回调的用户数据
 * @return Slab 句柄，失败返回 NULL
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentos_slab_destroy() 释放
 * @threadsafe 是（per-CPU freelist + 全局锁）
 */
agentos_slab_t *agentos_slab_create(size_t obj_size,
                                     size_t objs_per_slab,
                                     agentos_slab_ctor_t ctor,
                                     agentos_slab_dtor_t dtor,
                                     void *user_data);

/**
 * @brief 销毁 Slab 分配器
 *
 * 释放所有 slab 页和 Slab 结构本身。
 * 注意：不会对已分配对象调用析构函数（调用者需确保先释放所有对象）。
 *
 * @param slab Slab 句柄
 *
 * @ownership slab: TRANSFER
 * @threadsafe 否（调用者需确保无并发访问）
 */
void agentos_slab_destroy(agentos_slab_t *slab);

/* ================================================================
 * 分配/释放 API
 * ================================================================ */

/**
 * @brief 从 Slab 分配一个对象
 *
 * 优先从当前 CPU 的 freelist 获取，失败时从全局 partial 链获取。
 * 分配后自动调用构造回调（如果已设置）。
 *
 * @param slab Slab 句柄
 * @return 对象指针，失败返回 NULL
 *
 * @ownership 返回的对象由调用者管理，需通过 agentos_slab_free() 归还
 * @threadsafe 是
 */
void *agentos_slab_alloc(agentos_slab_t *slab);

/**
 * @brief 释放对象回 Slab
 *
 * 释放前自动调用析构回调（如果已设置）。
 * 对象归还到当前 CPU 的 freelist，如果 freelist 已满则归还到全局 partial 链。
 *
 * @param slab Slab 句柄
 * @param obj  对象指针（NULL 无操作）
 *
 * @ownership obj: TRANSFER
 * @threadsafe 是
 */
void agentos_slab_free(agentos_slab_t *slab, void *obj);

/* ================================================================
 * 统计与诊断 API
 * ================================================================ */

/**
 * @brief 获取 Slab 统计信息
 *
 * @param slab  Slab 句柄
 * @param stats 输出统计信息
 * @return 0 成功，非0失败
 */
int agentos_slab_get_stats(agentos_slab_t *slab, agentos_slab_stats_t *stats);

/**
 * @brief 收缩 Slab：释放所有空 slab 页
 *
 * @param slab Slab 句柄
 * @return 释放的 slab 页数
 */
size_t agentos_slab_shrink(agentos_slab_t *slab);

/**
 * @brief 验证 Slab 内部一致性
 *
 * @param slab Slab 句柄
 * @return true 一致，false 损坏
 */
bool agentos_slab_validate(agentos_slab_t *slab);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SLAB_H */