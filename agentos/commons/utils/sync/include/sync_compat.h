/**
 * @file sync_compat.h
 * @brief 统一线程同步原语模块 - 向后兼容层
 * 
 * 提供与POSIX线程和Windows线程API兼容的接口，便于现有代码逐步迁移到统一同步原语模块。
 * 包含跨平台同步原语包装器和迁移辅助宏。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SYNC_COMPAT_H
#define AGENTOS_SYNC_COMPAT_H

#include <stdbool.h>
#include "sync.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sync_compat_api 同步兼容API
 * @{
 */

/* ==================== 互斥锁兼容API ==================== */

/**
 * @brief 创建互斥锁（兼容pthread_mutex_init）
 * 
 * @param[out] mutex 互斥锁句柄
 * @param[in] attrs 属性（可空）
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_mutex_init(sync_mutex_t* mutex, const void* attrs) {
    (void)attrs;
    return sync_mutex_create(mutex) ? 0 : -1;
}

/**
 * @brief 销毁互斥锁（兼容pthread_mutex_destroy）
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_mutex_destroy(sync_mutex_t* mutex) {
    return sync_mutex_destroy(mutex) ? 0 : -1;
}

/**
 * @brief 锁定互斥锁（兼容pthread_mutex_lock）
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_mutex_lock(sync_mutex_t* mutex) {
    return sync_mutex_lock(mutex) ? 0 : -1;
}

/**
 * @brief 尝试锁定互斥锁（兼容pthread_mutex_trylock）
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功锁定返回0，已锁定返回-1，失败返回其他错误码
 */
static inline int agentos_mutex_trylock(sync_mutex_t* mutex) {
    return sync_mutex_trylock(mutex) ? 0 : -1;
}

/**
 * @brief 解锁互斥锁（兼容pthread_mutex_unlock）
 * 
 * @param[in] mutex 互斥锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_mutex_unlock(sync_mutex_t* mutex) {
    return sync_mutex_unlock(mutex) ? 0 : -1;
}

/* ==================== 条件变量兼容API ==================== */

/**
 * @brief 创建条件变量（兼容pthread_cond_init）
 * 
 * @param[out] cond 条件变量句柄
 * @param[in] attrs 属性（可空）
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_cond_init(sync_cond_t* cond, const void* attrs) {
    (void)attrs;
    return sync_cond_create(cond) ? 0 : -1;
}

/**
 * @brief 销毁条件变量（兼容pthread_cond_destroy）
 * 
 * @param[in] cond 条件变量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_cond_destroy(sync_cond_t* cond) {
    return sync_cond_destroy(cond) ? 0 : -1;
}

/**
 * @brief 等待条件变量（兼容pthread_cond_wait）
 * 
 * @param[in] cond 条件变量句柄
 * @param[in] mutex 关联的互斥锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_cond_wait(sync_cond_t* cond, sync_mutex_t* mutex) {
    return sync_cond_wait(cond, mutex) ? 0 : -1;
}

/**
 * @brief 限时等待条件变量（兼容pthread_cond_timedwait）
 * 
 * @param[in] cond 条件变量句柄
 * @param[in] mutex 关联的互斥锁句柄
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return 成功返回0，超时返回-1，失败返回其他错误码
 */
static inline int agentos_cond_timedwait(sync_cond_t* cond, sync_mutex_t* mutex, int timeout_ms) {
    return sync_cond_timedwait(cond, mutex, timeout_ms) ? 0 : -1;
}

/**
 * @brief 通知一个等待线程（兼容pthread_cond_signal）
 * 
 * @param[in] cond 条件变量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_cond_signal(sync_cond_t* cond) {
    return sync_cond_signal(cond) ? 0 : -1;
}

/**
 * @brief 通知所有等待线程（兼容pthread_cond_broadcast）
 * 
 * @param[in] cond 条件变量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_cond_broadcast(sync_cond_t* cond) {
    return sync_cond_broadcast(cond) ? 0 : -1;
}

/* ==================== 读写锁兼容API ==================== */

/**
 * @brief 创建读写锁（兼容pthread_rwlock_init）
 * 
 * @param[out] rwlock 读写锁句柄
 * @param[in] attrs 属性（可空）
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_rwlock_init(sync_rwlock_t* rwlock, const void* attrs) {
    (void)attrs;
    return sync_rwlock_create(rwlock) ? 0 : -1;
}

/**
 * @brief 销毁读写锁（兼容pthread_rwlock_destroy）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_rwlock_destroy(sync_rwlock_t* rwlock) {
    return sync_rwlock_destroy(rwlock) ? 0 : -1;
}

/**
 * @brief 获取读锁（兼容pthread_rwlock_rdlock）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_rwlock_rdlock(sync_rwlock_t* rwlock) {
    return sync_rwlock_rdlock(rwlock) ? 0 : -1;
}

/**
 * @brief 尝试获取读锁（兼容pthread_rwlock_tryrdlock）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，已锁定返回-1，失败返回其他错误码
 */
