/**
 * @file heapstore_log.c
 * @brief AgentOS 数据分区日志管理实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "heapstore_log.h"
#include "private.h"
#include "utils.h"

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

#define heapstore_LOG_MAX_LINE_LEN 4096
#define heapstore_LOG_BUFFER_SIZE 8192
#define heapstore_LOG_MAX_SERVICE_LEN 64

static heapstore_log_level_t s_log_level = heapstore_LOG_INFO;
static pthread_mutex_t s_log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE* s_main_log_file = NULL;
static char s_log_root_path[512] = {0};
static bool s_initialized = false;
static char s_current_date[16] = {0};

typedef struct {
    char service_name[heapstore_LOG_MAX_SERVICE_LEN];
    FILE* file;
    pthread_mutex_t lock;
} service_log_t;

static service_log_t s_service_logs[32];
static size_t s_service_log_count = 0;

static pthread_mutex_t s_service_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 将日志级别转换为字符串
 *
 * @param level 日志级别
 * @return const char* 级别对应的字符串
 */
static const char* level_to_string(heapstore_log_level_t level) {
    switch (level) {
        case heapstore_LOG_ERROR: return "ERROR";
        case heapstore_LOG_WARN: return "WARN";
        case heapstore_LOG_INFO: return "INFO";
        case heapstore_LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 获取日志基础路径
 *
 * @return const char* 日志基础路径
 */
static const char* get_log_base_path(void) {
    static char base_path[256] = "heapstore/logs";
    return base_path;
}

/**
 * @brief 更新当前日期缓存
 */
static void update_current_date(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(s_current_date, sizeof(g_current_date), "%Y-%m-%d", tm_info);
}

static FILE* get_main_log_file(void) {
    if (!s_initialized) {
        return NULL;
    }

    update_current_date();

    if (s_main_log_file) {
        return g_main_log_file;
    }

    const char* base = get_log_base_path();
    strncpy(s_log_root_path, base, sizeof(g_log_root_path) - 1);

    char kernel_path[512];
    snprintf(kernel_path, sizeof(kernel_path), "%s/kernel", base);
    heapstore_ensure_directory(kernel_path);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/kernel/agentos.log", base);

    g_main_log_file = fopen(filepath, "a");
    return g_main_log_file;
}

/**
 * @brief 获取服务日志文件
 *
 * @param service 服务名称，如果为 NULL 或空则返回主日志文件
 * @return FILE* 日志文件指针
 */
static FILE* get_service_log_file(const char* service) {
    if (!service || !service[0]) {
        return get_main_log_file();
    }

    pthread_mutex_lock(&s_service_lock);

    for (size_t i = 0; i < s_service_log_count; i++) {
        if (strcmp(s_service_logs[i].service_name, service) == 0) {
            FILE* fp = g_service_logs[i].file;
            pthread_mutex_unlock(&g_service_lock);
            return fp;
        }
    }

    if (g_service_log_count < 32) {
        const char* base = get_log_base_path();
        char service_path[512];
        snprintf(service_path, sizeof(service_path), "%s/services", base);
        heapstore_ensure_directory(service_path);

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/services/%s.log", base, service);

        FILE* fp = fopen(filepath, "a");
        if (fp) {
            strncpy(g_service_logs[g_service_log_count].service_name, service, heapstore_LOG_MAX_SERVICE_LEN - 1);
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

heapstore_error_t heapstore_log_init(void) {
    if (g_initialized) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }

    const char* base = get_log_base_path();
    strncpy(g_log_root_path, base, sizeof(g_log_root_path) - 1);

    heapstore_ensure_directory(base);
    heapstore_ensure_directory("heapstore/logs/kernel");
    heapstore_ensure_directory("heapstore/logs/services");
    heapstore_ensure_directory("heapstore/logs/apps");

    update_current_date();
    g_main_log_file = fopen("heapstore/logs/kernel/agentos.log", "a");
    if (!g_main_log_file) {
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    g_initialized = true;
    s_log_level = heapstore_LOG_INFO;

    return heapstore_SUCCESS;
}

void heapstore_log_shutdown(void) {
    if (!g_initialized) {
        fprintf(stderr, "[heapstore_LOG WARN] Shutdown called but not initialized\n");
        return;
    }

    pthread_mutex_lock(&s_log_lock);

    if (g_main_log_file) {
        fflush(g_main_log_file);
        fclose(g_main_log_file);
        fprintf(stdout, "[heapstore_LOG INFO] Main log file closed\n");
        g_main_log_file = NULL;
    }

    pthread_mutex_lock(&g_service_lock);
    for (size_t i = 0; i < g_service_log_count; i++) {
        if (g_service_logs[i].file) {
            fflush(g_service_logs[i].file);
            fclose(g_service_logs[i].file);
            g_service_logs[i].file = NULL;
        }
        pthread_mutex_destroy(&g_service_logs[i].lock);
    }
    fprintf(stdout, "[heapstore_LOG INFO] Closed %zu service log files\n", g_service_log_count);
    g_service_log_count = 0;
    pthread_mutex_unlock(&g_service_lock);

    g_initialized = false;
    fprintf(stdout, "[heapstore_LOG INFO] Logging system shutdown complete\n");

    pthread_mutex_unlock(&g_log_lock);
}

void heapstore_log_write(heapstore_log_level_t level, 
                        const char* service, 
                        const char* trace_id, 
                        const char* file, 
                        int line, 
                        const char* format, ...) {
    if (!g_initialized) {
        return;
    }

    if (level > g_log_level) {
        return;
    }

    va_list args;
    va_start(args, format);
    heapstore_log_writev(level, service, trace_id, file, line, format, args);
    va_end(args);
}

void heapstore_log_writev(heapstore_log_level_t level, 
                         const char* service, 
                         const char* trace_id, 
                         const char* file, 
                         int line, 
                         const char* format, 
                         va_list args) {
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

    char message[heapstore_LOG_MAX_LINE_LEN];
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

heapstore_log_level_t heapstore_log_get_level(void) {
    return g_log_level;
}

void heapstore_log_set_level(heapstore_log_level_t level) {
    g_log_level = level;
}

heapstore_error_t heapstore_log_get_service_path(const char* service, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        fprintf(stderr, "[heapstore_LOG ERROR] Invalid buffer parameter\n");
        return heapstore_ERR_INVALID_PARAM;
    }

    const char* base = get_log_base_path();
    if (service && service[0]) {
        snprintf(buffer, buffer_size, "%s/services/%s.log", base, service);
    } else {
        snprintf(buffer, buffer_size, "%s/kernel/agentos.log", base);
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_rotate(void) {
    if (!g_initialized) {
        fprintf(stderr, "[heapstore_LOG ERROR] Log rotate called but not initialized\n");
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_log_lock);

    if (g_main_log_file) {
        fflush(g_main_log_file);
        fclose(g_main_log_file);
        g_main_log_file = NULL;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char old_path[512];
    snprintf(old_path, sizeof(old_path), "heapstore/logs/kernel/agentos.log");

    char new_path[512];
    snprintf(new_path, sizeof(new_path), "heapstore/logs/kernel/agentos_%s.log", timestamp);

    if (rename(old_path, new_path) != 0) {
        fprintf(stderr, "[heapstore_LOG ERROR] Failed to rotate log file: %s -> %s\n", old_path, new_path);
        pthread_mutex_unlock(&g_log_lock);
        return heapstore_ERR_FILE_OPERATION_FAILED;
    }

    g_main_log_file = fopen(old_path, "a");
    if (!g_main_log_file) {
        fprintf(stderr, "[heapstore_LOG ERROR] Failed to create new log file after rotation: %s\n", old_path);
        pthread_mutex_unlock(&g_log_lock);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(stdout, "[heapstore_LOG INFO] Log rotated: %s -> %s\n", old_path, new_path);
    pthread_mutex_unlock(&g_log_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_cleanup(int days_to_keep, uint64_t* freed_bytes) {
    if (!g_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_get_file_info(const char* service, heapstore_log_file_info_t* info) {
    if (!info) {
        fprintf(stderr, "[heapstore_LOG ERROR] Invalid info parameter (NULL)\n");
        return heapstore_ERR_INVALID_PARAM;
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
    } else {
        fprintf(stderr, "[heapstore_LOG WARN] Failed to get file info: %s\n", filepath);
        return heapstore_ERR_FILE_NOT_FOUND;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_get_stats(uint32_t* total_files, uint64_t* total_size_bytes, time_t* oldest_timestamp) {
    if (!total_files || !total_size_bytes) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *total_files = 0;
    *total_size_bytes = 0;
    *oldest_timestamp = 0;

    return heapstore_SUCCESS;
}

