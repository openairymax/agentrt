#include "error.h"
/**
 * @file log_store_service.c
 * @brief 内核日志存储服务实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "../../include/heapstore.h"
#include "../../include/heapstore_log.h"
#include "../../include/utils.h"
#include "atomic_compat.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "memory_compat.h"

static void log_store_service_check_rotation(const char *current_file);
#include "agentos_dirent.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/**
 * @brief 日志存储服务上下文
 */
typedef struct {
    char storage_path[512];
    uint64_t max_storage_bytes;
    uint32_t rotation_count;
    atomic_uint_fast32_t is_initialized;
} log_store_service_ctx_t;

static log_store_service_ctx_t g_ctx = {0};

/**
 * @brief 初始化日志存储服务
 *
 * @param storage_path 存储路径
 * @param max_storage_bytes 最大存储字节数
 * @return int 0成功，非0错误码
 */
int log_store_service_init(const char *storage_path, uint64_t max_storage_bytes)
{
    if (!storage_path) {
        return AGENTOS_EINVAL;
    }

    if (atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire)) {
        return 0;
    }

    // 设置存储路径
    AGENTOS_STRNCPY_TERM(g_ctx.storage_path, storage_path, sizeof(g_ctx.storage_path));

    g_ctx.max_storage_bytes =
        max_storage_bytes > 0 ? max_storage_bytes : 100 * 1024 * 1024;  // 默认100MB
    g_ctx.rotation_count = 10;  // 默认保留10个日志文件

    // 创建存储目录
#ifdef _WIN32
    if (_mkdir(g_ctx.storage_path) != 0) {
        // 如果目录已存在，忽略错误
        if (errno != EEXIST) {
            return AGENTOS_ERR_NOT_FOUND;
        }
    }
#else
    if (mkdir(g_ctx.storage_path, 0755) != 0) {
        // 如果目录已存在，忽略错误
        if (errno != EEXIST) {
            return AGENTOS_ERR_NOT_FOUND;
        }
    }
#endif

    atomic_store_explicit(&g_ctx.is_initialized, 1, memory_order_release);
    return 0;
}

/**
 * @brief 存储日志条目
 *
 * @param level 日志级别
 * @param component 组件名称
 * @param message 日志消息
 * @param timestamp 时间戳（NULL表示使用当前时间）
 * @return int 0成功，非0错误码
 */
int log_store_service_store_entry(heapstore_log_level_t level, const char *component,
                                  const char *message, const time_t *timestamp)
{
    if (!atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire) || !component ||
        !message) {
        return AGENTOS_EINVAL;
    }

    time_t now = timestamp ? *timestamp : time(NULL);
    struct tm tm_buf;
    struct tm *tm_info = localtime_r(&now, &tm_buf);
    if (!tm_info) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    // 构建日志文件名
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/log_%04d%02d%02d.log", g_ctx.storage_path,
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);

    // 打开日志文件
    FILE *f = fopen(filename, "a");
    if (!f) {
        return AGENTOS_ERR_NULL_POINTER;
    }

    // 写入日志条目
    const char *level_str = "UNKNOWN";
    switch (level) {
    case HEAPSTORE_LOG_FATAL:
        level_str = "FATAL";
        break;
    case HEAPSTORE_LOG_ERROR:
        level_str = "ERROR";
        break;
    case HEAPSTORE_LOG_WARN:
        level_str = "WARN";
        break;
    case HEAPSTORE_LOG_INFO:
        level_str = "INFO";
        break;
    case HEAPSTORE_LOG_DEBUG:
        level_str = "DEBUG";
        break;
    }

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    {
    char _buf[512];
    snprintf(_buf, sizeof(_buf), "[%s] [%s] [%s] %s\n", time_buf, level_str, component, message);
    fputs(_buf, f);
}
    fclose(f);

    log_store_service_check_rotation(filename);

    return 0;
}

/**
 * @brief 检查并执行日志轮转
 *
 * @param current_file 当前日志文件
 */