static inline int agentos_rwlock_tryrdlock(sync_rwlock_t* rwlock) {
    return sync_rwlock_tryrdlock(rwlock) ? 0 : -1;
}

/**
 * @brief 获取写锁（兼容pthread_rwlock_wrlock）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_rwlock_wrlock(sync_rwlock_t* rwlock) {
    return sync_rwlock_wrlock(rwlock) ? 0 : -1;
}

/**
 * @brief 尝试获取写锁（兼容pthread_rwlock_trywrlock）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，已锁定返回-1，失败返回其他错误码
 */
static inline int agentos_rwlock_trywrlock(sync_rwlock_t* rwlock) {
    return sync_rwlock_trywrlock(rwlock) ? 0 : -1;
}

/**
 * @brief 解锁读写锁（兼容pthread_rwlock_unlock）
 * 
 * @param[in] rwlock 读写锁句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_rwlock_unlock(sync_rwlock_t* rwlock) {
    return sync_rwlock_unlock(rwlock) ? 0 : -1;
}

/* ==================== 信号量兼容API ==================== */

/**
 * @brief 创建信号量（兼容sem_init）
 * 
 * @param[out] sem 信号量句柄
 * @param[in] pshared 共享标志（跨进程）
 * @param[in] value 初始值
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_init(sync_sem_t* sem, int pshared, unsigned int value) {
    (void)pshared;
    return sync_sem_create(sem, value) ? 0 : -1;
}

/**
 * @brief 销毁信号量（兼容sem_destroy）
 * 
 * @param[in] sem 信号量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_destroy(sync_sem_t* sem) {
    return sync_sem_destroy(sem) ? 0 : -1;
}

/**
 * @brief 等待信号量（兼容sem_wait）
 * 
 * @param[in] sem 信号量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_wait(sync_sem_t* sem) {
    return sync_sem_wait(sem) ? 0 : -1;
}

/**
 * @brief 尝试等待信号量（兼容sem_trywait）
 * 
 * @param[in] sem 信号量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_trywait(sync_sem_t* sem) {
    return sync_sem_trywait(sem) ? 0 : -1;
}

/**
 * @brief 限时等待信号量（兼容sem_timedwait）
 * 
 * @param[in] sem 信号量句柄
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return 成功返回0，超时返回-1，失败返回其他错误码
 */
static inline int agentos_sem_timedwait(sync_sem_t* sem, int timeout_ms) {
    return sync_sem_timedwait(sem, timeout_ms) ? 0 : -1;
}

/**
 * @brief 发布信号量（兼容sem_post）
 * 
 * @param[in] sem 信号量句柄
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_post(sync_sem_t* sem) {
    return sync_sem_post(sem) ? 0 : -1;
}

/**
 * @brief 获取信号量值（兼容sem_getvalue）
 * 
 * @param[in] sem 信号量句柄
 * @param[out] value 当前值
 * @return 成功返回0，失败返回错误码
 */
static inline int agentos_sem_getvalue(sync_sem_t* sem, int* value) {
    return sync_sem_getvalue(sem, value) ? 0 : -1;
}

/* ==================== 兼容性宏定义 ==================== */

/**
 * @def AGENTOS_MUTEX_INIT(mutex, attrs)
 * @brief 安全互斥锁初始化宏
 */
#define AGENTOS_MUTEX_INIT(mutex, attrs) agentos_mutex_init(mutex, attrs)

/**
 * @def AGENTOS_MUTEX_DESTROY(mutex)
 * @brief 安全互斥锁销毁宏
 */
#define AGENTOS_MUTEX_DESTROY(mutex) agentos_mutex_destroy(mutex)

/**
 * @def AGENTOS_MUTEX_LOCK(mutex)
 * @brief 安全互斥锁锁定宏
 */
#define AGENTOS_MUTEX_LOCK(mutex) agentos_mutex_lock(mutex)

/**
 * @def AGENTOS_MUTEX_TRYLOCK(mutex)
 * @brief 安全互斥锁尝试锁定宏
 */
#define AGENTOS_MUTEX_TRYLOCK(mutex) agentos_mutex_trylock(mutex)

/**
 * @def AGENTOS_MUTEX_UNLOCK(mutex)
 * @brief 安全互斥锁解锁宏
 */
#define AGENTOS_MUTEX_UNLOCK(mutex) agentos_mutex_unlock(mutex)

/**
 * @def AGENTOS_COND_INIT(cond, attrs)
 * @brief 安全条件变量初始化宏
 */
#define AGENTOS_COND_INIT(cond, attrs) agentos_cond_init(cond, attrs)

/**
 * @def AGENTOS_COND_DESTROY(cond)
 * @brief 安全条件变量销毁宏
 */
#define AGENTOS_COND_DESTROY(cond) agentos_cond_destroy(cond)

