/**
 * @file ipc.h
 * @brief 内核 IPC 接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_IPC_H
#define AGENTOS_IPC_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** IPC 通道句柄 */
typedef struct agentos_ipc_channel agentos_ipc_channel_t;

/** IPC 消息 */
typedef struct {
    uint32_t    code;       /**< 消息码（方法ID） */
    const void* data;       /**< 消息数据 */
    size_t      size;       /**< 数据大小 */
    int32_t     fd;         /**< 可能传递的文件描述符 */
    uint64_t    msg_id;     /**< 消息ID，用于匹配请求和响应 */
} agentos_ipc_message_t;

/** IPC 回调函数，处理收到的消息 */
typedef agentos_error_t (*agentos_ipc_callback_t)(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* userdata);

/**
 * @brief 初始化 IPC 子系统
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_init(void);

/**
 * @brief 清理 IPC 子系统
 */
void agentos_ipc_cleanup(void);

/**
 * @brief 创建 IPC 通道（服务端）
 * @param name 通道名称（全局唯一）
 * @param callback 消息回调
 * @param userdata 回调用户数据
 * @param out_channel 输出通道句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 连接到 IPC 通道（客户端）
 * @param name 目标通道名称
 * @param out_channel 输出连接句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_connect(
    const char* name,
    agentos_ipc_channel_t** out_channel);

/**
 * @brief 发送消息并等待响应（同步调用）
 * @param channel 连接句柄
 * @param msg 消息内容
 * @param response 接收响应数据的缓冲区（可为 NULL）
 * @param response_size 缓冲区大小
 * @param timeout_ms 超时毫秒（0表示无限等待）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms);

/**
 * @brief 发送消息（单向，不等待响应）
 * @param channel 连接句柄
 * @param msg 消息内容
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

/**
 * @brief 回复消息（在回调中使用）
 * @param channel 原通道
 * @param msg 回复消息
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

/**
 * @brief 关闭通道/连接
 * @param channel 句柄
 */
void agentos_ipc_close(agentos_ipc_channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_H */