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

#define OBSERVE_D_DEFAULT_PORT 8085
#define OBSERVE_D_MAX_BUFFER 65536
#define OBSERVE_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/observe.sock"
#define OBSERVE_D_MAX_METRICS 256

typedef struct {
    char* name;
    double value;
    char* unit;
    uint64_t updated_at;
} observe_metric_t;

typedef struct {
    agentos_socket_t server_fd;
    agentos_mutex_t lock;
    volatile int running;
    uint64_t start_time;
    uint64_t observe_count;
    uint64_t error_count;
    observe_metric_t metrics[OBSERVE_D_MAX_METRICS];
    size_t metric_count;
    int tcp_port;
    char* socket_path;
} observe_d_service_t;

static observe_d_service_t g_service = {0};
static volatile int g_shutdown = 0;

static void observe_d_signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int observe_d_init(observe_d_service_t* svc, int port, const char* sock) {
    if (!svc) return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : OBSERVE_D_DEFAULT_PORT;
    svc->socket_path = sock ? strdup(sock) : strdup(OBSERVE_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    SVC_LOG_INFO("observe_d: init complete");
    return AGENTOS_SUCCESS;
}

static int observe_d_start(observe_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("observe_d: failed to create socket at %s", svc->socket_path);
        return -1;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1",
                                                       (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("observe_d: failed to create TCP server");
        return -1;
    }
#endif

    svc->running = 1;
    SVC_LOG_INFO("observe_d: service started");
    return AGENTOS_SUCCESS;
}

static int observe_d_stop(observe_d_service_t* svc, int force) {
    if (!svc) return AGENTOS_EINVAL;

    agentos_mutex_lock(&svc->lock);
    svc->running = 0;
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

    SVC_LOG_INFO("observe_d: service stopped (force=%d)", force);
    return AGENTOS_SUCCESS;
}

static int observe_d_destroy(observe_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

    for (size_t i = 0; i < svc->metric_count; i++) {
        free(svc->metrics[i].name);
        free(svc->metrics[i].unit);
    }
    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
    }
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    free(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("observe_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static int observe_d_healthcheck(observe_d_service_t* svc) {
    if (!svc) return 0;
    return svc->running ? 1 : 0;
}

static int observe_d_record_metric(observe_d_service_t* svc, const char* name,
                                    double value, const char* unit) {
    if (!svc || !name || svc->metric_count >= OBSERVE_D_MAX_METRICS) return -1;

    size_t idx = svc->metric_count;
    for (size_t i = 0; i < svc->metric_count; i++) {
        if (svc->metrics[i].name && strcmp(svc->metrics[i].name, name) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == svc->metric_count) {
        svc->metrics[idx].name = strdup(name);
        svc->metrics[idx].unit = unit ? strdup(unit) : strdup("count");
        svc->metric_count++;
    }

    svc->metrics[idx].value = value;
    svc->metrics[idx].updated_at = (uint64_t)time(NULL);
    return 0;
}

static void observe_d_handle_request(observe_d_service_t* svc,
                                      agentos_socket_t client_fd) {
    char buffer[OBSERVE_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);
    svc->observe_count++;

    observe_d_record_metric(svc, "observe.requests", (double)svc->observe_count, "count");
    observe_d_record_metric(svc, "observe.errors", (double)svc->error_count, "count");

    char response[4096];
    uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;

    int off = snprintf(response, sizeof(response),
        "{"
        "\"service\":\"observe_d\","
        "\"status\":\"ok\","
        "\"observed\":%llu,"
        "\"metric_count\":%zu,"
        "\"uptime_sec\":%llu,"
        "\"healthy\":%d,"
        "\"metrics\":[",
        (unsigned long long)svc->observe_count,
        svc->metric_count,
        (unsigned long long)uptime,
        observe_d_healthcheck(svc));

    agentos_mutex_unlock(&svc->lock);

    for (size_t i = 0; i < svc->metric_count && off < (int)(sizeof(response) - 200); i++) {
        agentos_mutex_lock(&svc->lock);
        observe_metric_t m = svc->metrics[i];
        agentos_mutex_unlock(&svc->lock);

        if (m.name) {
            int added = snprintf(response + off, sizeof(response) - (size_t)off,
                "%s{\"name\":\"%s\",\"value\":%.6f,\"unit\":\"%s\"}",
                i > 0 ? "," : "",
                m.name, m.value, m.unit ? m.unit : "");
            if (added > 0) off += added;
        }
    }

    snprintf(response + off, sizeof(response) - (size_t)off, "]}");

    agentos_socket_send(client_fd, response, strlen(response));
    agentos_socket_close(client_fd);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#ifndef _WIN32
    signal(SIGINT, observe_d_signal_handler);
    signal(SIGTERM, observe_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (observe_d_init(&g_service, OBSERVE_D_DEFAULT_PORT,
                       OBSERVE_D_DEFAULT_SOCKET) != AGENTOS_SUCCESS)
        return 1;
    if (observe_d_start(&g_service) != AGENTOS_SUCCESS) {
        observe_d_destroy(&g_service);
        return 1;
    }

    while (!g_shutdown && g_service.running) {
        agentos_socket_t client = agentos_socket_accept(g_service.server_fd, 1000);
        if (client != AGENTOS_INVALID_SOCKET) {
            observe_d_handle_request(&g_service, client);
        }
    }

    observe_d_stop(&g_service, 0);
    observe_d_destroy(&g_service);
    return 0;
}
