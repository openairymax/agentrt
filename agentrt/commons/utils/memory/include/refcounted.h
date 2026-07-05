/**
 * @file refcounted.h
 * @brief P1.21: 所有权模型 + 引用计数规范
 *
 * 提供线程安全的引用计数基础结构 refcounted_t，
 * 配合 refcount_alloc/retain/release API 管理共享对象的生命周期。
 *
 * 设计：
 *   - _Atomic uint32_t refcount 用于线程安全的引用计数
 *   - deleter 回调函数在引用计数归零时自动调用
 *   - 基于 CAS 的无锁递增/递减操作
 *
 * 所有权模型：
 *   @ownership alloc   — refcount_alloc() 返回持有 1 个引用的对象
 *   @ownership retain  — refcount_retain() 增加 1 个引用，调用者获得所有权
 *   @ownership release — refcount_release() 释放 1 个引用，归零时自动销毁
 *   @ownership borrow  — 裸指针访问不修改引用计数，调用者不能持有跨 scope
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_REFCOUNTED_H
#define AGENTRT_REFCOUNTED_H

#include "memory_compat.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 引用计数基础结构 ==================== */

/**
 * @brief P1.21.1: 引用计数基础结构（嵌入到目标对象的首部）
 *
 * 使用方式：
 *   typedef struct {
 *       refcounted_t rc;          // 必须放第一个字段
 *       char data[];              // 实际数据
 *   } my_shared_buf_t;
 */
typedef struct {
    _Atomic uint32_t refcount;     /**< 线程安全引用计数 */
    void (*deleter)(void *obj);    /**< 归零时的销毁回调 */
} refcounted_t;

/* ==================== 获取引用计数 ==================== */

/**
 * @brief 获取当前引用计数值（仅供调试）
 * @param rc refcounted_t 指针
 * @return 当前引用计数
 */
static inline uint32_t refcount_get(const refcounted_t *rc)
{
    if (!rc) return 0;
    return atomic_load_explicit(&rc->refcount, memory_order_acquire);
}

/* ==================== 分配 API ==================== */

/**
 * @brief P1.21.2: 分配带引用计数的对象
 *
 * @ownership alloc — 返回对象持有 1 个引用
 *
 * @param obj_size    对象总大小（含 refcounted_t 和数据）
 * @param deleter     归零时的销毁回调（可为 NULL 使用默认 free）
 * @return 分配的对象指针，失败返回 NULL
 */
static inline void *refcount_alloc(size_t obj_size, void (*deleter)(void *obj))
{
    if (obj_size < sizeof(refcounted_t)) {
        return NULL;  /* SEC-03: 大小不足 */
    }

    refcounted_t *rc = (refcounted_t *)AGENTRT_CALLOC(1, obj_size);
    if (!rc) return NULL;

    atomic_init(&rc->refcount, 1);
    rc->deleter = deleter;

    return (void *)rc;
}

/* ==================== 引用计数操作 API ==================== */

/**
 * @brief P1.21.2: 增加引用计数（retain）
 *
 * @ownership retain — 调用者获得 1 个新引用
 *
 * @param obj  对象指针（必须是 refcounted_t 首部开始的指针）
 * @return obj 本身（方便链式调用），NULL 如果 obj 为 NULL
 *
 * 使用示例：
 *   my_buf_t *buf2 = (my_buf_t *)refcount_retain((refcounted_t *)buf1);
 */
static inline void *refcount_retain(void *obj)
{
    if (!obj) return NULL;

    refcounted_t *rc = (refcounted_t *)obj;
    uint32_t old = atomic_fetch_add_explicit(&rc->refcount, 1, memory_order_relaxed);

    /* 防止 use-after-free: 如果引用计数从 0 增加，说明对象已被销毁 */
    if (old == 0) {
        atomic_store_explicit(&rc->refcount, 0, memory_order_release);
        return NULL;
    }

    return obj;
}

/**
 * @brief P1.21.2: 释放引用计数（release）
 *
 * @ownership release — 调用者释放 1 个引用
 *
 * 当引用计数归零时，自动调用 deleter 销毁对象。
 * 调用后 obj 指针失效，调用者不应再访问。
 *
 * @param obj  对象指针（必须是 refcounted_t 首部开始的指针）
 * @return true 表示对象已被销毁（引用计数归零），false 表示仍有其他引用
 */
static inline bool refcount_release(void *obj)
{
    if (!obj) return false;

    refcounted_t *rc = (refcounted_t *)obj;
    uint32_t old = atomic_fetch_sub_explicit(&rc->refcount, 1, memory_order_acq_rel);

    if (old == 1) {
        /* 最后一个引用，销毁对象 */
        if (rc->deleter) {
            rc->deleter(obj);
        } else {
            AGENTRT_FREE(obj);
        }
        return true;
    }

    if (old == 0) {
        /* 双重释放检测 */
        atomic_store_explicit(&rc->refcount, 0, memory_order_release);
    }

    return false;
}

/* ==================== 辅助宏 ==================== */

/**
 * @brief 嵌入 refcounted_t 到结构体首部的便捷宏
 *
 * 使用方式：
 *   typedef struct {
 *       AGENTRT_REFCOUNTED_HEADER;
 *       char buffer[4096];
 *   } ipc_shared_buf_t;
 *
 *   ipc_shared_buf_t *buf = (ipc_shared_buf_t *)
 *       refcount_alloc(sizeof(ipc_shared_buf_t), NULL);
 */
#define AGENTRT_REFCOUNTED_HEADER refcounted_t _rc

/**
 * @brief 获取包含 refcounted_t 的结构体指针
 * @param ptr   refcounted_t 首部地址
 * @param type  包含结构体类型
 * @param member refcounted_t 字段名
 */
#define REFCOUNTED_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief 便捷 retain 宏（自动类型转换）
 * @param obj  对象指针
 * @return 同类型指针
 */
#define REFCOUNT_RETAIN(obj) \
    ((typeof(obj))refcount_retain((void *)(obj)))

/**
 * @brief 便捷 release 宏
 * @param obj  对象指针
 * @return true 已销毁
 */
#define REFCOUNT_RELEASE(obj) \
    refcount_release((void *)(obj))

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_REFCOUNTED_H */