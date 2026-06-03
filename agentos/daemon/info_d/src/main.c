#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_event_loop.h"
#include "atomic_compat.h"
#include "logging.h"
#include "platform.h"
#include "svc_logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#define INFO_D_DEFAULT_PORT 8083
#define INFO_D_MAX_BUFFER 65536
#define INFO_D_DEFAULT_SOCKET AGENTOS_RUNTIME_DIR "/info.sock"
#define INFO_D_COLLECT_INTERVAL_SEC 5
#define INFO_D_HISTORY_SIZE 64

typedef struct {
    double cpu_usage_pct;
    uint64_t total_memory_kb;
    uint64_t free_memory_kb;
    uint64_t used_memory_kb;
    double memory_usage_pct;
    uint64_t disk_total_kb;
    uint64_t disk_free_kb;
    uint64_t disk_used_kb;
    double disk_usage_pct;
    int cpu_cores;
    uint64_t uptime_sec;
    uint64_t timestamp;
} system_info_snapshot_t;

typedef struct {
    agentos_socket_t server_fd;
    agentos_mutex_t lock;
    agentos_thread_t collect_thread;
    atomic_int running;
    atomic_int collect_running;
    atomic_int force_stop;
    uint64_t start_time;
    uint64_t request_count;
    uint64_t error_count;
    uint64_t last_collect_time;
    system_info_snapshot_t latest_snapshot;
    system_info_snapshot_t history[INFO_D_HISTORY_SIZE];
    size_t history_count;
    size_t history_head;
    int tcp_port;
    char *socket_path;
} info_d_service_t;

static info_d_service_t g_service = {0};
static atomic_int g_shutdown = 0;
static agentos_event_loop_t *g_event_loop = NULL;

static void info_d_signal_handler(int sig)
{
    (void)sig;
    atomic_store_explicit(&g_shutdown, 1, memory_order_seq_cst);
    if (g_event_loop)
        agentos_event_loop_stop(g_event_loop);
}

static void svc_log_toggle_handler(int sig)
{
    (void)sig;
    static int debug_mode = 0;
    debug_mode = !debug_mode;
    log_set_module_level("*", debug_mode ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
}

static void info_d_handle_request(info_d_service_t *svc, agentos_socket_t client_fd);

static int info_d_on_client(int fd, uint32_t events, void *user_data)
{
    (void)events;
    info_d_service_t *svc = (info_d_service_t *)user_data;
    if (!svc || !svc->running) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "svc is NULL or not running");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_socket_t client = agentos_socket_accept(fd, 0);
    if (client != AGENTOS_INVALID_SOCKET) {
        info_d_handle_request(svc, client);
    }
    return 0;
}

static int info_d_collect_system_info(system_info_snapshot_t *snap)
{
    if (!snap) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "snap is NULL");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    memset(snap, 0, sizeof(*snap));
    snap->timestamp = (uint64_t)time(NULL);

#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    snap->cpu_cores = (int)sys_info.dwNumberOfProcessors;

    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        snap->total_memory_kb = (uint64_t)(mem_status.ullTotalPhys / 1024);
        snap->free_memory_kb = (uint64_t)(mem_status.ullAvailPhys / 1024);
        snap->used_memory_kb = snap->total_memory_kb - snap->free_memory_kb;
        if (snap->total_memory_kb > 0)
            snap->memory_usage_pct =
                (double)snap->used_memory_kb / (double)snap->total_memory_kb * 100.0;
    }

    ULARGE_INTEGER total_bytes, free_bytes, avail_bytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &avail_bytes, &total_bytes, &free_bytes)) {
        snap->disk_total_kb = (uint64_t)(total_bytes.QuadPart / 1024);
        snap->disk_free_kb = (uint64_t)(free_bytes.QuadPart / 1024);
        snap->disk_used_kb = snap->disk_total_kb - snap->disk_free_kb;
        if (snap->disk_total_kb > 0)
            snap->disk_usage_pct = (double)snap->disk_used_kb / (double)snap->disk_total_kb * 100.0;
    }
#else
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    snap->cpu_cores = (int)(nproc > 0 ? nproc : 1);

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        snap->total_memory_kb = (uint64_t)(si.totalram / 1024);
        snap->free_memory_kb = (uint64_t)(si.freeram / 1024);
        snap->used_memory_kb = snap->total_memory_kb - snap->free_memory_kb;
        if (snap->total_memory_kb > 0)
            snap->memory_usage_pct =
                (double)snap->used_memory_kb / (double)snap->total_memory_kb * 100.0;
        snap->uptime_sec = (uint64_t)si.uptime;
    }

    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        snap->disk_total_kb = (uint64_t)(vfs.f_blocks * vfs.f_frsize / 1024);
        snap->disk_free_kb = (uint64_t)(vfs.f_bfree * vfs.f_frsize / 1024);
        snap->disk_used_kb = snap->disk_total_kb - snap->disk_free_kb;
        if (snap->disk_total_kb > 0)
            snap->disk_usage_pct = (double)snap->disk_used_kb / (double)snap->disk_total_kb * 100.0;
    }

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0)
        clk_tck = 100;
    snap->cpu_usage_pct = 0.0;
