/**
 * @file refcount.c
 * @brief 引用计数对象实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.21: 基于 C11 _Atomic 的线程安全引用计数。
 */

#include "refcount.h"

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

void *refcount_alloc(size_t size, void (*deleter)(refcounted_t *))
{
    if (size < sizeof(refcounted_t) || !deleter)
        return NULL;

    refcounted_t *obj = (refcounted_t *)calloc(1, size);
    if (!obj)
        return NULL;

    atomic_init(&obj->refcount, 1);
    obj->deleter = deleter;

    return (void *)obj;
}

uint32_t refcount_retain(refcounted_t *obj)
{
    if (!obj)
        return 0;

    /* 原子递增，使用 memory_order_acq_rel 保证可见性 */
    uint32_t old = atomic_fetch_add_explicit(&obj->refcount, 1,
                                              memory_order_acq_rel);
    return old + 1;
}

uint32_t refcount_release(refcounted_t *obj)
{
    if (!obj)
        return 0;

    /* 原子递减，使用 memory_order_acq_rel */
    uint32_t old = atomic_fetch_sub_explicit(&obj->refcount, 1,
                                              memory_order_acq_rel);
    uint32_t new_count = old - 1;

    if (new_count == 0 && obj->deleter) {
        /* 引用计数归零，调用释放回调 */
        obj->deleter(obj);
    }

    return new_count;
}

uint32_t refcount_get(refcounted_t *obj)
{
    if (!obj)
        return 0;

    return atomic_load_explicit(&obj->refcount, memory_order_acquire);
}
