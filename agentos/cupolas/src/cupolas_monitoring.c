/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_monitoring.c - Monitoring Interface: Prometheus / OpenTelemetry
 */

/**
 * @file cupolas_monitoring.c
 * @brief Monitoring Interface - Prometheus / OpenTelemetry
 * @author Spharx AgentOS Team
 * @date 2024
 *
 * This module implements monitoring interface:
 * - Prometheus HTTP endpoint (pull mode)
 * - OpenTelemetry OTLP push
 * - StatsD protocol support
 * - Health check endpoint
 */

#include "cupolas_monitoring.h"
#include "cupolas_metrics.h"
#include "utils/cupolas_utils.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#if cupolas_PLATFORM_WINDOWS
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define MAX_METRICS_BUFFER (64 * 1024)
#define MAX_HEALTH_CHECKS 32

#define MAX_FILTER_PATTERNS 16
#define MAX_PATTERN_LEN 128
#define HTTP_RESPONSE_BUF (256 * 1024)

typedef struct health_check_entry {
    char name[128];
    health_check_fn_t callback;
    bool registered;
} health_check_entry_t;

struct cupolas_monitoring {
    monitoring_config_t manager;
    monitoring_status_t status;

    cupolas_rwlock_t lock;

    char metrics_buffer[MAX_METRICS_BUFFER];
    size_t metrics_buffer_size;

    health_check_entry_t health_checks[MAX_HEALTH_CHECKS];
    size_t health_check_count;

    char include_patterns[MAX_FILTER_PATTERNS][MAX_PATTERN_LEN];
    size_t include_count;
    char exclude_patterns[MAX_FILTER_PATTERNS][MAX_PATTERN_LEN];
    size_t exclude_count;

    cupolas_thread_t reporter_thread;
    bool reporter_running;

    int http_fd;
    cupolas_thread_t http_thread;
    bool http_running;
    uint16_t http_port;

    cupolas_thread_t collector_thread;
    bool collector_running;
    uint32_t collect_interval_ms;

    uint64_t last_report_time;
    char last_error[512];

    cupolas_monitoring_t* instance;
};

static cupolas_monitoring_t* g_monitoring = NULL;
static cupolas_rwlock_t g_monitoring_lock = {0};

const char* monitoring_backend_string(monitoring_backend_t backend) {
    switch (backend) {
        case MONITORING_BACKEND_PROMETHEUS:  return "prometheus";
        case MONITORING_BACKEND_OPENTELEMETRY: return "opentelemetry";
        case MONITORING_BACKEND_STATSD:     return "statsd";
        default:                            return "none";
    }
}

const char* monitoring_status_string(monitoring_status_t status) {
    switch (status) {
        case MONITORING_STATUS_STOPPED:   return "stopped";
        case MONITORING_STATUS_STARTING:  return "starting";
        case MONITORING_STATUS_RUNNING:   return "running";
        case MONITORING_STATUS_ERROR:     return "error";
        case MONITORING_STATUS_STOPPING:  return "stopping";
        default:                          return "unknown";
    }
}

/* ========== System Metrics Collection (Linux /proc) ========== */

#if cupolas_PLATFORM_POSIX
#include <sys/resource.h>

static uint64_t get_process_rss_bytes(void) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[256];
    uint64_t rss = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            unsigned long kb = 0;
            if (sscanf(line + 6, "%lu", &kb) == 1)
                rss = kb * 1024UL;
            break;
        }
    }
    fclose(f);
    return rss;
}

static double get_process_cpu_seconds(void) {
    FILE* f = fopen("/proc/self/stat", "r");
    if (!f) return 0.0;
    char line[1024];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0.0; }
    fclose(f);

    long utime = 0, stime = 0;
    int field = 0;
    const char* p = line;
    while (*p && field < 15) {
        if (*p == ' ') field++;
        else if (field == 13) utime = strtol(p, NULL, 10);
        else if (field == 14) stime = strtol(p, NULL, 10);
        p++;
    }
    static long clk_tck = 0;
    if (clk_tck == 0) clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) clk_tck = 100;
    return (double)(utime + stime) / (double)clk_tck;
}

