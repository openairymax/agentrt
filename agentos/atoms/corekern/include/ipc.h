/**
 * @file ipc.h
 * @brief 内核级 IPC 接口定义（微内核专用）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @section 架构定位
 * 本模块提供**内核级进程间通信（IPC）**功能，是AgentOS微内核架构的核心组件。
 * 
 * **设计原则：**
 * - 轻量级：消息结构仅40字节，避免不必要的开销
 * - 高性能：微秒级延迟，适合内核态高频调用
 * - 零依赖：不依赖commons或其他外部模块
 * - 简洁性：API设计遵循Unix哲学（做一件事并做好）
 * 
 * **与 应用层IPC 的区别：**
 * - 本模块：agentos_kernel_ipc_message_t（轻量、快速、内核专用）
 * - 应用层：agentos_ipc_message_t（完整、标准化、跨模块通信）
 * - 详细说明见：commons/include/agentos_types.h
 * 
 * **使用场景：**
 * - 微内核内部服务间通信
 * - 系统调用层的IPC实现
 * - 需要极致性能的关键路径
 * 
 * @note 不应在应用层代码中直接使用本模块，应通过commons的IPC抽象层
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
 * @brief 内核级 IPC 消息结构（轻量级，微内核专用）
 * 
 * @details 这是AgentOS **Level 1 内核级IPC**的消息类型定义。
 * 
 * **设计理由（为什么不用完整的应用层IPC）：**
 * 1. 微内核需要极致性能：40字节 vs 200+字节
 * 2. 零依赖原则：不引入commons的复杂类型系统
 * 3. 简单性：内核态编程应避免复杂的抽象
 * 4. Liedtke微内核原则：内核最小化，机制与策略分离
 * 
 * **字段说明：**
 * - code: 消息类型码（如：0x01=数据, 0x02=控制, 0x03=响应）
 * - data: 消息负载指针（零拷贝设计）
 * - size: 负载大小（字节）
 * - fd: 文件描述符（用于传递文件句柄）
 * - msg_id: 消息唯一标识（用于请求-响应匹配）
 * 
 * @note 与应用层的 agentos_ipc_message_t 区分：
 *       应用层使用完整的header+payload结构（见commons/include/agentos_types.h）
 */
typedef struct {
    uint32_t    code;      /**< 消息码 */
    const void* data;      /**< 消息数据 */
    size_t      size;      /**< 数据大小 */
    int32_t     fd;        /**< 文件描述符 */
    uint64_t    msg_id;    /**< 消息 ID */
} agentos_kernel_ipc_message_t;

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
    const agentos_kernel_ipc_message_t* msg,
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
    const agentos_kernel_ipc_message_t* msg);

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
    agentos_kernel_ipc_message_t* out_msg);

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

/**
 * @brief 同步调用（带超时等待响应）
 *
 * @param channel [in] 通道句柄（必须通过 agentos_ipc_connect 获取）
 * @param msg [in] 请求消息
 * @param response [out] 响应缓冲区
 * @param response_size [in/out] 输入时为缓冲区大小，输出时为实际响应大小
 * @param timeout_ms [in] 超时时间（毫秒）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms);

/**
 * @brief 回复消息（唤醒等待的调用者）
 *
 * @param channel [in] 通道句柄
 * @param msg [in] 回复消息（msg_id 必须匹配请求）
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_H */
