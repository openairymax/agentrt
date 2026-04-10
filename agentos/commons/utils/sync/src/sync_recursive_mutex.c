/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_recursive_mutex.c
 * @brief 递归互斥锁实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <time.h>

sync_result_t sync_recursive_mutex_create(sync_recursive_mutex_t* mutex,
                                         const sync_attr_t* attr) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_recursive_mutex* m = (struct sync_recursive_mutex*)AGENTOS_CALLOC(
        1, sizeof(struct sync_recursive_mutex));
    if (m == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    m->type = SYNC_TYPE_RECURSIVE_MUTEX;
    m->recursive_count = 0;
    if (attr != NULL && attr->name != NULL) {
        m->name = sync_internal_strdup(attr->name);
    }
    memset(&m->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeCriticalSection(&m->mutex);
#else
    pthread_mutexattr_t attr_mutex;
    pthread_mutexattr_init(&attr_mutex);
    pthread_mutexattr_settype(&attr_mutex, PTHREAD_MUTEX_RECURSIVE);
    int result = pthread_mutex_init(&m->mutex, &attr_mutex);
    pthread_mutexattr_destroy(&attr_mutex);
    if (result != 0) {
        AGENTOS_FREE(m->name);
        AGENTOS_FREE(m);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    m->initialized = true;
    *mutex = m;
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_destroy(sync_recursive_mutex_t mutex) {
    if (mutex == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!mutex->initialized) {
        AGENTOS_FREE(mutex->name);
        AGENTOS_FREE(mutex);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    DeleteCriticalSection(&mutex->mutex);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif

    AGENTOS_FREE(mutex->name);
    AGENTOS_FREE(mutex);
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_lock(sync_recursive_mutex_t mutex,
                                       const sync_timeout_t* timeout) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

    int64_t start_time = 0;
    if (timeout != NULL && timeout->timeout_ms > 0) {
        start_time = (int64_t)clock();
    }

#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    DWORD result = WaitForSingleObject(mutex->mutex, wait_ms);
    if (result == WAIT_TIMEOUT) {
        sync_internal_update_stats_timeout(&mutex->stats);
        return SYNC_ERROR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = pthread_mutex_lock(&mutex->mutex);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        rc = pthread_mutex_timedlock(&mutex->mutex, &ts);
        if (rc == ETIMEDOUT) {
            sync_internal_update_stats_timeout(&mutex->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        if (rc != 0) {
            return sync_internal_posix_error_to_result(rc);
        }
    }
#endif

    mutex->recursive_count++;
    int64_t elapsed = 0;
    if (start_time > 0) {
        elapsed = ((int64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    }
    sync_internal_update_stats_lock(&mutex->stats, elapsed);
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_unlock(sync_recursive_mutex_t mutex) {
    if (mutex == NULL || !mutex->initialized) {
        return SYNC_ERROR_INVALID;
    }

    if (mutex->recursive_count > 0) {
        mutex->recursive_count--;
    }

#ifdef _WIN32
    LeaveCriticalSection(&mutex->mutex);
#else
    int rc = pthread_mutex_unlock(&mutex->mutex);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    mutex->stats.unlock_count++;
    return SYNC_SUCCESS;
}

sync_result_t sync_recursive_mutex_get_count(sync_recursive_mutex_t mutex,
                                            size_t* count) {
    if (mutex == NULL || count == NULL) {
        return SYNC_ERROR_INVALID;
    }
    *count = mutex->recursive_count;
    return SYNC_SUCCESS;
}