static int get_thread_count(void) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 1;
    char line[256];
    int threads = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Threads:", 8) == 0) {
            sscanf(line + 8, "%d", &threads);
            break;
        }
    }
    fclose(f);
    return threads;
}

#else

static uint64_t get_process_rss_bytes(void) { return 0; }
static double get_process_cpu_seconds(void) { return 0.0; }
static int get_thread_count(void) { return 1; }

#endif

static void* collector_thread_func(void* arg) {
    cupolas_monitoring_t* mgr = (cupolas_monitoring_t*)arg;

    while (mgr->collector_running) {
        metrics_gauge_set(METRIC_PROCESS_MEMORY_BYTES, NULL,
                          (double)get_process_rss_bytes());
        metrics_gauge_set(METRIC_PROCESS_CPU_SECONDS, NULL,
                          get_process_cpu_seconds());
        metrics_gauge_set(METRIC_THREAD_COUNT, NULL,
                          (double)get_thread_count());

        for (uint32_t i = 0; i < mgr->collect_interval_ms / 100 && mgr->collector_running; i++) {
            cupolas_sleep_ms(100);
        }
    }

    return NULL;
}

static void* reporter_thread_func(void* arg) {
    cupolas_monitoring_t* mgr = (cupolas_monitoring_t*)arg;

    while (mgr->reporter_running) {
        cupolas_sleep_ms(mgr->collect_interval_ms * 2);

        cupolas_rwlock_wrlock(&mgr->lock);

        metrics_export_prometheus(mgr->metrics_buffer, sizeof(mgr->metrics_buffer) - 1);
        mgr->metrics_buffer_size = strlen(mgr->metrics_buffer);
        mgr->last_report_time = metrics_get_timestamp_ns();

        cupolas_rwlock_unlock(&mgr->lock);
    }

    return NULL;
}

/* ========== Minimal Prometheus HTTP Server ========== */

static void send_http_response(int fd, int status_code,
                               const char* status_text,
                               const char* content_type,
                               const char* body, size_t body_len) {
    char header[512];
    size_t hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    (void)send(fd, header, hdr_len, MSG_NOSIGNAL);
    if (body && body_len > 0)
        (void)send(fd, body, body_len, MSG_NOSIGNAL);
}

static void handle_http_request(cupolas_monitoring_t* mgr, int client_fd,
                                char* req_buf, size_t req_len) {
    (void)req_len;

    char method[16] = {0}, path[256] = {0};
    sscanf(req_buf, "%15s %255s", method, path);

    if (strcmp(method, "GET") != 0) {
        const char* body = "Method Not Allowed";
        send_http_response(client_fd, 405, "Method Not Allowed",
                           "text/plain", body, strlen(body));
        return;
    }

    if (strcmp(path, "/metrics") == 0) {
        cupolas_rwlock_rdlock(&mgr->lock);

        char buf[HTTP_RESPONSE_BUF];
        size_t len = metrics_export_prometheus(buf, sizeof(buf));

        cupolas_rwlock_unlock(&mgr->lock);

        if (len > 0) {
            send_http_response(client_fd, 200, "OK",
                               "text/plain; version=0.0.4; charset=utf-8",
                               buf, len);
        } else {
            const char* body = "# No metrics available\n";
            send_http_response(client_fd, 200, "OK",
                               "text/plain; version=0.0.4; charset=utf-8",
                               body, strlen(body));
        }
    } else if (strcmp(path, "/health") == 0) {
        health_check_result_t results[MAX_HEALTH_CHECKS];
        int count = cupolas_monitoring_check_health(mgr, results, MAX_HEALTH_CHECKS);

        char buf[4096];
        size_t off = snprintf(buf, sizeof(buf), "{\n");
        bool all_healthy = true;

        for (int i = 0; i < count && off < sizeof(buf) - 256; i++) {
            off += snprintf(buf + off, sizeof(buf) - off,
                "  \"%s\": %s,\n",
                results[i].component ? results[i].component : "unknown",
                results[i].healthy ? "true" : "false");
            if (!results[i].healthy) all_healthy = false;
        }

        off += snprintf(buf + off, sizeof(buf) - off,
                        "  \"status\": \"%s\"\n}\n",
                        all_healthy ? "healthy" : "unhealthy");

        send_http_response(client_fd, all_healthy ? 200 : 503,
                           all_healthy ? "OK" : "Service Unavailable",
                           "application/json", buf, off);
    } else if (strcmp(path, "/") == 0 || strcmp(path, "/") == 0) {
        const char* body =
            "<html><head><title>Cupolas Monitoring</title></head><body>"
            "<h2>AgentOS Cupolas Monitoring</h2>"
            "<ul>"
            "<li><a href=\"/metrics\">/metrics</a> - Prometheus exposition format</li>"
            "<li><a href=\"/health\">/health</a> - Health check endpoint</li>"
            "</ul></body></html>";
        send_http_response(client_fd, 200, "OK", "text/html", body, strlen(body));
    } else {
        const char* body = "Not Found";
        send_http_response(client_fd, 404, "Not Found",
                           "text/plain", body, strlen(body));
    }
}