static void log_store_service_check_rotation(const char *current_file)
{
    // 获取文件大小
    FILE *f = fopen(current_file, "rb");
    if (!f) {
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);

    // 如果文件超过最大大小，执行轮转
    if (file_size > (long)g_ctx.max_storage_bytes) {
        // 这里可以实现更复杂的轮转逻辑
        // 目前只是简单的实现
        char backup_file[512];
        snprintf(backup_file, sizeof(backup_file), "%s.1", current_file);

        // 在实际实现中，这里应该处理多个备份文件
        remove(backup_file);
        rename(current_file, backup_file);
    }
}

/**
 * @brief 查询存储的日志
 *
 * @param start_time 开始时间
 * @param end_time 结束时间
 * @param level 日志级别（可选的过滤条件）
 * @param component 组件名称（可选的过滤条件）
 * @param out_entries 输出日志条目数组
 * @param max_entries 最大条目数
 * @return int 返回的条目数，或错误码
 */
int log_store_service_query_entries(const time_t *start_time, const time_t *end_time,
                                    heapstore_log_level_t level, const char *component,
                                    char ***out_entries, int max_entries)
{
    if (!out_entries || max_entries <= 0) {
        return AGENTOS_EINVAL;
    }

    if (!atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire)) {
        return AGENTOS_ERR_OVERFLOW;
    }

    *out_entries = NULL;

    DIR *dir = opendir(g_ctx.storage_path);
    if (!dir) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char **results;
    SAFE_MALLOC_ARRAY(results, max_entries, sizeof(char *));
    if (!results) {
        closedir(dir);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    int found_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && found_count < max_entries) {
        if (strncmp(entry->d_name, "log_", 4) != 0) {
            continue;
        }

        char filepath[768];
        snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx.storage_path, entry->d_name);

        FILE *f = fopen(filepath, "r");
        if (!f) {
            continue;
        }

        char line[2048];
        while (fgets(line, sizeof(line), f) != NULL && found_count < max_entries) {
            line[strcspn(line, "\r\n")] = '\0';

            if (strlen(line) < 20) {
                continue;
            }

            time_t log_time = 0;
            char level_str[16] = {0};
            char comp_str[128] = {0};

            const char *p = line;
            const char *bracket1 = strchr(p, '[');
            if (!bracket1) { continue; }
            const char *close1 = strchr(bracket1, ']');
            if (!close1) { continue; }
            const char *bracket2 = strchr(close1, '[');
            if (!bracket2) { continue; }
            const char *close2 = strchr(bracket2, ']');
            if (!close2) { continue; }
            size_t level_len = (size_t)(close2 - bracket2 - 1);
            if (level_len >= sizeof(level_str)) { level_len = sizeof(level_str) - 1; }
            __builtin_memcpy(level_str, bracket2 + 1, level_len);
            level_str[level_len] = '\0';
            const char *bracket3 = strchr(close2, '[');
            int parsed = 1;
            if (bracket3) {
                const char *close3 = strchr(bracket3, ']');
                if (close3) {
                    size_t comp_len = (size_t)(close3 - bracket3 - 1);
                    if (comp_len >= sizeof(comp_str)) { comp_len = sizeof(comp_str) - 1; }
                    __builtin_memcpy(comp_str, bracket3 + 1, comp_len);
                    comp_str[comp_len] = '\0';
                    parsed = 2;
                }
            }
            if (parsed < 2) {
                continue;
            }

            struct tm tm_info;
            __builtin_memset(&tm_info, 0, sizeof(tm_info));
            char time_buf[32];
            const char *ts_start = line + 1;
            const char *ts_end = strchr(ts_start, ']');
            if (ts_end && (size_t)(ts_end - ts_start) > 0) {
                size_t ts_len = (size_t)(ts_end - ts_start);
                if (ts_len >= sizeof(time_buf)) { ts_len = sizeof(time_buf) - 1; }
                __builtin_memcpy(time_buf, ts_start, ts_len);
                time_buf[ts_len] = '\0';
                strptime(time_buf, "%Y-%m-%d %H:%M:%S", &tm_info);
                log_time = mktime(&tm_info);
            }

            if (start_time && *start_time > 0 && log_time < *start_time) {
                continue;
            }
            if (end_time && *end_time > 0 && log_time > *end_time) {
                continue;
            }

            heapstore_log_level_t entry_level = HEAPSTORE_LOG_INFO;
            if (strcmp(level_str, "ERROR") == 0)
                entry_level = HEAPSTORE_LOG_ERROR;
            else if (strcmp(level_str, "WARN") == 0)
                entry_level = HEAPSTORE_LOG_WARN;
            else if (strcmp(level_str, "DEBUG") == 0)
                entry_level = HEAPSTORE_LOG_DEBUG;

            if (level != (heapstore_log_level_t)-1 && entry_level != level) {
                continue;
            }

            if (component && component[0] != '\0' && strstr(comp_str, component) == NULL) {
                continue;
            }

            results[found_count] = AGENTOS_STRDUP(line);
            if (!results[found_count]) {
                for (int i = 0; i < found_count; i++) {
                    AGENTOS_FREE(results[i]);
                }
                AGENTOS_FREE(results);
                fclose(f);
                closedir(dir);
                return AGENTOS_ERR_OUT_OF_MEMORY;
            }
            found_count++;
        }

        fclose(f);
    }

    closedir(dir);

    if (found_count == 0) {
        AGENTOS_FREE(results);
        *out_entries = NULL;
        return 0;
    }

    char **final_results;
    SAFE_MALLOC_ARRAY(final_results, found_count, sizeof(char *));
    if (!final_results) {
        for (int i = 0; i < found_count; i++) {
            AGENTOS_FREE(results[i]);
        }
        AGENTOS_FREE(results);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    __builtin_memcpy(final_results, results, found_count * sizeof(char *));
    AGENTOS_FREE(results);

    *out_entries = final_results;
    return found_count;
}

