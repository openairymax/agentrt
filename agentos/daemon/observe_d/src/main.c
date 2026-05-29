#include "memory_compat.h"
/*
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "atomic_compat.h"
#include "error.h"
#include "platform.h"
#include "svc_logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define OBSERVE_D_DEFAULT_PORT 8085
#define OBSERVE_D_METRICS_PORT 9090
#define OBSERVE_D_MAX_BUFFER 65536
#define OBSERVE_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/observe.sock"
#define OBSERVE_D_MAX_METRICS 256
#define OBSERVE_D_HTTP_BACKLOG 16

typedef enum { OBSERVE_METRIC_GAUGE, OBSERVE_METRIC_COUNTER } observe_metric_type_t;

typedef struct {
    char *name;
    char *help;
    observe_metric_type_t type;
    double value;
    char *unit;
    uint64_t updated_at;
} observe_metric_t;

typedef struct {
    agentos_socket_t server_fd;
    agentos_socket_t http_fd;
    agentos_mutex_t lock;
    agentos_thread_t http_thread;
    atomic_int running;
    atomic_int http_running;
    atomic_int force_stop;
    uint64_t start_time;
    uint64_t observe_count;
    uint64_t error_count;
    uint64_t http_request_count;
    observe_metric_t metrics[OBSERVE_D_MAX_METRICS];
    size_t metric_count;
    int tcp_port;
    int metrics_port;
    char *socket_path;
} observe_d_service_t;

static observe_d_service_t g_service = {0};
static atomic_int g_shutdown = 0;

static void observe_d_signal_handler(int sig)
{

    atomic_store_explicit(&g_shutdown, 1, memory_order_seq_cst);
}

static observe_metric_t *observe_d_find_or_create_metric(observe_d_service_t *svc, const char *name)
{
    for (size_t i = 0; i < svc->metric_count; i++) {
        if (svc->metrics[i].name && strcmp(svc->metrics[i].name, name) == 0) {
            return &svc->metrics[i];
        }
    }

    if (svc->metric_count >= OBSERVE_D_MAX_METRICS)
        return NULL;

    observe_metric_t *m = &svc->metrics[svc->metric_count];
    m->name = AGENTOS_STRDUP(name);
    m->help = AGENTOS_STRDUP(name);
    m->type = OBSERVE_METRIC_GAUGE;
    m->value = 0.0;
    m->unit = AGENTOS_STRDUP("count");
    m->updated_at = (uint64_t)time(NULL);
    svc->metric_count++;
    return m;
}

static int observe_d_record_metric(observe_d_service_t *svc, const char *name, double value,
                                   const char *unit, observe_metric_type_t type)
{
    if (!svc || !name)
        return AGENTOS_ERR_INVALID_PARAM;

    agentos_mutex_lock(&svc->lock);
    observe_metric_t *m = observe_d_find_or_create_metric(svc, name);
    if (!m) {
        agentos_mutex_unlock(&svc->lock);
        return AGENTOS_ERR_UNKNOWN;
    }

    if (type == OBSERVE_METRIC_COUNTER)
        m->value += value;
    else
        m->value = value;

    m->type = type;
    m->updated_at = (uint64_t)time(NULL);
    if (unit) {
        AGENTOS_FREE(m->unit);
        m->unit = AGENTOS_STRDUP(unit);
    }
    agentos_mutex_unlock(&svc->lock);
    return 0;
}

static int observe_d_format_prometheus(observe_d_service_t *svc, char *buffer, size_t buffer_size)
{
    if (!svc || !buffer || buffer_size < 128)
        return AGENTOS_ERR_INVALID_PARAM;

    int off = 0;
    agentos_mutex_lock(&svc->lock);

    for (size_t i = 0; i < svc->metric_count && off < (int)(buffer_size - 256); i++) {
        observe_metric_t *m = &svc->metrics[i];
        if (!m->name)
            continue;

        const char *type_str = m->type == OBSERVE_METRIC_COUNTER ? "counter" : "gauge";

        int added = snprintf(buffer + off, buffer_size - (size_t)off,
                             "# HELP %s %s\n"
                             "# TYPE %s %s\n"
                             "%s %.6f %llu\n",
                             m->name, m->help ? m->help : m->name, m->name, type_str, m->name,
                             m->value, (unsigned long long)(m->updated_at * 1000));
        if (added > 0)
            off += added;
    }

    agentos_mutex_unlock(&svc->lock);
    return off;
}

static int observe_d_handle_http_request(observe_d_service_t *svc, agentos_socket_t client_fd)
{
    char buffer[OBSERVE_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return AGENTOS_ERR_UNKNOWN;
    }
    buffer[n] = '\0';

    svc->http_request_count++;

    int is_metrics =
        (strstr(buffer, "GET /metrics") != NULL || strstr(buffer, "GET /metrics HTTP") != NULL);
    int is_health =
        (strstr(buffer, "GET /health") != NULL || strstr(buffer, "GET /healthz") != NULL);

    if (is_metrics) {
        char metrics_buf[65536];
        int metrics_len = observe_d_format_prometheus(svc, metrics_buf, sizeof(metrics_buf));

        char header[512];
        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/plain; version=0.0.4\r\n"
                                  "Content-Length: %d\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  metrics_len > 0 ? metrics_len : 0);

        agentos_socket_send(client_fd, header, (size_t)header_len);
        if (metrics_len > 0)
            agentos_socket_send(client_fd, metrics_buf, (size_t)metrics_len);
    } else if (is_health) {
        uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;
        char health_buf[512];
        int health_len = snprintf(health_buf, sizeof(health_buf),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: %d\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"status\":\"ok\",\"uptime_sec\":%llu,\"metrics\":%zu}\r\n",
                                  0, (unsigned long long)uptime, svc->metric_count);

        int content_start = 0;
        for (int i = 0; i < health_len; i++) {
            if (health_buf[i] == '{') {
                content_start = i;
                break;
            }
        }
        (void)(health_len - content_start);

        char final_buf[1024];
        int final_len = snprintf(
            final_buf, sizeof(final_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"status\":\"ok\",\"uptime_sec\":%llu,\"metrics_count\":%zu}",
            (int)snprintf(NULL, 0, "{\"status\":\"ok\",\"uptime_sec\":%llu,\"metrics_count\":%zu}",
                          (unsigned long long)uptime, svc->metric_count),
            (unsigned long long)uptime, svc->metric_count);

        agentos_socket_send(client_fd, final_buf, (size_t)final_len);
    } else {
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 9\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "Not Found";
        agentos_socket_send(client_fd, not_found, strlen(not_found));
    }

    agentos_socket_close(client_fd);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI observe_d_http_loop(LPVOID arg)
{
#else
static void *observe_d_http_loop(void *arg)
{
#endif
    observe_d_service_t *svc = (observe_d_service_t *)arg;
    if (!svc) {
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    while (svc->http_running) {
        agentos_socket_t client = agentos_socket_accept(svc->http_fd, 1000);
        if (client != AGENTOS_INVALID_SOCKET) {
            observe_d_handle_http_request(svc, client);
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int observe_d_init(observe_d_service_t *svc, int port, const char *sock)
{
    if (!svc)
        return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : OBSERVE_D_DEFAULT_PORT;
    svc->metrics_port = OBSERVE_D_METRICS_PORT;
    svc->socket_path = sock ? AGENTOS_STRDUP(sock) : AGENTOS_STRDUP(OBSERVE_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    observe_d_record_metric(svc, "agentos_observe_requests_total", 0.0, "count",
                            OBSERVE_METRIC_COUNTER);
    observe_d_record_metric(svc, "agentos_observe_errors_total", 0.0, "count",
                            OBSERVE_METRIC_COUNTER);
    observe_d_record_metric(svc, "agentos_observe_http_requests_total", 0.0, "count",
                            OBSERVE_METRIC_COUNTER);
    observe_d_record_metric(svc, "agentos_observe_metrics_count", 0.0, "count",
                            OBSERVE_METRIC_GAUGE);
    observe_d_record_metric(svc, "agentos_observe_uptime_seconds", 0.0, "seconds",
                            OBSERVE_METRIC_GAUGE);

    SVC_LOG_INFO("observe_d: init complete (prometheus_port=%d)", svc->metrics_port);
    return AGENTOS_SUCCESS;
}

static int observe_d_start_http_server(observe_d_service_t *svc)
{
#ifndef _WIN32
    svc->http_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svc->http_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("observe_d: failed to create HTTP socket");
        return AGENTOS_ERR_UNKNOWN;
    }

    int reuse = 1;
    setsockopt(svc->http_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)svc->metrics_port);

    if (bind(svc->http_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        SVC_LOG_ERROR("observe_d: failed to bind HTTP port %d", svc->metrics_port);
        agentos_socket_close(svc->http_fd);
        svc->http_fd = AGENTOS_INVALID_SOCKET;
        return AGENTOS_ERR_UNKNOWN;
    }

    if (listen(svc->http_fd, OBSERVE_D_HTTP_BACKLOG) < 0) {
        SVC_LOG_ERROR("observe_d: failed to listen on HTTP port %d", svc->metrics_port);
        agentos_socket_close(svc->http_fd);
        svc->http_fd = AGENTOS_INVALID_SOCKET;
        return AGENTOS_ERR_UNKNOWN;
    }

    svc->http_running = 1;
    agentos_thread_create(&svc->http_thread, observe_d_http_loop, svc);

    SVC_LOG_INFO("observe_d: prometheus metrics endpoint started on :%d/metrics",
                 svc->metrics_port);
    return 0;
#else
    SVC_LOG_WARN("observe_d: prometheus HTTP server not yet supported on Windows");
    return 0;
#endif
}

static int observe_d_stop_http_server(observe_d_service_t *svc, int force)
{
    svc->http_running = 0;

    if (svc->http_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->http_fd);
        svc->http_fd = AGENTOS_INVALID_SOCKET;
    }

    if (!force) {
        agentos_thread_join(svc->http_thread, NULL);
    }

    SVC_LOG_INFO("observe_d: prometheus endpoint stopped (force=%d)", force);
    return 0;
}

static int observe_d_start(observe_d_service_t *svc)
{
    if (!svc)
        return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("observe_d: failed to create socket at %s", svc->socket_path);
        return AGENTOS_ERR_UNKNOWN;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1", (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("observe_d: failed to create TCP server");
        return AGENTOS_ERR_UNKNOWN;
    }
#endif

    svc->running = 1;
    svc->force_stop = 0;

    observe_d_start_http_server(svc);

    SVC_LOG_INFO("observe_d: service started");
    return AGENTOS_SUCCESS;
}

static int observe_d_stop(observe_d_service_t *svc, int force)
{
    if (!svc)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(&svc->lock);
    svc->running = 0;
    if (force)
        svc->force_stop = 1;
    agentos_mutex_unlock(&svc->lock);

    observe_d_stop_http_server(svc, force);

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

static int observe_d_destroy(observe_d_service_t *svc)
{
    if (!svc)
        return AGENTOS_EINVAL;

    if (svc->http_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->http_fd);
        svc->http_fd = AGENTOS_INVALID_SOCKET;
    }

    for (size_t i = 0; i < svc->metric_count; i++) {
        AGENTOS_FREE(svc->metrics[i].name);
        AGENTOS_FREE(svc->metrics[i].help);
        AGENTOS_FREE(svc->metrics[i].unit);
    }
    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
    }
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    AGENTOS_FREE(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("observe_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static int observe_d_healthcheck(observe_d_service_t *svc)
{
    if (!svc)
        return 0;
    if (!svc->running)
        return 0;

#ifndef _WIN32
    return (svc->http_fd != AGENTOS_INVALID_SOCKET && svc->http_running) ? 1 : 0;
#else
    return svc->running ? 1 : 0;
#endif
}

static void observe_d_handle_request(observe_d_service_t *svc, agentos_socket_t client_fd)
{
    char buffer[OBSERVE_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);
    svc->observe_count++;
    agentos_mutex_unlock(&svc->lock);

    observe_d_record_metric(svc, "agentos_observe_requests_total", 1.0, "count",
                            OBSERVE_METRIC_COUNTER);

    int healthy = observe_d_healthcheck(svc);
    uint64_t uptime = (uint64_t)time(NULL) - svc->start_time;

    observe_d_record_metric(svc, "agentos_observe_uptime_seconds", (double)uptime, "seconds",
                            OBSERVE_METRIC_GAUGE);

    agentos_mutex_lock(&svc->lock);
    size_t mcount = svc->metric_count;
    agentos_mutex_unlock(&svc->lock);

    char response[4096];
    snprintf(response, sizeof(response),
             "{"
             "\"service\":\"observe_d\","
             "\"status\":\"ok\","
             "\"observed\":%llu,"
             "\"metric_count\":%zu,"
             "\"http_requests\":%llu,"
             "\"uptime_sec\":%llu,"
             "\"healthy\":%s,"
             "\"prometheus\":{"
             "\"port\":%d,"
             "\"endpoint\":\"/metrics\""
             "}"
             "}",
             (unsigned long long)svc->observe_count, mcount,
             (unsigned long long)svc->http_request_count, (unsigned long long)uptime,
             healthy ? "true" : "false", svc->metrics_port);

    agentos_socket_send(client_fd, response, strlen(response));
    agentos_socket_close(client_fd);
}

int main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{

#ifndef _WIN32
    signal(SIGINT, observe_d_signal_handler);
    signal(SIGTERM, observe_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (observe_d_init(&g_service, OBSERVE_D_DEFAULT_PORT, OBSERVE_D_DEFAULT_SOCKET) !=
        AGENTOS_SUCCESS)
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

    observe_d_stop(&g_service, g_shutdown ? 1 : 0);
    observe_d_destroy(&g_service);
    return 0;
}
