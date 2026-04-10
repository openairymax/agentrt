/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_condition.c
 * @brief 条件变量实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"
#include <time.h>

sync_result_t sync_condition_create(sync_condition_t* condition,
                                 const sync_attr_t* attr) {
    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_condition* c = (struct sync_condition*)AGENTOS_CALLOC(
        1, sizeof(struct sync_condition));
    if (c == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    c->type = SYNC_TYPE_CONDITION;
    if (attr != NULL && attr->name != NULL) {
        c->name = sync_internal_strdup(attr->name);
    }
    memset(&c->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    InitializeConditionVariable(&c->cond);
#else
    int result = pthread_cond_init(&c->cond, NULL);
    if (result != 0) {
        AGENTOS_FREE(c->name);
        AGENTOS_FREE(c);
        return sync_internal_posix_error_to_result(result);
    }
#endif

    c->initialized = true;
    *condition = c;
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_destroy(sync_condition_t condition) {
    if (condition == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!condition->initialized) {
        AGENTOS_FREE(condition->name);
        AGENTOS_FREE(condition);
        return SYNC_SUCCESS;
    }

#ifndef _WIN32
    pthread_cond_destroy(&condition->cond);
#endif

    AGENTOS_FREE(condition->name);
    AGENTOS_FREE(condition);
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_wait(sync_condition_t condition,
                               sync_mutex_t mutex,
                               const sync_timeout_t* timeout) {
    if (condition == NULL || mutex == NULL || !condition->initialized) {
        return SYNC_ERROR_INVALID;
    }

    int64_t start_time = 0;
    if (timeout != NULL && timeout->timeout_ms > 0) {
        start_time = (int64_t)clock();
    }

#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    BOOL result = SleepConditionVariableCS(&condition->cond, &mutex->mutex, wait_ms);
    if (!result) {
        if (GetLastError() == ERROR_TIMEOUT) {
            sync_internal_update_stats_timeout(&condition->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int rc;
    if (timeout == NULL || timeout->timeout_ms == 0) {
        rc = pthread_cond_wait(&condition->cond, &mutex->mutex);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout->timeout_ms / 1000;
        ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        rc = pthread_cond_timedwait(&condition->cond, &mutex->mutex, &ts);
        if (rc == ETIMEDOUT) {
            sync_internal_update_stats_timeout(&condition->stats);
            return SYNC_ERROR_TIMEOUT;
        }
        if (rc != 0) {
            return sync_internal_posix_error_to_result(rc);
        }
    }
#endif

    sync_internal_update_stats_lock(&condition->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_condition_signal(sync_condition_t condition) {
    if (condition == NULL || !condition->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    WakeConditionVariable(&condition->cond);
#else
    int rc = pthread_cond_signal(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    return SYNC_SUCCESS;
}

sync_result_t sync_condition_broadcast(sync_condition_t condition) {
    if (condition == NULL || !condition->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    WakeAllConditionVariable(&condition->cond);
#else
    int rc = pthread_cond_broadcast(&condition->cond);
    if (rc != 0) {
        return sync_internal_posix_error_to_result(rc);
    }
#endif

    return SYNC_SUCCESS;
}
