/**
 * @file timer.c
 * @brief 定时器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt_time.h"
#include "atomic_compat.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>
#include "error_compat.h"
#include "error.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


typedef struct agentrt_timer {
    agentrt_timer_callback_t callback;
    void *userdata;
    uint32_t interval_ms;
    int one_shot;
    int active;
    uint64_t next_fire_ns;
    struct agentrt_timer *next;
} agentrt_timer_t;

static agentrt_timer_t *timer_list = NULL;
static agentrt_mutex_t *timer_lock = NULL;
static atomic_int timer_processing = 0;

agentrt_timer_t *agentrt_timer_create(agentrt_timer_callback_t callback, void *userdata)
{

    if (!callback) {
        AGENTRT_LOG_ERROR("agentrt_timer_create: null callback");
        return NULL;
    }

    agentrt_timer_t *timer = (agentrt_timer_t *)AGENTRT_CALLOC(1, sizeof(agentrt_timer_t));
    if (!timer) {
        AGENTRT_LOG_ERROR("agentrt_timer_create: calloc failed, ENOMEM");
        return NULL;
    }

    timer->callback = callback;
    timer->userdata = userdata;
    return timer;
}

agentrt_error_t agentrt_timer_start(agentrt_timer_t *timer, uint32_t interval_ms, int one_shot)
{

    if (!timer || interval_ms == 0) {
        AGENTRT_LOG_ERROR("agentrt_timer_start: invalid parameter, timer=%p interval_ms=%u", (void *)timer, interval_ms);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (!timer_lock) {
        agentrt_mutex_t *new_lock = agentrt_mutex_create();
        if (!new_lock) {
            AGENTRT_LOG_ERROR("agentrt_timer_start: mutex create failed, ENOMEM");
            ATM_RET_ERR(AGENTRT_ENOMEM);
        }

        agentrt_mutex_t *expected = NULL;
        if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&timer_lock, (void **)&expected,
                                                (void *)new_lock, memory_order_seq_cst,
                                                memory_order_seq_cst)) {
            agentrt_mutex_free(new_lock);
        }
    }

    agentrt_mutex_lock(timer_lock);

    if (timer->active) {
        agentrt_timer_t **pp = &timer_list;
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
    timer->next_fire_ns = agentrt_time_monotonic_ns() + (uint64_t)interval_ms * 1000000ULL;

    timer->next = timer_list;
    timer_list = timer;

    agentrt_mutex_unlock(timer_lock);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_timer_stop(agentrt_timer_t *timer)
{
    if (!timer) {
        AGENTRT_LOG_ERROR("agentrt_timer_stop: null timer");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (!timer_lock)
        return AGENTRT_SUCCESS;

    agentrt_mutex_lock(timer_lock);

    agentrt_timer_t **pp = &timer_list;
    while (*pp) {
        if (*pp == timer) {
            *pp = timer->next;
            timer->next = NULL;
            timer->active = 0;
            break;
        }
        pp = &(*pp)->next;
    }

    agentrt_mutex_unlock(timer_lock);
    return AGENTRT_SUCCESS;
}

void agentrt_timer_destroy(agentrt_timer_t *timer)
{
    if (!timer)
        return;

    if (!timer_lock) {
        AGENTRT_FREE(timer);
        return;
    }

    agentrt_mutex_lock(timer_lock);

    agentrt_timer_t **pp = &timer_list;
    while (*pp) {
        if (*pp == timer) {
            *pp = timer->next;
            timer->next = NULL;
            timer->active = 0;
            break;
        }
        pp = &(*pp)->next;
    }

    agentrt_mutex_unlock(timer_lock);

    AGENTRT_FREE(timer);
}

void agentrt_time_timer_process(void)
{
    if (!timer_lock)
        return;

    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&timer_processing, &expected, 1,
                                                 memory_order_seq_cst, memory_order_seq_cst)) {
        return;
    }

    typedef struct {
        agentrt_timer_callback_t callback;
        void *userdata;
        agentrt_timer_t *source;
        int is_one_shot;
    } fire_entry_t;

    int capacity = 64;
    fire_entry_t *to_fire = (fire_entry_t *)AGENTRT_CALLOC(capacity, sizeof(fire_entry_t));
    if (!to_fire) {
        AGENTRT_LOG_ERROR("agentrt_time_timer_process: calloc failed for fire entries, capacity=%d", capacity);
        atomic_store_explicit(&timer_processing, 0, memory_order_seq_cst);
        return;
    }
    int fire_count = 0;

    agentrt_mutex_lock(timer_lock);

    uint64_t now = agentrt_time_monotonic_ns();
    agentrt_timer_t *timer = timer_list;

    while (timer) {
        agentrt_timer_t *next = timer->next;

        if (timer->active && now >= timer->next_fire_ns) {
            if (fire_count >= capacity) {
                int new_capacity = capacity * 2;
                fire_entry_t *new_buf =
                    (fire_entry_t *)AGENTRT_REALLOC(to_fire, new_capacity * sizeof(fire_entry_t));
                if (!new_buf) {
                    AGENTRT_LOG_WARN("agentrt_time_timer_process: realloc failed, fire_count=%d new_capacity=%d", fire_count, new_capacity);
                    break;
                }
                to_fire = new_buf;
                capacity = new_capacity;
                __builtin_memset(to_fire + fire_count, 0, (capacity - fire_count) * sizeof(fire_entry_t));
            }

            to_fire[fire_count].callback = timer->callback;
            to_fire[fire_count].userdata = timer->userdata;
            to_fire[fire_count].source = timer;
            to_fire[fire_count].is_one_shot = timer->one_shot;
            fire_count++;

            if (timer->one_shot) {
                timer->active = 0;
            } else {
                timer->next_fire_ns = now + (uint64_t)timer->interval_ms * 1000000ULL;
            }
        }

        timer = next;
    }

    agentrt_mutex_unlock(timer_lock);

    for (int i = 0; i < fire_count; i++) {
        to_fire[i].callback(to_fire[i].userdata);
    }

    AGENTRT_FREE(to_fire);
    atomic_store_explicit(&timer_processing, 0, memory_order_seq_cst);
}

void agentrt_time_timer_cleanup(void)
{
    if (!timer_lock)
        return;

    agentrt_mutex_lock(timer_lock);

    while (timer_list) {
        agentrt_timer_t *timer = timer_list;
        timer_list = timer->next;
        AGENTRT_FREE(timer);
    }

    agentrt_mutex_unlock(timer_lock);

    agentrt_mutex_free(timer_lock);
    timer_lock = NULL;
}
