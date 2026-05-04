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
#include <sys/utsname.h>
#endif

#define INFO_D_DEFAULT_PORT 8083
#define INFO_D_MAX_BUFFER 65536
#define INFO_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/info.sock"

typedef struct {
    agentos_socket_t server_fd;
    agentos_mutex_t lock;
    volatile int running;
    uint64_t start_time;
    uint64_t request_count;
    uint64_t error_count;
    int tcp_port;
    char* socket_path;
} info_d_service_t;

static info_d_service_t g_service = {0};
static volatile int g_shutdown = 0;

static void info_d_signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static int info_d_init(info_d_service_t* svc, int port, const char* sock) {
    if (!svc) return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : INFO_D_DEFAULT_PORT;
    svc->socket_path = sock ? strdup(sock) : strdup(INFO_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    SVC_LOG_INFO("info_d: init complete");
    return AGENTOS_SUCCESS;
}

static int info_d_start(info_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("info_d: failed to create socket at %s", svc->socket_path);
        return -1;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1",
                                                       (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("info_d: failed to create TCP server");
        return -1;
    }
#endif

    svc->running = 1;
    SVC_LOG_INFO("info_d: service started");
    return AGENTOS_SUCCESS;
}

static int info_d_stop(info_d_service_t* svc, int force) {
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

    SVC_LOG_INFO("info_d: service stopped (force=%d)", force);
    return AGENTOS_SUCCESS;
}

static int info_d_destroy(info_d_service_t* svc) {
    if (!svc) return AGENTOS_EINVAL;

    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
    }
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    free(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("info_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static int info_d_healthcheck(info_d_service_t* svc) {
    if (!svc) return 0;
    int healthy = svc->running ? 1 : 0;

    if (svc->error_count > svc->request_count / 2 && svc->request_count > 10) {
        healthy = 0;
    }

    return healthy;
}

static void info_d_handle_request(info_d_service_t* svc, agentos_socket_t client_fd) {
    char buffer[INFO_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);
    svc->request_count++;
    agentos_mutex_unlock(&svc->lock);

    char response[4096];
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    int cores = (int)sys_info.dwNumberOfProcessors;
    const char* platform_name = "Windows";
#else
    struct utsname uts;
    int cores = 1;
    const char* platform_name = "Linux";
    if (uname(&uts) == 0) {
        platform_name = uts.sysname;
    }
#endif

    uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;
    snprintf(response, sizeof(response),
        "{"
        "\"service\":\"info_d\","
        "\"status\":\"ok\","
        "\"platform\":\"%s\","
        "\"cpu_cores\":%d,"
        "\"uptime_sec\":%llu,"
        "\"requests\":%llu,"
        "\"errors\":%llu,"
        "\"healthy\":%d"
        "}",
        platform_name, cores,
        (unsigned long long)uptime,
        (unsigned long long)svc->request_count,
        (unsigned long long)svc->error_count,
        info_d_healthcheck(svc));

    agentos_socket_send(client_fd, response, strlen(response));
    agentos_socket_close(client_fd);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#ifdef _WIN32
    SetConsoleCtrlHandler(NULL, TRUE);
#else
    signal(SIGINT, info_d_signal_handler);
    signal(SIGTERM, info_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (info_d_init(&g_service, INFO_D_DEFAULT_PORT, INFO_D_DEFAULT_SOCKET) != AGENTOS_SUCCESS)
        return 1;
    if (info_d_start(&g_service) != AGENTOS_SUCCESS) {
        info_d_destroy(&g_service);
        return 1;
    }

    while (!g_shutdown && g_service.running) {
        agentos_socket_t client = agentos_socket_accept(g_service.server_fd, 1000);
        if (client != AGENTOS_INVALID_SOCKET) {
            info_d_handle_request(&g_service, client);
        }
    }

    info_d_stop(&g_service, 0);
    info_d_destroy(&g_service);
    return 0;
}