static void* http_server_thread_func(void* arg) {
    cupolas_monitoring_t* mgr = (cupolas_monitoring_t*)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        CUPOLAS_LOG_ERROR("monitoring: socket() failed");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(mgr->http_port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        CUPOLAS_LOG_ERROR("monitoring: bind() failed on port %d", mgr->http_port);
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 8) < 0) {
        CUPOLAS_LOG_ERROR("monitoring: listen() failed");
        close(server_fd);
        return NULL;
    }

    mgr->http_fd = server_fd;
    CUPOLAS_LOG("monitoring: HTTP server listening on port %d", mgr->http_port);

    while (mgr->http_running) {
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        int sel = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (sel <= 0 || !FD_ISSET(server_fd, &readfds))
            continue;

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        char req_buf[8192] = {0};
        ssize_t n = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
        if (n > 0) {
            handle_http_request(mgr, client_fd, req_buf, (size_t)n);
        }

        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }

    close(server_fd);
    mgr->http_fd = -1;
    CUPOLAS_LOG("monitoring: HTTP server stopped");

    return NULL;
}

cupolas_monitoring_t* cupolas_monitoring_create(const monitoring_config_t* manager) {
    cupolas_monitoring_t* mgr = (cupolas_monitoring_t*)cupolas_mem_alloc(sizeof(cupolas_monitoring_t));
    if (!mgr) {
        return NULL;
    }

    memset(mgr, 0, sizeof(cupolas_monitoring_t));

    if (manager) {
        memcpy(&mgr->manager, manager, sizeof(monitoring_config_t));
    } else {
        memset(&mgr->manager, 0, sizeof(monitoring_config_t));
        mgr->manager.backend = MONITORING_BACKEND_PROMETHEUS;
        mgr->manager.prometheus.listen_addr = "0.0.0.0";
        mgr->manager.prometheus.port = 9090;
        mgr->manager.prometheus.endpoint = "/metrics";
        mgr->manager.reporting_interval_ms = 10000;
    }

    mgr->status = MONITORING_STATUS_STOPPED;
    cupolas_rwlock_init(&mgr->lock);

    mgr->http_fd = -1;
    mgr->http_running = false;
    mgr->http_port = manager ? manager->prometheus.port : 9090;
    if (mgr->http_port == 0) mgr->http_port = 9090;

    mgr->collector_running = false;
    mgr->collect_interval_ms = manager ? manager->reporting_interval_ms : 10000;
    if (mgr->collect_interval_ms < 1000) mgr->collect_interval_ms = 1000;

    return mgr;
}

void cupolas_monitoring_destroy(cupolas_monitoring_t* mgr) {
    if (!mgr) return;

    cupolas_monitoring_stop(mgr);

    cupolas_rwlock_destroy(&mgr->lock);

    cupolas_mem_free(mgr);
}

