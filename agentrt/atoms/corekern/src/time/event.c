/**
 * @file event.c
 * @brief 事件同步与事件循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt_time.h"
#include "atomic_compat.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "task.h"

#include <stdlib.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


struct agentrt_event {
    atomic_int signaled;
    agentrt_mutex_t *mutex;
    agentrt_cond_t *cond;
};

static atomic_int eventloop_running = 0;

agentrt_event_t *agentrt_event_create(void)
{
    agentrt_event_t *ev = (agentrt_event_t *)AGENTRT_CALLOC(1, sizeof(agentrt_event_t));
    if (!ev) return NULL;

    atomic_init(&ev->signaled, 0);
    ev->mutex = agentrt_mutex_create();
    ev->cond = agentrt_cond_create();

    if (!ev->mutex || !ev->cond) {
        if (ev->mutex)
            agentrt_mutex_free(ev->mutex);
        if (ev->cond)
            agentrt_cond_free(ev->cond);
        AGENTRT_FREE(ev);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return ev;
}

agentrt_error_t agentrt_event_wait(agentrt_event_t *event, uint32_t timeout_ms)
{
    if (!event)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agentrt_mutex_lock(event->mutex);
    if (atomic_load_explicit(&event->signaled, memory_order_acquire)) {
        atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
        agentrt_mutex_unlock(event->mutex);
        return AGENTRT_SUCCESS;
    }

    agentrt_error_t err = agentrt_cond_timedwait(event->cond, event->mutex, timeout_ms);
    if (err == AGENTRT_SUCCESS) {
        atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
    }
    agentrt_mutex_unlock(event->mutex);
    return err;
}

agentrt_error_t agentrt_event_signal(agentrt_event_t *event)
{
    if (!event)
        ATM_RET_ERR(AGENTRT_EINVAL);
    agentrt_mutex_lock(event->mutex);
    atomic_store_explicit(&event->signaled, 1, memory_order_seq_cst);
    agentrt_mutex_unlock(event->mutex);
    agentrt_cond_broadcast(event->cond);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_event_reset(agentrt_event_t *event)
{
    if (!event)
        ATM_RET_ERR(AGENTRT_EINVAL);
    agentrt_mutex_lock(event->mutex);
    atomic_store_explicit(&event->signaled, 0, memory_order_seq_cst);
    agentrt_mutex_unlock(event->mutex);
    return AGENTRT_SUCCESS;
}

void agentrt_event_destroy(agentrt_event_t *event)
{
    if (!event)
        return;
    if (event->mutex)
        agentrt_mutex_free(event->mutex);
    if (event->cond)
        agentrt_cond_free(event->cond);
    AGENTRT_FREE(event);
}

agentrt_error_t agentrt_time_eventloop_init(void)
{
    atomic_store_explicit(&eventloop_running, 0, memory_order_seq_cst);
    return AGENTRT_SUCCESS;
}

void agentrt_time_eventloop_run(void)
{
    atomic_store_explicit(&eventloop_running, 1, memory_order_seq_cst);

    while (atomic_load_explicit(&eventloop_running, memory_order_seq_cst)) {
        agentrt_time_timer_process();
        agentrt_task_yield();
    }
}

void agentrt_time_eventloop_stop(void)
{
    atomic_store_explicit(&eventloop_running, 0, memory_order_seq_cst);
}

void agentrt_time_eventloop_cleanup(void)
{
    agentrt_time_eventloop_stop();
}
