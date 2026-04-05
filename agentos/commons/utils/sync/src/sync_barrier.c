/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_barrier.c
 * @brief 屏障实现
 *
 * @author AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <time.h>

sync_result_t sync_barrier_create(sync_barrier_t* barrier,
                                unsigned int count,
                                const sync_attr_t* attr) {
    if (barrier == NULL || count == 0) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_barrier* b = (struct sync_barrier*)AGENTOS_CALLOC(
        1, sizeof(struct sync_barrier));
    if (b == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    b->type = SYNC_TYPE_BARRIER;
    if (attr != NULL && attr->name != NULL) {
        b->name = sync_internal_strdup(attr->name);
    }
    memset(&b->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeCriticalSection(&b->cs);
    InitializeConditionVariable(&b->cond);
    b->count = count;
    b->current = 0;
    b->generation = 0;
#else
    int result = pthread_barrier_init(&b->barrier, NULL, count);
    if (result != 0) {
        AGENTOS_FREE(b->name);
        AGENTOS_FREE(b);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    b->initialized = true;
    *barrier = b;
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_destroy(sync_barrier_t barrier) {
    if (barrier == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!barrier->initialized) {
        AGENTOS_FREE(barrier->name);
        AGENTOS_FREE(barrier);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    DeleteCriticalSection(&barrier->cs);
#else
    pthread_barrier_destroy(&barrier->barrier);
#endif

    AGENTOS_FREE(barrier->name);
    AGENTOS_FREE(barrier);
    return SYNC_SUCCESS;
}

sync_result_t sync_barrier_wait(sync_barrier_t barrier, const sync_timeout_t* timeout) {
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    barrier->current++;

    if (barrier->current >= barrier->count) {
        barrier->current = 0;
        barrier->generation++;
        WakeAllConditionVariable(&barrier->cond);
        LeaveCriticalSection(&barrier->cs);
        return SYNC_SUCCESS;
    }

    unsigned int gen = barrier->generation;
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;

    while (barrier->generation == gen) {
        if (!SleepConditionVariableCS(&barrier->cond, &barrier->cs, wait_ms)) {
            if (GetLastError() == ERROR_TIMEOUT) {
                LeaveCriticalSection(&barrier->cs);
                return SYNC_ERROR_TIMEOUT;
            }
        }
    }
    LeaveCriticalSection(&barrier->cs);
    return SYNC_SUCCESS;
#else
    int rc = pthread_barrier_wait(&barrier->barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        return SYNC_SUCCESS;
    } else if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
    return SYNC_SUCCESS;
#endif
}

sync_result_t sync_barrier_reset(sync_barrier_t barrier, unsigned int new_count) {
    if (barrier == NULL || !barrier->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    EnterCriticalSection(&barrier->cs);
    if (new_count > 0) {
        barrier->count = new_count;
    }
    barrier->current = 0;
    barrier->generation++;
    LeaveCriticalSection(&barrier->cs);
    return SYNC_SUCCESS;
#else
    (void)new_count;
    return SYNC_ERROR_INVALID;
#endif
}
