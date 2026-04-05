/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file sync.h
 * @brief 统一线程同步原语模块 - 核心层API
 * 
 * 提供跨平台、安全、高效的线程同步原语，包括互斥锁、条件变量、信号量、
 * 读写锁、自旋锁、屏障等。支持Windows和POSIX系统。
 * 
 * @author AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性原则
 */

#ifndef AGENTOS_SYNC_H
#define AGENTOS_SYNC_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sync_api 线程同步API
 * @{
 */

/**
 * @brief 同步原语类型
 */
typedef enum {
    SYNC_TYPE_UNKNOWN = 0,               /**< 未知类型 */
    SYNC_TYPE_MUTEX,                     /**< 互斥锁 */
    SYNC_TYPE_RECURSIVE_MUTEX,           /**< 递归互斥锁 */
    SYNC_TYPE_RWLOCK,                    /**< 读写锁 */
    SYNC_TYPE_SPINLOCK,                  /**< 自旋锁 */
    SYNC_TYPE_SEMAPHORE,                 /**< 信号量 */
    SYNC_TYPE_CONDITION,                 /**< 条件变量 */
    SYNC_TYPE_BARRIER,                   /**< 屏障 */
    SYNC_TYPE_EVENT                      /**< 事件 */
} sync_type_t;

/**
 * @brief 锁对象类型（用于 sync_get_type 函数的类型安全调用）
 */
typedef enum {
    SYNC_LOCK_MUTEX,                    /**< 互斥锁 */
    SYNC_LOCK_RECURSIVE_MUTEX,          /**< 递归互斥锁 */
    SYNC_LOCK_RWLOCK,                    /**< 读写锁 */
    SYNC_LOCK_SPINLOCK,                  /**< 自旋锁 */
    SYNC_LOCK_SEMAPHORE,                 /**< 信号量 */
    SYNC_LOCK_CONDITION,                 /**< 条件变量 */
    SYNC_LOCK_BARRIER,                   /**< 屏障 */
    SYNC_LOCK_EVENT                      /**< 事件 */
} sync_lock_type_t;

/**
 * @brief 锁操作结果
 */
typedef enum {
    SYNC_SUCCESS = 0,                    /**< 操作成功 */
    SYNC_ERROR_TIMEOUT,                  /**< 操作超时 */
    SYNC_ERROR_DEADLOCK,                 /**< 检测到死锁 */
    SYNC_ERROR_INVALID,                  /**< 无效参数或状态 */
    SYNC_ERROR_MEMORY,                   /**< 内存分配失败 */
    SYNC_ERROR_PERMISSION,               /**< 权限不足 */
    SYNC_ERROR_BUSY,                     /**< 资源繁忙 */
    SYNC_ERROR_UNKNOWN                   /**< 未知错误 */
} sync_result_t;

/**
 * @brief 锁选项标志
 */
typedef enum {
    SYNC_FLAG_NONE = 0,                  /**< 无特殊标志 */
    SYNC_FLAG_SHARED = 1 << 0,           /**< 共享锁（读写锁） */
    SYNC_FLAG_EXCLUSIVE = 1 << 1,        /**< 排他锁（读写锁） */
    SYNC_FLAG_TRY = 1 << 2,              /**< 尝试获取，不阻塞 */
    SYNC_FLAG_TIMEOUT = 1 << 3,          /**< 支持超时 */
    SYNC_FLAG_RECURSIVE = 1 << 4,        /**< 递归锁 */
    SYNC_FLAG_ERROR_CHECK = 1 << 5,      /**< 错误检查锁 */
    SYNC_FLAG_PRIORITY_INHERIT = 1 << 6, /**< 优先级继承 */
    SYNC_FLAG_ROBUST = 1 << 7            /**< 健壮锁（进程间） */
} sync_flag_t;

/**
 * @brief 超时选项
 */
typedef struct {
    uint64_t timeout_ms;                 /**< 超时时间（毫秒） */
    bool absolute;                       /**< 是否为绝对时间 */
} sync_timeout_t;

/**
 * @brief 互斥锁句柄（不透明类型）
 */
typedef struct sync_mutex* sync_mutex_t;

/**
 * @brief 递归互斥锁句柄
 */
typedef struct sync_recursive_mutex* sync_recursive_mutex_t;

/**
 * @brief 读写锁句柄
 */
typedef struct sync_rwlock* sync_rwlock_t;

/**
 * @brief 自旋锁句柄
 */
typedef struct sync_spinlock* sync_spinlock_t;

/**
 * @brief 信号量句柄
 */
typedef struct sync_semaphore* sync_semaphore_t;

/**
 * @brief 条件变量句柄
 */
typedef struct sync_condition* sync_condition_t;

/**
 * @brief 屏障句柄
 */
typedef struct sync_barrier* sync_barrier_t;

/**
 * @brief 事件句柄
 */
typedef struct sync_event* sync_event_t;

/**
 * @brief 锁属性
 */
typedef struct {
    sync_type_t type;                    /**< 锁类型 */
    uint32_t flags;                      /**< 标志位 */
    const char* name;                    /**< 锁名称（用于调试） */
    void* context;                       /**< 用户上下文 */
} sync_attr_t;

/**
 * @brief 锁统计信息
 */
typedef struct {
    size_t lock_count;                   /**< 加锁次数 */
    size_t unlock_count;                 /**< 解锁次数 */
    size_t wait_count;                   /**< 等待次数 */
    size_t timeout_count;                /**< 超时次数 */
    size_t deadlock_count;               /**< 死锁检测次数 */
    uint64_t total_wait_time_ms;         /**< 总等待时间（毫秒） */
    uint64_t max_wait_time_ms;           /**< 最大等待时间（毫秒） */
} sync_stats_t;

/**
 * @brief 死锁检测信息
 */
typedef struct {
    size_t thread_count;                 /**< 涉及线程数 */
    size_t lock_count;                   /**< 涉及锁数量 */
    uint64_t detection_time;             /**< 检测时间戳 */
    char** thread_names;                 /**< 线程名称数组 */
    char** lock_names;                   /**< 锁名称数组 */
} sync_deadlock_info_t;

/**
 * @brief 错误回调函数类型
 * 
 * @param[in] result 错误结果
 * @param[in] lock_name 锁名称
 * @param[in] context 用户上下文
 */
typedef void (*sync_error_callback_t)(sync_result_t result, 
                                     const char* lock_name, 
                                     void* context);

/**
 * @brief 初始化同步模块
 * 
 * @param[in] error_callback 错误回调函数（可选）
 * @param[in] context 用户上下文（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_init(sync_error_callback_t error_callback, void* context);

/**
 * @brief 清理同步模块
 */
void sync_cleanup(void);

/**
 * @brief 创建互斥锁
 * 
 * @param[out] mutex 互斥锁句柄
 * @param[in] attr 锁属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_mutex_create(sync_mutex_t* mutex, const sync_attr_t* attr);

/**
 * @brief 销毁互斥锁
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_mutex_destroy(sync_mutex_t mutex);

/**
 * @brief 加锁互斥锁
 * 
 * @param[in] mutex 互斥锁句柄
 * @param[in] timeout 超时设置（可选，NULL表示无限等待）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_mutex_lock(sync_mutex_t mutex, const sync_timeout_t* timeout);

/**
 * @brief 尝试加锁互斥锁
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_mutex_try_lock(sync_mutex_t mutex);

/**
 * @brief 解锁互斥锁
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_mutex_unlock(sync_mutex_t mutex);

/**
 * @brief 创建递归互斥锁
 * 
 * @param[out] mutex 递归互斥锁句柄
 * @param[in] attr 锁属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_recursive_mutex_create(sync_recursive_mutex_t* mutex, 
                                         const sync_attr_t* attr);

/**
 * @brief 销毁递归互斥锁
 * 
 * @param[in] mutex 递归互斥锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_recursive_mutex_destroy(sync_recursive_mutex_t mutex);

/**
 * @brief 加锁递归互斥锁
 * 
 * @param[in] mutex 递归互斥锁句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_recursive_mutex_lock(sync_recursive_mutex_t mutex, 
                                       const sync_timeout_t* timeout);

/**
 * @brief 解锁递归互斥锁
 * 
 * @param[in] mutex 递归互斥锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_recursive_mutex_unlock(sync_recursive_mutex_t mutex);

/**
 * @brief 获取递归互斥锁的递归计数
 * 
 * @param[in] mutex 递归互斥锁句柄
 * @param[out] count 递归计数
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_recursive_mutex_get_count(sync_recursive_mutex_t mutex, 
                                            size_t* count);

/**
 * @brief 创建读写锁
 * 
 * @param[out] rwlock 读写锁句柄
 * @param[in] attr 锁属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_create(sync_rwlock_t* rwlock, const sync_attr_t* attr);

/**
 * @brief 销毁读写锁
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_destroy(sync_rwlock_t rwlock);

/**
 * @brief 获取读锁（共享锁）
 * 
 * @param[in] rwlock 读写锁句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_read_lock(sync_rwlock_t rwlock, 
                                   const sync_timeout_t* timeout);

/**
 * @brief 尝试获取读锁
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock);

/**
 * @brief 获取写锁（排他锁）
 * 
 * @param[in] rwlock 读写锁句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_write_lock(sync_rwlock_t rwlock, 
                                    const sync_timeout_t* timeout);

/**
 * @brief 尝试获取写锁
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock);

/**
 * @brief 解锁读写锁
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_rwlock_unlock(sync_rwlock_t rwlock);

/**
 * @brief 创建自旋锁
 * 
 * @param[out] spinlock 自旋锁句柄
 * @param[in] attr 锁属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_spinlock_create(sync_spinlock_t* spinlock, 
                                  const sync_attr_t* attr);

/**
 * @brief 销毁自旋锁
 * 
 * @param[in] spinlock 自旋锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_spinlock_destroy(sync_spinlock_t spinlock);

/**
 * @brief 加锁自旋锁
 * 
 * @param[in] spinlock 自旋锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_spinlock_lock(sync_spinlock_t spinlock);

/**
 * @brief 尝试加锁自旋锁
 * 
 * @param[in] spinlock 自旋锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_spinlock_try_lock(sync_spinlock_t spinlock);

/**
 * @brief 解锁自旋锁
 * 
 * @param[in] spinlock 自旋锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_spinlock_unlock(sync_spinlock_t spinlock);

/**
 * @brief 创建信号量
 * 
 * @param[out] semaphore 信号量句柄
 * @param[in] initial_value 初始值
 * @param[in] max_value 最大值（0表示无限制）
 * @param[in] attr 信号量属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_create(sync_semaphore_t* semaphore,
                                   unsigned int initial_value,
                                   unsigned int max_value,
                                   const sync_attr_t* attr);

/**
 * @brief 销毁信号量
 * 
 * @param[in] semaphore 信号量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_destroy(sync_semaphore_t semaphore);

/**
 * @brief 等待信号量
 * 
 * @param[in] semaphore 信号量句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_wait(sync_semaphore_t semaphore, 
                                 const sync_timeout_t* timeout);

/**
 * @brief 尝试等待信号量
 * 
 * @param[in] semaphore 信号量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_try_wait(sync_semaphore_t semaphore);

/**
 * @brief 发布信号量
 * 
 * @param[in] semaphore 信号量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_post(sync_semaphore_t semaphore);

/**
 * @brief 获取信号量当前值
 * 
 * @param[in] semaphore 信号量句柄
 * @param[out] value 当前值
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_semaphore_get_value(sync_semaphore_t semaphore, 
                                      unsigned int* value);

/**
 * @brief 创建条件变量
 * 
 * @param[out] condition 条件变量句柄
 * @param[in] attr 条件变量属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_condition_create(sync_condition_t* condition, 
                                   const sync_attr_t* attr);

/**
 * @brief 销毁条件变量
 * 
 * @param[in] condition 条件变量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_condition_destroy(sync_condition_t condition);

/**
 * @brief 等待条件变量
 * 
 * @param[in] condition 条件变量句柄
 * @param[in] mutex 关联的互斥锁
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_condition_wait(sync_condition_t condition, 
                                 sync_mutex_t mutex,
                                 const sync_timeout_t* timeout);

/**
 * @brief 唤醒一个等待条件变量的线程
 * 
 * @param[in] condition 条件变量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_condition_signal(sync_condition_t condition);

/**
 * @brief 唤醒所有等待条件变量的线程
 * 
 * @param[in] condition 条件变量句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_condition_broadcast(sync_condition_t condition);

/**
 * @brief 创建屏障
 * 
 * @param[out] barrier 屏障句柄
 * @param[in] count 需要等待的线程数
 * @param[in] attr 屏障属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_barrier_create(sync_barrier_t* barrier, 
                                 unsigned int count,
                                 const sync_attr_t* attr);

/**
 * @brief 销毁屏障
 * 
 * @param[in] barrier 屏障句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_barrier_destroy(sync_barrier_t barrier);

/**
 * @brief 等待屏障
 * 
 * @param[in] barrier 屏障句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_barrier_wait(sync_barrier_t barrier, 
                               const sync_timeout_t* timeout);

/**
 * @brief 重置屏障
 * 
 * @param[in] barrier 屏障句柄
 * @param[in] new_count 新的线程数（0表示保持不变）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_barrier_reset(sync_barrier_t barrier, unsigned int new_count);

/**
 * @brief 创建事件
 * 
 * @param[out] event 事件句柄
 * @param[in] manual_reset 是否手动重置
 * @param[in] initial_state 初始状态
 * @param[in] attr 事件属性（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_event_create(sync_event_t* event,
                               bool manual_reset,
                               bool initial_state,
                               const sync_attr_t* attr);

/**
 * @brief 销毁事件
 * 
 * @param[in] event 事件句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_event_destroy(sync_event_t event);

/**
 * @brief 设置事件为有信号状态
 * 
 * @param[in] event 事件句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_event_set(sync_event_t event);

/**
 * @brief 重置事件为无信号状态
 * 
 * @param[in] event 事件句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_event_reset(sync_event_t event);

/**
 * @brief 等待事件
 * 
 * @param[in] event 事件句柄
 * @param[in] timeout 超时设置（可选）
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_event_wait(sync_event_t event, 
                             const sync_timeout_t* timeout);

/**
 * @brief 获取锁统计信息
 * 
 * @param[in] lock 锁句柄（任何类型）
 * @param[out] stats 统计信息
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_get_stats(void* lock, sync_stats_t* stats);

/**
 * @brief 重置锁统计信息
 * 
 * @param[in] lock 锁句柄
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_reset_stats(void* lock);

/**
 * @brief 检查死锁
 * 
 * @param[out] info 死锁信息（如果检测到）
 * @param[in] max_info_size 最大信息大小
 * @return 检测到死锁返回SYNC_ERROR_DEADLOCK，否则返回SYNC_SUCCESS
 */
sync_result_t sync_check_deadlock(sync_deadlock_info_t* info, 
                                 size_t max_info_size);

/**
 * @brief 设置锁名称
 * 
 * @param[in] lock 锁句柄
 * @param[in] name 锁名称
 * @return 成功返回SYNC_SUCCESS，失败返回错误码
 */
sync_result_t sync_set_name(void* lock, const char* name);

/**
 * @brief 获取锁名称
 * 
 * @param[in] lock 锁句柄
 * @return 锁名称（如果未设置返回NULL）
 */
const char* sync_get_name(void* lock);

/**
 * @brief 获取当前线程ID
 * 
 * @return 线程ID
 */
uint64_t sync_get_thread_id(void);

/**
 * @brief 获取锁类型
 *
 * @param[in] lock 锁句柄
 * @param[in] lock_type 锁的实际类型（用于安全转换）
 * @return 锁类型标识
 * @note 调用者必须确保 lock 和 lock_type 匹配，否则行为未定义
 */
sync_type_t sync_get_type(void* lock, sync_lock_type_t lock_type);

/**
 * @brief 线程休眠
 * 
 * @param[in] ms 休眠时间（毫秒）
 */
void sync_sleep(unsigned int ms);

/**
 * @brief 获取当前时间戳（毫秒）
 * 
 * @return 时间戳
 */
uint64_t sync_get_timestamp_ms(void);

/**
 * @brief 原子操作：比较并交换
 * 
 * @param[inout] ptr 指针
 * @param[in] expected 期望值
 * @param[in] desired 期望值
 * @return 成功返回true，失败返回false
 */
bool sync_atomic_cas(volatile void* ptr, uintptr_t expected, uintptr_t desired);

/**
 * @brief 原子操作：增加
 * 
 * @param[inout] ptr 指针
 * @param[in] value 增加值
 * @return 增加前的值
 */
uintptr_t sync_atomic_add(volatile void* ptr, uintptr_t value);

/**
 * @brief 原子操作：减少
 * 
 * @param[inout] ptr 指针
 * @param[in] value 减少值
 * @return 减少前的值
 */
uintptr_t sync_atomic_sub(volatile void* ptr, uintptr_t value);

/**
 * @brief 原子操作：获取
 * 
 * @param[in] ptr 指针
 * @return 当前值
 */
uintptr_t sync_atomic_load(volatile void* ptr);

/**
 * @brief 原子操作：存储
 * 
 * @param[inout] ptr 指针
 * @param[in] value 要存储的值
 */
void sync_atomic_store(volatile void* ptr, uintptr_t value);

/** @} */ // end of sync_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYNC_H */