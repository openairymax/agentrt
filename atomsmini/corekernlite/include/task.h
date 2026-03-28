/**
 * @file task.h
 * @brief 任务调度接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LITE_TASK_H
#define AGENTOS_LITE_TASK_H

#include "export.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 任务优先级
 */
typedef enum {
    AGENTOS_LITE_TASK_PRIORITY_LOW = 0,
    AGENTOS_LITE_TASK_PRIORITY_NORMAL = 1,
    AGENTOS_LITE_TASK_PRIORITY_HIGH = 2,
    AGENTOS_LITE_TASK_PRIORITY_REALTIME = 3,
} agentos_lite_task_priority_t;

/**
 * @brief 互斥锁句柄
 */
typedef struct agentos_lite_mutex agentos_lite_mutex_t;

/**
 * @brief 条件变量句柄
 */
typedef struct agentos_lite_cond agentos_lite_cond_t;

/**
 * @brief 线程句柄
 */
typedef struct agentos_lite_thread agentos_lite_thread_t;

/**
 * @brief 线程属性
 */
typedef struct {
    const char* name;                     /**< 线程名称 */
    agentos_lite_task_priority_t priority; /**< 线程优先级 */
    size_t stack_size;                    /**< 栈大小（0表示默认） */
} agentos_lite_thread_attr_t;

/**
 * @brief 创建互斥锁
 * @return 互斥锁句柄，失败返回NULL
 */
AGENTOS_LITE_API agentos_lite_mutex_t* agentos_lite_mutex_create(void);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁句柄
 */
AGENTOS_LITE_API void agentos_lite_mutex_destroy(agentos_lite_mutex_t* mutex);

/**
 * @brief 加锁
 * @param mutex 互斥锁句柄
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_lock(agentos_lite_mutex_t* mutex);

/**
 * @brief 尝试加锁
 * @param mutex 互斥锁句柄
 * @return 错误码（AGENTOS_LITE_EBUSY表示锁已被占用）
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_trylock(agentos_lite_mutex_t* mutex);

/**
 * @brief 解锁
 * @param mutex 互斥锁句柄
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mutex_unlock(agentos_lite_mutex_t* mutex);

/**
 * @brief 创建条件变量
 * @return 条件变量句柄，失败返回NULL
 */
AGENTOS_LITE_API agentos_lite_cond_t* agentos_lite_cond_create(void);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量句柄
 */
AGENTOS_LITE_API void agentos_lite_cond_destroy(agentos_lite_cond_t* cond);

/**
 * @brief 等待条件变量
 * @param cond 条件变量句柄
 * @param mutex 互斥锁句柄
 * @param timeout_ms 超时时间（毫秒，0表示无限等待）
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_wait(
    agentos_lite_cond_t* cond, agentos_lite_mutex_t* mutex, uint32_t timeout_ms);

/**
 * @brief 唤醒一个等待线程
 * @param cond 条件变量句柄
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_signal(agentos_lite_cond_t* cond);

/**
 * @brief 唤醒所有等待线程
 * @param cond 条件变量句柄
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_broadcast(agentos_lite_cond_t* cond);

/**
 * @brief 创建线程
 * @param[out] thread 线程句柄
 * @param attr 线程属性（可为NULL）
 * @param entry 线程入口函数
 * @param arg 线程参数
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_create(
    agentos_lite_thread_t* thread, const agentos_lite_thread_attr_t* attr,
    void (*entry)(void*), void* arg);

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_join(agentos_lite_thread_t thread);

/**
 * @brief 线程休眠
 * @param ms 休眠时间（毫秒）
 */
AGENTOS_LITE_API void agentos_lite_task_sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_TASK_H */