/**
 * @file event.c
 * @brief 事件同步与事件循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_time.h"
#include "atomic_compat.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "task.h"

#include <stdlib.h>
#include "error.h"

struct agentos_event {
    atomic_int signaled;
    agentos_mutex_t *mutex;
    agentos_cond_t *cond;
};

static atomic_int eventloop_running = 0;

agentos_event_t *agentos_event_create(void)
{
    agentos_event_t *ev = (agentos_event_t *)AGENTOS_CALLOC(1, sizeof(agentos_event_t));
    if (!ev) return NULL;

    atomic_init(&ev->signaled, 0);
    ev->mutex = agentos_mutex_create();
    ev->cond = agentos_cond_create();

    if (!ev->mutex || !ev->cond) {
        if (ev->mutex)
            agentos_mutex_free(ev->mutex);
        if (ev->cond)
            agentos_cond_free(ev->cond);
        AGENTOS_FREE(ev);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    return ev;
}

agentos_error_t agentos_event_wait(agentos_event_t *event, uint32_t timeout_ms)
{
    if (!event)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(event->mutex);
    if (atomic_load_explicit(&event->signaled, memory_order_acquire)) {
        atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
        agentos_mutex_unlock(event->mutex);
        return AGENTOS_SUCCESS;
    }

    agentos_error_t err = agentos_cond_timedwait(event->cond, event->mutex, timeout_ms);
    if (err == AGENTOS_SUCCESS) {
        atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
    }
    agentos_mutex_unlock(event->mutex);
    return err;
}

agentos_error_t agentos_event_signal(agentos_event_t *event)
{
    if (!event)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(event->mutex);
    atomic_store_explicit(&event->signaled, 1, memory_order_seq_cst);
    agentos_mutex_unlock(event->mutex);
    agentos_cond_broadcast(event->cond);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_event_reset(agentos_event_t *event)
{
    if (!event)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(event->mutex);
    atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
    agentos_mutex_unlock(event->mutex);
    return AGENTOS_SUCCESS;
}

void agentos_event_destroy(agentos_event_t *event)
{
    if (!event)
        return;
    if (event->mutex)
        agentos_mutex_free(event->mutex);
    if (event->cond)
        agentos_cond_free(event->cond);
    AGENTOS_FREE(event);
}

agentos_error_t agentos_time_eventloop_init(void)
{
    atomic_store_explicit(&eventloop_running, 0, memory_order_seq_cst);
    return AGENTOS_SUCCESS;
}

void agentos_time_eventloop_run(void)
{
    atomic_store_explicit(&eventloop_running, 1, memory_order_seq_cst);

    while (atomic_load_explicit(&eventloop_running, memory_order_seq_cst)) {
        agentos_time_timer_process();
        agentos_task_yield();
    }
}

void agentos_time_eventloop_stop(void)
{
    atomic_store_explicit(&eventloop_running, 0, memory_order_seq_cst);
}

void agentos_time_eventloop_cleanup(void)
{
    agentos_time_eventloop_stop();
}
