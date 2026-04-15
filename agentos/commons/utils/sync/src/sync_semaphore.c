/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_semaphore.c
 * @brief 信号量实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <agentos/check.h>
#include <time.h>

sync_result_t sync_semaphore_create(sync_semaphore_t* semaphore,
                                   unsigned int initial_value,
                                   unsigned int max_value,
                                   const sync_attr_t* attr) {
    CHECK_NULL_RET(semaphore, SYNC_ERROR_INVALID);

    struct sync_semaphore* s = (struct sync_semaphore*)AGENTOS_CALLOC(
        1, sizeof(struct sync_semaphore));
    CHECK_NULL_RET(s, SYNC_ERROR_MEMORY);

    s->type = SYNC_TYPE_SEMAPHORE;
    s->max_value = max_value;
    if (attr != NULL && attr->name != NULL) {
        s->name = sync_internal_strdup(attr->name);
    }
    memset(&s->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    s->semaphore = CreateSemaphore(NULL, initial_value, max_value, NULL);
    if (s->semaphore == NULL) {
        AGENTOS_FREE(s->name);
        AGENTOS_FREE(s);
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int result = sem_init(&s->semaphore, 0, initial_value);
    if (result != 0) {
        AGENTOS_FREE(s->name);
        AGENTOS_FREE(s);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    s->initialized = true;
    *semaphore = s;
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_destroy(sync_semaphore_t semaphore) {
    if (semaphore == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!semaphore->initialized) {
        AGENTOS_FREE(semaphore->name);
        AGENTOS_FREE(semaphore);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    CloseHandle(semaphore->semaphore);
#else
    sem_destroy(&semaphore->semaphore);
#endif

    AGENTOS_FREE(semaphore->name);
    AGENTOS_FREE(semaphore);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_wait(sync_semaphore_t semaphore,
                                const sync_timeout_t* timeout) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }

    int64_t start_time = 0;
    if (timeout != NULL && timeout->timeout_ms > 0) {
        start_time = (int64_t)clock();
    }

#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    DWORD result = WaitForSingleObject(semaphore->semaphore, wait_ms);
    if (result == WAIT_TIMEOUT) {
        sync_internal_update_stats_timeout(&semaphore->stats);
        return SYNC_ERROR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = sem_wait(&semaphore->semaphore);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        rc = sem_timedwait(&semaphore->semaphore, &ts);
        if (rc == -1 && errno == ETIMEDOUT) {
            sync_internal_update_stats_timeout(&semaphore->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        if (rc != 0) {
            return sync_internal_posix_error_to_result(errno);
        }
    }
#endif

    int64_t elapsed = 0;
    if (start_time > 0) {
        elapsed = ((int64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    }
    sync_internal_update_stats_wait(&semaphore->stats, elapsed);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_try_wait(sync_semaphore_t semaphore) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    DWORD result = WaitForSingleObject(semaphore->semaphore, 0);
    if (result == WAIT_TIMEOUT) {
        return SYNC_ERROR_BUSY;
    }
    if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc = sem_trywait(&semaphore->semaphore);
    if (rc == -1 && errno == EAGAIN) {
        return SYNC_ERROR_BUSY;
    }
    if (rc != 0) {
        return sync_internal_posix_error_to_result(errno);
    }
#endif

    sync_internal_update_stats_wait(&semaphore->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_post(sync_semaphore_t semaphore) {
    if (semaphore == NULL || !semaphore->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    if (!ReleaseSemaphore(semaphore->semaphore, 1, NULL)) {
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc = sem_post(&semaphore->semaphore);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    return SYNC_SUCCESS;
}

sync_result_t sync_semaphore_get_value(sync_semaphore_t semaphore,
                                      unsigned int* value) {
    if (semaphore == NULL || value == NULL) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    LONG val;
    if (!ReleaseSemaphore(semaphore->semaphore, 0, &val)) {
        return SYNC_ERROR_UNKNOWN;
    }
    *value = (unsigned int)val;
#else
    int val = sem_getvalue(&semaphore->semaphore, &val);
    if (val != 0) {
        return sync_internal_posix_error_to_result(val);
    }
    *value = (unsigned int)val;
#endif

    return SYNC_SUCCESS;
}
