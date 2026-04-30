/**
 * @file sync.c
 * @brief 同步原语实现（基于平台原生线程原语）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "task.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)


/* agentos_mutex_t 在 platform.h 中定义为 agentos_mutex_t */
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
        agentos_mutex_destroy(mutex);
    }
}

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
    if (cond) {
        AGENTOS_FREE(cond);
    }
}

#else

#include <time.h>
#include <errno.h>
#include "platform.h"
#include <stdint.h>

/* agentos_mutex_t 在 platform.h 中定义为 agentos_mutex_t */
/* agentos_cond_t 在 platform.h 中定义为 agentos_cond_t */

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* mutex = (agentos_mutex_t*)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (!mutex) return NULL;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (agentos_mutex_init(mutex) != 0) {
        pthread_mutexattr_destroy(&attr);
        AGENTOS_FREE(mutex);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return mutex;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    if (mutex) {
        agentos_mutex_destroy(mutex);
    }
}

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
    if (cond) {
        agentos_cond_destroy(cond);
    }
}

#endif
