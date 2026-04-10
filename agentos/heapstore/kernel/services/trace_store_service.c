/**
 * @file trace_store_service.c
 * @brief 内核追踪数据存储服务实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "../../include/heapstore_trace.h"
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
 * @brief 追踪存储服务上下文
 */
typedef struct {
    char storage_path[512];
    uint64_t max_storage_bytes;
    uint32_t sampling_rate;  // 采样率，每N个追踪点存储1个
    volatile uint32_t is_initialized;
    uint64_t total_traces_stored;
    uint64_t total_bytes_stored;
} trace_store_service_ctx_t;

static trace_store_service_ctx_t g_ctx = {0};

/**
 * @brief 初始化追踪存储服务
 *
 * @param storage_path 存储路径
 * @param max_storage_bytes 最大存储字节数
 * @param sampling_rate 采样率（1表示存储所有追踪点）
 * @return int 0成功，非0错误码
 */
int trace_store_service_init(const char* storage_path, 
                             uint64_t max_storage_bytes,
                             uint32_t sampling_rate)
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
    
    g_ctx.max_storage_bytes = max_storage_bytes > 0 ? max_storage_bytes : 500 * 1024 * 1024; // 默认500MB
    g_ctx.sampling_rate = sampling_rate > 0 ? sampling_rate : 1; // 默认采样所有
    g_ctx.total_traces_stored = 0;
    g_ctx.total_bytes_stored = 0;
    
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
 * @brief 存储追踪点
 *
 * @param trace_point 追踪点数据
 * @return int 0成功，非0错误码
 */
int trace_store_service_store_point(const heapstore_trace_point_t* trace_point)
{
    if (!g_ctx.is_initialized || !trace_point) {
        return -1;
    }
    
    // 应用采样率
    static uint32_t counter = 0;
    counter++;
    if (counter % g_ctx.sampling_rate != 0) {
        return 0; // 跳过此次存储
    }
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    if (!tm_info) {
        return -2;
    }
    
    // 构建追踪文件名
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/trace_%04d%02d%02d.bin",
             g_ctx.storage_path,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday);
    
    // 打开追踪文件
    FILE* f = fopen(filename, "ab");
    if (!f) {
        return -3;
    }
    
    // 写入追踪点
    size_t written = fwrite(trace_point, sizeof(heapstore_trace_point_t), 1, f);
    fclose(f);
    
    if (written != 1) {
        return -4;
    }
    
    // 更新统计
    g_ctx.total_traces_stored++;
    g_ctx.total_bytes_stored += sizeof(heapstore_trace_point_t);
    
    // 检查存储限制
    trace_store_service_check_storage_limit(filename);
    
    return 0;
}

/**
 * @brief 批量存储追踪点
 *
 * @param trace_points 追踪点数组
 * @param count 追踪点数量
 * @return int 成功存储的数量，或错误码
 */
int trace_store_service_store_batch(const heapstore_trace_point_t* trace_points, int count)
{
    if (!g_ctx.is_initialized || !trace_points || count <= 0) {
        return -1;
    }
    
    int stored = 0;
    for (int i = 0; i < count; i++) {
        if (trace_store_service_store_point(&trace_points[i]) == 0) {
            stored++;
        }
    }
    
    return stored;
}

/**
 * @brief 检查存储限制并执行清理
 *
 * @param current_file 当前追踪文件
 */
static void trace_store_service_check_storage_limit(const char* current_file)
{
    if (g_ctx.total_bytes_stored <= g_ctx.max_storage_bytes) {
        return;
    }
    
    // 超出存储限制，清理最旧的文件
    // 简化实现：只记录警告
    // 在实际实现中，这里应该删除最旧的文件
    fprintf(stderr, "Warning: Trace storage limit exceeded (%llu bytes > %llu bytes)\n",
            (unsigned long long)g_ctx.total_bytes_stored,
            (unsigned long long)g_ctx.max_storage_bytes);
}

/**
 * @brief 查询追踪数据
 *
 * @param query 查询条件
 * @param out_traces 输出追踪点数组
 * @param max_traces 最大追踪点数
 * @return int 返回的追踪点数，或错误码
 */
int trace_store_service_query_traces(const heapstore_trace_query_t* query,
                                     heapstore_trace_point_t** out_traces,
                                     int max_traces)
{
    if (!query || !out_traces || max_traces <= 0) {
        return -1;
    }
    
    // 简化实现：返回0个追踪点
    *out_traces = NULL;
    return 0;
}

/**
 * @brief 释放查询结果
 *
 * @param traces 追踪点数组
 * @param count 追踪点数
 */
void trace_store_service_free_traces(heapstore_trace_point_t* traces, int count)
{
    if (!traces) {
        return;
    }
    free(traces);
}

/**
 * @brief 导出追踪数据
 *
 * @param start_time 开始时间
 * @param end_time 结束时间
 * @param export_format 导出格式（"json", "csv", "binary"）
 * @param export_path 导出路径
 * @return int 导出的字节数，或错误码
 */
int trace_store_service_export_traces(const time_t* start_time,
                                      const time_t* end_time,
                                      const char* export_format,
                                      const char* export_path)
{
    if (!export_format || !export_path) {
        return -1;
    }
    
    // 简化实现：创建空导出文件
    FILE* f = fopen(export_path, "w");
    if (!f) {
        return -2;
    }
    
    if (strcmp(export_format, "json") == 0) {
        fprintf(f, "{\"traces\": []}\n");
    } else if (strcmp(export_format, "csv") == 0) {
        fprintf(f, "timestamp,component,operation,duration_ns,success\n");
    }
    
    fclose(f);
    
    // 返回文件大小
    f = fopen(export_path, "rb");
    if (!f) {
        return 0;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    return (int)size;
}

/**
 * @brief 获取追踪存储统计信息
 *
 * @param out_total_traces 输出总追踪点数
 * @param out_total_bytes 输出总存储字节数
 * @param out_sampling_rate 输出采样率
 * @return int 0成功，非0错误码
 */
int trace_store_service_get_stats(uint64_t* out_total_traces,
                                  uint64_t* out_total_bytes,
                                  uint32_t* out_sampling_rate)
{
    if (!g_ctx.is_initialized) {
        return -1;
    }
    
    if (out_total_traces) *out_total_traces = g_ctx.total_traces_stored;
    if (out_total_bytes) *out_total_bytes = g_ctx.total_bytes_stored;
    if (out_sampling_rate) *out_sampling_rate = g_ctx.sampling_rate;
    
    return 0;
}

/**
 * @brief 清理旧的追踪数据
 *
 * @param days_to_keep 保留天数
 * @return int 删除的文件数
 */
int trace_store_service_cleanup_old_files(int days_to_keep)
{
    if (days_to_keep <= 0) {
        days_to_keep = 7; // 默认保留7天追踪数据
    }
    
    // 简化实现：返回0
    return 0;
}

/**
 * @brief 关闭追踪存储服务
 */
void trace_store_service_shutdown(void)
{
    if (!g_ctx.is_initialized) {
        return;
    }
    
    memset(&g_ctx, 0, sizeof(g_ctx));
}