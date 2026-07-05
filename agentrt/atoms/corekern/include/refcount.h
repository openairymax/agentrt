/**
 * @file refcount.h
 * @brief 引用计数对象 — 所有权模型基础设施
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.21: 引用计数用于 IPC 共享缓冲区等跨模块共享对象。
 * 基于 _Atomic uint32_t 实现线程安全的引用计数。
 *
 * 所有权语义：
 *   - refcount_alloc()  → OWNER（初始 refcount=1）
 *   - refcount_retain() → BORROW（增加 refcount）
 *   - refcount_release()→ TRANSFER（减少 refcount，到0时调用 deleter）
 */

#ifndef AGENTRT_REFCOUNT_H
#define AGENTRT_REFCOUNT_H

#include "export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 引用计数对象头部
 *
 * 嵌入在引用计数对象的开头。使用方式：
 *   struct my_obj {
 *       refcounted_t rc;    // 必须是第一个字段
 *       int my_data;
 *   };
 */
typedef struct refcounted {
    uint32_t _Atomic refcount;  /**< 原子引用计数 */
    void (*deleter)(struct refcounted *); /**< 释放回调 */
} refcounted_t;

/**
 * @brief 分配引用计数对象
 *
 * @param size 对象总大小（含 refcounted_t 头部）
 * @param deleter 释放回调（refcount 降为 0 时调用）
 * @return 对象指针，失败返回 NULL
 *
 * @ownership 返回的指针: OWNER（refcount=1）
 * @threadsafe 是
 */
AGENTRT_API void *refcount_alloc(size_t size, void (*deleter)(refcounted_t *));

/**
 * @brief 增加引用计数
 *
 * @param obj 引用计数对象
 * @return 增加后的引用计数
 *
 * @ownership obj: BORROW（调用者必须已持有引用）
 * @threadsafe 是
 */
AGENTRT_API uint32_t refcount_retain(refcounted_t *obj);

/**
 * @brief 减少引用计数
 *
 * 当引用计数降为 0 时，调用 deleter 释放对象。
 *
 * @param obj 引用计数对象
 * @return 减少后的引用计数（0 表示对象已释放）
 *
 * @ownership obj: TRANSFER（调用者释放自己的引用）
 * @threadsafe 是
 */
AGENTRT_API uint32_t refcount_release(refcounted_t *obj);

/**
 * @brief 获取当前引用计数
 *
 * @param obj 引用计数对象
 * @return 当前引用计数
 *
 * @threadsafe 是（原子读取）
 */
AGENTRT_API uint32_t refcount_get(refcounted_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_REFCOUNT_H */
