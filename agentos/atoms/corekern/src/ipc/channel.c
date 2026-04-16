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
#include <agentos/memory.h>

/* Check macros for unified error handling */
#include <agentos/check.h>
#include <string.h>
#include <stdio.h>

/* platform.h provides agentos_mutex_t, agentos_cond_t and init/destroy functions */
#include "platform.h"

/* ==================== 兼容层：create/destroy 包装 ==================== */

static inline agentos_mutex_t* agentos_mutex_create_compat(void) {
    agentos_mutex_t* m = (agentos_mutex_t*)AGENTOS_CALLOC(1, sizeof(agentos_mutex_t));
    if (m && agentos_mutex_init(m) != 0) {
        AGENTOS_FREE(m);
        return NULL;
    }
    return m;
}

static inline void agentos_mutex_destroy_compat(agentos_mutex_t* m) {
    if (m) {
        agentos_mutex_destroy(m);
        AGENTOS_FREE(m);
    }
}

static inline agentos_cond_t* agentos_cond_create_compat(void) {
    agentos_cond_t* c = (agentos_cond_t*)AGENTOS_CALLOC(1, sizeof(agentos_cond_t));
    if (c && agentos_cond_init(c) != 0) {
        AGENTOS_FREE(c);
        return NULL;
    }
    return c;
}

static inline void agentos_cond_destroy_compat(agentos_cond_t* c) {
    if (c) {
        agentos_cond_destroy(c);
        AGENTOS_FREE(c);
    }
}

#define agentos_mutex_create() agentos_mutex_create_compat()
#define agentos_mutex_destroy_ptr(p) agentos_mutex_destroy_compat(p)
#define agentos_cond_create() agentos_cond_create_compat()
#define agentos_cond_destroy_ptr(p) agentos_cond_destroy_compat(p)

/**
 * @brief IPC 消息队列节点
 */
typedef struct ipc_message_node {
    agentos_kernel_ipc_message_t msg;
    struct ipc_message_node* next;
} ipc_message_node_t;

/**
 * @brief IPC 通道内部结构
 */
struct agentos_ipc_channel {
    int fd;                     /**< 文件描述符 */
    agentos_ipc_port_t port;    /**< 端口号 */
    int is_server;              /**< 是否为服务器端 */
    agentos_mutex_t* lock;      /**< 锁（指针） */
    agentos_cond_t* cond;       /**< 条件变量 */
    ipc_message_node_t* queue;  /**< 消息队列 */
    size_t queue_size;          /**< 队列大小 */
};

/**
 * @brief 创建 IPC 通道
 */
agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel) {
    CHECK_NULL(name);
    CHECK_NULL(out_channel);

    (void)callback;  /* 回调函数暂未使用 */
    (void)userdata;   /* 用户数据暂未使用 */

    agentos_ipc_channel_t* channel = NULL;
    CALLOC_CHECK(channel, 1, sizeof(agentos_ipc_channel_t), error_return);

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
    CHECK_NULL_GOTO(channel->lock, cleanup);

    /* 创建条件变量 */
    channel->cond = agentos_cond_create();
    CHECK_NULL_GOTO(channel->cond, cleanup);

    /* 初始化消息队列 */
    channel->queue = NULL;
    channel->queue_size = 0;

    *out_channel = channel;
    return AGENTOS_SUCCESS;

cleanup:
    if (channel->cond) {
        agentos_cond_destroy_ptr(channel->cond);
    }
    if (channel->lock) {
        agentos_mutex_destroy_ptr(channel->lock);
    }
    AGENTOS_FREE(channel);
    return AGENTOS_ENOMEM;

error_return:
    return AGENTOS_ENOMEM;
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
        
        /* 清理消息队列 */
        ipc_message_node_t* node = channel->queue;
        while (node) {
            ipc_message_node_t* next = node->next;
            AGENTOS_FREE(node);
            node = next;
        }
        channel->queue = NULL;
        channel->queue_size = 0;
        
        channel->fd = -1;
        agentos_mutex_unlock(channel->lock);

        if (channel->cond) {
            agentos_cond_destroy_ptr(channel->cond);
            channel->cond = NULL;
        }
        agentos_mutex_destroy_ptr(channel->lock);
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
    const agentos_kernel_ipc_message_t* msg) {
    if (!channel || !msg) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(channel->lock);

    /* 创建消息节点 */
    ipc_message_node_t* node = (ipc_message_node_t*)AGENTOS_CALLOC(1, sizeof(ipc_message_node_t));
    if (!node) {
        agentos_mutex_unlock(channel->lock);
        return AGENTOS_ENOMEM;
    }

    /* 复制消息内容 */
    memcpy(&node->msg, msg, sizeof(agentos_kernel_ipc_message_t));
    node->next = NULL;

    /* 添加到队列尾部 */
    if (!channel->queue) {
        channel->queue = node;
    } else {
        ipc_message_node_t* tail = channel->queue;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = node;
    }
    channel->queue_size++;

    /* 通知等待的接收者 */
    agentos_cond_signal(channel->cond);

    agentos_mutex_unlock(channel->lock);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 接收数据
 */
agentos_error_t agentos_ipc_recv(
    agentos_ipc_channel_t* channel,
    uint32_t timeout_ms,
    agentos_kernel_ipc_message_t* out_msg) {
    if (!channel || !out_msg) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(channel->lock);

    /* 等待消息到达 */
    if (!channel->queue && timeout_ms > 0) {
        agentos_cond_timedwait(channel->cond, channel->lock, timeout_ms);
    }

    /* 检查队列 */
    if (!channel->queue) {
        agentos_mutex_unlock(channel->lock);
        return AGENTOS_ETIMEDOUT;
    }

    /* 从队列头部取出消息 */
    ipc_message_node_t* node = channel->queue;
    memcpy(out_msg, &node->msg, sizeof(agentos_kernel_ipc_message_t));
    
    channel->queue = node->next;
    channel->queue_size--;
    AGENTOS_FREE(node);

    agentos_mutex_unlock(channel->lock);

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
