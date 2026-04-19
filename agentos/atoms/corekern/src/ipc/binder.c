/**
 * @file binder.c
 * @brief Binder 风格 IPC 实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现高效的进程间通信机制：
 * - 同步调用（带超时）
 * - 异步发送
 * - 回复机制
 *
 * 使用条件变量优化超时等待，避免忙轮询消耗 CPU 资源。
 */

#include "ipc.h"
#include "task.h"
#include "mem.h"
#include "agentos_time.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
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

/* 使用兼容层宏替换 */
#define agentos_mutex_create() agentos_mutex_create_compat()
#define agentos_mutex_destroy_ptr(p) agentos_mutex_destroy_compat(p)
#define agentos_cond_create() agentos_cond_create_compat()
#define agentos_cond_destroy_ptr(p) agentos_cond_destroy_compat(p)

/* ==================== 平台相关宏定�?==================== */

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define ATOMIC_ADD(ptr, val)  InterlockedAdd((volatile LONG*)(ptr), (val))
#define ATOMIC_SUB(ptr, val)  InterlockedAdd((volatile LONG*)(ptr), -(val))
#define ATOMIC_LOAD(ptr)      (*(volatile int*)(ptr))
#define ATOMIC_STORE(ptr, v)  (*(volatile int*)(ptr)) = (v)
static volatile uint64_t next_msg_id = 1;
static uint64_t generate_msg_id(void) {
    return InterlockedIncrement64((volatile LONG64*)&next_msg_id) - 1;
}
#else
#define ATOMIC_ADD(ptr, val)  __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST)
#define ATOMIC_SUB(ptr, val)  __atomic_sub_fetch(ptr, val, __ATOMIC_SEQ_CST)
#define ATOMIC_LOAD(ptr)      __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE(ptr, v)  __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST)
static volatile uint64_t next_msg_id = 1;
static uint64_t generate_msg_id(void) {
    return __atomic_fetch_add(&next_msg_id, 1, __ATOMIC_SEQ_CST);
}
#endif

/* ==================== 常量定义 ==================== */

#define MAX_CHANNEL_NAME 64

/* ==================== 数据结构 ==================== */

/**
 * @brief Binder 节点（服务端�?
 */
typedef struct binder_node {
    char name[MAX_CHANNEL_NAME];
    agentos_ipc_callback_t callback;
    void* userdata;
    agentos_ipc_channel_t* channel;
    struct binder_node* next;
    volatile int ref_count;
} binder_node_t;

/**
 * @brief 待处理调用（带条件变量）
 */
typedef struct pending_call {
    uint64_t msg_id;
    void* response_buf;
    size_t response_capacity;
    size_t* response_size_ptr;
    volatile int completed;
    struct pending_call* next;

    /* 条件变量用于高效等待 */
    agentos_cond_t cond;
    agentos_mutex_t cond_lock;
} pending_call_t;

/**
 * @brief IPC 通道结构
 */
struct agentos_ipc_channel {
    char name[MAX_CHANNEL_NAME];
    binder_node_t* local_node;
    binder_node_t* remote_target;
    agentos_mutex_t* lock;
    pending_call_t* pending_calls;
};

/* ==================== 全局状�?==================== */

static binder_node_t* root_nodes = NULL;
static agentos_mutex_t* binder_global_lock = NULL;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 查找 Binder 节点
 * @param name 节点名称
 * @return 节点指针，未找到返回 NULL
 * @note 必须在持�?binder_global_lock 时调�?
 */
static binder_node_t* find_node_locked(const char* name) {
    binder_node_t* node = root_nodes;
    while (node) {
        if (strcmp(node->name, name) == 0) return node;
        node = node->next;
    }
    return NULL;
}

/**
 * @brief 查找待处理调�?
 * @param ch 通道
 * @param msg_id 消息ID
 * @return 待处理调用指�?
 * @note 必须在持�?channel->lock 时调�?
 */
static pending_call_t* find_pending_call_locked(
    agentos_ipc_channel_t* ch, uint64_t msg_id) {
    pending_call_t* pc = ch->pending_calls;
    while (pc) {
        if (pc->msg_id == msg_id) return pc;
        pc = pc->next;
    }
    return NULL;
}

/**
 * @brief 移除待处理调�?
 * @param ch 通道
 * @param pc 待处理调�?
 * @note 必须在持�?channel->lock 时调�?
 */
static void remove_pending_call_locked(
    agentos_ipc_channel_t* ch, pending_call_t* pc) {
    pending_call_t** pp = &ch->pending_calls;
    while (*pp) {
        if (*pp == pc) {
            *pp = pc->next;
            return;
        }
        pp = &(*pp)->next;
    }
}

/**
 * @brief 创建待处理调用对�?
 * @return 待处理调用指针，失败返回 NULL
 */
static pending_call_t* pending_call_create(void) {
    pending_call_t* pc = (pending_call_t*)AGENTOS_CALLOC(1, sizeof(pending_call_t));
    if (!pc) return NULL;

    if (agentos_mutex_init(&pc->cond_lock) != 0) {
        AGENTOS_FREE(pc);
        return NULL;
    }

    if (agentos_cond_init(&pc->cond) != 0) {
        agentos_mutex_destroy(&pc->cond_lock);
        AGENTOS_FREE(pc);
        return NULL;
    }

    return pc;
}

