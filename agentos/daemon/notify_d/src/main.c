/*
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#define NOTIFY_D_DEFAULT_PORT 8084
#define NOTIFY_D_MAX_BUFFER 65536
#define NOTIFY_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/notify.sock"
#define NOTIFY_D_MAX_PENDING 1024

typedef struct {
    char* message;
    char* channel;
    uint64_t timestamp;
} notify_entry_t;

typedef struct {
    agentos_socket_t server_fd;
    agentos_mutex_t lock;
    volatile int running;
    uint64_t start_time;
    uint64_t notified_count;
    uint64_t error_count;
    notify_entry_t* pending[NOTIFY_D_MAX_PENDING];
    size_t pending_head;
    size_t pending_tail;
    size_t pending_count;
    int tcp_port;
    char* socket_path;
} notify_d_service_t;

static notify_d_service_t g_service = {0};
static volatile int g_shutdown = 0;

static void notify_d_signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int notify_d_init(notify_d_service_t* svc, int port, const char* sock) {
    if (!svc) return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : NOTIFY_D_DEFAULT_PORT;
    svc->socket_path = sock ? strdup(sock) : strdup(NOTIFY_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    SVC_LOG_INFO("notify_d: init complete");
    return AGENTOS_SUCCESS;
}

static int notify_d_start(notify_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("notify_d: failed to create socket at %s", svc->socket_path);
        return -1;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1",
                                                       (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("notify_d: failed to create TCP server");
        return -1;
    }
#endif

    svc->running = 1;
    SVC_LOG_INFO("notify_d: service started");
    return AGENTOS_SUCCESS;
}

static int notify_d_stop(notify_d_service_t* svc, int force) {
    if (!svc) return AGENTOS_EINVAL;

    agentos_mutex_lock(&svc->lock);
    svc->running = 0;

    if (force) {
        for (size_t i = 0; i < svc->pending_count; i++) {
            size_t idx = (svc->pending_head + i) % NOTIFY_D_MAX_PENDING;
            free(svc->pending[idx]->message);
            free(svc->pending[idx]->channel);
            free(svc->pending[idx]);
        }
        svc->pending_count = 0;
        svc->pending_head = 0;
        svc->pending_tail = 0;
    }
    agentos_mutex_unlock(&svc->lock);

    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
        svc->server_fd = AGENTOS_INVALID_SOCKET;
    }

    if (force) {
#ifndef _WIN32
        unlink(svc->socket_path);
#endif
    }

    SVC_LOG_INFO("notify_d: service stopped (force=%d, pending=%zu)",
                 force, svc->pending_count);
    return AGENTOS_SUCCESS;
}

static int notify_d_destroy(notify_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

    notify_d_stop(svc, 1);
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    free(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("notify_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static int notify_d_healthcheck(notify_d_service_t* svc) {
    if (!svc) return 0;
    int healthy = svc->running ? 1 : 0;

    if (svc->pending_count >= NOTIFY_D_MAX_PENDING) {
        healthy = 0;
    }

    return healthy;
}

static int notify_d_enqueue(notify_d_service_t* svc, const char* msg,
                             const char* channel) {
    if (!svc || !msg) return -1;

    if (svc->pending_count >= NOTIFY_D_MAX_PENDING) return -2;

    notify_entry_t* entry = (notify_entry_t*)calloc(1, sizeof(notify_entry_t));
    if (!entry) return -3;

    entry->message = strdup(msg);
    entry->channel = channel ? strdup(channel) : strdup("default");
    entry->timestamp = (uint64_t)time(NULL);

    svc->pending[svc->pending_tail] = entry;
    svc->pending_tail = (svc->pending_tail + 1) % NOTIFY_D_MAX_PENDING;
    svc->pending_count++;

    return 0;
}

static void notify_d_handle_request(notify_d_service_t* svc,
                                     agentos_socket_t client_fd) {
    char buffer[NOTIFY_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);

    int ret = notify_d_enqueue(svc, buffer, "inbound");
    if (ret == 0) {
        svc->notified_count++;
    } else {
        svc->error_count++;
    }

    uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;
    agentos_mutex_unlock(&svc->lock);

    char response[4096];
    snprintf(response, sizeof(response),
        "{"
        "\"service\":\"notify_d\","
        "\"status\":\"%s\","
        "\"accepted\":%d,"
        "\"queued\":%llu,"
        "\"pending\":%zu,"
        "\"uptime_sec\":%llu,"
        "\"healthy\":%d"
        "}",
        ret == 0 ? "ok" : "error",
        ret == 0 ? 1 : 0,
        (unsigned long long)svc->notified_count,
        svc->pending_count,
        (unsigned long long)uptime,
        notify_d_healthcheck(svc));

    agentos_socket_send(client_fd, response, strlen(response));
    agentos_socket_close(client_fd);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#ifndef _WIN32
    signal(SIGINT, notify_d_signal_handler);
    signal(SIGTERM, notify_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (notify_d_init(&g_service, NOTIFY_D_DEFAULT_PORT,
                      NOTIFY_D_DEFAULT_SOCKET) != AGENTOS_SUCCESS)
        return 1;
    if (notify_d_start(&g_service) != AGENTOS_SUCCESS) {
        notify_d_destroy(&g_service);
        return 1;
    }

    while (!g_shutdown && g_service.running) {
        agentos_socket_t client = agentos_socket_accept(g_service.server_fd, 1000);
        if (client != AGENTOS_INVALID_SOCKET) {
            notify_d_handle_request(&g_service, client);
        }
    }

    notify_d_stop(&g_service, 0);
    notify_d_destroy(&g_service);
    return 0;
}
