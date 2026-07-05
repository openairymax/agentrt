#ifndef AGENTRT_EVENT_LOOP_H
#define AGENTRT_EVENT_LOOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AGENTRT_EVENT_LOOP_MAX_EVENTS 1024
#define AGENTRT_EVENT_LOOP_MAX_TIMERS 64

typedef enum {
    AGENTRT_EVENT_TYPE_READ = 1,
    AGENTRT_EVENT_TYPE_WRITE = 2,
    AGENTRT_EVENT_TYPE_TIMER = 4,
    AGENTRT_EVENT_TYPE_SIGNAL = 8
} agentrt_event_type_t;

typedef struct agentrt_event_loop agentrt_event_loop_t;

typedef int (*agentrt_event_callback_t)(int fd, uint32_t events, void *user_data);

typedef void (*agentrt_timer_callback_t)(agentrt_event_loop_t *loop, uint64_t timer_id,
                                         void *user_data);

agentrt_event_loop_t *agentrt_event_loop_create(int max_events);

void agentrt_event_loop_destroy(agentrt_event_loop_t *loop);

int agentrt_event_loop_add_fd(agentrt_event_loop_t *loop, int fd, uint32_t events,
                              agentrt_event_callback_t cb, void *user_data);

int agentrt_event_loop_add_fd_lt(agentrt_event_loop_t *loop, int fd, uint32_t events,
                                 agentrt_event_callback_t cb, void *user_data);

int agentrt_event_loop_mod_fd(agentrt_event_loop_t *loop, int fd, uint32_t events);

void agentrt_event_loop_remove_fd(agentrt_event_loop_t *loop, int fd);

uint64_t agentrt_event_loop_add_timer(agentrt_event_loop_t *loop, uint64_t interval_ms,
                                      agentrt_timer_callback_t cb, void *user_data);

int agentrt_event_loop_cancel_timer(agentrt_event_loop_t *loop, uint64_t timer_id);

int agentrt_event_loop_run(agentrt_event_loop_t *loop);

void agentrt_event_loop_stop(agentrt_event_loop_t *loop);

int agentrt_event_loop_get_fd_count(agentrt_event_loop_t *loop);

int agentrt_event_loop_wakeup(agentrt_event_loop_t *loop);

#ifdef __cplusplus
}
#endif

#endif
