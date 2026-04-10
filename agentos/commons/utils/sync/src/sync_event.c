/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_event.c
 * @brief 事件实现
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#include "sync_platform.h"

sync_result_t sync_event_create(sync_event_t* event,
                              bool manual_reset,
                              bool initial_state,
                              const sync_attr_t* attr) {
    if (event == NULL) {
        return SYNC_ERROR_INVALID;
    }

    struct sync_event* e = (struct sync_event*)AGENTOS_CALLOC(
        1, sizeof(struct sync_event));
    if (e == NULL) {
        return SYNC_ERROR_MEMORY;
    }

    e->type = SYNC_TYPE_EVENT;
    e->manual_reset = manual_reset;
    e->signaled = initial_state;
    if (attr != NULL && attr->name != NULL) {
        e->name = sync_internal_strdup(attr->name);
    }
    memset(&e->stats, 0, sizeof(sync_stats_t));

#ifdef _WIN32
    e->event = CreateEvent(NULL, (BOOL)manual_reset, (BOOL)initial_state, NULL);
    if (e->event == NULL) {
        AGENTOS_FREE(e->name);
        AGENTOS_FREE(e);
        return SYNC_ERROR_UNKNOWN;
    }
#else
    int result1 = pthread_mutex_init(&e->mutex, NULL);
    if (result1 != 0) {
        AGENTOS_FREE(e->name);
        AGENTOS_FREE(e);
        return sync_internal_posix_error_to_result(result1);
    }
    int result2 = pthread_cond_init(&e->cond, NULL);
    if (result2 != 0) {
        pthread_mutex_destroy(&e->mutex);
        AGENTOS_FREE(e->name);
        AGENTOS_FREE(e);
        return sync_internal_posix_error_to_result(result2);
    }
#endif

    e->initialized = true;
    *event = e;
    return SYNC_SUCCESS;
}

sync_result_t sync_event_destroy(sync_event_t event) {
    if (event == NULL) {
        return SYNC_ERROR_INVALID;
    }

    if (!event->initialized) {
        AGENTOS_FREE(event->name);
        AGENTOS_FREE(event);
        return SYNC_SUCCESS;
    }

#ifdef _WIN32
    CloseHandle(event->event);
#else
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
#endif

    AGENTOS_FREE(event->name);
    AGENTOS_FREE(event);
    return SYNC_SUCCESS;
}

sync_result_t sync_event_wait(sync_event_t event, const sync_timeout_t* timeout) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    DWORD wait_ms = (timeout == NULL) ? INFINITE : (DWORD)timeout->timeout_ms;
    DWORD result = WaitForSingleObject(event->event, wait_ms);
    if (result == WAIT_TIMEOUT) {
        sync_internal_update_stats_timeout(&event->stats);
        return SYNC_ERROR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0) {
        return SYNC_ERROR_UNKNOWN;
    }
    if (!event->manual_reset) {
        ResetEvent(event->event);
    }
#else
    pthread_mutex_lock(&event->mutex);
    while (!event->signaled) {
        if (timeout == NULL) {
            int rc = pthread_cond_wait(&event->cond, &event->mutex);
            if (rc != 0) {
                pthread_mutex_unlock(&event->mutex);
                return sync_internal_posix_error_to_result(rc);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout->timeout_ms / 1000;
            ts.tv_nsec += (timeout->timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            int rc = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&event->mutex);
                sync_internal_update_stats_timeout(&event->stats);
                return SYNC_ERROR_TIMEOUT;
            }
            if (rc != 0) {
                pthread_mutex_unlock(&event->mutex);
                return sync_internal_posix_error_to_result(rc);
            }
        }
    }
    if (!event->manual_reset) {
        event->signaled = false;
    }
    pthread_mutex_unlock(&event->mutex);
#endif

    sync_internal_update_stats_wait(&event->stats, 0);
    return SYNC_SUCCESS;
}

sync_result_t sync_event_signal(sync_event_t event) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    SetEvent(event->event);
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    pthread_cond_broadcast(&event->cond);
    pthread_mutex_unlock(&event->mutex);
#endif

    return SYNC_SUCCESS;
}

sync_result_t sync_event_reset(sync_event_t event) {
    if (event == NULL || !event->initialized) {
        return SYNC_ERROR_INVALID;
    }

#ifdef _WIN32
    ResetEvent(event->event);
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
#endif

    return SYNC_SUCCESS;
}
