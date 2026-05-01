/**
 * @file sync_common.c
 * @brief Synchronization primitives - common implementation
 *
 * Provides mutex, condition variable, semaphore, and rwlock
 * implementations using the platform abstraction layer.
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "include/sync_common.h"
#include "src/sync_platform.h"
#include <stdlib.h>
#include <string.h>

int sync_mutex_init(sync_mutex_t* mutex) {
    if (!mutex) return -1;
    platform_mutex_t* m = (platform_mutex_t*)malloc(sizeof(platform_mutex_t));
    if (!m) return -1;
    if (platform_mutex_init(m) != 0) { free(m); return -1; }
    mutex->mutex = m;
    mutex->initialized = true;
    return 0;
}

void sync_mutex_destroy(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) return;
    platform_mutex_destroy((platform_mutex_t*)mutex->mutex);
    free(mutex->mutex);
    mutex->mutex = NULL;
    mutex->initialized = false;
}

int sync_mutex_lock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) return -1;
    return platform_mutex_lock((platform_mutex_t*)mutex->mutex);
}

int sync_mutex_unlock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) return -1;
    return platform_mutex_unlock((platform_mutex_t*)mutex->mutex);
}

int sync_mutex_trylock(sync_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) return -1;
#ifdef _WIN32
    return platform_mutex_lock((platform_mutex_t*)mutex->mutex);
#else
    return pthread_mutex_trylock((platform_mutex_t*)mutex->mutex);
#endif
}

int sync_cond_init(sync_cond_t* cond) {
    if (!cond) return -1;
    platform_condition_t* c = (platform_condition_t*)malloc(sizeof(platform_condition_t));
    if (!c) return -1;
    if (platform_condition_init(c) != 0) { free(c); return -1; }
    cond->cond = c;
    cond->initialized = true;
    return 0;
}

void sync_cond_destroy(sync_cond_t* cond) {
    if (!cond || !cond->initialized) return;
    platform_condition_destroy((platform_condition_t*)cond->cond);
    free(cond->cond);
    cond->cond = NULL;
    cond->initialized = false;
}

int sync_cond_wait(sync_cond_t* cond, sync_mutex_t* mutex) {
    if (!cond || !mutex || !cond->initialized || !mutex->initialized) return -1;
    return platform_condition_wait((platform_condition_t*)cond->cond,
                                   (platform_mutex_t*)mutex->mutex);
}

int sync_cond_timedwait(sync_cond_t* cond, sync_mutex_t* mutex, uint32_t timeout_ms) {
    if (!cond || !mutex || !cond->initialized || !mutex->initialized) return -1;
#ifdef _WIN32
    return platform_condition_wait((platform_condition_t*)cond->cond,
                                   (platform_mutex_t*)mutex->mutex);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    return pthread_cond_timedwait((platform_condition_t*)cond->cond,
                                  (platform_mutex_t*)mutex->mutex, &ts);
#endif
}

int sync_cond_signal(sync_cond_t* cond) {
    if (!cond || !cond->initialized) return -1;
    return platform_condition_signal((platform_condition_t*)cond->cond);
}

int sync_cond_broadcast(sync_cond_t* cond) {
    if (!cond || !cond->initialized) return -1;
    return platform_condition_broadcast((platform_condition_t*)cond->cond);
}

int sync_sem_init(sync_sem_t* sem, uint32_t value) {
    if (!sem) return -1;
    platform_semaphore_t* s = (platform_semaphore_t*)malloc(sizeof(platform_semaphore_t));
    if (!s) return -1;
    if (platform_semaphore_init(s, value) != 0) { free(s); return -1; }
    sem->sem = s;
    sem->initialized = true;
    sem->value = value;
    return 0;
}

void sync_sem_destroy(sync_sem_t* sem) {
    if (!sem || !sem->initialized) return;
    platform_semaphore_destroy((platform_semaphore_t*)sem->sem);
    free(sem->sem);
    sem->sem = NULL;
    sem->initialized = false;
    sem->value = 0;
}

int sync_sem_wait(sync_sem_t* sem) {
    if (!sem || !sem->initialized) return -1;
    int ret = platform_semaphore_wait((platform_semaphore_t*)sem->sem);
    if (ret == 0 && sem->value > 0) sem->value--;
    return ret;
}

int sync_sem_timedwait(sync_sem_t* sem, uint32_t timeout_ms) {
    if (!sem || !sem->initialized) return -1;
#ifdef _WIN32
    return platform_semaphore_wait((platform_semaphore_t*)sem->sem);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    int ret = sem_timedwait((platform_semaphore_t*)sem->sem, &ts);
    if (ret == 0 && sem->value > 0) sem->value--;
    return ret;
#endif
}

int sync_sem_trywait(sync_sem_t* sem) {
    if (!sem || !sem->initialized) return -1;
#ifdef _WIN32
    return platform_semaphore_wait((platform_semaphore_t*)sem->sem);
#else
    int ret = sem_trywait((platform_semaphore_t*)sem->sem);
    if (ret == 0 && sem->value > 0) sem->value--;
    return ret;
#endif
}

int sync_sem_post(sync_sem_t* sem) {
    if (!sem || !sem->initialized) return -1;
    int ret = platform_semaphore_post((platform_semaphore_t*)sem->sem);
    if (ret == 0) sem->value++;
    return ret;
}

int sync_sem_getvalue(sync_sem_t* sem, uint32_t* value) {
    if (!sem || !value || !sem->initialized) return -1;
    *value = sem->value;
    return 0;
}

int sync_rwlock_init(sync_rwlock_t* rwlock) {
    if (!rwlock) return -1;
    platform_rwlock_t* r = (platform_rwlock_t*)malloc(sizeof(platform_rwlock_t));
    if (!r) return -1;
    if (platform_rwlock_init(r) != 0) { free(r); return -1; }
    rwlock->rwlock = r;
    rwlock->initialized = true;
    return 0;
}

void sync_rwlock_destroy(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return;
    platform_rwlock_destroy((platform_rwlock_t*)rwlock->rwlock);
    free(rwlock->rwlock);
    rwlock->rwlock = NULL;
    rwlock->initialized = false;
}

int sync_rwlock_rdlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return -1;
    return platform_rwlock_rdlock((platform_rwlock_t*)rwlock->rwlock);
}

int sync_rwlock_wrlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return -1;
    return platform_rwlock_wrlock((platform_rwlock_t*)rwlock->rwlock);
}

int sync_rwlock_tryrdlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return -1;
    return platform_rwlock_rdlock((platform_rwlock_t*)rwlock->rwlock);
}

int sync_rwlock_trywrlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return -1;
    return platform_rwlock_wrlock((platform_rwlock_t*)rwlock->rwlock);
}

int sync_rwlock_unlock(sync_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return -1;
    return platform_rwlock_unlock((platform_rwlock_t*)rwlock->rwlock);
}
