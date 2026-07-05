/**
 * @file sync_platform.c
 * @brief Platform abstraction layer implementation for sync primitives
 *
 * Provides cross-platform implementations of mutex, condition variable,
 * semaphore, rwlock, spinlock, barrier, and event primitives.
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sync_platform.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <synchapi.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

#include "error.h"

#define SYNC_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", \
         agentrt_error_str(c)); return (c); } while(0)



int platform_mutex_init(platform_mutex_t *mutex)
{
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

int platform_mutex_destroy(platform_mutex_t *mutex)
{
#ifdef _WIN32
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

int platform_mutex_lock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

int platform_mutex_unlock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

int platform_mutex_trylock(platform_mutex_t *mutex)
{
#ifdef _WIN32
    return TryEnterCriticalSection(mutex) ? 0 : -1;
#else
    return pthread_mutex_trylock(mutex);
#endif
}

int platform_recursive_mutex_init(platform_recursive_mutex_t *mutex)
{
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
#endif
}

int platform_recursive_mutex_destroy(platform_recursive_mutex_t *mutex)
{
#ifdef _WIN32
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex);
#endif
}

int platform_recursive_mutex_lock(platform_recursive_mutex_t *mutex)
{
#ifdef _WIN32
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex);
#endif
}

int platform_recursive_mutex_unlock(platform_recursive_mutex_t *mutex)
{
#ifdef _WIN32
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex);
#endif
}

int platform_rwlock_init(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    InitializeSRWLock(rwlock);
    return 0;
#else
    return pthread_rwlock_init(rwlock, NULL);
#endif
}

int platform_rwlock_destroy(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    (void)rwlock;
    return 0;
#else
    return pthread_rwlock_destroy(rwlock);
#endif
}

int platform_rwlock_rdlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    AcquireSRWLockShared(rwlock);
    return 0;
#else
    return pthread_rwlock_rdlock(rwlock);
#endif
}

int platform_rwlock_wrlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    AcquireSRWLockExclusive(rwlock);
    return 0;
#else
    return pthread_rwlock_wrlock(rwlock);
#endif
}

int platform_rwlock_tryrdlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    return TryAcquireSRWLockShared(rwlock) ? 0 : -1;
#else
    return pthread_rwlock_tryrdlock(rwlock);
#endif
}

int platform_rwlock_trywrlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    return TryAcquireSRWLockExclusive(rwlock) ? 0 : -1;
#else
    return pthread_rwlock_trywrlock(rwlock);
#endif
}

int platform_rwlock_unlock(platform_rwlock_t *rwlock)
{
#ifdef _WIN32
    void *state = *(void **)rwlock;
    if (state && ((uintptr_t)state & 0x1) == 0) {
        ReleaseSRWLockShared(rwlock);
    } else {
        ReleaseSRWLockExclusive(rwlock);
    }
    return 0;
#else
    return pthread_rwlock_unlock(rwlock);
#endif
}

int platform_spinlock_init(platform_spinlock_t *spinlock)
{
#ifdef _WIN32
    *spinlock = 0;
    return 0;
#else
    return pthread_spin_init(spinlock, PTHREAD_PROCESS_PRIVATE);
#endif
}

int platform_spinlock_destroy(platform_spinlock_t *spinlock)
{
#ifdef _WIN32
    *spinlock = 0;
    return 0;
#else
    return pthread_spin_destroy(spinlock);
#endif
}

int platform_spinlock_lock(platform_spinlock_t *spinlock)
{
#ifdef _WIN32
    int expected = 0;
    while (!atomic_compare_exchange_strong_explicit(spinlock, &expected, 1, memory_order_acquire,
                                                    memory_order_relaxed)) {
        expected = 0;
        SwitchToThread();
    }
    return 0;
#else
    return pthread_spin_lock(spinlock);
#endif
}

int platform_spinlock_unlock(platform_spinlock_t *spinlock)
{
#ifdef _WIN32
    atomic_store_explicit(spinlock, 0, memory_order_release);
    return 0;
#else
    return pthread_spin_unlock(spinlock);
#endif
}

int platform_semaphore_init(platform_semaphore_t *semaphore, unsigned int value)
{
#ifdef _WIN32
    *semaphore = CreateSemaphore(NULL, (LONG)value, (LONG)0x7FFFFFFF, NULL);
    return (*semaphore != NULL) ? 0 : -1;
#else
    return sem_init(semaphore, 0, value);
#endif
}

int platform_semaphore_destroy(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    CloseHandle(*semaphore);
    return 0;
#else
    return sem_destroy(semaphore);
#endif
}

int platform_semaphore_wait(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    return (WaitForSingleObject(*semaphore, INFINITE) == WAIT_OBJECT_0) ? 0 : -1;
#else
    return (sem_wait(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_post(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    return ReleaseSemaphore(*semaphore, 1, NULL) ? 0 : -1;
#else
    return (sem_post(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_timedwait(platform_semaphore_t *semaphore, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(*semaphore, timeout_ms);
    return (ret == WAIT_OBJECT_0) ? 0 : -1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return (sem_timedwait(semaphore, &ts) == 0) ? 0 : -1;
#endif
}

int platform_semaphore_trywait(platform_semaphore_t *semaphore)
{
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(*semaphore, 0);
    return (ret == WAIT_OBJECT_0) ? 0 : -1;
#else
    return (sem_trywait(semaphore) == 0) ? 0 : -1;
#endif
}

int platform_condition_init(platform_condition_t *cond)
{
#ifdef _WIN32
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL);
#endif
}

int platform_condition_destroy(platform_condition_t *cond)
{
#ifdef _WIN32
    (void)cond;
    return 0;
#else
    return pthread_cond_destroy(cond);
#endif
}

int platform_condition_wait(platform_condition_t *cond, platform_mutex_t *mutex)
{
#ifdef _WIN32
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
#else
    return pthread_cond_wait(cond, mutex);
#endif
}

int platform_condition_timedwait(platform_condition_t *cond, platform_mutex_t *mutex, uint32_t timeout_ms)
{
#ifdef _WIN32
    return SleepConditionVariableCS(cond, mutex, timeout_ms) ? 0 : -1;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(cond, mutex, &ts);
#endif
}

int platform_condition_signal(platform_condition_t *cond)
{
#ifdef _WIN32
    WakeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_signal(cond);
#endif
}

int platform_condition_broadcast(platform_condition_t *cond)
{
#ifdef _WIN32
    WakeAllConditionVariable(cond);
    return 0;
#else
    return pthread_cond_broadcast(cond);
#endif
}

int platform_barrier_init(platform_barrier_t *barrier, unsigned int count)
{
#ifdef _WIN32
    InitializeCriticalSection(&barrier->cs);
    InitializeConditionVariable(&barrier->cond);
    barrier->count = count;
    barrier->current = 0;
    barrier->generation = 0;
    return 0;
#else
    return pthread_barrier_init(barrier, NULL, count);
#endif
}

int platform_barrier_destroy(platform_barrier_t *barrier)
{
#ifdef _WIN32
    DeleteCriticalSection(&barrier->cs);
    return 0;
#else
    return pthread_barrier_destroy(barrier);
#endif
}

int platform_barrier_wait(platform_barrier_t *barrier)
{
#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    unsigned int gen = barrier->generation;
    barrier->current++;
    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        WakeAllConditionVariable(&barrier->cond);
        LeaveCriticalSection(&barrier->cs);
        return 1;
    }
    while (gen == barrier->generation) {
        SleepConditionVariableCS(&barrier->cond, &barrier->cs, INFINITE);
    }
    LeaveCriticalSection(&barrier->cs);
    return 0;
#else
    int ret = pthread_barrier_wait(barrier);
    return (ret == PTHREAD_BARRIER_SERIAL_THREAD) ? 1 : (ret == 0 ? 0 : -1);
#endif
}

int platform_event_init(platform_event_t *event, bool manual_reset)
{
#ifdef _WIN32
    *event = CreateEvent(NULL, manual_reset ? TRUE : FALSE, FALSE, NULL);
    return (*event != NULL) ? 0 : -1;
#else
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);
    event->signaled = false;
    event->manual_reset = manual_reset;
    return 0;
#endif
}

int platform_event_destroy(platform_event_t *event)
{
#ifdef _WIN32
    CloseHandle(*event);
    return 0;
#else
    pthread_mutex_destroy(&event->mutex);
    pthread_cond_destroy(&event->cond);
    return 0;
#endif
}

int platform_event_set(platform_event_t *event)
{
#ifdef _WIN32
    return SetEvent(*event) ? 0 : -1;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);
    } else {
        pthread_cond_signal(&event->cond);
    }
    pthread_mutex_unlock(&event->mutex);
    return 0;
#endif
}

int platform_event_reset(platform_event_t *event)
{
#ifdef _WIN32
    return ResetEvent(*event) ? 0 : -1;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
    return 0;
#endif
}

int platform_event_wait(platform_event_t *event, uint64_t timeout_ms)
{
#ifdef _WIN32
    DWORD result = WaitForSingleObject(*event, (DWORD)timeout_ms);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#else
    pthread_mutex_lock(&event->mutex);
    while (!event->signaled) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&event->mutex);
            SYNC_RET_ERR(AGENTRT_EINVAL);
        }
        if (timeout_ms == (uint64_t)-1) {
            pthread_cond_wait(&event->cond, &event->mutex);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            int ret = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&event->mutex);
                SYNC_RET_ERR(AGENTRT_EINVAL);
            }
        }
    }
    if (!event->manual_reset) {
        event->signaled = false;
    }
    pthread_mutex_unlock(&event->mutex);
    return 0;
#endif
}

uint64_t platform_get_timestamp_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

uint64_t platform_get_thread_id(void)
{
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}
