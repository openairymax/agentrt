#include "agentrt.h"
/**
 * @file binder.c
 * @brief Binder 风格 IPC 实现（含消息队列接收模式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现高效的进程间通信机制：
 * - Binder 风格同步调用（带超时）
 * - Binder 风格异步发送与回复
 * - 消息队列接收模式（recv/get_fd）
 *
 * 使用条件变量优化超时等待，避免忙轮询消耗 CPU 资源。
 */

#include "agentrt_time.h"
#include "atomic_compat.h"
#include "ipc.h"
#include "mem.h"
#include "memory_compat.h"
#include "platform.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/* ==================== 兼容层：create/destroy 包装 ==================== */

static inline agentrt_mutex_t *agentrt_mutex_create_compat(void)
{
    agentrt_mutex_t *m = (agentrt_mutex_t *)AGENTRT_CALLOC(1, sizeof(agentrt_mutex_t));
    if (m && agentrt_mutex_init(m) != 0) {
        AGENTRT_FREE(m);
        AGENTRT_LOG_ERROR("agentrt_mutex_create_compat: mutex init failed");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    return m;
}

static inline void agentrt_mutex_destroy_compat(agentrt_mutex_t *m)
{
    if (m) {
        agentrt_mutex_destroy(m);
        AGENTRT_FREE(m);
    }
}

static inline agentrt_cond_t *agentrt_cond_create_compat(void)
{
    agentrt_cond_t *c = (agentrt_cond_t *)AGENTRT_CALLOC(1, sizeof(agentrt_cond_t));
    if (c && agentrt_cond_init(c) != 0) {
        AGENTRT_FREE(c);
        AGENTRT_LOG_ERROR("agentrt_cond_create_compat: cond init failed");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    return c;
}

static inline void agentrt_cond_destroy_compat(agentrt_cond_t *c)
{
    if (c) {
        agentrt_cond_destroy(c);
        AGENTRT_FREE(c);
    }
}

#define agentrt_mutex_create() agentrt_mutex_create_compat()
#define agentrt_mutex_destroy_ptr(p) agentrt_mutex_destroy_compat(p)
#define agentrt_cond_create() agentrt_cond_create_compat()
#define agentrt_cond_destroy_ptr(p) agentrt_cond_destroy_compat(p)

#define MAX_CHANNEL_NAME 64

/* ==================== 数据结构（前向声明，供 OOM 池使用） ==================== */

typedef struct binder_node {
    char name[MAX_CHANNEL_NAME];
    agentrt_ipc_callback_t callback;
    void *userdata;
    agentrt_ipc_channel_t *channel;
    struct binder_node *next;
    atomic_int ref_count;
} binder_node_t;

typedef struct pending_call {
    uint64_t msg_id;
    void *response_buf;
    size_t response_capacity;
    size_t *response_size_ptr;
    atomic_int completed;
    struct pending_call *next;

    agentrt_cond_t cond;
    agentrt_mutex_t cond_lock;
} pending_call_t;

typedef struct ipc_message_node {
    agentrt_kernel_ipc_message_t msg;
    struct ipc_message_node *next;
} ipc_message_node_t;

/* ==================== SEC-13: OOM 关键路径预分配内存池 ==================== */

#define IPC_OOM_PREALLOC_MSG_NODES 32     /**< OOM 时预分配的 IPC 消息节点数 */
#define IPC_OOM_PREALLOC_PENDING_CALLS 8  /**< OOM 时预分配的待处理调用数 */

static ipc_message_node_t g_ipc_oom_msg_nodes[IPC_OOM_PREALLOC_MSG_NODES];
static bool g_ipc_oom_msg_used[IPC_OOM_PREALLOC_MSG_NODES];
static pending_call_t g_ipc_oom_pending_calls[IPC_OOM_PREALLOC_PENDING_CALLS];
static bool g_ipc_oom_pending_used[IPC_OOM_PREALLOC_PENDING_CALLS];
static bool g_ipc_oom_initialized = false;
static agentrt_mutex_t g_ipc_oom_lock_data;
static agentrt_mutex_t *g_ipc_oom_lock = NULL;

static void ipc_oom_pool_init(void)
{
    if (g_ipc_oom_initialized) return;

    __builtin_memset(g_ipc_oom_msg_nodes, 0, sizeof(g_ipc_oom_msg_nodes));
    __builtin_memset(g_ipc_oom_msg_used, 0, sizeof(g_ipc_oom_msg_used));
    __builtin_memset(g_ipc_oom_pending_calls, 0, sizeof(g_ipc_oom_pending_calls));
    __builtin_memset(g_ipc_oom_pending_used, 0, sizeof(g_ipc_oom_pending_used));

    /* 预初始化 pending_call 池中的 mutex 和 cond */
    for (int i = 0; i < IPC_OOM_PREALLOC_PENDING_CALLS; i++) {
        agentrt_mutex_init(&g_ipc_oom_pending_calls[i].cond_lock);
        agentrt_cond_init(&g_ipc_oom_pending_calls[i].cond);
    }

    agentrt_mutex_init(&g_ipc_oom_lock_data);
    g_ipc_oom_lock = &g_ipc_oom_lock_data;
    g_ipc_oom_initialized = true;
}

static void ipc_oom_pool_cleanup(void)
{
    if (!g_ipc_oom_initialized) return;

    /* 销毁所有预初始化的 pending_call mutex 和 cond */
    for (int i = 0; i < IPC_OOM_PREALLOC_PENDING_CALLS; i++) {
        agentrt_cond_destroy(&g_ipc_oom_pending_calls[i].cond);
        agentrt_mutex_destroy(&g_ipc_oom_pending_calls[i].cond_lock);
    }

    if (g_ipc_oom_lock) {
        agentrt_mutex_destroy(g_ipc_oom_lock);
        g_ipc_oom_lock = NULL;
    }
    g_ipc_oom_initialized = false;
}

static ipc_message_node_t *ipc_oom_msg_node_alloc(void)
{
    if (!g_ipc_oom_initialized || !g_ipc_oom_lock) return NULL;

    agentrt_mutex_lock(g_ipc_oom_lock);
    for (int i = 0; i < IPC_OOM_PREALLOC_MSG_NODES; i++) {
        if (!g_ipc_oom_msg_used[i]) {
            g_ipc_oom_msg_used[i] = true;
            __builtin_memset(&g_ipc_oom_msg_nodes[i], 0, sizeof(ipc_message_node_t));
            agentrt_mutex_unlock(g_ipc_oom_lock);
            return &g_ipc_oom_msg_nodes[i];
        }
    }
    agentrt_mutex_unlock(g_ipc_oom_lock);
    return NULL;
}

static void ipc_oom_msg_node_free(ipc_message_node_t *node)
{
    if (!node || !g_ipc_oom_initialized || !g_ipc_oom_lock) return;

    /* 检查是否属于 OOM 池 */
    ptrdiff_t index = node - g_ipc_oom_msg_nodes;
    if (index < 0 || index >= IPC_OOM_PREALLOC_MSG_NODES) return;

    agentrt_mutex_lock(g_ipc_oom_lock);
    g_ipc_oom_msg_used[index] = false;
    agentrt_mutex_unlock(g_ipc_oom_lock);
}

static pending_call_t *ipc_oom_pending_call_alloc(void)
{
    if (!g_ipc_oom_initialized || !g_ipc_oom_lock) return NULL;

    agentrt_mutex_lock(g_ipc_oom_lock);
    for (int i = 0; i < IPC_OOM_PREALLOC_PENDING_CALLS; i++) {
        if (!g_ipc_oom_pending_used[i]) {
            g_ipc_oom_pending_used[i] = true;
            /* 重置 pending_call 字段（保留已初始化的 mutex/cond） */
            pending_call_t *pc = &g_ipc_oom_pending_calls[i];
            pc->msg_id = 0;
            pc->response_buf = NULL;
            pc->response_capacity = 0;
            pc->response_size_ptr = NULL;
            atomic_store_explicit(&pc->completed, 0, memory_order_seq_cst);
            pc->next = NULL;
            agentrt_mutex_unlock(g_ipc_oom_lock);
            return pc;
        }
    }
    agentrt_mutex_unlock(g_ipc_oom_lock);
    return NULL;
}

static void ipc_oom_pending_call_free(pending_call_t *pc)
{
    if (!pc || !g_ipc_oom_initialized || !g_ipc_oom_lock) return;

    /* 检查是否属于 OOM 池 */
    ptrdiff_t index = pc - g_ipc_oom_pending_calls;
    if (index < 0 || index >= IPC_OOM_PREALLOC_PENDING_CALLS) return;

    agentrt_mutex_lock(g_ipc_oom_lock);
    g_ipc_oom_pending_used[index] = false;
    agentrt_mutex_unlock(g_ipc_oom_lock);
}

/* ==================== 平台相关宏定义 ==================== */

static atomic_uint64_t next_msg_id = 1;
static uint64_t generate_msg_id(void)
{
    return atomic_fetch_add_explicit(&next_msg_id, 1, memory_order_seq_cst);
}

/* ==================== 常量定义 ==================== */

/* ==================== IPC Channel 结构 ==================== */

struct agentrt_ipc_channel {
    char name[MAX_CHANNEL_NAME];
    binder_node_t *local_node;
    binder_node_t *remote_target;
    agentrt_mutex_t *lock;
    pending_call_t *pending_calls;

    int32_t fd;
    agentrt_ipc_port_t port;
    int is_server;
    agentrt_cond_t *cond;
    ipc_message_node_t *queue;
    size_t queue_size;
};

/* ==================== 全局状态 ==================== */

static binder_node_t *root_nodes = NULL;
static agentrt_mutex_t *binder_global_lock = NULL;

static int ensure_binder_lock(void)
{
    if (binder_global_lock)
        return AGENTRT_SUCCESS;
    agentrt_mutex_t *lock = agentrt_mutex_create();
    if (!lock) {
        AGENTRT_LOG_ERROR("ensure_binder_lock: mutex create failed, ENOMEM");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    agentrt_mutex_t *expected = NULL;
    if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&binder_global_lock,
                                            (void **)&expected, (void *)lock, memory_order_acq_rel,
                                            memory_order_acquire)) {
        agentrt_mutex_destroy_ptr(lock);
    }
    return AGENTRT_SUCCESS;
}

/* ==================== 内部辅助函数 ==================== */

static binder_node_t *find_node_locked(const char *name)
{
    binder_node_t *node = root_nodes;
    while (node) {
        if (strcmp(node->name, name) == 0)
            return node;
        node = node->next;
    }
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}

static pending_call_t *find_pending_call_locked(agentrt_ipc_channel_t *ch, uint64_t msg_id)
{
    pending_call_t *pc = ch->pending_calls;
    while (pc) {
        if (pc->msg_id == msg_id)
            return pc;
        pc = pc->next;
    }
    AGENTRT_LOG_WARN("find_pending_call_locked: pending call not found, msg_id=%llu", (unsigned long long)msg_id);
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}

static void remove_pending_call_locked(agentrt_ipc_channel_t *ch, pending_call_t *pc)
{
    pending_call_t **pp = &ch->pending_calls;
    while (*pp) {
        if (*pp == pc) {
            *pp = pc->next;
            return;
        }
        pp = &(*pp)->next;
    }
}

static pending_call_t *pending_call_create(void)
{
    pending_call_t *pc = (pending_call_t *)AGENTRT_CALLOC(1, sizeof(pending_call_t));
    if (!pc) {
        /* SEC-13: OOM 回退 — 尝试从预分配池获取 */
        pc = ipc_oom_pending_call_alloc();
        if (pc) return pc;
        return NULL;
    }

    if (agentrt_mutex_init(&pc->cond_lock) != 0) {
        AGENTRT_LOG_ERROR("pending_call_create: mutex init failed");
        AGENTRT_FREE(pc);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    if (agentrt_cond_init(&pc->cond) != 0) {
        AGENTRT_LOG_ERROR("pending_call_create: cond init failed");
        agentrt_mutex_destroy(&pc->cond_lock);
        AGENTRT_FREE(pc);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    return pc;
}

static void pending_call_destroy(pending_call_t *pc)
{
    if (!pc)
        return;

    /* SEC-13: 检查是否属于 OOM 预分配池 */
    if (pc >= g_ipc_oom_pending_calls &&
        pc < g_ipc_oom_pending_calls + IPC_OOM_PREALLOC_PENDING_CALLS) {
        ipc_oom_pending_call_free(pc);
        return;
    }

    agentrt_cond_destroy(&pc->cond);
    agentrt_mutex_destroy(&pc->cond_lock);
    AGENTRT_FREE(pc);
}

static void channel_queue_clear_locked(agentrt_ipc_channel_t *ch)
{
    if (!ch)
        return;
    ipc_message_node_t *node = ch->queue;
    while (node) {
        ipc_message_node_t *next = node->next;
        /* SEC-13: 检查是否属于 OOM 预分配池 */
        if (node >= g_ipc_oom_msg_nodes &&
            node < g_ipc_oom_msg_nodes + IPC_OOM_PREALLOC_MSG_NODES) {
            ipc_oom_msg_node_free(node);
        } else {
            AGENTRT_FREE(node);
        }
        node = next;
    }
    ch->queue = NULL;
    ch->queue_size = 0;
}

/* ==================== 公共接口实现 ==================== */

agentrt_error_t agentrt_ipc_init(void)
{
    /* SEC-13: 初始化 OOM 预分配池 */
    ipc_oom_pool_init();

    if (!binder_global_lock) {
        agentrt_mutex_t *new_lock = agentrt_mutex_create();
        if (!new_lock) {
            AGENTRT_LOG_ERROR("agentrt_ipc_init: mutex create failed, ENOMEM");
            ATM_RET_ERR(AGENTRT_ENOMEM);
        }

        agentrt_mutex_t *expected = NULL;
        if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&binder_global_lock,
                                                (void **)&expected, (void *)new_lock,
                                                memory_order_seq_cst, memory_order_seq_cst)) {
            agentrt_mutex_free(new_lock);
        }
    }
    return AGENTRT_SUCCESS;
}

void agentrt_ipc_cleanup(void)
{
    if (!binder_global_lock)
        return;

    agentrt_mutex_lock(binder_global_lock);
    while (root_nodes) {
        binder_node_t *node = root_nodes;
        root_nodes = node->next;
        if (node->channel) {
            while (node->channel->pending_calls) {
                pending_call_t *pc = node->channel->pending_calls;
                node->channel->pending_calls = pc->next;
                pending_call_destroy(pc);
            }
            channel_queue_clear_locked(node->channel);
            if (node->channel->cond) {
                agentrt_cond_destroy_ptr(node->channel->cond);
                node->channel->cond = NULL;
            }
            if (node->channel->lock) {
                agentrt_mutex_destroy_ptr(node->channel->lock);
                node->channel->lock = NULL;
            }
            AGENTRT_FREE(node->channel);
        }
        AGENTRT_FREE(node);
    }
    agentrt_mutex_unlock(binder_global_lock);

    agentrt_mutex_destroy_ptr(binder_global_lock);
    binder_global_lock = NULL;

    /* SEC-13: 清理 OOM 预分配池 */
    ipc_oom_pool_cleanup();
}

agentrt_error_t agentrt_ipc_create_channel(const char *name, agentrt_ipc_callback_t callback,
                                           void *userdata, agentrt_ipc_channel_t **out_channel)
{

    if (!name || !out_channel) {
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: null parameter, name=%p out_channel=%p", (void *)name, (void *)out_channel);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    if (strlen(name) >= MAX_CHANNEL_NAME) {
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: name too long, len=%zu max=%d", strlen(name), MAX_CHANNEL_NAME);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    int err = ensure_binder_lock();
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_ipc_channel_t *ch = NULL;
    binder_node_t *node = NULL;

    agentrt_mutex_lock(binder_global_lock);

    if (callback && find_node_locked(name)) {
        agentrt_mutex_unlock(binder_global_lock);
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: channel already exists, name=%s", name);
        ATM_RET_ERR(AGENTRT_EEXIST);
    }

    ch = (agentrt_ipc_channel_t *)AGENTRT_CALLOC(1, sizeof(agentrt_ipc_channel_t));
    if (!ch) {
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: channel alloc failed, ENOMEM");
        goto cleanup;
    }

AGENTRT_STRNCPY_TERM(ch->name, name, MAX_CHANNEL_NAME);
    ch->fd = -1;
    ch->is_server = callback ? 1 : 0;

    ch->lock = agentrt_mutex_create();
    if (!ch->lock) {
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: channel lock create failed, ENOMEM");
        goto cleanup;
    }

    ch->cond = agentrt_cond_create();
    if (!ch->cond) {
        AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: channel cond create failed, ENOMEM");
        goto cleanup;
    }

    if (callback) {
        node = (binder_node_t *)AGENTRT_CALLOC(1, sizeof(binder_node_t));
        if (!node) {
            AGENTRT_LOG_ERROR("agentrt_ipc_create_channel: binder node alloc failed, ENOMEM");
            goto cleanup;
        }

AGENTRT_STRNCPY_TERM(node->name, name, MAX_CHANNEL_NAME);
        node->callback = callback;
        node->userdata = userdata;
        node->channel = ch;
        node->ref_count = 1;

        node->next = root_nodes;
        root_nodes = node;

        ch->local_node = node;
    }

    agentrt_mutex_unlock(binder_global_lock);
    *out_channel = ch;
    return AGENTRT_SUCCESS;

cleanup:
    agentrt_mutex_unlock(binder_global_lock);
    if (ch) {
        if (ch->cond)
            agentrt_cond_destroy_ptr(ch->cond);
        if (ch->lock)
            agentrt_mutex_destroy_ptr(ch->lock);
        AGENTRT_FREE(ch);
    }
    AGENTRT_FREE(node);
    ATM_RET_ERR(AGENTRT_ENOMEM);
}

agentrt_error_t agentrt_ipc_connect(const char *name, agentrt_ipc_channel_t **out_channel)
{

    if (!name || !out_channel) {
        AGENTRT_LOG_ERROR("agentrt_ipc_connect: null parameter, name=%p out_channel=%p", (void *)name, (void *)out_channel);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    int err = ensure_binder_lock();
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_mutex_lock(binder_global_lock);

    binder_node_t *target = find_node_locked(name);
    if (!target) {
        agentrt_mutex_unlock(binder_global_lock);
        AGENTRT_LOG_ERROR("agentrt_ipc_connect: target not found, name=%s", name);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    agentrt_ipc_channel_t *ch =
        (agentrt_ipc_channel_t *)AGENTRT_CALLOC(1, sizeof(agentrt_ipc_channel_t));
    if (!ch) {
        agentrt_mutex_unlock(binder_global_lock);
        AGENTRT_LOG_ERROR("agentrt_ipc_connect: channel alloc failed, ENOMEM");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

AGENTRT_STRNCPY_TERM(ch->name, name, MAX_CHANNEL_NAME);
    ch->remote_target = target;
    ch->fd = -1;
    atomic_fetch_add_explicit(&target->ref_count, 1, memory_order_seq_cst);

    ch->lock = agentrt_mutex_create();
    if (!ch->lock) {
        AGENTRT_LOG_ERROR("agentrt_ipc_connect: channel lock create failed, ENOMEM");
        atomic_fetch_sub_explicit(&target->ref_count, 1, memory_order_seq_cst);
        AGENTRT_FREE(ch);
        agentrt_mutex_unlock(binder_global_lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    ch->cond = agentrt_cond_create();
    if (!ch->cond) {
        AGENTRT_LOG_ERROR("agentrt_ipc_connect: channel cond create failed, ENOMEM");
        agentrt_mutex_destroy_ptr(ch->lock);
        atomic_fetch_sub_explicit(&target->ref_count, 1, memory_order_seq_cst);
        AGENTRT_FREE(ch);
        agentrt_mutex_unlock(binder_global_lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    agentrt_mutex_unlock(binder_global_lock);
    *out_channel = ch;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_ipc_call(agentrt_ipc_channel_t *channel,
                                 const agentrt_kernel_ipc_message_t *msg, void *response,
                                 size_t *response_size, uint32_t timeout_ms)
{

    if (!channel || !msg) {
        AGENTRT_LOG_ERROR("agentrt_ipc_call: null parameter, channel=%p msg=%p", (void *)channel, (void *)msg);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    if (!channel->remote_target) {
        AGENTRT_LOG_ERROR("agentrt_ipc_call: no remote target, channel=%p", (void *)channel);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    agentrt_kernel_ipc_message_t call_msg = *msg;
    call_msg.msg_id = generate_msg_id();

    pending_call_t *pc = pending_call_create();
    if (!pc) {
        AGENTRT_LOG_ERROR("agentrt_ipc_call: pending call create failed, ENOMEM");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    pc->msg_id = call_msg.msg_id;
    pc->response_buf = response;
    pc->response_capacity = response_size ? *response_size : 0;
    pc->response_size_ptr = response_size;
    pc->completed = 0;
    pc->next = NULL;

    agentrt_mutex_lock(channel->lock);
    pc->next = channel->pending_calls;
    channel->pending_calls = pc;
    agentrt_mutex_unlock(channel->lock);

    agentrt_error_t cb_err = channel->remote_target->callback(
        channel->remote_target->channel, &call_msg, channel->remote_target->userdata);

    if (cb_err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("agentrt_ipc_call: remote callback failed, err=%d msg_id=%llu", cb_err, (unsigned long long)call_msg.msg_id);
        agentrt_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, pc);
        agentrt_mutex_unlock(channel->lock);
        pending_call_destroy(pc);
        return cb_err;
    }

    if (timeout_ms == 0) {
        agentrt_mutex_lock(channel->lock);
        remove_pending_call_locked(channel, pc);
        agentrt_mutex_unlock(channel->lock);
        pending_call_destroy(pc);
        return AGENTRT_SUCCESS;
    }

    agentrt_mutex_lock(&pc->cond_lock);

    uint64_t start_time = agentrt_time_monotonic_ms();
    while (!atomic_load_explicit(&pc->completed, memory_order_seq_cst)) {
        uint64_t elapsed = agentrt_time_monotonic_ms() - start_time;
        if (elapsed >= timeout_ms) {
            agentrt_mutex_unlock(&pc->cond_lock);

            AGENTRT_LOG_WARN("agentrt_ipc_call: timed out, msg_id=%llu timeout_ms=%u elapsed=%llu", (unsigned long long)call_msg.msg_id, timeout_ms, (unsigned long long)elapsed);

            agentrt_mutex_lock(channel->lock);
            remove_pending_call_locked(channel, pc);
            agentrt_mutex_unlock(channel->lock);
            pending_call_destroy(pc);
            ATM_RET_ERR(AGENTRT_ETIMEDOUT);
        }

        uint32_t remaining = (uint32_t)(timeout_ms - elapsed);
        if (remaining > 100)
            remaining = 100;

        agentrt_cond_timedwait(&pc->cond, &pc->cond_lock, remaining);
    }

    agentrt_mutex_unlock(&pc->cond_lock);

    agentrt_mutex_lock(channel->lock);
    remove_pending_call_locked(channel, pc);
    agentrt_mutex_unlock(channel->lock);

    pending_call_destroy(pc);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_ipc_send(agentrt_ipc_channel_t *channel,
                                 const agentrt_kernel_ipc_message_t *msg)
{

    if (!channel || !msg) {
        AGENTRT_LOG_ERROR("agentrt_ipc_send: null parameter, channel=%p msg=%p", (void *)channel, (void *)msg);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (channel->remote_target) {
        agentrt_kernel_ipc_message_t send_msg = *msg;
        send_msg.msg_id = generate_msg_id();

        agentrt_error_t err = channel->remote_target->callback(
            channel->remote_target->channel, &send_msg, channel->remote_target->userdata);

        return err;
    }

    agentrt_mutex_lock(channel->lock);

    ipc_message_node_t *node = (ipc_message_node_t *)AGENTRT_CALLOC(1, sizeof(ipc_message_node_t));
    if (!node) {
        /* SEC-13: OOM 回退 — 尝试从预分配池获取 */
        node = ipc_oom_msg_node_alloc();
        if (!node) {
            AGENTRT_LOG_ERROR("agentrt_ipc_send: msg node alloc failed (OOM pool exhausted), ENOMEM");
            agentrt_mutex_unlock(channel->lock);
            ATM_RET_ERR(AGENTRT_ENOMEM);
        }
    }

    __builtin_memcpy(&node->msg, msg, sizeof(agentrt_kernel_ipc_message_t));
    node->next = NULL;

    if (!channel->queue) {
        channel->queue = node;
    } else {
        ipc_message_node_t *tail = channel->queue;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = node;
    }
    channel->queue_size++;

    if (channel->cond) {
        agentrt_cond_signal(channel->cond);
    }

    agentrt_mutex_unlock(channel->lock);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_ipc_reply(agentrt_ipc_channel_t *channel,
                                  const agentrt_kernel_ipc_message_t *msg)
{

    if (!channel || !msg) {
        AGENTRT_LOG_ERROR("agentrt_ipc_reply: null parameter, channel=%p msg=%p", (void *)channel, (void *)msg);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    agentrt_mutex_lock(channel->lock);

    pending_call_t *pc = find_pending_call_locked(channel, msg->msg_id);
    if (!pc) {
        agentrt_mutex_unlock(channel->lock);
        AGENTRT_LOG_WARN("agentrt_ipc_reply: pending call not found, msg_id=%llu", (unsigned long long)msg->msg_id);
        ATM_RET_ERR(AGENTRT_ENOENT);
    }

    if (msg->data && pc->response_buf) {
        size_t copy_size = msg->size;
        if (copy_size > pc->response_capacity) {
            copy_size = pc->response_capacity;
        }
        __builtin_memcpy(pc->response_buf, msg->data, copy_size);
    }

    if (pc->response_size_ptr) {
        *pc->response_size_ptr = msg->size;
    }

    atomic_store_explicit(&pc->completed, 1, memory_order_seq_cst);

    agentrt_cond_signal(&pc->cond);

    agentrt_mutex_unlock(channel->lock);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_ipc_recv(agentrt_ipc_channel_t *channel, uint32_t timeout_ms,
                                 agentrt_kernel_ipc_message_t *out_msg)
{

    if (!channel || !out_msg) {
        AGENTRT_LOG_ERROR("agentrt_ipc_recv: null parameter, channel=%p out_msg=%p", (void *)channel, (void *)out_msg);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    agentrt_mutex_lock(channel->lock);

    if (!channel->queue && timeout_ms > 0 && channel->cond) {
        agentrt_cond_timedwait(channel->cond, channel->lock, timeout_ms);
    }

    if (!channel->queue) {
        agentrt_mutex_unlock(channel->lock);
        AGENTRT_LOG_WARN("agentrt_ipc_recv: queue empty after wait, timeout_ms=%u", timeout_ms);
        ATM_RET_ERR(AGENTRT_ETIMEDOUT);
    }

    ipc_message_node_t *node = channel->queue;
    __builtin_memcpy(out_msg, &node->msg, sizeof(agentrt_kernel_ipc_message_t));

    channel->queue = node->next;
    channel->queue_size--;

    /* SEC-13: 检查是否属于 OOM 预分配池再释放 */
    if (node >= g_ipc_oom_msg_nodes &&
        node < g_ipc_oom_msg_nodes + IPC_OOM_PREALLOC_MSG_NODES) {
        ipc_oom_msg_node_free(node);
    } else {
        AGENTRT_FREE(node);
    }

    agentrt_mutex_unlock(channel->lock);

    return AGENTRT_SUCCESS;
}

int32_t agentrt_ipc_get_fd(agentrt_ipc_channel_t *channel)
{
    if (!channel) {
        AGENTRT_LOG_ERROR("agentrt_ipc_get_fd: null channel");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    return channel->fd;
}

agentrt_error_t agentrt_ipc_close(agentrt_ipc_channel_t *channel)
{
    if (!channel) {
        AGENTRT_LOG_ERROR("agentrt_ipc_close: null channel");
        ATM_RET_ERR(AGENTRT_EINVAL);
    }
    int err = ensure_binder_lock();
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("agentrt_ipc_close: ensure_binder_lock failed, err=%d", err);
        return err;
    }

    if (channel->remote_target) {
        if (atomic_fetch_sub_explicit(&channel->remote_target->ref_count, 1,
                                      memory_order_seq_cst) == 1) {
            agentrt_mutex_lock(binder_global_lock);
            binder_node_t **pp = &root_nodes;
            while (*pp) {
                if (*pp == channel->remote_target) {
                    binder_node_t *dead = *pp;
                    *pp = dead->next;
                    if (dead->channel && dead->channel != channel) {
                        agentrt_ipc_close(dead->channel);
                    }
                    AGENTRT_FREE(dead);
                    break;
                }
                pp = &(*pp)->next;
            }
            agentrt_mutex_unlock(binder_global_lock);
        }
    }

    if (channel->lock) {
        agentrt_mutex_lock(channel->lock);

        while (channel->pending_calls) {
            pending_call_t *pc = channel->pending_calls;
            channel->pending_calls = pc->next;
            pending_call_destroy(pc);
        }

        channel_queue_clear_locked(channel);

        if (channel->cond) {
            agentrt_cond_destroy_ptr(channel->cond);
            channel->cond = NULL;
        }

        agentrt_mutex_unlock(channel->lock);
        agentrt_mutex_destroy_ptr(channel->lock);
        channel->lock = NULL;
    }

    AGENTRT_FREE(channel);
    return AGENTRT_SUCCESS;
}
