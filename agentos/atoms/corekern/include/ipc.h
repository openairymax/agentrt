/**
 * @file ipc.h
 * @brief 内核 IPC 接口定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 提供内核级进程间通信功能，包括通道创建、消息发送/接收等
 */

#ifndef AGENTOS_IPC_H
#define AGENTOS_IPC_H

/**
 * @brief API 版本声明 (MAJOR.MINOR.PATCH)
 *
 * 在相同 MAJOR 版本内保证 ABI 兼容
 * 破坏性更改需递增 MAJOR 并发布迁移说明
 */
#define AGENTOS_IPC_API_VERSION_MAJOR 1
#define AGENTOS_IPC_API_VERSION_MINOR 0
#define AGENTOS_IPC_API_VERSION_PATCH 0

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IPC 通道类型（不透明指针）
 */
typedef struct agentos_ipc_channel agentos_ipc_channel_t;

/**
 * @brief IPC 缓冲区类型（不透明指针）
 */
typedef struct agentos_ipc_buffer agentos_ipc_buffer_t;

/**
 * @brief IPC 端口号类型
 */
typedef uint16_t agentos_ipc_port_t;

/**
 * @brief IPC 消息结构
 */
typedef struct {
    uint32_t    code;      /**< 消息码 */
    const void* data;      /**< 消息数据 */
    size_t      size;      /**< 数据大小 */
    int32_t     fd;        /**< 文件描述符 */
    uint64_t    msg_id;    /**< 消息 ID */
} agentos_ipc_message_t;

/**
 * @brief IPC 消息回调函数类型
 *
 * @param channel [in] IPC 通道句柄
 * @param msg [in] 接收到的消息
 * @param userdata [in] 用户数据
 * @return agentos_error_t 错误码
 */
typedef agentos_error_t (*agentos_ipc_callback_t)(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* userdata);

/**
 * @brief 初始化 IPC 子系统
 *
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有 IPC 资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_ipc_cleanup()
 */
AGENTOS_API agentos_error_t agentos_ipc_init(void);

/**
 * @brief 清理 IPC 子系统
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_ipc_init()
 */
AGENTOS_API void agentos_ipc_cleanup(void);

/**
 * @brief 创建 IPC 通道
 *
 * @param name [in] 通道名称
 * @param callback [in] 消息回调函数
 * @param userdata [in] 用户数据
 * @param out_channel [out] 输出通道句柄
 * @return agentos_error_t 错误码
 *
 * @ownership out_channel 由调用者负责，通过 agentos_ipc_close() 释放
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_ipc_close()
 */
AGENTOS_API agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 连接到已存在的 IPC 通道
 *
 * @param name [in] 通道名称
 * @param out_channel [out] 输出通道句柄
 * @return agentos_error_t 错误码
 *
 * @ownership out_channel 由调用者负责，通过 agentos_ipc_close() 释放
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_ipc_close()
 */
AGENTOS_API agentos_error_t agentos_ipc_connect(
    const char* name,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 关闭 IPC 通道
 *
 * @param channel [in] 通道句柄
 * @return agentos_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_close(agentos_ipc_channel_t* channel);

/**
 * @brief 发送 IPC 消息
 *
 * @param channel [in] 通道句柄
 * @param msg [in] 消息结构
 * @return agentos_error_t 错误码
 *
 * @ownership msg 的生命周期由调用者管理
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

/**
 * @brief 接收 IPC 消息
 *
 * @param channel [in] 通道句柄
 * @param timeout_ms [in] 超时时间（毫秒）
 * @param out_msg [out] 输出消息
 * @return agentos_error_t 错误码
 *
 * @ownership out_msg 由调用者负责分配和释放
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_recv(
    agentos_ipc_channel_t* channel,
    uint32_t timeout_ms,
    agentos_ipc_message_t* out_msg);

/**
 * @brief 获取通道文件描述符
 *
 * @param channel [in] 通道句柄
 * @return int32_t 文件描述符
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API int32_t agentos_ipc_get_fd(agentos_ipc_channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_H */
