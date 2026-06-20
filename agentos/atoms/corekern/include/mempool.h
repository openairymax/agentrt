/**
 * @file mempool.h
 * @brief P3.16: mempool 最小保证分配器 — 紧急预留 + 对象池 + 最低保证分配
 *
 * mempool 在系统内存紧张（OOM）时提供最低保证分配能力，
 * 确保关键路径（IPC 消息传递）永远不会因内存不足而死锁。
 *
 * 设计特点：
 *   - P3.16.1: 紧急预留 + 对象池 + 最低保证分配
 *   - P3.16.2: IPC 路径预留 32MB 紧急内存
 *   - P3.16.3: OOM 时 IPC 消息分配从预留池获取
 *   - 两级分配：普通池（日常使用）+ 紧急池（OOM 专用）
 *   - 线程安全：分配/释放内部持锁
 *
 * 验证目标：OOM 场景下 IPC 不死锁
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_COREKERN_MEMPOOL_H
#define AGENTOS_COREKERN_MEMPOOL_H

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

/** IPC 路径默认紧急预留内存 (32MB) */
#define AGENTOS_MEMPOOL_IPC_EMERGENCY_RESERVE_MB  32

/** 默认最大对象大小 */
#define AGENTOS_MEMPOOL_DEFAULT_MAX_OBJ_SIZE       (64 * 1024)

/** mempool 名称最大长度 */
#define AGENTOS_MEMPOOL_NAME_MAX                   32

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief mempool 分配模式
 */
typedef enum {
    AGENTOS_MEMPOOL_MODE_NORMAL   = 0,  /**< 正常模式：优先普通池 */
    AGENTOS_MEMPOOL_MODE_EMERGENCY = 1, /**< 紧急模式：仅使用紧急池 */
} agentos_mempool_mode_t;

/**
 * @brief mempool 创建配置
 */
typedef struct {
    const char           *name;                  /**< 名称（调试用） */
    size_t                emergency_reserve;     /**< 紧急预留大小（字节，0=使用默认 32MB） */
    size_t                max_obj_size;          /**< 最大对象大小（0=默认 64KB） */
    bool                  enable_stats;          /**< 是否启用统计 */
} agentos_mempool_config_t;

/**
 * @brief mempool 统计信息
 */
typedef struct {
    /* 普通池 */
    uint64_t normal_allocs;           /**< 普通池分配次数 */
    uint64_t normal_frees;            /**< 普通池释放次数 */
    uint64_t normal_fail_count;       /**< 普通池分配失败次数 */
    size_t   normal_allocated;        /**< 普通池当前已分配（字节） */
    size_t   normal_peak;             /**< 普通池峰值（字节） */

    /* 紧急池 */
    uint64_t emergency_allocs;        /**< 紧急池分配次数 */
    uint64_t emergency_frees;         /**< 紧急池释放次数 */
    uint64_t emergency_fail_count;    /**< 紧急池分配失败次数 */
    size_t   emergency_allocated;     /**< 紧急池当前已分配（字节） */
    size_t   emergency_peak;          /**< 紧急池峰值（字节） */
    size_t   emergency_reserved;      /**< 紧急池总预留（字节） */

    /* 全局 */
    uint64_t oom_trigger_count;       /**< OOM 触发次数 */
    uint64_t oom_rescue_count;        /**< OOM 救援成功次数 */
    bool     emergency_mode;          /**< 当前是否处于紧急模式 */
} agentos_mempool_stats_t;

/**
 * @brief mempool 句柄（不透明指针）
 */
typedef struct agentos_mempool agentos_mempool_t;

/* ================================================================
 * API
 * ================================================================ */

/**
 * @brief 创建 mempool
 *
 * 分配并初始化 mempool，包括普通池和紧急池。
 * 紧急池在创建时预分配，确保 OOM 时可用。
 *
 * @param config 配置（NULL 使用默认：32MB 紧急预留）
 * @return mempool 句柄，失败返回 NULL
 *
 * 使用示例：
 *   agentos_mempool_config_t cfg = {
 *       .name = "ipc_mempool",
 *       .emergency_reserve = 32 * 1024 * 1024,
 *   };
 *   agentos_mempool_t *mp = agentos_mempool_create(&cfg);
 */
AGENTOS_API agentos_mempool_t *agentos_mempool_create(
    const agentos_mempool_config_t *config);

/**
 * @brief 销毁 mempool
 *
 * 释放所有内存，包括紧急预留。
 * 销毁前确保所有分配的对象已归还。
 *
 * @param mp mempool 句柄
 */
AGENTOS_API void agentos_mempool_destroy(agentos_mempool_t *mp);

/**
 * @brief 从 mempool 分配内存
 *
 * 分配策略：
 *   - 正常模式：优先从普通池分配，失败时从紧急池分配
 *   - 紧急模式：仅从紧急池分配
 *
 * 分配的内存对齐到 cache line (64B)。
 *
 * @param mp   mempool 句柄
 * @param size 分配大小（字节）
 * @return 内存指针，失败返回 NULL
 */
AGENTOS_API void *agentos_mempool_alloc(agentos_mempool_t *mp, size_t size);

/**
 * @brief 释放内存到 mempool
 *
 * 自动识别内存属于普通池还是紧急池并归还。
 *
 * @param mp  mempool 句柄
 * @param ptr 要释放的内存指针
 */
AGENTOS_API void agentos_mempool_free(agentos_mempool_t *mp, void *ptr);

/**
 * @brief 进入/退出紧急模式
 *
 * 紧急模式下所有分配仅从紧急池获取。
 * OOM handler 检测到 CRITICAL/FATAL 压力时调用。
 *
 * @param mp     mempool 句柄
 * @param enable true=进入紧急模式, false=退出
 */
AGENTOS_API void agentos_mempool_set_emergency(agentos_mempool_t *mp, bool enable);

/**
 * @brief 获取 mempool 统计信息
 *
 * @param mp        mempool 句柄
 * @param out_stats 输出统计（可为 NULL 跳过）
 */
AGENTOS_API void agentos_mempool_get_stats(agentos_mempool_t *mp,
                                            agentos_mempool_stats_t *out_stats);

/**
 * @brief 重置 mempool 统计计数器
 *
 * @param mp mempool 句柄
 */
AGENTOS_API void agentos_mempool_reset_stats(agentos_mempool_t *mp);

/**
 * @brief 获取紧急池剩余可用空间
 *
 * 用于 OOM handler 判断是否需要进一步降级。
 *
 * @param mp mempool 句柄
 * @return 剩余可用字节数，mp 为 NULL 返回 0
 */
AGENTOS_API size_t agentos_mempool_emergency_available(agentos_mempool_t *mp);

/**
 * @brief 获取 mempool 名称
 *
 * @param mp mempool 句柄
 * @return 名称字符串，mp 为 NULL 返回 "null"
 */
AGENTOS_API const char *agentos_mempool_name(agentos_mempool_t *mp);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COREKERN_MEMPOOL_H */