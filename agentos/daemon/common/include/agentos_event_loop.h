#ifndef AGENTOS_EVENT_LOOP_H
#define AGENTOS_EVENT_LOOP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGENTOS_EVENT_LOOP_MAX_EVENTS 1024
#define AGENTOS_EVENT_LOOP_MAX_TIMERS 64

typedef enum {
    AGENTOS_EVENT_TYPE_READ   = 1,
    AGENTOS_EVENT_TYPE_WRITE  = 2,
    AGENTOS_EVENT_TYPE_TIMER  = 4,
    AGENTOS_EVENT_TYPE_SIGNAL = 8
} agentos_event_type_t;

typedef struct agentos_event_loop agentos_event_loop_t;

typedef int (*agentos_event_callback_t)(int fd, uint32_t events, void* user_data);

typedef void (*agentos_timer_callback_t)(agentos_event_loop_t* loop, uint64_t timer_id, void* user_data);

agentos_event_loop_t* agentos_event_loop_create(int max_events);

void agentos_event_loop_destroy(agentos_event_loop_t* loop);

int agentos_event_loop_add_fd(agentos_event_loop_t* loop, int fd, uint32_t events,
                               agentos_event_callback_t cb, void* user_data);

int agentos_event_loop_add_fd_lt(agentos_event_loop_t* loop, int fd, uint32_t events,
                                  agentos_event_callback_t cb, void* user_data);

int agentos_event_loop_mod_fd(agentos_event_loop_t* loop, int fd, uint32_t events);

void agentos_event_loop_remove_fd(agentos_event_loop_t* loop, int fd);

uint64_t agentos_event_loop_add_timer(agentos_event_loop_t* loop, uint64_t interval_ms,
                                       agentos_timer_callback_t cb, void* user_data);

int agentos_event_loop_cancel_timer(agentos_event_loop_t* loop, uint64_t timer_id);

int agentos_event_loop_run(agentos_event_loop_t* loop);

void agentos_event_loop_stop(agentos_event_loop_t* loop);

int agentos_event_loop_get_fd_count(agentos_event_loop_t* loop);

int agentos_event_loop_wakeup(agentos_event_loop_t* loop);

#ifdef __cplusplus
}
#endif

#endif
