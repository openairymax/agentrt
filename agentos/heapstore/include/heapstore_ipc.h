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
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 共享类型已在 heapstore_types.h 中定义，此处仅用于 API 声明 */

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
 * @since v1.0.0
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
 * @since v1.0.0
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

 * @since v1.0.0*/
heapstore_error_t heapstore_ipc_get_channel(const char* channel_id, heapstore_ipc_channel_t* channel);

/**
 * @brief 更新通道活动
 *
 * @param channel_id [in] 通道 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
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

 * @since v1.0.0*/
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

 * @since v1.0.0*/
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

 * @since v1.0.0*/
heapstore_error_t heapstore_ipc_get_stats(uint32_t* channel_count, uint32_t* buffer_count, uint64_t* total_size);

/**
 * @brief 检查 IPC 系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
bool heapstore_ipc_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_IPC_H */