int cupolas_monitoring_start(cupolas_monitoring_t* mgr) {
    if (!mgr) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->status == MONITORING_STATUS_RUNNING) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    mgr->status = MONITORING_STATUS_STARTING;
    mgr->reporter_running = true;

    metrics_init(mgr->collect_interval_ms);

    metric_desc_t mem_desc = {
        .name = METRIC_PROCESS_MEMORY_BYTES,
        .help = "Current process resident memory in bytes",
        .type = METRIC_TYPE_GAUGE,
        .label_names = NULL,
        .label_count = 0
    };
    metrics_register(&mem_desc);

    metric_desc_t cpu_desc = {
        .name = METRIC_PROCESS_CPU_SECONDS,
        .help = "Total CPU time consumed by process",
        .type = METRIC_TYPE_GAUGE,
        .label_names = NULL,
        .label_count = 0
    };
    metrics_register(&cpu_desc);

    metric_desc_t thread_desc = {
        .name = METRIC_THREAD_COUNT,
        .help = "Number of threads in process",
        .type = METRIC_TYPE_GAUGE,
        .label_names = NULL,
        .label_count = 0
    };
    metrics_register(&thread_desc);

    mgr->collector_running = true;
    int ret = cupolas_thread_create(&mgr->collector_thread,
                                    collector_thread_func, mgr);
    if (ret != 0) {
        CUPOLAS_LOG_ERROR("monitoring: failed to create collector thread");
        mgr->collector_running = false;
    }

    if (mgr->manager.backend == MONITORING_BACKEND_PROMETHEUS ||
        mgr->manager.backend == MONITORING_BACKEND_ALL) {
        mgr->http_running = true;
        ret = cupolas_thread_create(&mgr->http_thread,
                                    http_server_thread_func, mgr);
        if (ret != 0) {
            CUPOLAS_LOG_ERROR("monitoring: failed to create HTTP server thread");
            mgr->http_running = false;
        }
    }

    mgr->status = MONITORING_STATUS_RUNNING;

    cupolas_rwlock_unlock(&mgr->lock);

    CUPOLAS_LOG("monitoring: started (port=%d, collect_ms=%u)",
                mgr->http_port, mgr->collect_interval_ms);

    return 0;
}

void cupolas_monitoring_stop(cupolas_monitoring_t* mgr) {
    if (!mgr) return;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->status != MONITORING_STATUS_RUNNING &&
        mgr->status != MONITORING_STATUS_STARTING) {
        cupolas_rwlock_unlock(&mgr->lock);
        return;
    }

    mgr->status = MONITORING_STATUS_STOPPING;
    mgr->reporter_running = false;

    mgr->http_running = false;
    if (mgr->http_fd >= 0) {
        shutdown(mgr->http_fd, SHUT_RDWR);
    }

    mgr->collector_running = false;

    cupolas_rwlock_unlock(&mgr->lock);

    if (mgr->http_running == false && mgr->http_fd >= 0) {
        void* retval = NULL;
        cupolas_thread_join(mgr->http_thread, &retval);
        (void)retval;
    }

    void* retval = NULL;
    cupolas_thread_join(mgr->reporter_thread, &retval);

    cupolas_thread_join(mgr->collector_thread, &retval);
    (void)retval;

    metrics_shutdown();

    cupolas_rwlock_wrlock(&mgr->lock);
    mgr->status = MONITORING_STATUS_STOPPED;
    mgr->http_fd = -1;
    cupolas_rwlock_unlock(&mgr->lock);

    CUPOLAS_LOG("monitoring: stopped");
}

monitoring_status_t cupolas_monitoring_get_status(cupolas_monitoring_t* mgr) {
    if (!mgr) return MONITORING_STATUS_ERROR;

    cupolas_rwlock_rdlock(&mgr->lock);
    monitoring_status_t status = mgr->status;
    cupolas_rwlock_unlock(&mgr->lock);

    return status;
}

