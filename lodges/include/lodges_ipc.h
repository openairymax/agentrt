/**
 * @file lodges_ipc.h
 * @brief AgentOS 数据分区 IPC 数据存储接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_lodges_IPC_H
#define AGENTOS_lodges_IPC_H

#include "lodges.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IPC 通道记录
 */
typedef struct {
    char channel_id[128];
    char name[256];
    char type[64];
    uint64_t created_at;
    uint64_t last_activity_at;
    size_t buffer_size;
    size_t current_usage;
    char status[32];
} lodges_ipc_channel_t;

/**
 * @brief IPC 缓冲区记录
 */
typedef struct {
    char buffer_id[128];
    char channel_id[128];
    uint64_t created_at;
    size_t size;
    uint32_t ref_count;
    char status[32];
} lodges_ipc_buffer_t;

/**
 * @brief 初始化 IPC 数据存储
 *
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_init(void);

/**
 * @brief 关闭 IPC 数据存储
 */
void lodges_ipc_shutdown(void);

/**
 * @brief 记录通道信息
 *
 * @param channel 通道信息
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_record_channel(const lodges_ipc_channel_t* channel);

/**
 * @brief 获取通道信息
 *
 * @param channel_id 通道 ID
 * @param channel 输出通道信息
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_get_channel(const char* channel_id, lodges_ipc_channel_t* channel);

/**
 * @brief 更新通道活动
 *
 * @param channel_id 通道 ID
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_update_channel_activity(const char* channel_id);

/**
 * @brief 记录缓冲区信息
 *
 * @param buffer 缓冲区信息
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_record_buffer(const lodges_ipc_buffer_t* buffer);

/**
 * @brief 获取缓冲区信息
 *
 * @param buffer_id 缓冲区 ID
 * @param buffer 输出缓冲区信息
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_get_buffer(const char* buffer_id, lodges_ipc_buffer_t* buffer);

/**
 * @brief 获取 IPC 存储统计信息
 *
 * @param channel_count 输出通道数量
 * @param buffer_count 输出缓冲区数量
 * @param total_size 输出总大小
 * @return lodges_error_t 错误码
 */
lodges_error_t lodges_ipc_get_stats(uint32_t* channel_count, uint32_t* buffer_count, uint64_t* total_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_lodges_IPC_H */

