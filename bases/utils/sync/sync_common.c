/**
 * @file sync_common.c
 * @brief 同步功能通用实现
 * 
 * 提供同步相关的共享功能，包括互斥锁、条件变量、信号量等
 * 减少同步相关代码的重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "include/sync_common.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

/**
 * @brief 初始化互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_init(sync_mutex_t* mutex) {
    if (!mutex) {
        return -1;
    }

#ifdef _WIN32
    mutex->mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex->mutex) {
        return -1;
    }
#else
    pthread_mutex_t* p_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!p_mutex) {
        return -1;
    }
    if (pthread_mutex_init(p_mutex, NULL) != 0) {
        free(p_mutex);
        return -1;
    }
    mutex->mutex = p_mutex;
#endif

    mutex->initialized = true;
    return 0;
}

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
void sync_mutex_destroy(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return;
    }

#ifdef _WIN32
    CloseHandle(mutex->mutex);
#else
    pthread_mutex_destroy((pthread_mutex_t*)mutex->mutex);
    free(mutex->mutex);
#endif

    mutex->mutex = NULL;
    mutex->initialized = false;
}

/**
 * @brief 加锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_lock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (WaitForSingleObject(mutex->mutex, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_mutex_lock((pthread_mutex_t*)mutex->mutex) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 解锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_unlock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (!ReleaseMutex(mutex->mutex)) {
        return -1;
    }
#else
    if (pthread_mutex_unlock((pthread_mutex_t*)mutex->mutex) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 尝试加锁互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_mutex_trylock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (WaitForSingleObject(mutex->mutex, 0) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_mutex_trylock((pthread_mutex_t*)mutex->mutex) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_init(sync_cond_t* cond) {
    if (!cond) {
        return -1;
    }

#ifdef _WIN32
    cond->cond = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!cond->cond) {
        return -1;
    }
#else
    pthread_cond_t* p_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    if (!p_cond) {
        return -1;
    }
    if (pthread_cond_init(p_cond, NULL) != 0) {
        free(p_cond);
        return -1;
    }
    cond->cond = p_cond;
#endif

    cond->initialized = true;
    return 0;
}

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
void sync_cond_destroy(sync_cond_t* cond) {
    if (!cond || !cond->initialized) {
        return;
    }

#ifdef _WIN32
    CloseHandle(cond->cond);
#else
    pthread_cond_destroy((pthread_cond_t*)cond->cond);
    free(cond->cond);
#endif

    cond->cond = NULL;
    cond->initialized = false;
}

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int sync_cond_wait(sync_cond_t* cond, sync_mutex_t* mutex) {
    if (!cond || !mutex || !cond->initialized || !mutex->initialized) {
        return -1;
    }

#ifdef _WIN32
    sync_mutex_unlock(mutex);
    if (WaitForSingleObject(cond->cond, INFINITE) != WAIT_OBJECT_0) {
        sync_mutex_lock(mutex);
        return -1;
    }
    sync_mutex_lock(mutex);
#else
    if (pthread_cond_wait((pthread_cond_t*)cond->cond, (pthread_mutex_t*)mutex->mutex) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 带超时的等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败
 */
