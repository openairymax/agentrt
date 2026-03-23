/**
 * @file partdata_log.c
 * @brief AgentOS 数据分区日志管理实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "partdata_log.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#endif

#define PARTDATA_LOG_MAX_LINE_LEN 4096
#define PARTDATA_LOG_BUFFER_SIZE 8192
#define PARTDATA_LOG_MAX_SERVICE_LEN 64

static partdata_log_level_t g_log_level = PARTDATA_LOG_INFO;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE* g_main_log_file = NULL;
static char g_log_root_path[512] = {0};
static bool g_initialized = false;
static char g_current_date[16] = {0};

typedef struct {
    char service_name[PARTDATA_LOG_MAX_SERVICE_LEN];
    FILE* file;
    pthread_mutex_t lock;
} service_log_t;

static service_log_t g_service_logs[32];
static size_t g_service_log_count = 0;

static pthread_mutex_t g_service_lock = PTHREAD_MUTEX_INITIALIZER;

static bool ensure_directory(const char* path) {
    if (!path) return false;

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    for (size_t i = 0; i < len; i++) {
        if (path_copy[i] == '\\' || path_copy[i] == '/') {
            if (i > 0 && path_copy[i - 1] != ':') {
                path_copy[i] = '\0';
                mkdir(path_copy, 0755);
                path_copy[i] = '/';
            }
        }
    }

    mkdir(path_copy, 0755);
    return true;
}

static const char* level_to_string(partdata_log_level_t level) {
    switch (level) {
        case PARTDATA_LOG_ERROR: return "ERROR";
        case PARTDATA_LOG_WARN: return "WARN";
        case PARTDATA_LOG_INFO: return "INFO";
        case PARTDATA_LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

static const char* get_log_base_path(void) {
    static char base_path[256] = "partdata/logs";
    return base_path;
}

static void update_current_date(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(g_current_date, sizeof(g_current_date), "%Y-%m-%d", tm_info);
}

static FILE* get_main_log_file(void) {
    if (!g_initialized) {
        return NULL;
    }

    update_current_date();

    if (g_main_log_file) {
        return g_main_log_file;
    }

    const char* base = get_log_base_path();
    strncpy(g_log_root_path, base, sizeof(g_log_root_path) - 1);

    char kernel_path[512];
    snprintf(kernel_path, sizeof(kernel_path), "%s/kernel", base);
    ensure_directory(kernel_path);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/kernel/agentos.log", base);

    g_main_log_file = fopen(filepath, "a");
    return g_main_log_file;
}

static FILE* get_service_log_file(const char* service) {
    if (!service || !service[0]) {
        return get_main_log_file();
    }

    pthread_mutex_lock(&g_service_lock);

    for (size_t i = 0; i < g_service_log_count; i++) {
        if (strcmp(g_service_logs[i].service_name, service) == 0) {
            FILE* fp = g_service_logs[i].file;
            pthread_mutex_unlock(&g_service_lock);
            return fp;
        }
    }

    if (g_service_log_count < 32) {
        const char* base = get_log_base_path();
        char service_path[512];
        snprintf(service_path, sizeof(service_path), "%s/services", base);
        ensure_directory(service_path);

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/services/%s.log", base, service);

        FILE* fp = fopen(filepath, "a");
        if (fp) {
            strncpy(g_service_logs[g_service_log_count].service_name, service, PARTDATA_LOG_MAX_SERVICE_LEN - 1);
            g_service_logs[g_service_log_count].file = fp;
            pthread_mutex_init(&g_service_logs[g_service_log_count].lock, NULL);
            g_service_log_count++;
        }

        pthread_mutex_unlock(&g_service_lock);
        return fp;
    }

    pthread_mutex_unlock(&g_service_lock);
    return get_main_log_file();
}

partdata_error_t partdata_log_init(void) {
    if (g_initialized) {
        return PARTDATA_ERR_ALREADY_INITIALIZED;
    }

    const char* base = get_log_base_path();
    strncpy(g_log_root_path, base, sizeof(g_log_root_path) - 1);

    ensure_directory(base);
    ensure_directory("partdata/logs/kernel");
    ensure_directory("partdata/logs/services");
    ensure_directory("partdata/logs/apps");

    update_current_date();
    g_main_log_file = fopen("partdata/logs/kernel/agentos.log", "a");
    if (!g_main_log_file) {
        return PARTDATA_ERR_FILE_OPEN_FAILED;
    }

    g_initialized = true;
    g_log_level = PARTDATA_LOG_INFO;

    return PARTDATA_SUCCESS;
}

void partdata_log_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    pthread_mutex_lock(&g_log_lock);

    if (g_main_log_file) {
        fclose(g_main_log_file);
        g_main_log_file = NULL;
    }

    pthread_mutex_lock(&g_service_lock);
    for (size_t i = 0; i < g_service_log_count; i++) {
        if (g_service_logs[i].file) {
            fclose(g_service_logs[i].file);
            g_service_logs[i].file = NULL;
        }
        pthread_mutex_destroy(&g_service_logs[i].lock);
    }
    g_service_log_count = 0;
    pthread_mutex_unlock(&g_service_lock);

    g_initialized = false;

    pthread_mutex_unlock(&g_log_lock);
}

void partdata_log_write(partdata_log_level_t level, const char* service, const char* trace_id, const char* file, int line, const char* format, ...) {
    if (!g_initialized) {
        return;
    }

    if (level > g_log_level) {
        return;
    }

    va_list args;
    va_start(args, format);
    partdata_log_writev(level, service, trace_id, file, line, format, args);
    va_end(args);
}

void partdata_log_writev(partdata_log_level_t level, const char* service, const char* trace_id, const char* file, int line, const char* format, va_list args) {
    if (!g_initialized) {
        return;
    }

    if (level > g_log_level) {
        return;
    }

    pthread_mutex_lock(&g_log_lock);

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    char msec[8];
    snprintf(msec, sizeof(msec), "%03d", (int)((now % 1000)));

    char message[PARTDATA_LOG_MAX_LINE_LEN];
    vsnprintf(message, sizeof(message), format, args);

    const char* filename = file;
    const char* last_slash = strrchr(file, '/');
    if (last_slash) {
        filename = last_slash + 1;
    }

    FILE* fp = get_service_log_file(service);

    if (trace_id && trace_id[0]) {
        fprintf(fp, "%s.%s [%s] [%s] [trace=%s] [%s:%d] %s\n",
                timestamp, msec, level_to_string(level),
                service ? service : "unknown", trace_id, filename, line, message);
    } else {
        fprintf(fp, "%s.%s [%s] [%s] [%s:%d] %s\n",
                timestamp, msec, level_to_string(level),
                service ? service : "unknown", filename, line, message);
    }

    fflush(fp);

    pthread_mutex_unlock(&g_log_lock);
}

partdata_log_level_t partdata_log_get_level(void) {
    return g_log_level;
}

void partdata_log_set_level(partdata_log_level_t level) {
    g_log_level = level;
}

partdata_error_t partdata_log_get_service_path(const char* service, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    const char* base = get_log_base_path();
    if (service && service[0]) {
        snprintf(buffer, buffer_size, "%s/services/%s.log", base, service);
    } else {
        snprintf(buffer, buffer_size, "%s/kernel/agentos.log", base);
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_log_rotate(void) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_log_lock);

    if (g_main_log_file) {
        fclose(g_main_log_file);
        g_main_log_file = NULL;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char old_path[512];
    snprintf(old_path, sizeof(old_path), "partdata/logs/kernel/agentos.log");

    char new_path[512];
    snprintf(new_path, sizeof(new_path), "partdata/logs/kernel/agentos_%s.log", timestamp);

    rename(old_path, new_path);

    g_main_log_file = fopen(old_path, "a");

    pthread_mutex_unlock(&g_log_lock);

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_log_cleanup(int days_to_keep, uint64_t* freed_bytes) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_log_get_file_info(const char* service, partdata_log_file_info_t* info) {
    if (!info) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    memset(info, 0, sizeof(*info));

    char filepath[512];
    const char* base = get_log_base_path();

    if (service && service[0]) {
        snprintf(filepath, sizeof(filepath), "%s/services/%s.log", base, service);
    } else {
        snprintf(filepath, sizeof(filepath), "%s/kernel/agentos.log", base);
    }

    strncpy(info->path, filepath, sizeof(info->path) - 1);

#ifdef _WIN32
    struct _stat st;
    if (_stat(filepath, &st) == 0) {
#else
    struct stat st;
    if (stat(filepath, &st) == 0) {
#endif
        info->size_bytes = (uint64_t)st.st_size;
        info->created_at = st.st_ctime;
        info->modified_at = st.st_mtime;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_log_get_stats(uint32_t* total_files, uint64_t* total_size_bytes, time_t* oldest_timestamp) {
    if (!total_files || !total_size_bytes) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    *total_files = 0;
    *total_size_bytes = 0;
    *oldest_timestamp = 0;

    return PARTDATA_SUCCESS;
}
