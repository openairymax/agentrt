/**
 * @file slab.h
 * @brief P3.15: Slab 分配器 — per-CPU freelist + 全局 partial 链 + 构造/析构回调
 *
 * Slab 分配器为固定大小的对象提供高效分配/释放，
 * 使用 per-CPU (per-thread) 空闲链表减少锁竞争，
 * 全局 partial 链表作为 CPU 间缓存的共享池。
 *
 * 设计特点：
 *   - P3.15.1: per-CPU freelist + 全局 partial 链
 *   - P3.15.2: agentos_slab_create/destroy/alloc/free API
 *   - 构造/析构回调：分配时自动初始化，释放时自动清理
 *   - 对象大小对齐到 cache line (64B)
 *   - 线程安全：per-CPU 操作无锁，全局链操作持锁
 *
 * 性能目标：高频分配场景性能提升 > 20%
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTOS_COREKERN_SLAB_H
#define AGENTOS_COREKERN_SLAB_H

#include "export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量
 * ================================================================ */

/** 默认每个 slab 页的对象数 */
#define AGENTOS_SLAB_DEFAULT_OBJS_PER_PAGE  64

/** 最大 CPU 数 */
#define AGENTOS_SLAB_MAX_CPUS               64

/** 默认 slab 名称最大长度 */
#define AGENTOS_SLAB_NAME_MAX               32

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 构造回调 — 分配对象时调用
 *
 * 用于初始化对象（如清零、设置默认值）。
 * 返回 NULL 表示构造失败，对象将被回收。
 *
 * @param obj  新分配的对象指针
 * @param arg  用户自定义参数
 * @return 构造后的对象指针，失败返回 NULL
 */
typedef void *(*agentos_slab_ctor_fn)(void *obj, void *arg);

/**
 * @brief 析构回调 — 释放对象时调用
 *
 * 用于清理对象（如释放子资源、关闭文件描述符）。
 *
 * @param obj  要释放的对象指针
 * @param arg  用户自定义参数
 */
typedef void (*agentos_slab_dtor_fn)(void *obj, void *arg);

/**
 * @brief Slab 创建配置
 */
typedef struct {
    const char            *name;             /**< Slab 名称（调试用） */
    size_t                 obj_size;         /**< 单个对象大小（字节） */
    uint32_t               objs_per_page;    /**< 每页对象数（0=默认64） */
    agentos_slab_ctor_fn   ctor;             /**< 构造回调（可为 NULL） */
    agentos_slab_dtor_fn   dtor;             /**< 析构回调（可为 NULL） */
    void                  *ctor_arg;         /**< 构造回调参数 */
    void                  *dtor_arg;         /**< 析构回调参数 */
    bool                   enable_stats;     /**< 是否启用统计 */
} agentos_slab_config_t;

/**
 * @brief Slab 统计信息
 */
typedef struct {
    uint64_t total_allocs;         /**< 总分配次数 */
    uint64_t total_frees;          /**< 总释放次数 */
    uint64_t cpu_hit_count;        /**< per-CPU freelist 命中次数 */
    uint64_t cpu_miss_count;       /**< per-CPU freelist 未命中次数 */
    uint64_t partial_hit_count;    /**< 全局 partial 链命中次数 */
    uint64_t page_alloc_count;     /**< 新页分配次数 */
    uint64_t page_free_count;      /**< 页回收次数 */
    uint64_t ctor_fail_count;      /**< 构造失败次数 */
    uint32_t active_objs;          /**< 当前活跃对象数 */
    uint32_t peak_objs;            /**< 峰值活跃对象数 */
    uint32_t cpu_freelist_len;     /**< 当前 CPU freelist 长度 */
    double   cpu_hit_rate;         /**< CPU 命中率 (0.0-1.0) */
} agentos_slab_stats_t;

/**
 * @brief Slab 分配器句柄（不透明指针）
 */
typedef struct agentos_slab agentos_slab_t;

/* ================================================================
 * API
 * ================================================================ */

/**
 * @brief 创建 Slab 分配器
 *
 * 分配并初始化一个新的 slab 分配器。
 * 使用默认配置时只需填入 obj_size 和 name。
 *
 * @param config 配置（NULL 返回 NULL）
 * @return slab 句柄，失败返回 NULL
 *
 * 使用示例：
 *   agentos_slab_config_t cfg = {
 *       .name = "ipc_msg",
 *       .obj_size = sizeof(ipc_msg_t),
 *   };
 *   agentos_slab_t *slab = agentos_slab_create(&cfg);
 */
AGENTOS_API agentos_slab_t *agentos_slab_create(const agentos_slab_config_t *config);

/**
 * @brief 销毁 Slab 分配器
 *
 * 释放所有 slab 页和元数据。销毁前确保所有对象已归还。
 *
 * @param slab slab 句柄
 */
AGENTOS_API void agentos_slab_destroy(agentos_slab_t *slab);

/**
 * @brief 从 Slab 分配一个对象
 *
 * 分配流程：
 *   1. 检查 per-CPU freelist，有则直接返回
 *   2. 检查全局 partial 链，有则从 partial 页取
 *   3. 分配新页，取第一个对象
 *   4. 调用构造回调（如有）
 *
 * @param slab slab 句柄
 * @return 对象指针，失败返回 NULL
 */
AGENTOS_API void *agentos_slab_alloc(agentos_slab_t *slab);

/**
 * @brief 释放对象到 Slab
 *
 * 释放流程：
 *   1. 调用析构回调（如有）
 *   2. 加入 per-CPU freelist
 *   3. 如果 freelist 过长，批量归还到全局 partial 链
 *
 * @param slab slab 句柄
 * @param obj  要释放的对象指针
 */
AGENTOS_API void agentos_slab_free(agentos_slab_t *slab, void *obj);

/**
 * @brief 获取 Slab 统计信息
 *
 * @param slab      slab 句柄
 * @param out_stats 输出统计（可为 NULL 跳过）
 */
AGENTOS_API void agentos_slab_get_stats(agentos_slab_t *slab,
                                         agentos_slab_stats_t *out_stats);

/**
 * @brief 重置 Slab 统计计数器
 *
 * @param slab slab 句柄
 */
AGENTOS_API void agentos_slab_reset_stats(agentos_slab_t *slab);

/**
 * @brief 获取 Slab 对象大小
 *
 * @param slab slab 句柄
 * @return 对象大小（字节），slab 为 NULL 返回 0
 */
AGENTOS_API size_t agentos_slab_obj_size(agentos_slab_t *slab);

/**
 * @brief 获取 Slab 名称
 *
 * @param slab slab 句柄
 * @return 名称字符串，slab 为 NULL 返回 "null"
 */
AGENTOS_API const char *agentos_slab_name(agentos_slab_t *slab);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COREKERN_SLAB_H */