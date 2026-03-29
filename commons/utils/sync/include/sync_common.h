/**
 * @file sync_common.h
 * @brief 同步功能通用定义
 * 
 * 提供同步相关的共享功能，包括互斥锁、条件变量、信号量等
 * 减少同步相关代码的重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef SYNC_COMMON_H
#define SYNC_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 互斥锁结构
 */
typedef struct {
    void* mutex;           /**< 底层互斥锁 */
    bool initialized;      /**< 是否已初始化 */
} sync_mutex_t;

/**
 * @brief 条件变量结构
 */
typedef struct {
    void* cond;            /**< 底层条件变量 */
    bool initialized;      /**< 是否已初始化 */
} sync_cond_t;

/**
 * @brief 信号量结构
 */
typedef struct {
    void* sem;             /**< 底层信号量 */
    bool initialized;      /**< 是否已初始化 */
    uint32_t value;        /**< 当前值 */
} sync_sem_t;

/**
 * @brief 读写锁结构
 */
typedef struct {
    void* rwlock;          /**< 底层读写锁 */
    bool initialized;      /**< 是否已初始化 */
} sync_rwlock_t;

/**
 * @brief 初始化互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_init(sync_mutex_t* mutex);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
void sync_mutex_destroy(sync_mutex_t* mutex);

/**
 * @brief 加锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_lock(sync_mutex_t* mutex);

/**
 * @brief 解锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_unlock(sync_mutex_t* mutex);

/**
 * @brief 尝试加锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_trylock(sync_mutex_t* mutex);

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_init(sync_cond_t* cond);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
void sync_cond_destroy(sync_cond_t* cond);

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_cond_wait(sync_cond_t* cond, sync_mutex_t* mutex);

/**
 * @brief 带超时的等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败
 */
int sync_cond_timedwait(sync_cond_t* cond, sync_mutex_t* mutex, uint32_t timeout_ms);

/**
 * @brief 唤醒一个等待条件变量的线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_signal(sync_cond_t* cond);

/**
 * @brief 唤醒所有等待条件变量的线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_broadcast(sync_cond_t* cond);

/**
 * @brief 初始化信号量
 * @param sem 信号量指针
 * @param value 初始值
 * @return 0 成功，非0 失败
 */
int sync_sem_init(sync_sem_t* sem, uint32_t value);

/**
 * @brief 销毁信号量
 * @param sem 信号量指针
 */
void sync_sem_destroy(sync_sem_t* sem);

/**
 * @brief 等待信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_wait(sync_sem_t* sem);

/**
 * @brief 带超时的等待信号量
 * @param sem 信号量指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败
 */
int sync_sem_timedwait(sync_sem_t* sem, uint32_t timeout_ms);

/**
 * @brief 尝试等待信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_trywait(sync_sem_t* sem);

/**
 * @brief 释放信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_post(sync_sem_t* sem);

/**
 * @brief 获取信号量当前值
 * @param sem 信号量指针
 * @param value 输出参数，用于存储当前值
 * @return 0 成功，非0 失败
 */
int sync_sem_getvalue(sync_sem_t* sem, uint32_t* value);

/**
 * @brief 初始化读写锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_init(sync_rwlock_t* rwlock);

/**
 * @brief 销毁读写锁
 * @param rwlock 读写锁指针
 */
void sync_rwlock_destroy(sync_rwlock_t* rwlock);

/**
 * @brief 读加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_rdlock(sync_rwlock_t* rwlock);

/**
 * @brief 写加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_wrlock(sync_rwlock_t* rwlock);

/**
 * @brief 尝试读加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_tryrdlock(sync_rwlock_t* rwlock);

/**
 * @brief 尝试写加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_trywrlock(sync_rwlock_t* rwlock);

/**
 * @brief 解锁读写锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_unlock(sync_rwlock_t* rwlock);

#ifdef __cplusplus
}
#endif

#endif // SYNC_COMMON_H