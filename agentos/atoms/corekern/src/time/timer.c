/**
 * @file timer.c
 * @brief 定时器实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_time.h"
#include "task.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>

typedef struct agentos_timer {
    agentos_timer_callback_t callback;
    void* userdata;
    uint32_t interval_ms;
    int one_shot;
    int active;
    uint64_t next_fire_ns;
    struct agentos_timer* next;
} agentos_timer_t;

static agentos_timer_t* timer_list = NULL;
static agentos_mutex_t* timer_lock = NULL;

agentos_timer_t* agentos_timer_create(
    agentos_timer_callback_t callback,
    void* userdata) {

    if (!callback) return NULL;

    agentos_timer_t* timer = (agentos_timer_t*)AGENTOS_CALLOC(1, sizeof(agentos_timer_t));
    if (!timer) return NULL;

    timer->callback = callback;
    timer->userdata = userdata;
    return timer;
}

agentos_error_t agentos_timer_start(
    agentos_timer_t* timer,
    uint32_t interval_ms,
    int one_shot) {

    if (!timer || interval_ms == 0) return AGENTOS_EINVAL;

    if (!timer_lock) {
        timer_lock = agentos_mutex_create();
        if (!timer_lock) return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(timer_lock);

    if (timer->active) {
        agentos_timer_t** pp = &timer_list;
        while (*pp) {
            if (*pp == timer) {
                *pp = timer->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }

    timer->interval_ms = interval_ms;
    timer->one_shot = one_shot;
    timer->active = 1;
    timer->next_fire_ns = agentos_time_monotonic_ns() + (uint64_t)interval_ms * 1000000ULL;

    timer->next = timer_list;
    timer_list = timer;

    agentos_mutex_unlock(timer_lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_timer_stop(agentos_timer_t* timer) {
    if (!timer) return AGENTOS_EINVAL;

    if (!timer_lock) return AGENTOS_SUCCESS;

    agentos_mutex_lock(timer_lock);

    agentos_timer_t** pp = &timer_list;
    while (*pp) {
        if (*pp == timer) {
            *pp = timer->next;
            timer->active = 0;
            break;
        }
        pp = &(*pp)->next;
    }

    agentos_mutex_unlock(timer_lock);
    return AGENTOS_SUCCESS;
}

void agentos_timer_destroy(agentos_timer_t* timer) {
    if (!timer) return;
    agentos_timer_stop(timer);
    AGENTOS_FREE(timer);
}

void agentos_time_timer_process(void) {
    if (!timer_lock) return;

    agentos_mutex_lock(timer_lock);

    uint64_t now = agentos_time_monotonic_ns();
    agentos_timer_t* timer = timer_list;

    while (timer) {
        if (timer->active && now >= timer->next_fire_ns) {
            agentos_timer_callback_t cb = timer->callback;
            void* ud = timer->userdata;

            if (timer->one_shot) {
                timer->active = 0;
            } else {
                timer->next_fire_ns = now + (uint64_t)timer->interval_ms * 1000000ULL;
            }

            cb(ud);
        }
        timer = timer->next;
    }

    agentos_mutex_unlock(timer_lock);
}
