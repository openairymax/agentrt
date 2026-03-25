/**
 * @file task.h
 * @brief AgentOS Lite 任务调度接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供轻量级的任务调度功能：
 * - 基于原生线程的任务管理
 * - 互斥锁和条件变量
 * - 任务优先级设置
 */

#ifndef AGENTOS_LITE_TASK_H
#define AGENTOS_LITE_TASK_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t agentos_lite_task_id_t;

typedef struct agentos_lite_mutex agentos_lite_mutex_t;
typedef struct agentos_lite_cond agentos_lite_cond_t;
typedef struct agentos_lite_thread agentos_lite_thread_t;

#define AGENTOS_LITE_TASK_PRIORITY_MIN     0
#define AGENTOS_LITE_TASK_PRIORITY_LOW     25
#define AGENTOS_LITE_TASK_PRIORITY_NORMAL  50
#define AGENTOS_LITE_TASK_PRIORITY_HIGH    75
#define AGENTOS_LITE_TASK_PRIORITY_MAX     100

typedef enum {
    AGENTOS_LITE_TASK_STATE_CREATED,
    AGENTOS_LITE_TASK_STATE_READY,
    AGENTOS_LITE_TASK_STATE_RUNNING,
    AGENTOS_LITE_TASK_STATE_BLOCKED,
    AGENTOS_LITE_TASK_STATE_TERMINATED
} agentos_lite_task_state_t;

typedef struct {
    const char* name;
    int priority;
    size_t stack_size;
} agentos_lite_thread_attr_t;

/**
 * @brief 初始化任务调度子系统
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_task_init(void);

/**
 * @brief 清理任务调度子系统
 */
AGENTOS_LITE_API void agentos_lite_task_cleanup(void);

/**
 * @brief 创建互斥锁
 * @return 成功返回互斥锁指针，失败返回NULL
 */
AGENTOS_LITE_API agentos_lite_mutex_t* agentos_lite_mutex_create(void);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
AGENTOS_LITE_API void agentos_lite_mutex_destroy(agentos_lite_mutex_t* mutex);

/**
 * @brief 加锁
 * @param mutex 互斥锁指针
 */
AGENTOS_LITE_API void agentos_lite_mutex_lock(agentos_lite_mutex_t* mutex);

/**
 * @brief 尝试加锁
 * @param mutex 互斥锁指针
 * @return 成功返回0，失败返回非0
 */
AGENTOS_LITE_API int agentos_lite_mutex_trylock(agentos_lite_mutex_t* mutex);

/**
 * @brief 解锁
 * @param mutex 互斥锁指针
 */
AGENTOS_LITE_API void agentos_lite_mutex_unlock(agentos_lite_mutex_t* mutex);

/**
 * @brief 创建条件变量
 * @return 成功返回条件变量指针，失败返回NULL
 */
AGENTOS_LITE_API agentos_lite_cond_t* agentos_lite_cond_create(void);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
AGENTOS_LITE_API void agentos_lite_cond_destroy(agentos_lite_cond_t* cond);

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒），0表示无限等待
 * @return 成功返回AGENTOS_LITE_SUCCESS，超时返回AGENTOS_LITE_ETIMEDOUT
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_cond_wait(
    agentos_lite_cond_t* cond,
    agentos_lite_mutex_t* mutex,
    uint32_t timeout_ms);

/**
 * @brief 唤醒一个等待线程
 * @param cond 条件变量指针
 */
AGENTOS_LITE_API void agentos_lite_cond_signal(agentos_lite_cond_t* cond);

/**
 * @brief 唤醒所有等待线程
 * @param cond 条件变量指针
 */
AGENTOS_LITE_API void agentos_lite_cond_broadcast(agentos_lite_cond_t* cond);

/**
 * @brief 创建线程
 * @param thread 线程句柄指针
 * @param attr 线程属性（可为NULL使用默认值）
 * @param func 线程函数
 * @param arg 线程参数
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_create(
    agentos_lite_thread_t* thread,
    const agentos_lite_thread_attr_t* attr,
    void (*func)(void*),
    void* arg);

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_thread_join(agentos_lite_thread_t thread);

/**
 * @brief 获取当前任务ID
 * @return 当前任务ID
 */
AGENTOS_LITE_API agentos_lite_task_id_t agentos_lite_task_self(void);

/**
 * @brief 任务休眠
 * @param ms 休眠时间（毫秒）
 */
AGENTOS_LITE_API void agentos_lite_task_sleep(uint32_t ms);

/**
 * @brief 任务让出CPU
 */
AGENTOS_LITE_API void agentos_lite_task_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_TASK_H */
