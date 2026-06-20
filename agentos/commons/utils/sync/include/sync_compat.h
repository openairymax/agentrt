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

#include "sync.h"

#include <limits.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sync_compat_api 同步兼容API
 * @{
 */

/* ==================== 互斥锁兼容API ==================== */

#ifndef AGENTOS_PLATFORM_H  /* 避免与 platform.h 中的 agentos_mutex_init 等冲突 */

/**
 * @brief 创建互斥锁（兼容pthread_mutex_init）
 */
static inline int agentos_mutex_init(sync_mutex_t *mutex, const void *attrs)
{
    (void)attrs;
    return sync_mutex_create(mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁互斥锁（兼容pthread_mutex_destroy）
 */
static inline int agentos_mutex_destroy(sync_mutex_t *mutex)
{
    return sync_mutex_free(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 锁定互斥锁（兼容pthread_mutex_lock）
 */
static inline int agentos_mutex_lock(sync_mutex_t *mutex)
{
    return sync_mutex_lock_ex(*mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试锁定互斥锁（兼容pthread_mutex_trylock）
 */
static inline int agentos_mutex_trylock(sync_mutex_t *mutex)
{
    return sync_mutex_try_lock(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 解锁互斥锁（兼容pthread_mutex_unlock）
 */
static inline int agentos_mutex_unlock(sync_mutex_t *mutex)
{
    return sync_mutex_unlock_ex(*mutex) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 条件变量兼容API ==================== */

/**
 * @brief 创建条件变量（兼容pthread_cond_init）
 */
static inline int agentos_cond_init(sync_condition_t *cond, const void *attrs)
{
    (void)attrs;
    return sync_condition_create(cond, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁条件变量（兼容pthread_cond_destroy）
 */
static inline int agentos_cond_destroy(sync_condition_t *cond)
{
    return sync_condition_free(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 等待条件变量（兼容pthread_cond_wait）
 */
static inline int agentos_cond_wait(sync_condition_t *cond, sync_mutex_t *mutex)
{
    return sync_condition_wait_ex(*cond, *mutex, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 限时等待条件变量（兼容pthread_cond_timedwait）
 */
static inline int agentos_cond_timedwait(sync_condition_t *cond, sync_mutex_t *mutex, int timeout_ms)
{
    sync_timeout_t timeout;
    timeout.absolute = false;
    timeout.timeout_ms = (uint64_t)timeout_ms;
    return sync_condition_wait_ex(*cond, *mutex, &timeout) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 通知一个等待线程（兼容pthread_cond_signal）
 */
static inline int agentos_cond_signal(sync_condition_t *cond)
{
    return sync_condition_signal_ex(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 通知所有等待线程（兼容pthread_cond_broadcast）
 */
static inline int agentos_cond_broadcast(sync_condition_t *cond)
{
    return sync_condition_broadcast_ex(*cond) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 读写锁兼容API ==================== */

/**
 * @brief 创建读写锁（兼容pthread_rwlock_init）
 */
static inline int agentos_rwlock_init(sync_rwlock_t *rwlock, const void *attrs)
{
    (void)attrs;
    return sync_rwlock_create(rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁读写锁（兼容pthread_rwlock_destroy）
 */
static inline int agentos_rwlock_destroy(sync_rwlock_t *rwlock)
{
    return sync_rwlock_free(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取读锁（兼容pthread_rwlock_rdlock）
 */
static inline int agentos_rwlock_rdlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_read_lock_ex(*rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试获取读锁（兼容pthread_rwlock_tryrdlock）
 */
static inline int agentos_rwlock_tryrdlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_try_read_lock(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取写锁（兼容pthread_rwlock_wrlock）
 */
static inline int agentos_rwlock_wrlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_write_lock_ex(*rwlock, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试获取写锁（兼容pthread_rwlock_trywrlock）
 */
static inline int agentos_rwlock_trywrlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_try_write_lock(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 解锁读写锁（兼容pthread_rwlock_unlock）
 */
static inline int agentos_rwlock_unlock(sync_rwlock_t *rwlock)
{
    return sync_rwlock_unlock_ex(*rwlock) == SYNC_SUCCESS ? 0 : -1;
}

/* ==================== 信号量兼容API ==================== */

/**
 * @brief 创建信号量（兼容sem_init）
 */
static inline int agentos_sem_init(sync_semaphore_t *sem, int pshared, unsigned int value)
{
    (void)pshared;
    return sync_semaphore_create(sem, value, UINT_MAX, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 销毁信号量（兼容sem_destroy）
 */
static inline int agentos_sem_destroy(sync_semaphore_t *sem)
{
    return sync_semaphore_free(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 等待信号量（兼容sem_wait）
 */
static inline int agentos_sem_wait(sync_semaphore_t *sem)
{
    return sync_semaphore_wait_ex(*sem, NULL) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 尝试等待信号量（兼容sem_trywait）
 */
static inline int agentos_sem_trywait(sync_semaphore_t *sem)
{
    return sync_semaphore_try_wait(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 限时等待信号量（兼容sem_timedwait）
 */
static inline int agentos_sem_timedwait(sync_semaphore_t *sem, int timeout_ms)
{
    sync_timeout_t timeout;
    timeout.absolute = false;
    timeout.timeout_ms = (uint64_t)timeout_ms;
    return sync_semaphore_wait_ex(*sem, &timeout) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 发布信号量（兼容sem_post）
 */
static inline int agentos_sem_post(sync_semaphore_t *sem)
{
    return sync_semaphore_post_ex(*sem) == SYNC_SUCCESS ? 0 : -1;
}

/**
 * @brief 获取信号量值（兼容sem_getvalue）
 */
static inline int agentos_sem_getvalue(sync_semaphore_t *sem, int *value)
{
    unsigned int uval = 0;
    sync_result_t r = sync_semaphore_get_value(*sem, &uval);
    if (value) *value = (int)uval;
    return r == SYNC_SUCCESS ? 0 : -1;
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

#endif /* AGENTOS_PLATFORM_H */

/* ==================== 兼容性宏定义（始终可用） ====================
 * 注意：这些宏始终可用，即使 platform.h 已包含。
 * 当 platform.h 未包含时，宏调用上面的 sync_compat inline 包装函数。
 * 当 platform.h 已包含时，宏调用 platform.h 中的 agentos_* 函数。
 */
#ifndef AGENTOS_PLATFORM_H

#define AGENTOS_MUTEX_INIT(mutex, attrs) agentos_mutex_init(mutex, attrs)
#define AGENTOS_MUTEX_DESTROY(mutex) agentos_mutex_destroy(mutex)
#define AGENTOS_MUTEX_LOCK(mutex) agentos_mutex_lock(mutex)
#define AGENTOS_MUTEX_TRYLOCK(mutex) agentos_mutex_trylock(mutex)
#define AGENTOS_MUTEX_UNLOCK(mutex) agentos_mutex_unlock(mutex)

#define AGENTOS_COND_INIT(cond, attrs) agentos_cond_init(cond, attrs)
#define AGENTOS_COND_DESTROY(cond) agentos_cond_destroy(cond)
#define AGENTOS_COND_WAIT(cond, mutex) agentos_cond_wait(cond, mutex)
#define AGENTOS_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentos_cond_timedwait(cond, mutex, timeout_ms)
#define AGENTOS_COND_SIGNAL(cond) agentos_cond_signal(cond)
#define AGENTOS_COND_BROADCAST(cond) agentos_cond_broadcast(cond)

#define AGENTOS_RWLOCK_INIT(rwlock, attrs) agentos_rwlock_init(rwlock, attrs)
#define AGENTOS_RWLOCK_DESTROY(rwlock) agentos_rwlock_destroy(rwlock)
#define AGENTOS_RWLOCK_RDLOCK(rwlock) agentos_rwlock_rdlock(rwlock)
#define AGENTOS_RWLOCK_TRYRDLOCK(rwlock) agentos_rwlock_tryrdlock(rwlock)
#define AGENTOS_RWLOCK_WRLOCK(rwlock) agentos_rwlock_wrlock(rwlock)
#define AGENTOS_RWLOCK_TRYWRLOCK(rwlock) agentos_rwlock_trywrlock(rwlock)
#define AGENTOS_RWLOCK_UNLOCK(rwlock) agentos_rwlock_unlock(rwlock)

#define AGENTOS_SEM_INIT(sem, pshared, value) agentos_sem_init(sem, pshared, value)
#define AGENTOS_SEM_DESTROY(sem) agentos_sem_destroy(sem)
#define AGENTOS_SEM_WAIT(sem) agentos_sem_wait(sem)
#define AGENTOS_SEM_TRYWAIT(sem) agentos_sem_trywait(sem)
#define AGENTOS_SEM_TIMEDWAIT(sem, timeout_ms) agentos_sem_timedwait(sem, timeout_ms)
#define AGENTOS_SEM_POST(sem) agentos_sem_post(sem)
#define AGENTOS_SEM_GETVALUE(sem, value) agentos_sem_getvalue(sem, value)

#else /* AGENTOS_PLATFORM_H defined */

#define AGENTOS_MUTEX_INIT(mutex, attrs) agentos_mutex_init(mutex)
#define AGENTOS_MUTEX_DESTROY(mutex) agentos_mutex_destroy(mutex)
#define AGENTOS_MUTEX_LOCK(mutex) agentos_mutex_lock(mutex)
#define AGENTOS_MUTEX_TRYLOCK(mutex) agentos_mutex_trylock(mutex)
#define AGENTOS_MUTEX_UNLOCK(mutex) agentos_mutex_unlock(mutex)

#define AGENTOS_COND_INIT(cond, attrs) agentos_cond_init(cond)
#define AGENTOS_COND_DESTROY(cond) agentos_cond_destroy(cond)
#define AGENTOS_COND_WAIT(cond, mutex) agentos_cond_wait(cond, mutex)
#define AGENTOS_COND_TIMEDWAIT(cond, mutex, timeout_ms) \
    agentos_cond_timedwait(cond, mutex, timeout_ms)
#define AGENTOS_COND_SIGNAL(cond) agentos_cond_signal(cond)
#define AGENTOS_COND_BROADCAST(cond) agentos_cond_broadcast(cond)

#define AGENTOS_RWLOCK_INIT(rwlock, attrs) ((void)(rwlock), (void)(attrs), 0)
#define AGENTOS_RWLOCK_DESTROY(rwlock) ((void)(rwlock))
#define AGENTOS_RWLOCK_RDLOCK(rwlock) ((void)(rwlock))
#define AGENTOS_RWLOCK_TRYRDLOCK(rwlock) ((void)(rwlock))
#define AGENTOS_RWLOCK_WRLOCK(rwlock) ((void)(rwlock))
#define AGENTOS_RWLOCK_TRYWRLOCK(rwlock) ((void)(rwlock))
#define AGENTOS_RWLOCK_UNLOCK(rwlock) ((void)(rwlock))

#define AGENTOS_SEM_INIT(sem, pshared, value) ((void)(sem), (void)(pshared), (void)(value), 0)
#define AGENTOS_SEM_DESTROY(sem) ((void)(sem))
#define AGENTOS_SEM_WAIT(sem) ((void)(sem))
#define AGENTOS_SEM_TRYWAIT(sem) ((void)(sem))
#define AGENTOS_SEM_TIMEDWAIT(sem, timeout_ms) ((void)(sem), (void)(timeout_ms))
#define AGENTOS_SEM_POST(sem) ((void)(sem))
#define AGENTOS_SEM_GETVALUE(sem, value) ((void)(sem), (void)(value))

#endif /* AGENTOS_PLATFORM_H */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYNC_COMPAT_H */