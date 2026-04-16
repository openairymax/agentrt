/**
 * @file heapstore_log.c
 * @brief AgentOS 数据分区日志管理实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
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
#define heapstore_LOG_MAX_SERVICES 32
#define heapstore_LOG_MAX_PATH 512

static heapstore_log_level_t s_log_level = heapstore_LOG_INFO;
static pthread_mutex_t s_log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE* s_main_log_file = NULL;
static char s_log_root_path[heapstore_LOG_MAX_PATH] = {0};
static bool s_initialized = false;
static char s_current_date[16] = {0};

typedef struct {
    char service_name[heapstore_LOG_MAX_SERVICE_LEN];
    FILE* file;
    pthread_mutex_t lock;
} service_log_t;

static service_log_t s_service_logs[heapstore_LOG_MAX_SERVICES];
static size_t s_service_log_count = 0;

static pthread_mutex_t s_service_lock = PTHREAD_MUTEX_INITIALIZER;

static const char* level_to_string(heapstore_log_level_t level) {
    switch (level) {
        case heapstore_LOG_ERROR: return "ERROR";
        case heapstore_LOG_WARN: return "WARN";
        case heapstore_LOG_INFO: return "INFO";
        case heapstore_LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

static const char* get_log_base_path(void) {
    static char base_path[256] = "agentos/heapstore/logs";
    return base_path;
}

static void update_current_date(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(s_current_date, sizeof(s_current_date), "%Y-%m-%d", tm_info);
}

static FILE* get_main_log_file(void) {
    if (!s_initialized) {
        return NULL;
    }

    update_current_date();

    if (s_main_log_file) {
        return s_main_log_file;
    }

    const char* base = get_log_base_path();
    strncpy(s_log_root_path, base, sizeof(s_log_root_path) - 1);

    char kernel_path[heapstore_LOG_MAX_PATH];
    snprintf(kernel_path, sizeof(kernel_path), "%s/kernel", base);
    heapstore_ensure_directory(kernel_path);

    char filepath[heapstore_LOG_MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s/kernel/agentos.log", base);

    s_main_log_file = fopen(filepath, "a");
    return s_main_log_file;
}

static FILE* get_service_log_file(const char* service) {
    if (!service || !service[0]) {
        return get_main_log_file();
    }

    char safe_service[heapstore_LOG_MAX_SERVICE_LEN];
    if (heapstore_sanitize_path_component(safe_service, service, sizeof(safe_service)) != 0) {
        fprintf(stderr, "[heapstore_LOG SECURITY] Rejected unsafe service name: %s\n", service);
        return NULL;
    }

    pthread_mutex_lock(&s_service_lock);

    for (size_t i = 0; i < s_service_log_count; i++) {
        if (strcmp(s_service_logs[i].service_name, safe_service) == 0) {
            FILE* fp = s_service_logs[i].file;
            pthread_mutex_unlock(&s_service_lock);
            return fp;
        }
    }

    if (s_service_log_count < heapstore_LOG_MAX_SERVICES) {
        const char* base = get_log_base_path();
        char service_path[heapstore_LOG_MAX_PATH];
        snprintf(service_path, sizeof(service_path), "%s/services", base);
        heapstore_ensure_directory(service_path);

        char filepath[heapstore_LOG_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/services/%s.log", base, safe_service);

        FILE* fp = fopen(filepath, "a");
        if (fp) {
            strncpy(s_service_logs[s_service_log_count].service_name, safe_service, heapstore_LOG_MAX_SERVICE_LEN - 1);
            s_service_logs[s_service_log_count].file = fp;
            pthread_mutex_init(&s_service_logs[s_service_log_count].lock, NULL);
            s_service_log_count++;
        }

        pthread_mutex_unlock(&s_service_lock);
        return fp;
    }

    pthread_mutex_unlock(&s_service_lock);
    return get_main_log_file();
}

heapstore_error_t heapstore_log_init(void) {
    if (s_initialized) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }

    const char* base = get_log_base_path();
    strncpy(s_log_root_path, base, sizeof(s_log_root_path) - 1);

    heapstore_ensure_directory(base);
    heapstore_ensure_directory("agentos/heapstore/logs/kernel");
    heapstore_ensure_directory("agentos/heapstore/logs/services");
    heapstore_ensure_directory("agentos/heapstore/logs/apps");

    update_current_date();
    s_main_log_file = fopen("agentos/heapstore/logs/kernel/agentos.log", "a");
    if (!s_main_log_file) {
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    s_initialized = true;
    s_log_level = heapstore_LOG_INFO;

    return heapstore_SUCCESS;
}

void heapstore_log_shutdown(void) {
    if (!s_initialized) {
        fprintf(stderr, "[heapstore_LOG WARN] Shutdown called but not initialized\n");
        return;
    }

    pthread_mutex_lock(&s_log_lock);

    if (s_main_log_file) {
        fflush(s_main_log_file);
        fclose(s_main_log_file);
        fprintf(stdout, "[heapstore_LOG INFO] Main log file closed\n");
        s_main_log_file = NULL;
    }

    pthread_mutex_lock(&s_service_lock);
    for (size_t i = 0; i < s_service_log_count; i++) {
        if (s_service_logs[i].file) {
            fflush(s_service_logs[i].file);
            fclose(s_service_logs[i].file);
            s_service_logs[i].file = NULL;
        }
        pthread_mutex_destroy(&s_service_logs[i].lock);
    }
    fprintf(stdout, "[heapstore_LOG INFO] Closed %zu service log files\n", s_service_log_count);
    s_service_log_count = 0;
    pthread_mutex_unlock(&s_service_lock);

    s_initialized = false;
    fprintf(stdout, "[heapstore_LOG INFO] Logging system shutdown complete\n");

    pthread_mutex_unlock(&s_log_lock);
}

void heapstore_log_write(heapstore_log_level_t level,
                        const char* service,
                        const char* trace_id,
                        const char* file,
                        int line,
                        const char* format,
                        ...) {
    if (!s_initialized) {
        return;
    }

    if (level > s_log_level) {
        return;
    }

    update_current_date();

    pthread_mutex_lock(&s_log_lock);

    FILE* fp = service ? get_service_log_file(service) : get_main_log_file();
    if (!fp) {
        pthread_mutex_unlock(&s_log_lock);
        return;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    const char* level_str = level_to_string(level);

    va_list args;
    va_start(args, format);

    fprintf(fp, "[%s] [%s] [%s] [%s:%d] ",
            s_current_date, timestamp, level_str, file, line);

    if (trace_id) {
        fprintf(fp, "[trace:%s] ", trace_id);
    }

    vfprintf(fp, format, args);
    fprintf(fp, "\n");
    fflush(fp);

    va_end(args);

    pthread_mutex_unlock(&s_log_lock);
}

void heapstore_log_writev(heapstore_log_level_t level,
                         const char* service,
                         const char* trace_id,
                         const char* file,
                         int line,
                         const char* format,
                         va_list args) {
    if (!s_initialized) {
        return;
    }

    if (level > s_log_level) {
        return;
    }

    pthread_mutex_lock(&s_log_lock);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t now = ts.tv_sec;
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    char msec[8];
    snprintf(msec, sizeof(msec), "%03d", (int)(ts.tv_nsec / 1000000));

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

    pthread_mutex_unlock(&s_log_lock);
}

heapstore_log_level_t heapstore_log_get_level(void) {
    return s_log_level;
}

void heapstore_log_set_level(heapstore_log_level_t level) {
    s_log_level = level;
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
    if (!s_initialized) {
        fprintf(stderr, "[heapstore_LOG ERROR] Log rotate called but not initialized\n");
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_log_lock);

    if (s_main_log_file) {
        fflush(s_main_log_file);
        fclose(s_main_log_file);
        s_main_log_file = NULL;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char old_path[heapstore_LOG_MAX_PATH];
    snprintf(old_path, sizeof(old_path), "agentos/heapstore/logs/kernel/agentos.log");

    char new_path[heapstore_LOG_MAX_PATH];
    snprintf(new_path, sizeof(new_path), "agentos/heapstore/logs/kernel/agentos_%s.log", timestamp);

    if (rename(old_path, new_path) != 0) {
        fprintf(stderr, "[heapstore_LOG ERROR] Failed to rotate log file: %s -> %s\n", old_path, new_path);
        pthread_mutex_unlock(&s_log_lock);
        return heapstore_ERR_FILE_OPERATION_FAILED;
    }

    s_main_log_file = fopen(old_path, "a");
    if (!s_main_log_file) {
        fprintf(stderr, "[heapstore_LOG ERROR] Failed to create new log file after rotation: %s\n", old_path);
        pthread_mutex_unlock(&s_log_lock);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(stdout, "[heapstore_LOG INFO] Log rotated: %s -> %s\n", old_path, new_path);
    pthread_mutex_unlock(&s_log_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_cleanup(int days_to_keep, uint64_t* freed_bytes) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    if (days_to_keep <= 0) {
        return heapstore_SUCCESS;
    }

    time_t cutoff_time = time(NULL) - (days_to_keep * 86400);

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char search_path[heapstore_LOG_MAX_PATH];

    snprintf(search_path, sizeof(search_path), "%s/*", get_log_base_path());

    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) {
        return heapstore_SUCCESS;
    }

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char filepath[heapstore_LOG_MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s/%s", get_log_base_path(), find_data.cFileName);

            FILETIME ft_write = find_data.ftLastWriteTime;
            ULARGE_INTEGER uli;
            uli.LowPart = ft_write.dwLowDateTime;
            uli.HighPart = ft_write.dwHighDateTime;
            time_t file_time = (time_t)((uli.QuadPart - 116444736000000000ULL) / 10000000);

            if (file_time < cutoff_time) {
                uint64_t file_size = ((uint64_t)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow;
                if (DeleteFileA(filepath)) {
                    if (freed_bytes) *freed_bytes += file_size;
                }
            }
        }
    } while (FindNextFileA(h_find, &find_data));

    FindClose(h_find);
#else
    DIR* dir = opendir(get_log_base_path());
    if (!dir) {
        return heapstore_SUCCESS;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        char filepath[heapstore_LOG_MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", get_log_base_path(), entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (st.st_mtime < cutoff_time) {
                uint64_t file_size = (uint64_t)st.st_size;
                if (unlink(filepath) == 0) {
                    if (freed_bytes) *freed_bytes += file_size;
                }
            }
        }
    }

    closedir(dir);
#endif

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_get_file_info(const char* service, heapstore_log_file_info_t* info) {
    if (!info) {
        fprintf(stderr, "[heapstore_LOG ERROR] Invalid info parameter (NULL)\n");
        return heapstore_ERR_INVALID_PARAM;
    }

    memset(info, 0, sizeof(*info));

    char filepath[heapstore_LOG_MAX_PATH];
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

bool heapstore_log_is_healthy(void) {
    return s_initialized && s_main_log_file != NULL;
}