int cupolas_monitoring_report(cupolas_monitoring_t* mgr) {
    if (!mgr) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    metrics_export_prometheus(mgr->metrics_buffer, sizeof(mgr->metrics_buffer) - 1);
    mgr->metrics_buffer_size = strlen(mgr->metrics_buffer);

    mgr->last_report_time = metrics_get_timestamp_ns();

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

size_t cupolas_monitoring_export(cupolas_monitoring_t* mgr, char* buffer, size_t size) {
    if (!mgr || !buffer || size == 0) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);

    size_t copied = 0;
    if (mgr->metrics_buffer_size > 0 && size > mgr->metrics_buffer_size) {
        memcpy(buffer, mgr->metrics_buffer, mgr->metrics_buffer_size);
        copied = mgr->metrics_buffer_size;
    }

    cupolas_rwlock_unlock(&mgr->lock);

    return copied;
}

/**
 * @brief Export metrics in OpenTelemetry OTLP JSON format
 * @param[in] mgr Monitoring manager handle
 * @param[out] buffer Output buffer for OTLP JSON format metrics
 * @param[in] size Size of output buffer in bytes
 * @return Number of bytes written to buffer, or 0 on error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership buffer: caller provides buffer, function writes to it
 *
 * @details
 * Generates OpenTelemetry Protocol (OTLP) JSON export format:
 *
 * {
 *   "resourceMetrics": [{
 *     "resource": {"attributes": [{"key": "service.name", "value": {"stringValue": "cupolas"}}]},
 *     "scopeMetrics": [{
 *       "scope": {"name": "cupolas.monitoring"},
 *       "metrics": [
 *         {"name": "cupolas_permission_checks_total", "description": "...", "unit": "1",
 *          "sum": {"dataPoints": [{"attributes": [...], "asInt": 1234}]}}
 *       ]
 *     }]
 *   }],
 *   "timestamp_ns": 1704067200000000000
 * }
 */
size_t cupolas_monitoring_export_otlp(cupolas_monitoring_t* mgr, char* buffer, size_t size) {
    if (!mgr || !buffer || size == 0) {
        return 0;
    }

    cupolas_rwlock_rdlock(&mgr->lock);

    if (mgr->metrics_buffer_size == 0) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    size_t written = 0;
    written += snprintf(buffer + written, size - written,
        "{\n"
        "  \"resourceMetrics\": [{\n"
        "    \"resource\": {\n"
        "      \"attributes\": [\n"
        "        {\"key\": \"service.name\", \"value\": {\"stringValue\": \"%s\"}}\n"
        "      ]\n"
        "    },\n"
        "    \"scopeMetrics\": [{\n"
        "      \"scope\": {\"name\": \"cupolas.monitoring\"},\n"
        "      \"metrics\": [\n",
        mgr->manager.opentelemetry.service_name ?
            mgr->manager.opentelemetry.service_name : "cupolas");

    if (written >= size) {
        cupolas_rwlock_unlock(&mgr->lock);
        return 0;
    }

    bool first_metric = true;
    const char* metric_data = mgr->metrics_buffer;
    const char* line_start = metric_data;
    int line_num = 0;

    while (*metric_data && written < size) {
        if (*metric_data == '\n' || *(metric_data + 1) == '\0') {
            size_t line_len = (size_t)(metric_data - line_start);
            if (line_len > 0 && line_len < 256) {
                char line[256];
                strncpy(line, line_start, line_len);
                line[line_len] = '\0';

                if (strncmp(line, "# HELP ", 7) == 0 ||
                    strncmp(line, "# TYPE ", 7) == 0) {
                    line_start = metric_data + 1;
                    metric_data++;
                    line_num++;
                    continue;
                }

                if (line[0] != '#' && line[0] != '\0' && strchr(line, ' ') != NULL) {
                    char metric_name[128] = {0};
                    char metric_value[64] = {0};

                    char* space_pos = strrchr(line, ' ');
                    if (space_pos) {
                        size_t name_len = (size_t)(space_pos - line);
                        if (name_len < sizeof(metric_name)) {
                            strncpy(metric_name, line, name_len);
                            metric_name[name_len] = '\0';
                            strncpy(metric_value, space_pos + 1, sizeof(metric_value) - 1);
                            metric_value[sizeof(metric_value) - 1] = '\0';

                            if (!first_metric) {
                                written += snprintf(buffer + written, size - written,
                                    ",\n");
                            }
                            first_metric = false;

                            written += snprintf(buffer + written, size - written,
                                "        {\n"
                                "          \"name\": \"%s\",\n"
                                "          \"description\": \"%s metric exported from cupolas\",\n"
                                "          \"unit\": \"1\",\n"
                                "          \"sum\": {\n"
                                "            \"isMonotonic\": true,\n"
                                "            \"aggregationTemporality\": 2,\n"
                                "            \"dataPoints\": [{\n"
                                "              \"timeUnixNano\": %llu,\n"
                                "              \"asInt\": %s\n"
                                "            }]\n"
                                "          }\n"
                                "        }",
                                metric_name, metric_name,
                                (unsigned long long)mgr->last_report_time,
                                metric_value);

                            if (written >= size) {
                                cupolas_rwlock_unlock(&mgr->lock);
                                return 0;
                            }
                        }
                    }
                }
            }
            line_start = metric_data + 1;
        }
        metric_data++;
    }

    written += snprintf(buffer + written, size - written,
        "\n      ]\n"
        "    }]\n"
        "  }],\n"
        "  \"timestamp_ns\": %llu\n"
        "}\n",
        (unsigned long long)mgr->last_report_time);

    cupolas_rwlock_unlock(&mgr->lock);

    return (written < size) ? written : 0;
}