/**
 * @brief 释放查询结果
 *
 * @param entries 日志条目数组
 * @param count 条目数
 */
void log_store_service_free_entries(char **entries, int count)
{
    if (!entries) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            AGENTOS_FREE(entries[i]);
        }
    }
    AGENTOS_FREE(entries);
}

/**
 * @brief 清理旧的日志文件
 *
 * @param days_to_keep 保留天数
 * @return int 删除的文件数
 */
int log_store_service_cleanup_old_files(int days_to_keep)
{
    if (!atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire))
        return AGENTOS_EINVAL;
    if (days_to_keep <= 0) {
        days_to_keep = 30;
    }

    time_t cutoff = time(NULL) - ((time_t)days_to_keep * 86400);
    int deleted_count = 0;

    DIR *dir = opendir(g_ctx.storage_path);
    if (!dir)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx.storage_path, entry->d_name);

        struct stat file_stat;
        if (stat(filepath, &file_stat) != 0)
            continue;
        if (S_ISDIR(file_stat.st_mode))
            continue;

        if (file_stat.st_mtime < cutoff) {
            if (unlink(filepath) == 0) {
                deleted_count++;
            }
        }
    }

    closedir(dir);
    return deleted_count;
}

int log_store_service_get_status(uint64_t *out_total_bytes, uint32_t *out_file_count,
                                 time_t *out_oldest_timestamp)
{
    if (!atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire))
        return AGENTOS_EINVAL;

    uint64_t total_bytes = 0;
    uint32_t file_count = 0;
    time_t oldest_ts = 0;

    DIR *dir = opendir(g_ctx.storage_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;

            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", g_ctx.storage_path, entry->d_name);

            struct stat file_stat;
            if (stat(filepath, &file_stat) != 0)
                continue;
            if (S_ISDIR(file_stat.st_mode))
                continue;

            total_bytes += (uint64_t)file_stat.st_size;
            file_count++;
            if (oldest_ts == 0 || file_stat.st_mtime < oldest_ts) {
                oldest_ts = file_stat.st_mtime;
            }
        }
        closedir(dir);
    }

    if (out_total_bytes)
        *out_total_bytes = total_bytes;
    if (out_file_count)
        *out_file_count = file_count;
    if (out_oldest_timestamp)
        *out_oldest_timestamp = oldest_ts;

    return 0;
}

/**
 * @brief 关闭日志存储服务
 */
void log_store_service_shutdown(void)
{
    if (!atomic_load_explicit(&g_ctx.is_initialized, memory_order_acquire)) {
        return;
    }

    __builtin_memset(&g_ctx, 0, sizeof(g_ctx));
}
