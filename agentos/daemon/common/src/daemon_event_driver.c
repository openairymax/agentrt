#include "daemon_event_driver.h"

#include "error.h"
#include "jsonrpc_helpers.h"
#include "memory_compat.h"
#include "method_dispatcher.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

struct daemon_event_driver {
    agentos_event_loop_t *loop;
    thread_pool_t *pool;
    method_dispatcher_t *dispatcher;
    daemon_on_client_cb on_client;
    daemon_on_timer_cb on_timer;
    void *service_ctx;
    bool use_jsonrpc;
    int health_check_interval_sec;
    uint64_t health_timer_id;
};

static void socket_close_wrapper(void *arg)
{
    agentos_socket_close((agentos_socket_t)(uintptr_t)arg);
}

static int on_server_fd_event(int fd, uint32_t events, void *user_data)
{
    daemon_event_driver_t *driver = (daemon_event_driver_t *)user_data;
    if (!driver)
        return AGENTOS_ERR_INVALID_PARAM;

    if (!(events & AGENTOS_EVENT_TYPE_READ))
        return 0;

    while (1) {
        agentos_socket_t client_fd = agentos_socket_accept(fd, 0);
        if (client_fd == AGENTOS_INVALID_SOCKET)
            break;

        if (driver->on_client) {
            driver->on_client(driver->service_ctx, client_fd);
        } else if (driver->pool) {
            thread_pool_submit(driver->pool, socket_close_wrapper, (void *)(uintptr_t)client_fd);
        } else {
            agentos_socket_close(client_fd);
        }
    }

    return 0;
}

static void on_health_timer(agentos_event_loop_t *loop, uint64_t timer_id, void *user_data)
{
    (void)loop;
    (void)timer_id;
    daemon_event_driver_t *driver = (daemon_event_driver_t *)user_data;
    if (driver && driver->on_timer) {
        driver->on_timer(driver->service_ctx, timer_id);
    }
}

daemon_event_driver_t *daemon_event_driver_create(const daemon_event_config_t *config)
{
    if (!config)
        return NULL;

    daemon_event_driver_t *driver =
        (daemon_event_driver_t *)AGENTOS_CALLOC(1, sizeof(daemon_event_driver_t));
    if (!driver)
        return NULL;

    int max_events = config->max_events > 0 ? config->max_events : 64;
    driver->loop = agentos_event_loop_create(max_events);
    if (!driver->loop) {
        AGENTOS_FREE(driver);
        return NULL;
    }

    if (config->thread_pool_max > 0) {
        thread_pool_config_t tp_config;
        memset(&tp_config, 0, sizeof(tp_config));
        tp_config.min_threads = config->thread_pool_min > 0 ? config->thread_pool_min : 2;
        tp_config.max_threads = config->thread_pool_max;
        tp_config.queue_size =
            config->thread_pool_queue_size > 0 ? config->thread_pool_queue_size : 256;
        tp_config.idle_timeout_ms = 30000;
        driver->pool = thread_pool_create(&tp_config);
        if (!driver->pool) {
            SVC_LOG_WARN("Failed to create thread pool, continuing without pool");
        }
    }

    if (config->use_jsonrpc) {
        driver->dispatcher = method_dispatcher_create(16);
        if (!driver->dispatcher) {
            SVC_LOG_ERROR("Failed to create method dispatcher for JSON-RPC");
            if (driver->pool)
                thread_pool_destroy(driver->pool);
            agentos_event_loop_destroy(driver->loop);
            AGENTOS_FREE(driver);
            return NULL;
        }
    }

    driver->on_client = config->on_client;
    driver->on_timer = config->on_timer;
    driver->service_ctx = config->service_ctx;
    driver->use_jsonrpc = config->use_jsonrpc;
    driver->health_check_interval_sec =
        config->health_check_interval_sec > 0 ? config->health_check_interval_sec : 30;

    if (config->on_timer) {
        uint64_t interval_ms = (uint64_t)driver->health_check_interval_sec * 1000;
        driver->health_timer_id =
            agentos_event_loop_add_timer(driver->loop, interval_ms, on_health_timer, driver);
    }

    SVC_LOG_INFO("Daemon event driver created (max_events=%d, pool=%s, jsonrpc=%s)", max_events,
                 driver->pool ? "on" : "off", driver->dispatcher ? "on" : "off");

    return driver;
}

void daemon_event_driver_destroy(daemon_event_driver_t *driver)
{
    if (!driver)
        return;
    if (driver->pool)
        thread_pool_destroy(driver->pool);
    if (driver->dispatcher)
        method_dispatcher_destroy(driver->dispatcher);
    if (driver->loop)
        agentos_event_loop_destroy(driver->loop);
    AGENTOS_FREE(driver);
}

int daemon_event_driver_add_server_fd(daemon_event_driver_t *driver, int fd)
{
    if (!driver || fd < 0)
        return AGENTOS_ERR_INVALID_PARAM;
    return agentos_event_loop_add_fd_lt(driver->loop, fd, AGENTOS_EVENT_TYPE_READ,
                                        on_server_fd_event, driver);
}

int daemon_event_driver_add_fd(daemon_event_driver_t *driver, int fd, uint32_t events,
                               agentos_event_callback_t cb, void *user_data)
{
    if (!driver || fd < 0 || !cb)
        return AGENTOS_ERR_INVALID_PARAM;
    return agentos_event_loop_add_fd(driver->loop, fd, events, cb, user_data);
}

uint64_t daemon_event_driver_add_timer(daemon_event_driver_t *driver, uint64_t interval_ms,
                                       agentos_timer_callback_t cb, void *user_data)
{
    if (!driver || !cb)
        return 0;
    return agentos_event_loop_add_timer(driver->loop, interval_ms, cb, user_data);
}

int daemon_event_driver_cancel_timer(daemon_event_driver_t *driver, uint64_t timer_id)
{
    if (!driver)
        return AGENTOS_ERR_INVALID_PARAM;
    return agentos_event_loop_cancel_timer(driver->loop, timer_id);
}

int daemon_event_driver_run(daemon_event_driver_t *driver)
{
    if (!driver)
        return AGENTOS_ERR_INVALID_PARAM;
    SVC_LOG_INFO("Daemon event driver running");
    return agentos_event_loop_run(driver->loop);
}

void daemon_event_driver_stop(daemon_event_driver_t *driver)
{
    if (!driver)
        return;
    SVC_LOG_INFO("Daemon event driver stopping");
    agentos_event_loop_stop(driver->loop);
}

agentos_event_loop_t *daemon_event_driver_get_loop(daemon_event_driver_t *driver)
{
    return driver ? driver->loop : NULL;
}

thread_pool_t *daemon_event_driver_get_pool(daemon_event_driver_t *driver)
{
    return driver ? driver->pool : NULL;
}

method_dispatcher_t *daemon_event_driver_get_dispatcher(daemon_event_driver_t *driver)
{
    return driver ? driver->dispatcher : NULL;
}