int cupolas_monitoring_register_health_check(cupolas_monitoring_t* mgr,
                                         const char* name,
                                         health_check_fn_t callback) {
    if (!mgr || !name || !callback) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    if (mgr->health_check_count >= MAX_HEALTH_CHECKS) {
        cupolas_rwlock_unlock(&mgr->lock);
        return -1;
    }

    health_check_entry_t* entry = &mgr->health_checks[mgr->health_check_count++];
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    entry->callback = callback;
    entry->registered = true;

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

int cupolas_monitoring_check_health(cupolas_monitoring_t* mgr,
                                health_check_result_t* results,
                                size_t max_results) {
    if (!mgr || !results || max_results == 0) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);

    size_t count = 0;
    for (size_t i = 0; i < mgr->health_check_count && count < max_results; i++) {
        health_check_entry_t* entry = &mgr->health_checks[i];
        if (entry->registered && entry->callback) {
            results[count].timestamp_ns = metrics_get_timestamp_ns();
            results[count].healthy = entry->callback();
            results[count].component = entry->name;
            results[count].message = results[count].healthy ? "OK" : "FAILED";
            count++;
        }
    }

    cupolas_rwlock_unlock(&mgr->lock);

    return (int)count;
}

const char* cupolas_monitoring_get_listen_addr(cupolas_monitoring_t* mgr) {
    if (!mgr) return NULL;

    cupolas_rwlock_rdlock(&mgr->lock);
    static char addr[128];
    snprintf(addr, sizeof(addr), "%s:%u",
            mgr->manager.prometheus.listen_addr,
            mgr->manager.prometheus.port);
    cupolas_rwlock_unlock(&mgr->lock);

    return addr;
}

int cupolas_monitoring_set_filter(cupolas_monitoring_t* mgr,
                               const char** include_patterns,
                               const char** exclude_patterns) {
    if (!mgr) return -1;

    cupolas_rwlock_wrlock(&mgr->lock);

    mgr->include_count = 0;
    if (include_patterns) {
        for (size_t i = 0; include_patterns[i] && mgr->include_count < MAX_FILTER_PATTERNS; i++) {
            strncpy(mgr->include_patterns[mgr->include_count], include_patterns[i], MAX_PATTERN_LEN - 1);
            mgr->include_patterns[mgr->include_count][MAX_PATTERN_LEN - 1] = '\0';
            mgr->include_count++;
        }
    }

    mgr->exclude_count = 0;
    if (exclude_patterns) {
        for (size_t i = 0; exclude_patterns[i] && mgr->exclude_count < MAX_FILTER_PATTERNS; i++) {
            strncpy(mgr->exclude_patterns[mgr->exclude_count], exclude_patterns[i], MAX_PATTERN_LEN - 1);
            mgr->exclude_patterns[mgr->exclude_count][MAX_PATTERN_LEN - 1] = '\0';
            mgr->exclude_count++;
        }
    }

    cupolas_rwlock_unlock(&mgr->lock);

    return 0;
}

