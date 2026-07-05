#ifndef DAEMON_EVENT_DRIVER_H
#define DAEMON_EVENT_DRIVER_H

#include "agentrt_event_loop.h"
#include "method_dispatcher.h"
#include "svc_common.h"
#include "thread_pool.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct daemon_event_driver daemon_event_driver_t;

typedef int (*daemon_on_client_cb)(void *service, agentrt_socket_t client_fd);

typedef void (*daemon_on_timer_cb)(void *service, uint64_t timer_id);

typedef struct daemon_event_config {
    int max_events;
    int thread_pool_min;
    int thread_pool_max;
    int thread_pool_queue_size;
    int health_check_interval_sec;
    bool use_jsonrpc;
    daemon_on_client_cb on_client;
    daemon_on_timer_cb on_timer;
    void *service_ctx;
} daemon_event_config_t;

daemon_event_driver_t *daemon_event_driver_create(const daemon_event_config_t *config);

void daemon_event_driver_destroy(daemon_event_driver_t *driver);

int daemon_event_driver_add_server_fd(daemon_event_driver_t *driver, int fd);

int daemon_event_driver_add_fd(daemon_event_driver_t *driver, int fd, uint32_t events,
                               agentrt_event_callback_t cb, void *user_data);

uint64_t daemon_event_driver_add_timer(daemon_event_driver_t *driver, uint64_t interval_ms,
                                       agentrt_timer_callback_t cb, void *user_data);

int daemon_event_driver_cancel_timer(daemon_event_driver_t *driver, uint64_t timer_id);

int daemon_event_driver_run(daemon_event_driver_t *driver);

void daemon_event_driver_stop(daemon_event_driver_t *driver);

agentrt_event_loop_t *daemon_event_driver_get_loop(daemon_event_driver_t *driver);

thread_pool_t *daemon_event_driver_get_pool(daemon_event_driver_t *driver);

method_dispatcher_t *daemon_event_driver_get_dispatcher(daemon_event_driver_t *driver);

#ifdef __cplusplus
}
#endif

#endif
