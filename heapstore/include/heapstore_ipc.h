/**
 * @file heapstore_ipc.h
 * @brief AgentOS 数据分区 IPC 数据存储接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_IPC_H
#define AGENTOS_heapstore_IPC_H

#include "heapstore.h"

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
} heapstore_ipc_channel_t;

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
} heapstore_ipc_buffer_t;

/**
 * @brief 初始化 IPC 数据存储
 *
 * @return heapstore_error_t 错误码
 *
 * @ownership 内部管理所有资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @see heapstore_ipc_shutdown()
 */
heapstore_error_t heapstore_ipc_init(void);

/**
 * @brief 关闭 IPC 数据存储
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see heapstore_ipc_init()
 */
void heapstore_ipc_shutdown(void);

/**
 * @brief 记录通道信息
 *
 * @param channel [in] 通道信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 channel 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_ipc_record_channel(const heapstore_ipc_channel_t* channel);

/**
 * @brief 获取通道信息
 *
 * @param channel_id [in] 通道 ID
 * @param channel [out] 输出通道信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 channel 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_ipc_get_channel(const char* channel_id, heapstore_ipc_channel_t* channel);

/**
 * @brief 更新通道活动
 *
 * @param channel_id [in] 通道 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_ipc_update_channel_activity(const char* channel_id);

/**
 * @brief 记录缓冲区信息
 *
 * @param buffer [in] 缓冲区信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 buffer 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_ipc_record_buffer(const heapstore_ipc_buffer_t* buffer);

/**
 * @brief 获取缓冲区信息
 *
 * @param buffer_id [in] 缓冲区 ID
 * @param buffer [out] 输出缓冲区信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 buffer 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_ipc_get_buffer(const char* buffer_id, heapstore_ipc_buffer_t* buffer);

/**
 * @brief 获取 IPC 存储统计信息
 *
 * @param channel_count [out] 输出通道数量
 * @param buffer_count [out] 输出缓冲区数量
 * @param total_size [out] 输出总大小
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责所有输出参数的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_ipc_get_stats(uint32_t* channel_count, uint32_t* buffer_count, uint64_t* total_size);

/**
 * @brief 检查 IPC 系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是
 */
bool heapstore_ipc_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_IPC_H */
