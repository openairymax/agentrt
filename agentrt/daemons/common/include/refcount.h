/**
 * @file refcount.h
 * @brief P1.21: 所有权模型 + 原子引用计数规范
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 为所有公开 API 提供标准化的引用计数管理。
 * 核心结构体 refcounted_t 嵌入到需要引用计数的对象中，
 * 通过 refcount_retain()/refcount_release() 管理生命周期。
 *
 * 使用方式：
 * @code
 *   typedef struct {
 *       refcounted_t rc;           // 必须为第一个字段
 *       char *data;
 *       size_t data_len;
 *   } my_buffer_t;
 *
 *   my_buffer_t *buf = refcount_alloc(sizeof(my_buffer_t), my_deleter);
 *   refcount_retain(&buf->rc);     // 增加引用
 *   refcount_release(&buf->rc);    // 减少引用，到 0 时调用 deleter
 * @endcode
 *
 * @ownership 注解规范：
 *   @ownership OWNER   — 调用者获得所有权，需调用 release
 *   @ownership BORROW  — 调用者不持有所有权，不得 release
 *   @ownership TRANSFER — 所有权转移给被调用者
 */

#ifndef AGENTRT_REFCOUNT_H
#define AGENTRT_REFCOUNT_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 引用计数结构 ==================== */

/**
 * @brief 引用计数基础结构
 *
 * 嵌入到需要引用计数的结构体中（必须是第一个字段）。
 * 使用 _Atomic 保证线程安全。
 */
typedef struct refcounted_s {
    _Atomic uint32_t refcount;          /**< 引用计数 */
    void (*deleter)(void *object);      /**< 析构函数 */
    const char *type_name;              /**< 类型名（用于调试） */
} refcounted_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 分配带引用计数的对象
 *
 * 分配的内存已清零，refcount 初始化为 1。
 *
 * @param size 对象大小（包含 refcounted_t）
 * @param deleter 析构函数（计数归零时调用）
 * @return 对象指针，失败返回 NULL
 *
 * @ownership OWNER
 */
void *refcount_alloc(size_t size, void (*deleter)(void *object));

/**
 * @brief 增加引用计数
 *
 * @param rc 引用计数结构指针
 * @return 新的引用计数值
 *
 * @ownership RETAIN
 */
uint32_t refcount_retain(refcounted_t *rc);

/**
 * @brief 减少引用计数
 *
 * 当引用计数归零时，调用 deleter 释放对象。
 * 调用后不得再使用该对象。
 *
 * @param rc 引用计数结构指针
 * @return 新的引用计数值（0 表示已释放）
 *
 * @ownership RELEASE
 */
uint32_t refcount_release(refcounted_t *rc);

/* ==================== 查询 ==================== */

/**
 * @brief 获取当前引用计数
 *
 * @param rc 引用计数结构指针
 * @return 当前引用计数值
 */
uint32_t refcount_get(const refcounted_t *rc);

/**
 * @brief 检查是否为唯一引用
 *
 * @param rc 引用计数结构指针
 * @return true 如果引用计数为 1
 */
bool refcount_is_unique(const refcounted_t *rc);

/* ==================== 零拷贝引用 ==================== */

/**
 * @brief 创建零拷贝引用
 *
 * 不增加引用计数，仅用于栈上临时引用。
 * 调用者必须确保在 refcount 归零前不使用该引用。
 *
 * 用法：
 * @code
 *   refcounted_t *tmp = refcount_borrow(&buf->rc);
 *   // 使用 tmp...
 *   // 不要调用 refcount_release(tmp)
 * @endcode
 *
 * @param rc 引用计数结构指针
 * @return 借用指针
 *
 * @ownership BORROW
 */
static inline refcounted_t *refcount_borrow(refcounted_t *rc)
{
    return rc;
}

/* ==================== 全局引用计数统计 ==================== */

/**
 * @brief 获取全局引用计数统计
 *
 * @param out_total_allocs 输出总分配数
 * @param out_total_frees 输出总释放数
 * @param out_current_live 输出当前存活对象数
 */
void refcount_get_global_stats(uint64_t *out_total_allocs,
                               uint64_t *out_total_frees,
                               uint64_t *out_current_live);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_REFCOUNT_H */