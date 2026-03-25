/**
 * @file ipc.h
 * @brief 内核 IPC 接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_IPC_H
#define AGENTOS_IPC_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define AGENTOS_IPC_API_VERSION_MAJOR 1
#define AGENTOS_IPC_API_VERSION_MINOR 0
#define AGENTOS_IPC_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_ipc_channel agentos_ipc_channel_t;
typedef struct agentos_ipc_buffer agentos_ipc_buffer_t;

typedef struct {
    uint32_t    code;
    const void* data;
    size_t      size;
    int32_t     fd;
    uint64_t    msg_id;
} agentos_ipc_message_t;

typedef agentos_error_t (*agentos_ipc_callback_t)(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* userdata);

/**
 * @brief 初始化 IPC 子系统
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_ipc_init(void);

/**
 * @brief 清理 IPC 子系统
 */
AGENTOS_API void agentos_ipc_cleanup(void);

/**
 * @brief 创建 IPC 通道
 * @param name [in] 通道名称
 * @param callback [in] 消息回调函数
 * @param userdata [in] 用户数据
 * @param out_channel [out] 输出通道句柄
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_channel 由调用者负责通过 agentos_ipc_close() 释放
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 连接到已存在的 IPC 通道
 * @param name [in] 通道名称
 * @param out_channel [out] 输出通道句柄
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership out_channel 由调用者负责通过 agentos_ipc_close() 释放
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_connect(
    const char* name,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 发起同步 IPC 调用（带超时）
 * @param channel [in] 通道句柄
 * @param msg [in] 请求消息
 * @param response [out] 响应缓冲区
 * @param response_size [in,out] 缓冲区大小/实际响应大小
 * @param timeout_ms [in] 超时时间（毫秒）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms);

/**
 * @brief 发送异步消息
 * @param channel [in] 通道句柄
 * @param msg [in] 消息
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

/**
 * @brief 发送回复消息
 * @param channel [in] 通道句柄
 * @param msg [in] 原消息
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

/**
 * @brief 关闭 IPC 通道
 * @param channel [in] 通道句柄
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API void agentos_ipc_close(agentos_ipc_channel_t* channel);

/**
 * @brief 创建 IPC 缓冲区
 * @param capacity [in] 缓冲区容量
 * @return agentos_ipc_buffer_t* 缓冲区句柄，失败返回 NULL
 *
 * @ownership 返回的句柄由调用者负责通过 agentos_ipc_buffer_destroy() 释放
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_ipc_buffer_t* agentos_ipc_buffer_create(size_t capacity);

/**
 * @brief 销毁 IPC 缓冲区
 * @param buf [in] 缓冲区句柄
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API void agentos_ipc_buffer_destroy(agentos_ipc_buffer_t* buf);

/**
 * @brief 写入数据到缓冲区
 * @param buf [in] 缓冲区句柄
 * @param data [in] 数据指针
 * @param size [in] 数据大小
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_buffer_write(
    agentos_ipc_buffer_t* buf,
    const void* data,
    size_t size);

/**
 * @brief 从缓冲区读取数据
 * @param buf [in] 缓冲区句柄
 * @param out_data [out] 输出数据缓冲区
 * @param size [in] 要读取的数据大小
 * @param out_read [out] 实际读取的数据大小
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_ipc_buffer_read(
    agentos_ipc_buffer_t* buf,
    void* out_data,
    size_t size,
    size_t* out_read);

/**
 * @brief 获取缓冲区可用数据大小
 * @param buf [in] 缓冲区句柄
 * @return size_t 可用数据大小
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API size_t agentos_ipc_buffer_available(agentos_ipc_buffer_t* buf);

/**
 * @brief 重置缓冲区
 * @param buf [in] 缓冲区句柄
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API void agentos_ipc_buffer_reset(agentos_ipc_buffer_t* buf);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_H */