int sync_cond_timedwait(sync_cond_t* cond, sync_mutex_t* mutex, uint32_t timeout_ms) {
    if (!cond || !mutex || !cond->initialized || !mutex->initialized) {
        return -1;
    }

#ifdef _WIN32
    sync_mutex_unlock(mutex);
    if (WaitForSingleObject(cond->cond, timeout_ms) != WAIT_OBJECT_0) {
        sync_mutex_lock(mutex);
        return -1;
    }
    sync_mutex_lock(mutex);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    if (pthread_cond_timedwait((pthread_cond_t*)cond->cond, (pthread_mutex_t*)mutex->mutex, &ts) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 唤醒一个等待条件变量的线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_signal(sync_cond_t* cond) {
    if (!cond || !cond->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (!SetEvent(cond->cond)) {
        return -1;
    }
#else
    if (pthread_cond_signal((pthread_cond_t*)cond->cond) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 唤醒所有等待条件变量的线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int sync_cond_broadcast(sync_cond_t* cond) {
    if (!cond || !cond->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (!SetEvent(cond->cond)) {
        return -1;
    }
#else
    if (pthread_cond_broadcast((pthread_cond_t*)cond->cond) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 初始化信号量
 * @param sem 信号量指针
 * @param value 初始值
 * @return 0 成功，非0 失败
 */
int sync_sem_init(sync_sem_t* sem, uint32_t value) {
    if (!sem) {
        return -1;
    }

#ifdef _WIN32
    sem->sem = CreateSemaphore(NULL, value, 0xFFFFFFFF, NULL);
    if (!sem->sem) {
        return -1;
    }
#else
    sem_t* p_sem = (sem_t*)malloc(sizeof(sem_t));
    if (!p_sem) {
        return -1;
    }
    if (sem_init(p_sem, 0, value) != 0) {
        free(p_sem);
        return -1;
    }
    sem->sem = p_sem;
#endif

    sem->initialized = true;
    sem->value = value;
    return 0;
}

/**
 * @brief 销毁信号量
 * @param sem 信号量指针
 */
void sync_sem_destroy(sync_sem_t* sem) {
    if (!sem || !sem->initialized) {
        return;
    }

#ifdef _WIN32
    CloseHandle(sem->sem);
#else
    sem_destroy((sem_t*)sem->sem);
    free(sem->sem);
#endif

    sem->sem = NULL;
    sem->initialized = false;
    sem->value = 0;
}

/**
 * @brief 等待信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_wait(sync_sem_t* sem) {
    if (!sem || !sem->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (WaitForSingleObject(sem->sem, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }
    sem->value--;
#else
    if (sem_wait((sem_t*)sem->sem) != 0) {
        return -1;
    }
    sem_getvalue((sem_t*)sem->sem, (int*)&sem->value);
#endif

    return 0;
}

/**
 * @brief 带超时的等待信号量
 * @param sem 信号量指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败
 */
int sync_sem_timedwait(sync_sem_t* sem, uint32_t timeout_ms) {
    if (!sem || !sem->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (WaitForSingleObject(sem->sem, timeout_ms) != WAIT_OBJECT_0) {
        return -1;
    }
    sem->value--;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    if (sem_timedwait((sem_t*)sem->sem, &ts) != 0) {
        return -1;
    }
    sem_getvalue((sem_t*)sem->sem, (int*)&sem->value);
#endif

    return 0;
}

/**
 * @brief 尝试等待信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_trywait(sync_sem_t* sem) {
    if (!sem || !sem->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (WaitForSingleObject(sem->sem, 0) != WAIT_OBJECT_0) {
        return -1;
    }
    sem->value--;
#else
    if (sem_trywait((sem_t*)sem->sem) != 0) {
        return -1;
    }
    sem_getvalue((sem_t*)sem->sem, (int*)&sem->value);
#endif

    return 0;
}

/**
 * @brief 释放信号量
 * @param sem 信号量指针
 * @return 0 成功，非0 失败
 */
int sync_sem_post(sync_sem_t* sem) {
    if (!sem || !sem->initialized) {
        return -1;
    }

#ifdef _WIN32
    if (!ReleaseSemaphore(sem->sem, 1, NULL)) {
        return -1;
    }
    sem->value++;
#else
    if (sem_post((sem_t*)sem->sem) != 0) {
        return -1;
    }
    sem_getvalue((sem_t*)sem->sem, (int*)&sem->value);
#endif

    return 0;
}

/**
 * @brief 获取信号量当前值
 * @param sem 信号量指针
 * @param value 输出参数，用于存储当前值
 * @return 0 成功，非0 失败
 */
int sync_sem_getvalue(sync_sem_t* sem, uint32_t* value) {
    if (!sem || !value || !sem->initialized) {
        return -1;
    }

#ifdef _WIN32
    *value = sem->value;
#else
    if (sem_getvalue((sem_t*)sem->sem, (int*)value) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 初始化读写锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_init(sync_rwlock_t* rwlock) {
    if (!rwlock) {
        return -1;
    }

#ifdef _WIN32
    // Windows 没有原生读写锁，使用互斥锁模拟
    rwlock->rwlock = CreateMutex(NULL, FALSE, NULL);
    if (!rwlock->rwlock) {
        return -1;
    }
#else
    pthread_rwlock_t* p_rwlock = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
    if (!p_rwlock) {
        return -1;
    }
    if (pthread_rwlock_init(p_rwlock, NULL) != 0) {
        free(p_rwlock);
        return -1;
    }
    rwlock->rwlock = p_rwlock;
#endif

    rwlock->initialized = true;
    return 0;
}

/**
 * @brief 销毁读写锁
 * @param rwlock 读写锁指针
 */
void sync_rwlock_destroy(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return;
    }

#ifdef _WIN32
    CloseHandle(rwlock->rwlock);
#else
    pthread_rwlock_destroy((pthread_rwlock_t*)rwlock->rwlock);
    free(rwlock->rwlock);
#endif

    rwlock->rwlock = NULL;
    rwlock->initialized = false;
}

/**
 * @brief 读加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_rdlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return -1;
    }

#ifdef _WIN32
    // Windows 模拟，使用互斥锁
    if (WaitForSingleObject(rwlock->rwlock, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_rwlock_rdlock((pthread_rwlock_t*)rwlock->rwlock) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 写加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_wrlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return -1;
    }

#ifdef _WIN32
    // Windows 模拟，使用互斥锁
    if (WaitForSingleObject(rwlock->rwlock, INFINITE) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_rwlock_wrlock((pthread_rwlock_t*)rwlock->rwlock) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 尝试读加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_tryrdlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return -1;
    }

#ifdef _WIN32
    // Windows 模拟，使用互斥锁
    if (WaitForSingleObject(rwlock->rwlock, 0) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_rwlock_tryrdlock((pthread_rwlock_t*)rwlock->rwlock) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 尝试写加锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_trywrlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return -1;
    }

#ifdef _WIN32
    // Windows 模拟，使用互斥锁
    if (WaitForSingleObject(rwlock->rwlock, 0) != WAIT_OBJECT_0) {
        return -1;
    }
#else
    if (pthread_rwlock_trywrlock((pthread_rwlock_t*)rwlock->rwlock) != 0) {
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief 解锁读写锁
 * @param rwlock 读写锁指针
 * @return 0 成功，非0 失败
 */
int sync_rwlock_unlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return -1;
    }

#ifdef _WIN32
    // Windows 模拟，使用互斥锁
    if (!ReleaseMutex(rwlock->rwlock)) {
        return -1;
    }
#else
    if (pthread_rwlock_unlock((pthread_rwlock_t*)rwlock->rwlock) != 0) {
        return -1;
    }
#endif

    return 0;
}