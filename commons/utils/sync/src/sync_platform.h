/**
 * @file sync_platform.h
 * @brief 同步模块平台抽象层 - 内部使用
 * 
 * 提供跨平台的同步原语底层实现抽象
 * 支持Windows和POSIX系统
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef SYNC_PLATFORM_H
#define SYNC_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sync_platform
 * @{
 */

/* Windows平台特定实现 */
#ifdef _WIN32
#include <windows.h>
#include <synchapi.h>

/** Windows平台互斥锁类型 */
typedef CRITICAL_SECTION platform_mutex_t;

/** Windows平台递归互斥锁类型 */
typedef CRITICAL_SECTION platform_recursive_mutex_t;

/** Windows平台读写锁类型 */
typedef SRWLOCK platform_rwlock_t;

/** Windows平台自旋锁类型 */
typedef LONG platform_spinlock_t;

/** Windows平台信号量类型 */
typedef HANDLE platform_semaphore_t;

/** Windows平台条件变量类型 */
typedef CONDITION_VARIABLE platform_condition_t;

/** Windows平台屏障类型（模拟实现） */
typedef struct {
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cond;
    unsigned int count;
    unsigned int current;
    unsigned int generation;
} platform_barrier_t;

/** Windows平台事件类型 */
typedef HANDLE platform_event_t;

/* POSIX平台特定实现 */
#else
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

/** POSIX平台互斥锁类型 */
typedef pthread_mutex_t platform_mutex_t;

/** POSIX平台递归互斥锁类型 */
typedef pthread_mutex_t platform_recursive_mutex_t;

/** POSIX平台读写锁类型 */
typedef pthread_rwlock_t platform_rwlock_t;

/** POSIX平台自旋锁类型 */
typedef pthread_spinlock_t platform_spinlock_t;

/** POSIX平台信号量类型 */
typedef sem_t platform_semaphore_t;

/** POSIX平台条件变量类型 */
typedef pthread_cond_t platform_condition_t;

/** POSIX平台屏障类型 */
typedef pthread_barrier_t platform_barrier_t;

/** POSIX平台事件类型（模拟实现） */
typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    bool signaled;
    bool manual_reset;
} platform_event_t;

#endif /* _WIN32 */

/**
 * @brief 平台互斥锁初始化
 * @return 0 成功，非0 失败
 */
int platform_mutex_init(platform_mutex_t* mutex);

/**
 * @brief 平台互斥锁销毁
 * @param[in] mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_mutex_destroy(platform_mutex_t* mutex);

/**
 * @brief 平台互斥锁加锁
 * @param[in] mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_mutex_lock(platform_mutex_t* mutex);

/**
 * @brief 平台互斥锁解锁
 * @param[in] mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_mutex_unlock(platform_mutex_t* mutex);

/**
 * @brief 平台递归互斥锁初始化
 * @return 0 成功，非0 失败
 */
int platform_recursive_mutex_init(platform_recursive_mutex_t* mutex);

/**
 * @brief 平台递归互斥锁销毁
 * @param[in] mutex 递归互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_recursive_mutex_destroy(platform_recursive_mutex_t* mutex);

/**
 * @brief 平台递归互斥锁加锁
 * @param[in] mutex 递归互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_recursive_mutex_lock(platform_recursive_mutex_t* mutex);

/**
 * @brief 平台递归互斥锁解锁
 * @param[in] mutex 递归互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_recursive_mutex_unlock(platform_recursive_mutex_t* mutex);

/**
 * @brief 平台读写锁初始化
 * @return 0 成功，非0 失败
 */
int platform_rwlock_init(platform_rwlock_t* rwlock);

/**
 * @brief 平台读写锁销毁
 * @param[in] rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int platform_rwlock_destroy(platform_rwlock_t* rwlock);

/**
 * @brief 平台读锁加锁
 * @param[in] rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int platform_rwlock_rdlock(platform_rwlock_t* rwlock);

/**
 * @brief 平台写锁加锁
 * @param[in] rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int platform_rwlock_wrlock(platform_rwlock_t* rwlock);

/**
 * @brief 平台读写锁解锁
 * @param[in] rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int platform_rwlock_unlock(platform_rwlock_t* rwlock);

/**
 * @brief 平台自旋锁初始化
 * @return 0 成功，非0 失败
 */
