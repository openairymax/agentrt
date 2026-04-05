/**
 * @file sync.c
 * @brief 同步原语实现（基于平台原生线程原语）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

struct agentos_mutex {
    CRITICAL_SECTION cs;
    int initialized;
};

struct agentos_cond {
    CONDITION_VARIABLE cv;
};

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* m = (agentos_mutex_t*)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (!m) return NULL;
    InitializeCriticalSection(&m->cs);
    m->initialized = 1;
    return m;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    if (mutex) {
        if (mutex->initialized) {
            DeleteCriticalSection(&mutex->cs);
            mutex->initialized = 0;
        }
        AGENTOS_FREE(mutex);
    }
}

void agentos_mutex_lock(agentos_mutex_t* mutex) {
    if (mutex) EnterCriticalSection(&mutex->cs);
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    if (!mutex) return -1;
    return TryEnterCriticalSection(&mutex->cs) ? 0 : -1;
}

void agentos_mutex_unlock(agentos_mutex_t* mutex) {
    if (mutex) LeaveCriticalSection(&mutex->cs);
}

agentos_cond_t* agentos_cond_create(void) {
    agentos_cond_t* c = (agentos_cond_t*)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (!c) return NULL;
    InitializeConditionVariable(&c->cv);
    return c;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    AGENTOS_FREE(cond);
}

agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_EINVAL;
    BOOL ret;
    if (timeout_ms == 0) {
        ret = SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
    } else {
        ret = SleepConditionVariableCS(&cond->cv, &mutex->cs, (DWORD)timeout_ms);
    }
    return ret ? AGENTOS_SUCCESS : AGENTOS_ETIMEDOUT;
}

void agentos_cond_signal(agentos_cond_t* cond) {
    if (cond) WakeConditionVariable(&cond->cv);
}

void agentos_cond_broadcast(agentos_cond_t* cond) {
    if (cond) WakeAllConditionVariable(&cond->cv);
}

#else

#include <pthread.h>
#include <time.h>
#include <errno.h>

struct agentos_mutex {
    pthread_mutex_t ptm;
    int initialized;
};

struct agentos_cond {
    pthread_cond_t ptc;
};

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* m = (agentos_mutex_t*)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (!m) return NULL;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&m->ptm, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        AGENTOS_FREE(m);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    m->initialized = 1;
    return m;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    if (mutex) {
        if (mutex->initialized) {
            pthread_mutex_destroy(&mutex->ptm);
            mutex->initialized = 0;
        }
        AGENTOS_FREE(mutex);
    }
}

void agentos_mutex_lock(agentos_mutex_t* mutex) {
    if (mutex) pthread_mutex_lock(&mutex->ptm);
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    if (!mutex) return -1;
    return pthread_mutex_trylock(&mutex->ptm);
}

void agentos_mutex_unlock(agentos_mutex_t* mutex) {
    if (mutex) pthread_mutex_unlock(&mutex->ptm);
}

agentos_cond_t* agentos_cond_create(void) {
    agentos_cond_t* c = (agentos_cond_t*)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (!c) return NULL;
    if (pthread_cond_init(&c->ptc, NULL) != 0) {
        AGENTOS_FREE(c);
        return NULL;
    }
    return c;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    if (cond) {
        pthread_cond_destroy(&cond->ptc);
        AGENTOS_FREE(cond);
    }
}

agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_EINVAL;
    if (timeout_ms == 0) {
        pthread_cond_wait(&cond->ptc, &mutex->ptm);
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
    int ret = pthread_cond_timedwait(&cond->ptc, &mutex->ptm, &ts);
    return (ret == ETIMEDOUT) ? AGENTOS_ETIMEDOUT : AGENTOS_SUCCESS;
}

void agentos_cond_signal(agentos_cond_t* cond) {
    if (cond) pthread_cond_signal(&cond->ptc);
}

void agentos_cond_broadcast(agentos_cond_t* cond) {
    if (cond) pthread_cond_broadcast(&cond->ptc);
}

#endif
