/**
 * @file ipc_shared_buf.h
 * @brief P1.21.3: IPC 共享缓冲区 — 基于 refcounted_t 的引用计数管理
 *
 * 提供线程安全的 IPC 共享缓冲区，使用 refcounted_t 管理生命周期。
 * 支持零拷贝共享（多个消费者持有同一缓冲区引用）。
 *
 * 设计：
 *   - ipc_shared_buf_t 首部嵌入 refcounted_t，由 refcount_alloc/retain/release 管理
 *   - ipc_buf_create()  分配带引用计数的缓冲区（@ownership alloc，引用计数=1）
 *   - ipc_buf_dup()     增加引用计数（@ownership retain）
 *   - ipc_buf_release() 减少引用计数，归零时自动释放
 *
 * 线程安全：
 *   - 引用计数操作基于 _Atomic uint32_t，无锁原子操作
 *   - 生产者-消费者场景：生产者 create → 消费者 dup → 各自 release
 *
 * 使用示例：
 *   // 生产者
 *   ipc_shared_buf_t *buf = ipc_buf_create(4096);
 *   memcpy(buf->data, payload, payload_len);
 *   send_to_consumer(buf);  // 消费者会 dup
 *   ipc_buf_release(buf);   // 生产者释放自己的引用
 *
 *   // 消费者
 *   void on_message(ipc_shared_buf_t *buf) {
 *       process(buf->data, buf->size);
 *       ipc_buf_release(buf);  // 消费者释放引用
 *   }
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_IPC_SHARED_BUF_H
#define AGENTRT_IPC_SHARED_BUF_H

#include "refcounted.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IPC 共享缓冲区结构
 * ============================================================================ */

/**
 * @brief P1.21.3: IPC 共享缓冲区结构（嵌入 refcounted_t 首部）
 *
 * 使用柔性数组成员 data[] 存储实际数据，
 * 整个结构体通过 refcount_alloc 一次性分配。
 *
 * @ownership alloc  — ipc_buf_create() 返回引用计数=1 的缓冲区
 * @ownership retain — ipc_buf_dup() 增加引用计数
 * @ownership release — ipc_buf_release() 减少引用计数，归零时释放
 */
typedef struct {
    AGENTRT_REFCOUNTED_HEADER;   /**< 引用计数首部（refcounted_t _rc） */
    size_t   size;               /**< 数据大小（字节） */
    uint64_t timestamp;          /**< 创建时间戳（纳秒） */
    char     data[];             /**< 柔性数组：实际数据 */
} ipc_shared_buf_t;

/* ============================================================================
 * 生命周期 API
 * ============================================================================ */

/**
 * @brief P1.21.3: 创建带引用计数的 IPC 共享缓冲区
 *
 * @ownership alloc — 返回的缓冲区持有 1 个引用
 *
 * @param size 数据区大小（字节），不包括结构体头部
 * @return 分配的缓冲区指针，失败返回 NULL
 */
static inline ipc_shared_buf_t *ipc_buf_create(size_t size)
{
    /* 防止溢出：size + sizeof(ipc_shared_buf_t) */
    if (size > SIZE_MAX - sizeof(ipc_shared_buf_t)) {
        return NULL;
    }

    size_t total_size = sizeof(ipc_shared_buf_t) + size;
    ipc_shared_buf_t *buf = (ipc_shared_buf_t *)refcount_alloc(total_size, NULL);
    if (!buf) return NULL;

    buf->size = size;
    buf->timestamp = 0;  /* 调用者可设置 */

    return buf;
}

/**
 * @brief P1.21.3: 复制 IPC 共享缓冲区（增加引用计数）
 *
 * @ownership retain — 调用者获得 1 个新引用
 *
 * @param buf 源缓冲区（可为 NULL）
 * @return buf 本身（便于链式调用），NULL 如果 buf 为 NULL
 *
 * @note 不会复制数据，仅增加引用计数（零拷贝共享）
 */
static inline ipc_shared_buf_t *ipc_buf_dup(ipc_shared_buf_t *buf)
{
    return (ipc_shared_buf_t *)refcount_retain((void *)buf);
}

/**
 * @brief P1.21.3: 释放 IPC 共享缓冲区引用
 *
 * @ownership release — 调用者释放 1 个引用
 *
 * 当引用计数归零时，自动释放内存。
 * 调用后 buf 指针失效。
 *
 * @param buf 缓冲区指针（可为 NULL）
 */
static inline void ipc_buf_release(ipc_shared_buf_t *buf)
{
    refcount_release((void *)buf);
}

/* ============================================================================
 * 查询 API
 * ============================================================================ */

/**
 * @brief 获取缓冲区的当前引用计数（仅供调试）
 * @param buf 缓冲区指针
 * @return 当前引用计数，buf 为 NULL 返回 0
 */
static inline uint32_t ipc_buf_refcount(const ipc_shared_buf_t *buf)
{
    if (!buf) return 0;
    return refcount_get(&buf->_rc);
}

/**
 * @brief 获取缓冲区数据区大小
 * @param buf 缓冲区指针
 * @return 数据区大小（字节），buf 为 NULL 返回 0
 */
static inline size_t ipc_buf_size(const ipc_shared_buf_t *buf)
{
    if (!buf) return 0;
    return buf->size;
}

/**
 * @brief 计算缓冲区总内存占用
 * @param buf 缓冲区指针
 * @return 总字节数（含结构体和数据区），buf 为 NULL 返回 0
 */
static inline size_t ipc_buf_total_size(const ipc_shared_buf_t *buf)
{
    if (!buf) return 0;
    return sizeof(ipc_shared_buf_t) + buf->size;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_IPC_SHARED_BUF_H */