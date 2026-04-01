/**
 * @file task.h
 * @brief 任务调度接口（基于系统原生线程）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 提供基于系统原生线程的任务调度功能，包括互斥锁、条件变量、线程管理等
 */

#ifndef AGENTOS_TASK_H
#define AGENTOS_TASK_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 任务 ID 类型
 */
typedef uint64_t agentos_task_id_t;

/**
 * @brief 互斥锁类型（不透明指针）
 */
typedef struct agentos_mutex agentos_mutex_t;

/**
 * @brief 条件变量类型（不透明指针）
 */
typedef struct agentos_cond agentos_cond_t;

/**
 * @brief 线程句柄
 *
 * Windows: HANDLE
 * POSIX: pthread_t
 */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef HANDLE agentos_thread_t;
#else
    typedef struct agentos_thread agentos_thread_t;
#endif

/**
 * @brief 任务优先级常量
 */
#define AGENTOS_TASK_PRIORITY_MIN     0   /**< 最低优先级 */
#define AGENTOS_TASK_PRIORITY_LOW     25  /**< 低优先级 */
#define AGENTOS_TASK_PRIORITY_NORMAL  50  /**< 普通优先级 */
#define AGENTOS_TASK_PRIORITY_HIGH    75  /**< 高优先级 */
#define AGENTOS_TASK_PRIORITY_MAX     100 /**< 最高优先级 */

/**
 * @brief 任务状态枚举
 */
typedef enum {
    AGENTOS_TASK_STATE_CREATED,   /**< 已创建 */
    AGENTOS_TASK_STATE_READY,     /**< 就绪 */
    AGENTOS_TASK_STATE_RUNNING,    /**< 运行中 */
    AGENTOS_TASK_STATE_BLOCKED,    /**< 阻塞 */
    AGENTOS_TASK_STATE_TERMINATED  /**< 已终止 */
} agentos_task_state_t;

/**
 * @brief 线程属性结构
 */
typedef struct {
    const char* name;      /**< 线程名称 */
    int priority;          /**< 线程优先级 */
    size_t stack_size;    /**< 栈大小 */
} agentos_thread_attr_t;

/**
 * @brief 初始化任务调度系统
 *
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有任务调度资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_task_cleanup()
 */
AGENTOS_API agentos_error_t agentos_task_init(void);

/**
 * @brief 创建互斥锁
 *
 * @return agentos_mutex_t* 互斥锁句柄
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentos_mutex_destroy() 释放
 * @threadsafe 否，创建后使用是线程安全的
 * @reentrant 否
 *
 * @see agentos_mutex_destroy()
 * @see agentos_mutex_lock()
 * @see agentos_mutex_trylock()
 * @see agentos_mutex_unlock()
 */
AGENTOS_API agentos_mutex_t* agentos_mutex_create(void);

/**
 * @brief 销毁互斥锁
 *
 * @param mutex [in] 互斥锁句柄
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_mutex_create()
 */
AGENTOS_API void agentos_mutex_destroy(agentos_mutex_t* mutex);

/**
 * @brief 获取互斥锁
 *
 * @param mutex [in] 互斥锁句柄
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 如果锁已被其他线程持有，当前线程会阻塞
 *
 * @see agentos_mutex_unlock()
 */
AGENTOS_API void agentos_mutex_lock(agentos_mutex_t* mutex);

/**
 * @brief 尝试获取互斥锁（非阻塞）
 *
 * @param mutex [in] 互斥锁句柄
 * @return int 成功返回 0，失败返回非零值
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_mutex_lock()
 * @see agentos_mutex_unlock()
 */
AGENTOS_API int agentos_mutex_trylock(agentos_mutex_t* mutex);

/**
 * @brief 释放互斥锁
 *
 * @param mutex [in] 互斥锁句柄
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_mutex_lock()
 */
AGENTOS_API void agentos_mutex_unlock(agentos_mutex_t* mutex);

/**
 * @brief 创建条件变量
 *
 * @return agentos_cond_t* 条件变量句柄
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentos_cond_destroy() 释放
 * @threadsafe 否，创建后使用是线程安全的
 * @reentrant 否
 *
 * @see agentos_cond_destroy()
 * @see agentos_cond_wait()
 * @see agentos_cond_signal()
 * @see agentos_cond_broadcast()
 */
AGENTOS_API agentos_cond_t* agentos_cond_create(void);

/**
 * @brief 销毁条件变量
 *
 * @param cond [in] 条件变量句柄
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_cond_create()
 */
AGENTOS_API void agentos_cond_destroy(agentos_cond_t* cond);

/**
 * @brief 等待条件变量
 *
 * @param cond [in] 条件变量句柄
 * @param mutex [in] 互斥锁句柄（必须由调用者持有）
 * @param timeout_ms [in] 超时时间（毫秒），0 表示无限等待
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 等待时会释放 mutex，醒来时重新获取 mutex
 *
 * @see agentos_cond_signal()
 * @see agentos_cond_broadcast()
 */
AGENTOS_API agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms);

/**
 * @brief 发送信号（唤醒一个等待的线程）
 *
 * @param cond [in] 条件变量句柄
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_cond_wait()
 * @see agentos_cond_broadcast()
 */
AGENTOS_API void agentos_cond_signal(agentos_cond_t* cond);

/**
 * @brief 广播（唤醒所有等待的线程）
 *
 * @param cond [in] 条件变量句柄
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_cond_wait()
 * @see agentos_cond_signal()
 */
AGENTOS_API void agentos_cond_broadcast(agentos_cond_t* cond);

/**
 * @brief 创建线程
 *
 * @param thread [out] 输出线程句柄
 * @param attr [in] 线程属性（NULL 表示默认属性）
 * @param func [in] 线程函数
 * @param arg [in] 传递给线程函数的参数
 * @return agentos_error_t 错误码
 *
 * @ownership thread 由调用者管理，需通过 agentos_thread_join() 回收
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_thread_join()
 */
AGENTOS_API agentos_error_t agentos_thread_create(
    agentos_thread_t* thread,
    const agentos_thread_attr_t* attr,
    void (*func)(void*),
    void* arg);

/**
 * @brief 等待线程结束
 *
 * @param thread [in] 线程句柄
 * @param retval [out] 输出线程返回值（可为 NULL）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_thread_create()
 */
AGENTOS_API agentos_error_t agentos_thread_join(agentos_thread_t thread, void** retval);

/**
 * @brief 获取当前任务 ID
 *
 * @return agentos_task_id_t 当前任务 ID
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_task_id_t agentos_task_self(void);

/**
 * @brief 任务休眠
 *
 * @param ms [in] 休眠时间（毫秒）
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_task_sleep(uint32_t ms);

/**
 * @brief 让出 CPU 时间片
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_task_yield(void);

/**
 * @brief 设置任务优先级
 *
 * @param tid [in] 任务 ID
 * @param priority [in] 优先级（0-100）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_set_priority(agentos_task_id_t tid, int priority);

/**
 * @brief 获取任务优先级
 *
 * @param tid [in] 任务 ID
 * @param out_priority [out] 输出优先级
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责 out_priority 的分配和释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_get_priority(agentos_task_id_t tid, int* out_priority);

/**
 * @brief 获取任务状态
 *
 * @param tid [in] 任务 ID
 * @param out_state [out] 输出任务状态
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责 out_state 的分配和释放
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_task_get_state(agentos_task_id_t tid, agentos_task_state_t* out_state);

/**
 * @brief 清理任务调度系统
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_task_init()
 */
AGENTOS_API void agentos_task_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASK_H */
