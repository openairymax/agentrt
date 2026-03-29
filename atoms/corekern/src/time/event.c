/**
 * @file event.c
 * @brief 事件同步与事件循环实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "time.h"
#include "task.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

struct agentos_event {
    volatile int signaled;
    agentos_mutex_t* mutex;
    agentos_cond_t* cond;
};

static volatile int eventloop_running = 0;

agentos_event_t* agentos_event_create(void) {
    agentos_event_t* ev = (agentos_event_t*)AGENTOS_CALLOC(1, sizeof(agentos_event_t));
    if (!ev) return NULL;

    ev->signaled = 0;
    ev->mutex = agentos_mutex_create();
    ev->cond = agentos_cond_create();

    if (!ev->mutex || !ev->cond) {
        if (ev->mutex) agentos_mutex_destroy(ev->mutex);
        if (ev->cond) agentos_cond_destroy(ev->cond);
        AGENTOS_FREE(ev);
        return NULL;
    }
    return ev;
}

agentos_error_t agentos_event_wait(agentos_event_t* event, uint32_t timeout_ms) {
    if (!event) return AGENTOS_EINVAL;

    agentos_mutex_lock(event->mutex);
    if (event->signaled) {
        event->signaled = 0;
        agentos_mutex_unlock(event->mutex);
        return AGENTOS_SUCCESS;
    }

    agentos_error_t err = agentos_cond_wait(event->cond, event->mutex, timeout_ms);
    if (err == AGENTOS_SUCCESS) {
        event->signaled = 0;
    }
    agentos_mutex_unlock(event->mutex);
    return err;
}

agentos_error_t agentos_event_signal(agentos_event_t* event) {
    if (!event) return AGENTOS_EINVAL;
    agentos_mutex_lock(event->mutex);
    event->signaled = 1;
    agentos_mutex_unlock(event->mutex);
    agentos_cond_broadcast(event->cond);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_event_reset(agentos_event_t* event) {
    if (!event) return AGENTOS_EINVAL;
    event->signaled = 0;
    return AGENTOS_SUCCESS;
}

void agentos_event_destroy(agentos_event_t* event) {
    if (!event) return;
    if (event->mutex) agentos_mutex_destroy(event->mutex);
    if (event->cond) agentos_cond_destroy(event->cond);
    AGENTOS_FREE(event);
}

agentos_error_t agentos_time_eventloop_init(void) {
    return AGENTOS_SUCCESS;
}

void agentos_time_eventloop_run(void) {
    eventloop_running = 1;

    while (eventloop_running) {
        agentos_time_timer_process();
        agentos_task_yield();
    }
}

void agentos_time_eventloop_stop(void) {
    eventloop_running = 0;
}