#endif

    return 0;
}

#ifdef _WIN32
static DWORD WINAPI info_d_collect_loop(LPVOID arg)
{
#else
static void *info_d_collect_loop(void *arg)
{
#endif
    info_d_service_t *svc = (info_d_service_t *)arg;
    if (!svc) {
#ifdef _WIN32
        return 1;
#else
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
#endif
    }

    while (svc->collect_running) {
        system_info_snapshot_t snap;
        info_d_collect_system_info(&snap);

        agentos_mutex_lock(&svc->lock);
        memcpy(&svc->latest_snapshot, &snap, sizeof(snap));
        svc->last_collect_time = snap.timestamp;

        svc->history[svc->history_head] = snap;
        svc->history_head = (svc->history_head + 1) % INFO_D_HISTORY_SIZE;
        if (svc->history_count < INFO_D_HISTORY_SIZE)
            svc->history_count++;
        agentos_mutex_unlock(&svc->lock);

        for (int i = 0; i < INFO_D_COLLECT_INTERVAL_SEC && svc->collect_running; i++) {
#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int info_d_init(info_d_service_t *svc, int port, const char *sock)
{
    if (!svc)
        AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
    return AGENTOS_EINVAL;

    memset(svc, 0, sizeof(*svc));
    svc->tcp_port = port > 0 ? port : INFO_D_DEFAULT_PORT;
    svc->socket_path = sock ? AGENTOS_STRDUP(sock) : AGENTOS_STRDUP(INFO_D_DEFAULT_SOCKET);
    svc->start_time = (uint64_t)time(NULL);

    agentos_mutex_init(&svc->lock);
    agentos_socket_init();

    info_d_collect_system_info(&svc->latest_snapshot);
    svc->last_collect_time = svc->latest_snapshot.timestamp;

    SVC_LOG_INFO("info_d: init complete (cpu_cores=%d)", svc->latest_snapshot.cpu_cores);
    return AGENTOS_SUCCESS;
}

static int info_d_start(info_d_service_t *svc)
{
    if (!svc)
        AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
    return AGENTOS_EINVAL;

#ifndef _WIN32
    svc->server_fd = agentos_socket_create_unix_server(svc->socket_path);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("info_d: failed to create socket at %s", svc->socket_path);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to create unix socket");
        return AGENTOS_ERR_UNKNOWN;
    }
#else
    svc->server_fd = agentos_socket_create_tcp_server("127.0.0.1", (uint16_t)svc->tcp_port);
    if (svc->server_fd == AGENTOS_INVALID_SOCKET) {
        SVC_LOG_ERROR("info_d: failed to create TCP server");
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "failed to create TCP server");
        return AGENTOS_ERR_UNKNOWN;
    }
#endif

    svc->running = 1;
    svc->collect_running = 1;
    svc->force_stop = 0;

    agentos_thread_create(&svc->collect_thread, info_d_collect_loop, svc);

    SVC_LOG_INFO("info_d: service started (collect_interval=%ds)", INFO_D_COLLECT_INTERVAL_SEC);
    return AGENTOS_SUCCESS;
}

static int info_d_stop(info_d_service_t *svc, int force)
{
    if (!svc)
        AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
    return AGENTOS_EINVAL;

    agentos_mutex_lock(&svc->lock);
    svc->running = 0;
    svc->collect_running = 0;
    if (force)
        svc->force_stop = 1;
    agentos_mutex_unlock(&svc->lock);

    if (force) {
        agentos_thread_join(svc->collect_thread, NULL);
    } else {
        agentos_thread_join(svc->collect_thread, NULL);
    }

    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
        svc->server_fd = AGENTOS_INVALID_SOCKET;
    }

    if (force) {
#ifndef _WIN32
        unlink(svc->socket_path);
#endif
    }

    SVC_LOG_INFO("info_d: service stopped (force=%d, collections=%zu)", force, svc->history_count);
    return AGENTOS_SUCCESS;
}

static int info_d_destroy(info_d_service_t *svc)
{
    if (!svc)
        AGENTOS_ERROR_HANDLE(AGENTOS_EINVAL, "svc is NULL");
    return AGENTOS_EINVAL;

    if (svc->server_fd != AGENTOS_INVALID_SOCKET) {
        agentos_socket_close(svc->server_fd);
    }
    agentos_socket_cleanup();
    agentos_mutex_destroy(&svc->lock);
    AGENTOS_FREE(svc->socket_path);
    memset(svc, 0, sizeof(*svc));
    SVC_LOG_INFO("info_d: service destroyed");
    return AGENTOS_SUCCESS;
}

static uint64_t info_d_healthcheck(info_d_service_t *svc)
{
    if (!svc)
        return 0;

    uint64_t last_time = 0;
    agentos_mutex_lock(&svc->lock);
    last_time = svc->last_collect_time;
    agentos_mutex_unlock(&svc->lock);

    if (!svc->running)
        return 0;

    uint64_t now = (uint64_t)time(NULL);
    uint64_t staleness = now > last_time ? now - last_time : 0;
    if (staleness > (uint64_t)(INFO_D_COLLECT_INTERVAL_SEC * 3))
        return 0;

    return last_time;
}

static void info_d_handle_request(info_d_service_t *svc, agentos_socket_t client_fd)
{
    char buffer[INFO_D_MAX_BUFFER];
    ssize_t n = agentos_socket_recv(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        agentos_socket_close(client_fd);
        return;
    }
    buffer[n] = '\0';

    agentos_mutex_lock(&svc->lock);
    svc->request_count++;
    system_info_snapshot_t snap = svc->latest_snapshot;
    agentos_mutex_unlock(&svc->lock);

    char response[8192];
#ifdef _WIN32
    const char *platform_name = "Windows";
#else
    struct utsname uts;
    const char *platform_name = "Linux";
    if (uname(&uts) == 0) {
        platform_name = uts.sysname;
    }
#endif

    uint64_t service_uptime = (uint64_t)time(NULL) - svc->start_time;
    uint64_t last_collect = info_d_healthcheck(svc);

    snprintf(response, sizeof(response),
             "{"
             "\"service\":\"info_d\","
             "\"status\":\"ok\","
             "\"platform\":\"%s\","
             "\"service_uptime_sec\":%llu,"
             "\"requests\":%llu,"
             "\"errors\":%llu,"
             "\"healthy\":%s,"
             "\"system\":{"
             "\"cpu_cores\":%d,"
             "\"cpu_usage_pct\":%.2f,"
             "\"total_memory_kb\":%llu,"
             "\"free_memory_kb\":%llu,"
             "\"used_memory_kb\":%llu,"
             "\"memory_usage_pct\":%.2f,"
             "\"disk_total_kb\":%llu,"
             "\"disk_free_kb\":%llu,"
             "\"disk_used_kb\":%llu,"
             "\"disk_usage_pct\":%.2f,"
             "\"system_uptime_sec\":%llu"
             "},"
             "\"collection\":{"
             "\"interval_sec\":%d,"
             "\"last_collect_time\":%llu,"
             "\"history_count\":%zu"
             "}"
             "}",
             platform_name, (unsigned long long)service_uptime,
             (unsigned long long)svc->request_count, (unsigned long long)svc->error_count,
             last_collect > 0 ? "true" : "false", snap.cpu_cores, snap.cpu_usage_pct,
             (unsigned long long)snap.total_memory_kb, (unsigned long long)snap.free_memory_kb,
             (unsigned long long)snap.used_memory_kb, snap.memory_usage_pct,
             (unsigned long long)snap.disk_total_kb, (unsigned long long)snap.disk_free_kb,
             (unsigned long long)snap.disk_used_kb, snap.disk_usage_pct,
             (unsigned long long)snap.uptime_sec, INFO_D_COLLECT_INTERVAL_SEC,
             (unsigned long long)last_collect, svc->history_count);

    agentos_socket_send(client_fd, response, strlen(response));
    agentos_socket_close(client_fd);
}

int main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{

#ifdef _WIN32
    SetConsoleCtrlHandler(NULL, TRUE);
#else
    signal(SIGINT, info_d_signal_handler);
    signal(SIGTERM, info_d_signal_handler);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, svc_log_toggle_handler);
#endif

    agentos_log_init(NULL);
    atexit(log_cleanup);

    if (info_d_init(&g_service, INFO_D_DEFAULT_PORT, INFO_D_DEFAULT_SOCKET) != AGENTOS_SUCCESS)
        return 1;
    if (info_d_start(&g_service) != AGENTOS_SUCCESS) {
        info_d_destroy(&g_service);
        return 1;
    }

    g_event_loop = agentos_event_loop_create(64);
    if (!g_event_loop) {
        LOG_ERROR("Failed to create event loop");
        info_d_stop(&g_service, 1);
        info_d_destroy(&g_service);
        return 1;
    }

    agentos_event_loop_add_fd(g_event_loop, (int)g_service.server_fd, AGENTOS_EVENT_TYPE_READ,
                              info_d_on_client, &g_service);

    LOG_INFO("info_d running with epoll event loop on fd=%d", (int)g_service.server_fd);
    agentos_event_loop_run(g_event_loop);

    agentos_event_loop_destroy(g_event_loop);
    g_event_loop = NULL;

    info_d_stop(&g_service, g_shutdown ? 1 : 0);
    info_d_destroy(&g_service);
    log_cleanup();
    return 0;
}
