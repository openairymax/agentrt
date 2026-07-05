/**
 * @file refcount.c
 * @brief P1.21: 所有权模型 + 原子引用计数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 使用 _Atomic uint32_t 实现线程安全的引用计数。
 * 计数归零时自动调用 deleter 释放对象。
 *
 * 全局统计用于内存泄漏检测和调试。
 */

#include "refcount.h"
#include "logger.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 全局统计 ==================== */

static _Atomic uint64_t g_total_allocs = 0;
static _Atomic uint64_t g_total_frees = 0;
static _Atomic uint64_t g_current_live = 0;

/* ==================== 生命周期实现 ==================== */

void *refcount_alloc(size_t size, void (*deleter)(void *object))
{
    if (size < sizeof(refcounted_t)) {
        AGENTRT_LOG_ERROR("Refcount: size %zu too small (min %zu)",
                          size, sizeof(refcounted_t));
        return NULL;
    }

    void *ptr = AGENTRT_CALLOC(1, size);
    if (!ptr) {
        AGENTRT_LOG_ERROR("Refcount: OOM allocating %zu bytes", size);
        return NULL;
    }

    refcounted_t *rc = (refcounted_t *)ptr;
    atomic_init(&rc->refcount, 1);
    rc->deleter = deleter;
    rc->type_name = NULL;

    atomic_fetch_add_explicit(&g_total_allocs, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_current_live, 1, memory_order_relaxed);

    return ptr;
}

uint32_t refcount_retain(refcounted_t *rc)
{
    if (!rc) return 0;

    uint32_t old = atomic_fetch_add_explicit(&rc->refcount, 1,
                                             memory_order_relaxed);
    uint32_t new_count = old + 1;

    if (old == 0) {
        /* 从 0 增加是错误状态 — use-after-free */
        AGENTRT_LOG_ERROR("Refcount: retain on freed object (type=%s, "
                          "old=%u, new=%u)",
                          rc->type_name ? rc->type_name : "unknown",
                          old, new_count);
        atomic_store_explicit(&rc->refcount, 0, memory_order_release);
        return 0;
    }

    AGENTRT_LOG_TRACE("Refcount: retain (type=%s, %u → %u, "
                      "live=%llu)",
                      rc->type_name ? rc->type_name : "unknown",
                      old, new_count,
                      (unsigned long long)atomic_load_explicit(
                          &g_current_live, memory_order_relaxed));

    return new_count;
}

uint32_t refcount_release(refcounted_t *rc)
{
    if (!rc) return 0;

    uint32_t old = atomic_fetch_sub_explicit(&rc->refcount, 1,
                                             memory_order_acq_rel);
    if (old == 0) {
        AGENTRT_LOG_ERROR("Refcount: double-free on object (type=%s, "
                          "live=%llu)",
                          rc->type_name ? rc->type_name : "unknown",
                          (unsigned long long)atomic_load_explicit(
                              &g_current_live, memory_order_relaxed));
        return 0;
    }

    uint32_t new_count = old - 1;

    AGENTRT_LOG_TRACE("Refcount: release (type=%s, %u → %u, "
                      "live=%llu)",
                      rc->type_name ? rc->type_name : "unknown",
                      old, new_count,
                      (unsigned long long)atomic_load_explicit(
                          &g_current_live, memory_order_relaxed));

    if (new_count == 0) {
        /* 引用计数归零，调用 deleter */
        AGENTRT_LOG_DEBUG("Refcount: object freed (type=%s, "
                          "total_allocs=%llu, total_frees=%llu, live=%llu)",
                          rc->type_name ? rc->type_name : "unknown",
                          (unsigned long long)atomic_load_explicit(
                              &g_total_allocs, memory_order_relaxed),
                          (unsigned long long)atomic_load_explicit(
                              &g_total_frees, memory_order_relaxed),
                          (unsigned long long)atomic_load_explicit(
                              &g_current_live, memory_order_relaxed));

        if (rc->deleter) {
            rc->deleter(rc);
        } else {
            AGENTRT_FREE(rc);
        }

        atomic_fetch_add_explicit(&g_total_frees, 1, memory_order_relaxed);
        atomic_fetch_sub_explicit(&g_current_live, 1, memory_order_relaxed);
    }

    return new_count;
}

/* ==================== 查询实现 ==================== */

uint32_t refcount_get(const refcounted_t *rc)
{
    if (!rc) return 0;
    return atomic_load_explicit(&rc->refcount, memory_order_acquire);
}

bool refcount_is_unique(const refcounted_t *rc)
{
    return refcount_get(rc) == 1;
}

/* ==================== 全局统计实现 ==================== */

void refcount_get_global_stats(uint64_t *out_total_allocs,
                               uint64_t *out_total_frees,
                               uint64_t *out_current_live)
{
    if (out_total_allocs) {
        *out_total_allocs = atomic_load_explicit(
            &g_total_allocs, memory_order_relaxed);
    }
    if (out_total_frees) {
        *out_total_frees = atomic_load_explicit(
            &g_total_frees, memory_order_relaxed);
    }
    if (out_current_live) {
        *out_current_live = atomic_load_explicit(
            &g_current_live, memory_order_relaxed);
    }
}