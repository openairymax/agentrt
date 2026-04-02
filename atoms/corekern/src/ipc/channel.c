/**
 * @file channel.c
 * @brief IPC 通道辅助功能实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "mem.h"
#include "task.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief IPC 通道内部结构
 */
struct agentos_ipc_channel {
    int fd;                     /**< 文件描述符 */
    agentos_ipc_port_t port;    /**< 端口号 */
    int is_server;              /**< 是否为服务器端 */
    agentos_mutex_t* lock;      /**< 锁（指针） */
};

/**
 * @brief 创建 IPC 通道
 */
agentos_error_t agentos_ipc_create_channel(
    agentos_ipc_port_t port,
    int is_server,
    agentos_ipc_channel_t** out_channel) {
    if (!out_channel) {
        return AGENTOS_EINVAL;
    }

    agentos_ipc_channel_t* channel = 
        (agentos_ipc_channel_t*)AGENTOS_CALLOC(1, sizeof(agentos_ipc_channel_t));
    if (!channel) {
        return AGENTOS_ENOMEM;
    }

    channel->port = port;
    channel->is_server = is_server;
    channel->fd = -1;
    channel->lock = NULL;

    /* 创建锁 */
    channel->lock = agentos_mutex_create();
    if (!channel->lock) {
        AGENTOS_FREE(channel);
        return AGENTOS_ENOMEM;
    }

    *out_channel = channel;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 关闭 IPC 通道
 */
agentos_error_t agentos_ipc_close(agentos_ipc_channel_t* channel) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }

    if (channel->lock) {
        agentos_mutex_lock(channel->lock);
        channel->fd = -1;
        agentos_mutex_unlock(channel->lock);

        agentos_mutex_destroy(channel->lock);
        channel->lock = NULL;
    }
    AGENTOS_FREE(channel);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 发送数据
 */
agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const void* data,
    size_t len,
    uint32_t timeout_ms) {
    if (!channel || !data || len == 0) {
        return AGENTOS_EINVAL;
    }

    /* TODO: 实现实际的 IPC 发送逻辑 */
    (void)timeout_ms;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 接收数据
 */
agentos_error_t agentos_ipc_recv(
    agentos_ipc_channel_t* channel,
    void* buffer,
    size_t buf_len,
    size_t* out_len,
    uint32_t timeout_ms) {
    if (!channel || !buffer || !out_len) {
        return AGENTOS_EINVAL;
    }

    /* TODO: 实现实际的 IPC 接收逻辑 */
    (void)buf_len;
    (void)timeout_ms;
    *out_len = 0;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取文件描述符
 */
int32_t agentos_ipc_get_fd(agentos_ipc_channel_t* channel) {
    if (!channel) {
        return -1;
    }
    return channel->fd;
}