/**
 * @brief 销毁待处理调用对象
 * @param pc 待处理调用指�?
 */
static void pending_call_destroy(pending_call_t* pc) {
    if (!pc) return;
    agentos_cond_destroy(&pc->cond);
    agentos_mutex_destroy(&pc->cond_lock);
    AGENTOS_FREE(pc);
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始�?IPC 子系�?
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_ipc_init(void) {
    if (!binder_global_lock) {
        binder_global_lock = agentos_mutex_create();
        if (!binder_global_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理 IPC 子系�?
 */
void agentos_ipc_cleanup(void) {
    agentos_mutex_lock(binder_global_lock);
    while (root_nodes) {
        binder_node_t* node = root_nodes;
        root_nodes = node->next;
        if (node->channel) {
            agentos_mutex_destroy(node->channel->lock);
            while (node->channel->pending_calls) {
                pending_call_t* pc = node->channel->pending_calls;
                node->channel->pending_calls = pc->next;
                pending_call_destroy(pc);
            }
            AGENTOS_FREE(node->channel);
        }
        AGENTOS_FREE(node);
    }
    agentos_mutex_unlock(binder_global_lock);

    if (binder_global_lock) {
        agentos_mutex_destroy_ptr(binder_global_lock);
        binder_global_lock = NULL;
    }
}

/**
 * @brief 创建 IPC 通道（服务端�?
 * @param name 通道名称
 * @param callback 消息回调
 * @param userdata 用户数据
 * @param out_channel 输出通道
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel) {

    if (!name || !callback || !out_channel) return AGENTOS_EINVAL;
    if (strlen(name) >= MAX_CHANNEL_NAME) return AGENTOS_EINVAL;

    agentos_ipc_channel_t* ch = NULL;
    binder_node_t* node = NULL;

    agentos_mutex_lock(binder_global_lock);

    if (find_node_locked(name)) {
        agentos_mutex_unlock(binder_global_lock);
        return AGENTOS_EEXIST;
    }

    ch = (agentos_ipc_channel_t*)AGENTOS_CALLOC(1, sizeof(agentos_ipc_channel_t));
    if (!ch) goto cleanup;

    strncpy(ch->name, name, MAX_CHANNEL_NAME - 1);
    ch->name[MAX_CHANNEL_NAME - 1] = '\0';

    ch->lock = agentos_mutex_create();
    if (!ch->lock) goto cleanup;

    node = (binder_node_t*)AGENTOS_CALLOC(1, sizeof(binder_node_t));
    if (!node) goto cleanup;

    strncpy(node->name, name, MAX_CHANNEL_NAME - 1);
    node->name[MAX_CHANNEL_NAME - 1] = '\0';
    node->callback = callback;
    node->userdata = userdata;
    node->channel = ch;
    node->ref_count = 1;

    node->next = root_nodes;
    root_nodes = node;

    ch->local_node = node;

    agentos_mutex_unlock(binder_global_lock);
    *out_channel = ch;
    return AGENTOS_SUCCESS;

cleanup:
    agentos_mutex_unlock(binder_global_lock);
    if (ch) {
        if (ch->lock) agentos_mutex_destroy_ptr(ch->lock);
        AGENTOS_FREE(ch);
    }
    AGENTOS_FREE(node);
    return AGENTOS_ENOMEM;
}

/**
 * @brief 连接�?IPC 通道（客户端�?
 * @param name 通道名称
 * @param out_channel 输出通道
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_ipc_connect(
    const char* name,
    agentos_ipc_channel_t** out_channel) {

    if (!name || !out_channel) return AGENTOS_EINVAL;

    agentos_mutex_lock(binder_global_lock);

    binder_node_t* target = find_node_locked(name);
    if (!target) {
        agentos_mutex_unlock(binder_global_lock);
        return AGENTOS_ENOENT;
    }

    agentos_ipc_channel_t* ch = (agentos_ipc_channel_t*)AGENTOS_CALLOC(1, sizeof(agentos_ipc_channel_t));
    if (!ch) {
        agentos_mutex_unlock(binder_global_lock);
        return AGENTOS_ENOMEM;
    }

    strncpy(ch->name, name, MAX_CHANNEL_NAME - 1);
    ch->name[MAX_CHANNEL_NAME - 1] = '\0';
    ch->remote_target = target;
    ATOMIC_ADD(&target->ref_count, 1);

    ch->lock = agentos_mutex_create();
    if (!ch->lock) {
        ATOMIC_SUB(&target->ref_count, 1);
        AGENTOS_FREE(ch);
        agentos_mutex_unlock(binder_global_lock);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_unlock(binder_global_lock);
    *out_channel = ch;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 同步调用（带超时�?
 * @param channel 通道
 * @param msg 消息
 * @param response 响应缓冲�?
 * @param response_size 响应大小
 * @param timeout_ms 超时毫秒
 * @return AGENTOS_SUCCESS 成功
 *
 * @note 使用条件变量实现高效等待，避免忙轮询
 */
agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms) {

    if (!channel || !msg) return AGENTOS_EINVAL;
    if (!channel->remote_target) return AGENTOS_ENOENT;

    agentos_kernel_ipc_message_t call_msg = *msg;
    call_msg.msg_id = generate_msg_id();

    pending_call_t* pc = pending_call_create();
    if (!pc) return AGENTOS_ENOMEM;

    pc->msg_id = call_msg.msg_id;
    pc->response_buf = response;
    pc->response_capacity = response_size ? *response_size : 0;
    pc->response_size_ptr = response_size;
    pc->completed = 0;
    pc->next = NULL;

    agentos_mutex_lock(channel->lock);
    pc->next = channel->pending_calls;
    channel->pending_calls = pc;
    agentos_mutex_unlock(channel->lock);

    agentos_error_t cb_err = channel->remote_target->callback(
        channel->remote_target->channel, &call_msg,
        channel->remote_target->userdata);

    if (cb_err != AGENTOS_SUCCESS) {
        agentos_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, pc);
        agentos_mutex_unlock(channel->lock);
        pending_call_destroy(pc);
        return cb_err;
    }

    if (timeout_ms == 0) {
        agentos_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, pc);
        agentos_mutex_unlock(channel->lock);
        pending_call_destroy(pc);
        return AGENTOS_SUCCESS;
    }

    /* 使用条件变量等待，避免忙轮询 */
    agentos_mutex_lock(&pc->cond_lock);

    uint64_t start_time = agentos_time_monotonic_ms();
    while (!ATOMIC_LOAD(&pc->completed)) {
        uint64_t elapsed = agentos_time_monotonic_ms() - start_time;
        if (elapsed >= timeout_ms) {
            agentos_mutex_unlock(&pc->cond_lock);

            agentos_mutex_lock(channel->lock);
            remove_pending_call_locked(channel, pc);
            agentos_mutex_unlock(channel->lock);
            pending_call_destroy(pc);
            return AGENTOS_ETIMEDOUT;
        }

        uint32_t remaining = (uint32_t)(timeout_ms - elapsed);
        if (remaining > 100) remaining = 100; /* 最多等�?00ms后检�?*/

        agentos_cond_timedwait(&pc->cond, &pc->cond_lock, remaining);
    }

    agentos_mutex_unlock(&pc->cond_lock);

    agentos_mutex_lock(channel->lock);
    remove_pending_call_locked(channel, pc);
    agentos_mutex_unlock(channel->lock);

    pending_call_destroy(pc);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 异步发送消�?
 * @param channel 通道
 * @param msg 消息
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg) {

    if (!channel || !msg) return AGENTOS_EINVAL;
    if (!channel->remote_target) return AGENTOS_ENOENT;

    agentos_kernel_ipc_message_t send_msg = *msg;
    send_msg.msg_id = generate_msg_id();

    agentos_error_t err = channel->remote_target->callback(
        channel->remote_target->channel, &send_msg,
        channel->remote_target->userdata);

    return err;
}

/**
 * @brief 回复消息
 * @param channel 通道
 * @param msg 消息
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_kernel_ipc_message_t* msg) {

    if (!channel || !msg) return AGENTOS_EINVAL;

    agentos_mutex_lock(channel->lock);

    pending_call_t* pc = find_pending_call_locked(channel, msg->msg_id);
    if (!pc) {
        agentos_mutex_unlock(channel->lock);
        return AGENTOS_ENOENT;
    }

    if (msg->data && pc->response_buf) {
        size_t copy_size = msg->size;
        if (copy_size > pc->response_capacity) {
            copy_size = pc->response_capacity;
        }
        memcpy(pc->response_buf, msg->data, copy_size);
    }

    if (pc->response_size_ptr) {
        *pc->response_size_ptr = msg->size;
    }

    ATOMIC_STORE(&pc->completed, 1);

    /* 唤醒等待的线�?*/
    agentos_cond_signal(&pc->cond);

    agentos_mutex_unlock(channel->lock);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 关闭通道
 * @param channel 通道
 * @return agentos_error_t 错误码
 */
agentos_error_t agentos_ipc_close(agentos_ipc_channel_t* channel) {
    if (!channel) return AGENTOS_EINVAL;

    if (channel->remote_target) {
        if (ATOMIC_SUB(&channel->remote_target->ref_count, 1) == 0) {
            agentos_mutex_lock(binder_global_lock);
            binder_node_t** pp = &root_nodes;
            while (*pp) {
                if (*pp == channel->remote_target) {
                    binder_node_t* dead = *pp;
                    *pp = dead->next;
                    if (dead->channel && dead->channel != channel) {
                        agentos_ipc_close(dead->channel);
                    }
                    AGENTOS_FREE(dead);
                    break;
                }
                pp = &(*pp)->next;
            }
            agentos_mutex_unlock(binder_global_lock);
        }
    }

    if (channel->lock) {
        agentos_mutex_destroy(channel->lock);
    }

    AGENTOS_FREE(channel);
    return AGENTOS_SUCCESS;
}
