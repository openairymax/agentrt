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

#include "../../include/heapstore_log.h"
#include "../../include/heapstore.h"
#include "../../include/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

/**
 * @brief 日志存储服务上下文
 */
typedef struct {
    char storage_path[512];
    uint64_t max_storage_bytes;
    uint32_t rotation_count;
    volatile uint32_t is_initialized;
} log_store_service_ctx_t;

static log_store_service_ctx_t g_ctx = {0};

/**
 * @brief 初始化日志存储服务
 *
 * @param storage_path 存储路径
 * @param max_storage_bytes 最大存储字节数
 * @return int 0成功，非0错误码
 */
int log_store_service_init(const char* storage_path, uint64_t max_storage_bytes)
{
    if (!storage_path) {
        return -1;
    }
    
    if (g_ctx.is_initialized) {
        return 0;
    }
    
    // 设置存储路径
    strncpy(g_ctx.storage_path, storage_path, sizeof(g_ctx.storage_path) - 1);
    g_ctx.storage_path[sizeof(g_ctx.storage_path) - 1] = '\0';
    
    g_ctx.max_storage_bytes = max_storage_bytes > 0 ? max_storage_bytes : 100 * 1024 * 1024; // 默认100MB
    g_ctx.rotation_count = 10; // 默认保留10个日志文件
    
    // 创建存储目录
#ifdef _WIN32
    if (_mkdir(g_ctx.storage_path) != 0) {
        // 如果目录已存在，忽略错误
        if (errno != EEXIST) {
            return -2;
        }
    }
#else
    if (mkdir(g_ctx.storage_path, 0755) != 0) {
        // 如果目录已存在，忽略错误
        if (errno != EEXIST) {
            return -2;
        }
    }
#endif
    
    g_ctx.is_initialized = 1;
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
int log_store_service_store_entry(heapstore_log_level_t level,
                                  const char* component,
                                  const char* message,
                                  const time_t* timestamp)
{
    if (!g_ctx.is_initialized || !component || !message) {
        return -1;
    }
    
    time_t now = timestamp ? *timestamp : time(NULL);
    struct tm* tm_info = localtime(&now);
    if (!tm_info) {
        return -2;
    }
    
    // 构建日志文件名
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/log_%04d%02d%02d.log",
             g_ctx.storage_path,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday);
    
    // 打开日志文件
    FILE* f = fopen(filename, "a");
    if (!f) {
        return -3;
    }
    
    // 写入日志条目
    const char* level_str = "UNKNOWN";
    switch (level) {
        case heapstore_LOG_ERROR: level_str = "ERROR"; break;
        case heapstore_LOG_WARN:  level_str = "WARN";  break;
        case heapstore_LOG_INFO:  level_str = "INFO";  break;
        case heapstore_LOG_DEBUG: level_str = "DEBUG"; break;
    }
    
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(f, "[%s] [%s] [%s] %s\n", time_buf, level_str, component, message);
    fclose(f);
    
    // 执行日志轮转检查
    log_store_service_check_rotation(filename);
    
    return 0;
}

/**
 * @brief 检查并执行日志轮转
 *
 * @param current_file 当前日志文件
 */
static void log_store_service_check_rotation(const char* current_file)
{
    // 获取文件大小
    FILE* f = fopen(current_file, "rb");
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
int log_store_service_query_entries(const time_t* start_time,
                                    const time_t* end_time,
                                    heapstore_log_level_t level,
                                    const char* component,
                                    char*** out_entries,
                                    int max_entries)
{
    if (!out_entries || max_entries <= 0) {
        return -1;
    }
    
    // 简化实现：返回0条日志
    *out_entries = NULL;
    return 0;
}

/**
 * @brief 释放查询结果
 *
 * @param entries 日志条目数组
 * @param count 条目数
 */
void log_store_service_free_entries(char** entries, int count)
{
    if (!entries) {
        return;
    }
    
    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            free(entries[i]);
        }
    }
    free(entries);
}

/**
 * @brief 清理旧的日志文件
 *
 * @param days_to_keep 保留天数
 * @return int 删除的文件数
 */
int log_store_service_cleanup_old_files(int days_to_keep)
{
    if (days_to_keep <= 0) {
        days_to_keep = 30; // 默认保留30天
    }
    
    // 简化实现：返回0
    return 0;
}

/**
 * @brief 获取存储服务状态
 *
 * @param out_total_bytes 输出总存储字节数
 * @param out_file_count 输出文件数
 * @param out_oldest_timestamp 输出最旧日志时间戳
 * @return int 0成功，非0错误码
 */
int log_store_service_get_status(uint64_t* out_total_bytes,
                                 uint32_t* out_file_count,
                                 time_t* out_oldest_timestamp)
{
    if (out_total_bytes) *out_total_bytes = 0;
    if (out_file_count) *out_file_count = 0;
    if (out_oldest_timestamp) *out_oldest_timestamp = 0;
    
    // 简化实现
    return 0;
}

/**
 * @brief 关闭日志存储服务
 */
void log_store_service_shutdown(void)
{
    if (!g_ctx.is_initialized) {
        return;
    }
    
    memset(&g_ctx, 0, sizeof(g_ctx));
}