size_t cupolas_monitoring_get_metric_count(cupolas_monitoring_t* mgr) {
    if (!mgr) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);
    size_t count = metrics_get_count();
    cupolas_rwlock_unlock(&mgr->lock);

    return count;
}

uint64_t cupolas_monitoring_get_last_report_time(cupolas_monitoring_t* mgr) {
    if (!mgr) return 0;

    cupolas_rwlock_rdlock(&mgr->lock);
    uint64_t time = mgr->last_report_time;
    cupolas_rwlock_unlock(&mgr->lock);

    return time;
}

const char* cupolas_monitoring_get_last_error(cupolas_monitoring_t* mgr) {
    if (!mgr) return NULL;

    cupolas_rwlock_rdlock(&mgr->lock);
    const char* error = mgr->last_error[0] ? mgr->last_error : NULL;
    cupolas_rwlock_unlock(&mgr->lock);

    return error;
}

monitoring_config_t* monitoring_config_create_prometheus(uint16_t port) {
    monitoring_config_t* manager = (monitoring_config_t*)cupolas_mem_alloc(sizeof(monitoring_config_t));
    if (!manager) return NULL;

    memset(manager, 0, sizeof(monitoring_config_t));

    manager->backend = MONITORING_BACKEND_PROMETHEUS;
    manager->prometheus.listen_addr = "0.0.0.0";
    manager->prometheus.port = port;
    manager->prometheus.endpoint = "/metrics";
    manager->reporting_interval_ms = 10000;
    manager->buffer_size = MAX_METRICS_BUFFER;
    manager->enable_caching = true;

    return manager;
}

monitoring_config_t* monitoring_config_create_opentelemetry(const char* endpoint,
                                                           const char* service_name) {
    monitoring_config_t* manager = (monitoring_config_t*)cupolas_mem_alloc(sizeof(monitoring_config_t));
    if (!manager) return NULL;

    memset(manager, 0, sizeof(monitoring_config_t));

    manager->backend = MONITORING_BACKEND_OPENTELEMETRY;
    manager->opentelemetry.endpoint = endpoint;
    manager->opentelemetry.service_name = service_name;
    manager->reporting_interval_ms = 5000;
    manager->buffer_size = MAX_METRICS_BUFFER;
    manager->enable_caching = true;

    return manager;
}

void monitoring_config_destroy(monitoring_config_t* manager) {
    cupolas_mem_free(manager);
}

cupolas_monitoring_t* cupolas_monitoring_get_instance(void) {
    cupolas_rwlock_rdlock(&g_monitoring_lock);
    cupolas_monitoring_t* instance = g_monitoring;
    cupolas_rwlock_unlock(&g_monitoring_lock);
    return instance;
}

int cupolas_monitoring_init_instance(const monitoring_config_t* manager) {
    cupolas_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        cupolas_rwlock_unlock(&g_monitoring_lock);
        return 0;
    }

    g_monitoring = cupolas_monitoring_create(manager);
    if (!g_monitoring) {
        cupolas_rwlock_unlock(&g_monitoring_lock);
        return -1;
    }

    cupolas_rwlock_unlock(&g_monitoring_lock);

    return cupolas_monitoring_start(g_monitoring);
}

void cupolas_monitoring_shutdown_instance(void) {
    cupolas_rwlock_wrlock(&g_monitoring_lock);

    if (g_monitoring) {
        cupolas_monitoring_stop(g_monitoring);
        cupolas_monitoring_destroy(g_monitoring);
        g_monitoring = NULL;
    }

    cupolas_rwlock_unlock(&g_monitoring_lock);
}