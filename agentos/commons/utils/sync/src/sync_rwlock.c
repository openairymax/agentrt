/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_rwlock.c
 * @brief 读写锁实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <time.h>

sync_result_t sync_rwlock_create(sync_rwlock_t* rwlock, const sync_attr_t* attr) {
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_rwlock* r = (struct sync_rwlock*)AGENTOS_CALLOC(1, sizeof(struct sync_rwlock));
    if (r == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    r->type = SYNC_TYPE_RWLOCK;
    r->read_count = 0;
    r->is_writer = false;
    if (attr != NULL && attr->name != NULL) {
        r->name = sync_internal_strdup(attr->name);
    }
    memset(&r->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeSRWLock(&r->rwlock);
#else
    pthread_rwlockattr_t attr_rwlock;
    pthread_rwlockattr_init(&attr_rwlock);
    if (attr != NULL && (attr->flags & SYNC_FLAG_SHARED)) {
        pthread_rwlockattr_setpshared(&attr_rwlock, PTHREAD_PROCESS_SHARED);
    }
    int result = pthread_rwlock_init(&r->rwlock, &attr_rwlock);
    pthread_rwlockattr_destroy(&attr_rwlock);
    if (result != 0) {
        AGENTOS_FREE(r->name);
        AGENTOS_FREE(r);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    r->initialized = true;
    *rwlock = r;
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_destroy(sync_rwlock_t rwlock) {
    if (rwlock == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!rwlock->initialized) {
        AGENTOS_FREE(rwlock->name);
        AGENTOS_FREE(rwlock);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    (void)rwlock->rwlock;
#else
    pthread_rwlock_destroy(&rwlock->rwlock);
#endif

    AGENTOS_FREE(rwlock->name);
    AGENTOS_FREE(rwlock);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_read_lock(sync_rwlock_t rwlock,
                                   const sync_timeout_t* timeout) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

    int64_t start_time = 0;
    if (timeout != NULL && timeout->timeout_ms > 0) {
        start_time = (int64_t)clock();
    }

#ifdef _WIN32
    if (timeout == NULL) {
        AcquireSRWLockShared(&rwlock->rwlock);
    } else {
        if (!TryAcquireSRWLockShared(&rwlock->rwlock)) {
            sync_internal_update_stats_timeout(&rwlock->stats);
            return SYNC_ERROR_TIMEOUT;
        }
    }
#else
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = pthread_rwlock_rdlock(&rwlock->rwlock);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        rc = pthread_rwlock_timedrdlock(&rwlock->rwlock, &ts);
        if (rc == ETIMEDOUT) {
            sync_internal_update_stats_timeout(&rwlock->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        if (rc != 0) {
            return sync_internal_posix_error_to_result(rc);
        }
    }
#endif

    rwlock->read_count++;
    rwlock->is_writer = false;
    int64_t elapsed = 0;
    if (start_time > 0) {
        elapsed = ((int64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    }
    sync_internal_update_stats_lock(&rwlock->stats, elapsed);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_try_read_lock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    BOOL result = TryAcquireSRWLockShared(&rwlock->rwlock);
    if (!result) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    rwlock->read_count++;
    sync_internal_update_stats_lock(&rwlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_write_lock(sync_rwlock_t rwlock,
                                    const sync_timeout_t* timeout) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

    int64_t start_time = 0;
    if (timeout != NULL && timeout->timeout_ms > 0) {
        start_time = (int64_t)clock();
    }

#ifdef _WIN32
    if (timeout == NULL) {
        AcquireSRWLockExclusive(&rwlock->rwlock);
    } else {
        if (!TryAcquireSRWLockExclusive(&rwlock->rwlock)) {
            sync_internal_update_stats_timeout(&rwlock->stats);
            return SYNC_ERROR_TIMEOUT;
        }
    }
#else
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = pthread_rwlock_wrlock(&rwlock->rwlock);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        rc = pthread_rwlock_timedwrlock(&rwlock->rwlock, &ts);
        if (rc == ETIMEDOUT) {
            sync_internal_update_stats_timeout(&rwlock->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        if (rc != 0) {
            return sync_internal_posix_error_to_result(rc);
        }
    }
#endif

    rwlock->is_writer = true;
    int64_t elapsed = 0;
    if (start_time > 0) {
        elapsed = ((int64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    }
    sync_internal_update_stats_lock(&rwlock->stats, elapsed);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_try_write_lock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    BOOL result = TryAcquireSRWLockExclusive(&rwlock->rwlock);
    if (!result) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_rwlock_trywrlock(&rwlock->rwlock);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    sync_internal_update_stats_lock(&rwlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_rwlock_unlock(sync_rwlock_t rwlock) {
    if (rwlock == NULL || !rwlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    if (rwlock->is_writer) {
        ReleaseSRWLockExclusive(&rwlock->rwlock);
    } else {
        ReleaseSRWLockShared(&rwlock->rwlock);
    }
#else
    int rc = pthread_rwlock_unlock(&rwlock->rwlock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    if (rwlock->read_count > 0) {
        rwlock->read_count--;
    }
    rwlock->stats.unlock_count++;
    return SYNC_SUCCESS;
}
