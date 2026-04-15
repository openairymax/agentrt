/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_spinlock.c
 * @brief 自旋锁实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <agentos/check.h>

sync_result_t sync_spinlock_create(sync_spinlock_t* spinlock,
                                  const sync_attr_t* attr) {
    CHECK_NULL_RET(spinlock, SYNC_ERROR_INVALID);

    struct sync_spinlock* s = (struct sync_spinlock*)AGENTOS_CALLOC(
        1, sizeof(struct sync_spinlock));
    CHECK_NULL_RET(s, SYNC_ERROR_MEMORY);

    s->type = SYNC_TYPE_SPINLOCK;
    if (attr != NULL && attr->name != NULL) {
        s->name = sync_internal_strdup(attr->name);
    }
    memset(&s->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    s->lock = 0;
#else
    int result = pthread_spin_init(&s->lock, PTHREAD_PROCESS_PRIVATE);
    if (result != 0) {
        AGENTOS_FREE(s->name);
        AGENTOS_FREE(s);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    s->initialized = true;
    *spinlock = s;
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_destroy(sync_spinlock_t spinlock) {
    CHECK_NULL_RET(spinlock, SYNC_ERROR_INVALID);

    if (!spinlock->initialized) {
        AGENTOS_FREE(spinlock->name);
        AGENTOS_FREE(spinlock);
        return SYNC_SUCCESS;
    }

#ifndef _WIN32
    pthread_spin_destroy(&spinlock->lock);
#endif

    AGENTOS_FREE(spinlock->name);
    AGENTOS_FREE(spinlock);
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_lock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    while (InterlockedExchange(&spinlock->lock, 1) != 0) {
        while (spinlock->lock != 0) {
        }
    }
#else
    int rc = pthread_spin_lock(&spinlock->lock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    sync_internal_update_stats_lock(&spinlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_try_lock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    LONG expected = 0;
    if (InterlockedCompareExchange(&spinlock->lock, 1, expected) != expected) {
        return SYNC_ERROR_BUSY;
    }
#else
    int rc = pthread_spin_trylock(&spinlock->lock);
    if (rc == EBUSY) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    sync_internal_update_stats_lock(&spinlock->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_spinlock_unlock(sync_spinlock_t spinlock) {
    if (spinlock == NULL || !spinlock->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    InterlockedExchange(&spinlock->lock, 0);
#else
    int rc = pthread_spin_unlock(&spinlock->lock);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    spinlock->stats.unlock_count++;
    return SYNC_SUCCESS;
}