int platform_spinlock_init(platform_spinlock_t* spinlock);

/**
 * @brief 平台自旋锁销毁
 * @param[in] spinlock 自旋锁指针
 * @return 0 成功，非0 失败
 */
int platform_spinlock_destroy(platform_spinlock_t* spinlock);

/**
 * @brief 平台自旋锁加锁
 * @param[in] spinlock 自旋锁指针
 * @return 0 成功，非0 失败
 */
int platform_spinlock_lock(platform_spinlock_t* spinlock);

/**
 * @brief 平台自旋锁解锁
 * @param[in] spinlock 自旋锁指针
 * @return 0 成功，非0 失败
 */
int platform_spinlock_unlock(platform_spinlock_t* spinlock);

/**
 * @brief 平台信号量初始化
 * @param[in] semaphore 信号量指针
 * @param[in] value 初始值
 * @return 0 成功，非0 失败
 */
int platform_semaphore_init(platform_semaphore_t* semaphore, unsigned int value);

/**
 * @brief 平台信号量销毁
 * @param[in] semaphore 信号量指针
 * @return 0 成功，非0 失败
 */
int platform_semaphore_destroy(platform_semaphore_t* semaphore);

/**
 * @brief 平台信号量等待（P操作）
 * @param[in] semaphore 信号量指针
 * @return 0 成功，非0 失败
 */
int platform_semaphore_wait(platform_semaphore_t* semaphore);

/**
 * @brief 平台信号量发信号（V操作）
 * @param[in] semaphore 信号量指针
 * @return 0 成功，非0 失败
 */
int platform_semaphore_post(platform_semaphore_t* semaphore);

/**
 * @brief 平台条件变量初始化
 * @return 0 成功，非0 失败
 */
int platform_condition_init(platform_condition_t* cond);

/**
 * @brief 平台条件变量销毁
 * @param[in] cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int platform_condition_destroy(platform_condition_t* cond);

/**
 * @brief 平台条件变量等待
 * @param[in] cond 条件变量指针
 * @param[in] mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int platform_condition_wait(platform_condition_t* cond, platform_mutex_t* mutex);

/**
 * @brief 平台条件变量唤醒一个线程
 * @param[in] cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int platform_condition_signal(platform_condition_t* cond);

/**
 * @brief 平台条件变量唤醒所有线程
 * @param[in] cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int platform_condition_broadcast(platform_condition_t* cond);

/**
 * @brief 平台屏障初始化
 * @param[in] barrier 屏障指针
 * @param[in] count 等待线程数
 * @return 0 成功，非0 失败
 */
int platform_barrier_init(platform_barrier_t* barrier, unsigned int count);

/**
 * @brief 平台屏障销毁
 * @param[in] barrier 屏障指针
 * @return 0 成功，非0 失败
 */
int platform_barrier_destroy(platform_barrier_t* barrier);

/**
 * @brief 平台屏障等待
 * @param[in] barrier 屏障指针
 * @return 0 成功，非0 失败
 */
int platform_barrier_wait(platform_barrier_t* barrier);

/**
 * @brief 平台事件初始化
 * @param[in] event 事件指针
 * @param[in] manual_reset 是否手动重置
 * @return 0 成功，非0 失败
 */
int platform_event_init(platform_event_t* event, bool manual_reset);

/**
 * @brief 平台事件销毁
 * @param[in] event 事件指针
 * @return 0 成功，非0 失败
 */
int platform_event_destroy(platform_event_t* event);

/**
 * @brief 平台事件设置信号
 * @param[in] event 事件指针
 * @return 0 成功，非0 失败
 */
int platform_event_set(platform_event_t* event);

/**
 * @brief 平台事件重置信号
 * @param[in] event 事件指针
 * @return 0 成功，非0 失败
 */
int platform_event_reset(platform_event_t* event);

/**
 * @brief 平台事件等待
 * @param[in] event 事件指针
 * @param[in] timeout_ms 超时时间（毫秒），0表示无限等待
 * @return 0 成功，非0 失败
 */
int platform_event_wait(platform_event_t* event, uint64_t timeout_ms);

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳
 */
uint64_t platform_get_timestamp_ms(void);

/**
 * @brief 获取当前线程ID
 * @return 线程ID
 */
uint64_t platform_get_thread_id(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SYNC_PLATFORM_H */
