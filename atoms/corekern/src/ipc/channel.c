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
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel) {
    if (!name || !out_channel) {
        return AGENTOS_EINVAL;
    }

    (void)callback;  /* 回调函数暂未使用 */
    (void)userdata;   /* 用户数据暂未使用 */

    agentos_ipc_channel_t* channel =
        (agentos_ipc_channel_t*)AGENTOS_CALLOC(1, sizeof(agentos_ipc_channel_t));
    if (!channel) {
        return AGENTOS_ENOMEM;
    }

    /* 使用名称的哈希值作为端口号（简化实现） */
    unsigned int hash = 0;
    const char* p = name;
    while (*p) {
        hash = hash * 31 + (unsigned char)*p;
        p++;
    }
    channel->port = (agentos_ipc_port_t)(hash % 65535);
    channel->is_server = 1;
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
    const agentos_ipc_message_t* msg) {
    if (!channel || !msg) {
        return AGENTOS_EINVAL;
    }

    /* TODO: 实现实际的 IPC 发送逻辑 */
    (void)msg;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 接收数据
 */
agentos_error_t agentos_ipc_recv(
    agentos_ipc_channel_t* channel,
    uint32_t timeout_ms,
    agentos_ipc_message_t* out_msg) {
    if (!channel || !out_msg) {
        return AGENTOS_EINVAL;
    }

    /* TODO: 实现实际的 IPC 接收逻辑 */
    (void)timeout_ms;
    (void)out_msg;

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
