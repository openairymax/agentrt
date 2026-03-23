/**
 * @file binder.c
 * @brief Binder 风格 IPC 实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "task.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

#define MAX_CHANNEL_NAME 64

typedef struct binder_node {
    char name[MAX_CHANNEL_NAME];
    agentos_ipc_callback_t callback;
    void* userdata;
    agentos_ipc_channel_t* channel;
    struct binder_node* next;
    volatile int ref_count;
} binder_node_t;

typedef struct pending_call {
    uint64_t msg_id;
    void* response_buf;
    size_t response_capacity;
    size_t* response_size_ptr;
    volatile int completed;
    struct pending_call* next;
} pending_call_t;

struct agentos_ipc_channel {
    char name[MAX_CHANNEL_NAME];
    binder_node_t* local_node;
    binder_node_t* remote_target;
    agentos_mutex_t* lock;
    pending_call_t* pending_calls;
};

static binder_node_t* root_nodes = NULL;
static agentos_mutex_t* binder_global_lock = NULL;

static binder_node_t* find_node_locked(const char* name) {
    binder_node_t* node = root_nodes;
    while (node) {
        if (strcmp(node->name, name) == 0) return node;
        node = node->next;
    }
    return NULL;
}

static pending_call_t* find_pending_call_locked(
    agentos_ipc_channel_t* ch, uint64_t msg_id) {
    pending_call_t* pc = ch->pending_calls;
    while (pc) {
        if (pc->msg_id == msg_id) return pc;
        pc = pc->next;
    }
    return NULL;
}

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

agentos_error_t agentos_ipc_init(void) {
    if (!binder_global_lock) {
        binder_global_lock = agentos_mutex_create();
        if (!binder_global_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

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
                free(pc);
            }
            free(node->channel);
        }
        free(node);
    }
    agentos_mutex_unlock(binder_global_lock);

    if (binder_global_lock) {
        agentos_mutex_destroy(binder_global_lock);
        binder_global_lock = NULL;
    }
}

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

    ch = (agentos_ipc_channel_t*)calloc(1, sizeof(agentos_ipc_channel_t));
    if (!ch) goto cleanup;

    strncpy(ch->name, name, MAX_CHANNEL_NAME - 1);
    ch->name[MAX_CHANNEL_NAME - 1] = '\0';

    ch->lock = agentos_mutex_create();
    if (!ch->lock) goto cleanup;

    node = (binder_node_t*)calloc(1, sizeof(binder_node_t));
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
        if (ch->lock) agentos_mutex_destroy(ch->lock);
        free(ch);
    }
    free(node);
    return AGENTOS_ENOMEM;
}

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

    agentos_ipc_channel_t* ch = (agentos_ipc_channel_t*)calloc(1, sizeof(agentos_ipc_channel_t));
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
        free(ch);
        agentos_mutex_unlock(binder_global_lock);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_unlock(binder_global_lock);
    *out_channel = ch;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms) {

    if (!channel || !msg) return AGENTOS_EINVAL;
    if (!channel->remote_target) return AGENTOS_ENOENT;

    agentos_ipc_message_t call_msg = *msg;
    call_msg.msg_id = generate_msg_id();

    pending_call_t pc;
    pc.msg_id = call_msg.msg_id;
    pc.response_buf = response;
    pc.response_capacity = response_size ? *response_size : 0;
    pc.response_size_ptr = response_size;
    pc.completed = 0;
    pc.next = NULL;

    agentos_mutex_lock(channel->lock);
    pc.next = channel->pending_calls;
    channel->pending_calls = &pc;
    agentos_mutex_unlock(channel->lock);

    agentos_error_t cb_err = channel->remote_target->callback(
        channel->remote_target->channel, &call_msg,
        channel->remote_target->userdata);

    if (cb_err != AGENTOS_SUCCESS) {
        agentos_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, &pc);
        agentos_mutex_unlock(channel->lock);
        return cb_err;
    }

    if (timeout_ms == 0) {
        agentos_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, &pc);
        agentos_mutex_unlock(channel->lock);
        return AGENTOS_SUCCESS;
    }

    uint32_t waited = 0;
    const uint32_t poll_interval = 1;
    while (!ATOMIC_LOAD(&pc.completed)) {
        agentos_task_sleep(poll_interval);
        waited += poll_interval;
        if (waited >= timeout_ms) {
            agentos_mutex_lock(channel->lock);
            remove_pending_call_locked(channel, &pc);
            agentos_mutex_unlock(channel->lock);
            return AGENTOS_ETIMEDOUT;
        }
    }

    agentos_mutex_lock(channel->lock);
    remove_pending_call_locked(channel, &pc);
    agentos_mutex_unlock(channel->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg) {

    if (!channel || !msg) return AGENTOS_EINVAL;
    if (!channel->remote_target) return AGENTOS_ENOENT;

    agentos_ipc_message_t send_msg = *msg;
    send_msg.msg_id = generate_msg_id();

    agentos_error_t err = channel->remote_target->callback(
        channel->remote_target->channel, &send_msg,
        channel->remote_target->userdata);

    return err;
}

agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg) {

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

    agentos_mutex_unlock(channel->lock);
    return AGENTOS_SUCCESS;
}

void agentos_ipc_close(agentos_ipc_channel_t* channel) {
    if (!channel) return;

    if (channel->remote_target) {
        if (ATOMIC_SUB(&channel->remote_target->ref_count, 1) == 0) {
            agentos_mutex_lock(binder_global_lock);
            binder_node_t** pp = &root_nodes;
            while (*pp) {
                if (*pp == channel->remote_target) {
                    binder_node_t* dead = *pp;
                    *pp = dead->next;
                    free(dead);
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

    free(channel);
}
