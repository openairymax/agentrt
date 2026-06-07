#include "sync_common.h"

#include "src/sync_platform.h"
#include "types.h"

#include <stdlib.h>
#include <string.h>
#include "memory_compat.h"

int sync_mutex_init(sync_mutex_t *mutex)
{
    if (!mutex)
        return AGENTOS_EINVAL;
    platform_mutex_t *m = (platform_mutex_t *)AGENTOS_MALLOC(sizeof(platform_mutex_t));
    if (!m)
        return AGENTOS_EINVAL;
    if (platform_mutex_init(m) != 0) {
        AGENTOS_FREE(m);
        return AGENTOS_EINVAL;
    }
    mutex->mutex = m;
    mutex->initialized = true;
    return 0;
}

void sync_mutex_destroy(sync_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized)
        return;
    platform_mutex_destroy((platform_mutex_t *)mutex->mutex);
    AGENTOS_FREE(mutex->mutex);
    mutex->mutex = NULL;
    mutex->initialized = false;
}

int sync_mutex_lock(sync_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized)
        return AGENTOS_EINVAL;
    return platform_mutex_lock((platform_mutex_t *)mutex->mutex);
}

int sync_mutex_unlock(sync_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized)
        return AGENTOS_EINVAL;
    return platform_mutex_unlock((platform_mutex_t *)mutex->mutex);
}

int sync_mutex_trylock(sync_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized)
        return AGENTOS_EINVAL;
    return platform_mutex_trylock((platform_mutex_t *)mutex->mutex);
}

int sync_cond_init(sync_cond_t *cond)
{
    if (!cond)
        return AGENTOS_EINVAL;
    platform_condition_t *c = (platform_condition_t *)AGENTOS_MALLOC(sizeof(platform_condition_t));
    if (!c)
        return AGENTOS_EINVAL;
    if (platform_condition_init(c) != 0) {
        AGENTOS_FREE(c);
        return AGENTOS_EINVAL;
    }
    cond->cond = c;
    cond->initialized = true;
    return 0;
}

void sync_cond_destroy(sync_cond_t *cond)
{
    if (!cond || !cond->initialized)
        return;
    platform_condition_destroy((platform_condition_t *)cond->cond);
    AGENTOS_FREE(cond->cond);
    cond->cond = NULL;
    cond->initialized = false;
}

int sync_cond_wait(sync_cond_t *cond, sync_mutex_t *mutex)
{
    if (!cond || !mutex || !cond->initialized || !mutex->initialized)
        return AGENTOS_EINVAL;
    return platform_condition_wait((platform_condition_t *)cond->cond,
                                   (platform_mutex_t *)mutex->mutex);
}

int sync_cond_timedwait(sync_cond_t *cond, sync_mutex_t *mutex, uint32_t timeout_ms)
{
    if (!cond || !mutex || !cond->initialized || !mutex->initialized)
        return AGENTOS_EINVAL;
    return platform_condition_timedwait((platform_condition_t *)cond->cond,
                                        (platform_mutex_t *)mutex->mutex, timeout_ms);
}

int sync_cond_signal(sync_cond_t *cond)
{
    if (!cond || !cond->initialized)
        return AGENTOS_EINVAL;
    return platform_condition_signal((platform_condition_t *)cond->cond);
}

int sync_cond_broadcast(sync_cond_t *cond)
{
    if (!cond || !cond->initialized)
        return AGENTOS_EINVAL;
    return platform_condition_broadcast((platform_condition_t *)cond->cond);
}

int sync_sem_init(sync_sem_t *sem, uint32_t value)
{
    if (!sem)
        return AGENTOS_EINVAL;
    platform_semaphore_t *s = (platform_semaphore_t *)AGENTOS_MALLOC(sizeof(platform_semaphore_t));
    if (!s)
        return AGENTOS_EINVAL;
    if (platform_semaphore_init(s, value) != 0) {
        AGENTOS_FREE(s);
        return AGENTOS_EINVAL;
    }
    sem->sem = s;
    sem->initialized = true;
    sem->value = value;
    return 0;
}

void sync_sem_destroy(sync_sem_t *sem)
{
    if (!sem || !sem->initialized)
        return;
    platform_semaphore_destroy((platform_semaphore_t *)sem->sem);
    AGENTOS_FREE(sem->sem);
    sem->sem = NULL;
    sem->initialized = false;
    sem->value = 0;
}

int sync_sem_wait(sync_sem_t *sem)
{
    if (!sem || !sem->initialized)
        return AGENTOS_EINVAL;
    int ret = platform_semaphore_wait((platform_semaphore_t *)sem->sem);
    if (ret == 0 && sem->value > 0)
        sem->value--;
    return ret;
}

int sync_sem_timedwait(sync_sem_t *sem, uint32_t timeout_ms)
{
    if (!sem || !sem->initialized)
        return AGENTOS_EINVAL;
    int ret = platform_semaphore_timedwait((platform_semaphore_t *)sem->sem, timeout_ms);
    if (ret == 0 && sem->value > 0)
        sem->value--;
    return ret;
}

int sync_sem_trywait(sync_sem_t *sem)
{
    if (!sem || !sem->initialized)
        return AGENTOS_EINVAL;
    int ret = platform_semaphore_trywait((platform_semaphore_t *)sem->sem);
    if (ret == 0 && sem->value > 0)
        sem->value--;
    return ret;
}

int sync_sem_post(sync_sem_t *sem)
{
    if (!sem || !sem->initialized)
        return AGENTOS_EINVAL;
    int ret = platform_semaphore_post((platform_semaphore_t *)sem->sem);
    if (ret == 0)
        sem->value++;
    return ret;
}

int sync_sem_getvalue(sync_sem_t *sem, uint32_t *value)
{
    if (!sem || !value || !sem->initialized)
        return AGENTOS_EINVAL;
    *value = sem->value;
    return 0;
}

int sync_rwlock_init(sync_rwlock_t *rwlock)
{
    if (!rwlock)
        return AGENTOS_EINVAL;
    platform_rwlock_t *r = (platform_rwlock_t *)AGENTOS_MALLOC(sizeof(platform_rwlock_t));
    if (!r)
        return AGENTOS_EINVAL;
    if (platform_rwlock_init(r) != 0) {
        AGENTOS_FREE(r);
        return AGENTOS_EINVAL;
    }
    rwlock->rwlock = r;
    rwlock->initialized = true;
    return 0;
}

void sync_rwlock_destroy(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return;
    platform_rwlock_destroy((platform_rwlock_t *)rwlock->rwlock);
    AGENTOS_FREE(rwlock->rwlock);
    rwlock->rwlock = NULL;
    rwlock->initialized = false;
}

int sync_rwlock_rdlock(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return AGENTOS_EINVAL;
    return platform_rwlock_rdlock((platform_rwlock_t *)rwlock->rwlock);
}

int sync_rwlock_wrlock(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return AGENTOS_EINVAL;
    return platform_rwlock_wrlock((platform_rwlock_t *)rwlock->rwlock);
}

int sync_rwlock_tryrdlock(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return AGENTOS_EINVAL;
    return platform_rwlock_tryrdlock((platform_rwlock_t *)rwlock->rwlock);
}

int sync_rwlock_trywrlock(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return AGENTOS_EINVAL;
    return platform_rwlock_trywrlock((platform_rwlock_t *)rwlock->rwlock);
}

int sync_rwlock_unlock(sync_rwlock_t *rwlock)
{
    if (!rwlock || !rwlock->initialized)
        return AGENTOS_EINVAL;
    return platform_rwlock_unlock((platform_rwlock_t *)rwlock->rwlock);
}