/**
 * @def AGENTOS_COND_WAIT(cond, mutex)
 * @brief 安全条件变量等待宏
 */
#define AGENTOS_COND_WAIT(cond, mutex) agentos_cond_wait(cond, mutex)

/**
 * @def AGENTOS_COND_TIMEDWAIT(cond, mutex, timeout_ms)
 * @brief 安全条件变量限时等待宏
 */
#define AGENTOS_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentos_cond_timedwait(cond, mutex, timeout_ms)

/**
 * @def AGENTOS_COND_SIGNAL(cond)
 * @brief 安全条件变量通知一个线程宏
 */
#define AGENTOS_COND_SIGNAL(cond) agentos_cond_signal(cond)

/**
 * @def AGENTOS_COND_BROADCAST(cond)
 * @brief 安全条件变量通知所有线程宏
 */
#define AGENTOS_COND_BROADCAST(cond) agentos_cond_broadcast(cond)

/**
 * @def AGENTOS_RWLOCK_INIT(rwlock, attrs)
 * @brief 安全读写锁初始化宏
 */
#define AGENTOS_RWLOCK_INIT(rwlock, attrs) agentos_rwlock_init(rwlock, attrs)

/**
 * @def AGENTOS_RWLOCK_DESTROY(rwlock)
 * @brief 安全读写锁销毁宏
 */
#define AGENTOS_RWLOCK_DESTROY(rwlock) agentos_rwlock_destroy(rwlock)

/**
 * @def AGENTOS_RWLOCK_RDLOCK(rwlock)
 * @brief 安全读锁获取宏
 */
#define AGENTOS_RWLOCK_RDLOCK(rwlock) agentos_rwlock_rdlock(rwlock)

/**
 * @def AGENTOS_RWLOCK_TRYRDLOCK(rwlock)
 * @brief 安全读锁尝试获取宏
 */
#define AGENTOS_RWLOCK_TRYRDLOCK(rwlock) agentos_rwlock_tryrdlock(rwlock)

/**
 * @def AGENTOS_RWLOCK_WRLOCK(rwlock)
 * @brief 安全写锁获取宏
 */
#define AGENTOS_RWLOCK_WRLOCK(rwlock) agentos_rwlock_wrlock(rwlock)

/**
 * @def AGENTOS_RWLOCK_TRYWRLOCK(rwlock)
 * @brief 安全写锁尝试获取宏
 */
#define AGENTOS_RWLOCK_TRYWRLOCK(rwlock) agentos_rwlock_trywrlock(rwlock)

/**
 * @def AGENTOS_RWLOCK_UNLOCK(rwlock)
 * @brief 安全读写锁解锁宏
 */
#define AGENTOS_RWLOCK_UNLOCK(rwlock) agentos_rwlock_unlock(rwlock)

/**
 * @def AGENTOS_SEM_INIT(sem, pshared, value)
 * @brief 安全信号量初始化宏
 */
#define AGENTOS_SEM_INIT(sem, pshared, value) agentos_sem_init(sem, pshared, value)

/**
 * @def AGENTOS_SEM_DESTROY(sem)
 * @brief 安全信号量销毁宏
 */
#define AGENTOS_SEM_DESTROY(sem) agentos_sem_destroy(sem)

/**
 * @def AGENTOS_SEM_WAIT(sem)
 * @brief 安全信号量等待宏
 */
#define AGENTOS_SEM_WAIT(sem) agentos_sem_wait(sem)

/**
 * @def AGENTOS_SEM_TRYWAIT(sem)
 * @brief 安全信号量尝试等待宏
 */
#define AGENTOS_SEM_TRYWAIT(sem) agentos_sem_trywait(sem)

/**
 * @def AGENTOS_SEM_TIMEDWAIT(sem, timeout_ms)
 * @brief 安全信号量限时等待宏
 */
#define AGENTOS_SEM_TIMEDWAIT(sem, timeout_ms) agentos_sem_timedwait(sem, timeout_ms)

/**
 * @def AGENTOS_SEM_POST(sem)
 * @brief 安全信号量发布宏
 */
#define AGENTOS_SEM_POST(sem) agentos_sem_post(sem)

/**
 * @def AGENTOS_SEM_GETVALUE(sem, value)
 * @brief 安全信号量获取值宏
 */
#define AGENTOS_SEM_GETVALUE(sem, value) agentos_sem_getvalue(sem, value)

/**
 * @brief 迁移辅助：替换标准同步原语
 * 
 * 建议的迁移步骤：
 * 1. 包含本头文件
 * 2. 将pthread_mutex_*替换为AGENTOS_MUTEX_*宏
 * 3. 将pthread_cond_*替换为AGENTOS_COND_*宏
 * 4. 将sem_*替换为AGENTOS_SEM_*宏
 * 5. 使用sync_thread_*函数替代pthread_create/join等
 */

/** @} */ // end of sync_compat_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYNC_COMPAT_H */