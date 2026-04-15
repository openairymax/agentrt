/**
 * @file sync.c
 * @brief 同步原语实现（基于平台原生线程原语）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

/* agentos_mutex_t 在 platform.h 中定义为 CRITICAL_SECTION */
/* agentos_cond_t 在 platform.h 中定义为 CONDITION_VARIABLE */

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* mutex = (agentos_mutex_t*)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (!mutex) return NULL;
    if (agentos_mutex_init(mutex) != 0) {
        AGENTOS_FREE(mutex);
        return NULL;
    }
    return mutex;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    if (mutex) {
        DeleteCriticalSection(mutex);
        AGENTOS_FREE(mutex);
    }
}

#if 0
int agentos_mutex_lock(agentos_mutex_t* mutex) {
    if (!mutex) return AGENTOS_EINVAL;
    EnterCriticalSection(mutex);
    return AGENTOS_SUCCESS;
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    if (!mutex) return -1;
    return TryEnterCriticalSection(mutex) ? 0 : -1;
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    if (!mutex) return AGENTOS_EINVAL;
    LeaveCriticalSection(mutex);
    return AGENTOS_SUCCESS;
}
#endif

agentos_cond_t* agentos_cond_create(void) {
    agentos_cond_t* cond = (agentos_cond_t*)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (!cond) return NULL;
    if (agentos_cond_init(cond) != 0) {
        AGENTOS_FREE(cond);
        return NULL;
    }
    return cond;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    AGENTOS_FREE(cond);
}

#if 0
agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_EINVAL;
    BOOL ret;
    if (timeout_ms == 0) {
        ret = SleepConditionVariableCS(cond, mutex, INFINITE);
    } else {
        ret = SleepConditionVariableCS(cond, mutex, (DWORD)timeout_ms);
    }
    return ret ? AGENTOS_SUCCESS : AGENTOS_ETIMEDOUT;
}
#endif

#if 0
int agentos_cond_signal(agentos_cond_t* cond) {
    if (!cond) return AGENTOS_EINVAL;
    WakeConditionVariable(cond);
    return AGENTOS_SUCCESS;
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    if (!cond) return AGENTOS_EINVAL;
    WakeAllConditionVariable(cond);
    return AGENTOS_SUCCESS;
}
#endif

#else

#include <pthread.h>
#include <time.h>
#include <errno.h>

/* agentos_mutex_t 在 platform.h 中定义为 pthread_mutex_t */
/* agentos_cond_t 在 platform.h 中定义为 pthread_cond_t */

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* mutex = (agentos_mutex_t*)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (!mutex) return NULL;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        AGENTOS_FREE(mutex);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return mutex;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    if (mutex) {
        pthread_mutex_destroy(mutex);
        AGENTOS_FREE(mutex);
    }
}

#if 0
int agentos_mutex_lock(agentos_mutex_t* mutex) {
    if (!mutex) return AGENTOS_EINVAL;
    int ret = pthread_mutex_lock(mutex);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EBUSY;
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    if (!mutex) return -1;
    return pthread_mutex_trylock(mutex);
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    if (!mutex) return AGENTOS_EINVAL;
    int ret = pthread_mutex_unlock(mutex);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EBUSY;
}
#endif

agentos_cond_t* agentos_cond_create(void) {
    agentos_cond_t* cond = (agentos_cond_t*)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (!cond) return NULL;
    if (pthread_cond_init(cond, NULL) != 0) {
        AGENTOS_FREE(cond);
        return NULL;
    }
    return cond;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    if (cond) {
        pthread_cond_destroy(cond);
        AGENTOS_FREE(cond);
    }
}

#if 0
agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_EINVAL;
    if (timeout_ms == 0) {
        pthread_cond_wait(cond, mutex);
        return AGENTOS_SUCCESS;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    return (ret == ETIMEDOUT) ? AGENTOS_ETIMEDOUT : AGENTOS_SUCCESS;
}
#endif

#if 0
int agentos_cond_signal(agentos_cond_t* cond) {
    if (!cond) return AGENTOS_EINVAL;
    int ret = pthread_cond_signal(cond);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EBUSY;
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    if (!cond) return AGENTOS_EINVAL;
    int ret = pthread_cond_broadcast(cond);
    return (ret == 0) ? AGENTOS_SUCCESS : AGENTOS_EBUSY;
}
#endif

#endif
