/**
 * @file timer.c
 * @brief 定时器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_time.h"
#include "task.h"
#include <stdlib.h>

#include "memory_compat.h"
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
static volatile int timer_processing = 0;

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
            timer->next = NULL;
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

    if (!timer_lock) {
        AGENTOS_FREE(timer);
        return;
    }

    agentos_mutex_lock(timer_lock);

    agentos_timer_t** pp = &timer_list;
    int found = 0;
    while (*pp) {
        if (*pp == timer) {
            *pp = timer->next;
            timer->next = NULL;
            timer->active = 0;
            found = 1;
            break;
        }
        pp = &(*pp)->next;
    }

    agentos_mutex_unlock(timer_lock);

    if (!found) {
        return;
    }

    AGENTOS_FREE(timer);
}

static void remove_timer_from_list(agentos_timer_t* target) {
    agentos_timer_t** pp = &timer_list;
    while (*pp) {
        if (*pp == target) {
            *pp = target->next;
            target->next = NULL;
            break;
        }
        pp = &(*pp)->next;
    }
}

void agentos_time_timer_process(void) {
    if (!timer_lock) return;

    if (__sync_val_compare_and_swap(&timer_processing, 0, 1) != 0) {
        return;
    }

    typedef struct {
        agentos_timer_callback_t callback;
        void* userdata;
        agentos_timer_t* source;
        int is_one_shot;
    } fire_entry_t;

    int capacity = 64;
    fire_entry_t* to_fire = (fire_entry_t*)AGENTOS_CALLOC(capacity, sizeof(fire_entry_t));
    if (!to_fire) {
        __sync_fetch_and_and(&timer_processing, 0);
        return;
    }
    int fire_count = 0;

    agentos_mutex_lock(timer_lock);

    uint64_t now = agentos_time_monotonic_ns();
    agentos_timer_t* timer = timer_list;

    while (timer) {
        agentos_timer_t* next = timer->next;

        if (timer->active && now >= timer->next_fire_ns) {
            if (fire_count >= capacity) {
                int new_capacity = capacity * 2;
                fire_entry_t* new_buf = (fire_entry_t*)AGENTOS_REALLOC(to_fire, new_capacity * sizeof(fire_entry_t));
                if (!new_buf) break;
                to_fire = new_buf;
                capacity = new_capacity;
                memset(to_fire + fire_count, 0, (capacity - fire_count) * sizeof(fire_entry_t));
            }

            to_fire[fire_count].callback = timer->callback;
            to_fire[fire_count].userdata = timer->userdata;
            to_fire[fire_count].source = timer;
            to_fire[fire_count].is_one_shot = timer->one_shot;
            fire_count++;

            if (timer->one_shot) {
                remove_timer_from_list(timer);
                timer->active = 0;
            } else {
                timer->next_fire_ns = now + (uint64_t)timer->interval_ms * 1000000ULL;
            }
        }

        timer = next;
    }

    agentos_mutex_unlock(timer_lock);

    for (int i = 0; i < fire_count; i++) {
        to_fire[i].callback(to_fire[i].userdata);

        if (to_fire[i].is_one_shot) {
            AGENTOS_FREE(to_fire[i].source);
        }
    }

    AGENTOS_FREE(to_fire);
    __sync_fetch_and_and(&timer_processing, 0);
}

void agentos_time_timer_cleanup(void) {
    if (!timer_lock) return;

    agentos_mutex_lock(timer_lock);

    while (timer_list) {
        agentos_timer_t* timer = timer_list;
        timer_list = timer->next;
        AGENTOS_FREE(timer);
    }

    agentos_mutex_unlock(timer_lock);

    agentos_mutex_free(timer_lock);
    timer_lock = NULL;
}
