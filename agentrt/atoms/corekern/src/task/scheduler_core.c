#include "agentrt.h"
/**
 * @file scheduler_core.c
 * @brief 调度器核心层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include "scheduler_core.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "atomic_compat.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/* ==================== 静态全局状态 ==================== */

/* W09.4 (BUG-4 修复): g_core_ctx 和 g_ctx_init_lock 声明为 _Atomic 指针。
 * 原声明为非 _Atomic 指针，但通过 (_Atomic void **) 强制转换访问，
 * 在 C11 标准下属于未定义行为（对非 _Atomic 限定对象使用原子操作）。
 * 修复：声明为 _Atomic，所有访问通过 atomic_load_explicit/atomic_store_explicit。 */
static _Atomic(scheduler_core_ctx_t *) g_core_ctx = NULL;

static _Atomic(void *) g_ctx_init_lock = NULL;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 创建并初始化核心上下? * @return 新创建的上下文，失败返回NULL
 */
static scheduler_core_ctx_t *create_core_ctx(void)
{
    scheduler_core_ctx_t *ctx =
        (scheduler_core_ctx_t *)AGENTRT_CALLOC(1, sizeof(scheduler_core_ctx_t));
    if (!ctx) return NULL;

    /* 创建任务表互斥锁 */
    ctx->task_table_lock = agentrt_mutex_create();
    if (!ctx->task_table_lock) {
        AGENTRT_FREE(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    /* 初始化原子状态 */
    atomic_init(&ctx->initialized, 0);
    /* W09.4: atomic_uint64_t 应使用 atomic_init 初始化，而非直接赋值 */
    atomic_init(&ctx->next_task_id, (uint64_t)1);
    ctx->task_count = 0;

    /* 清零哈希?*/
    __builtin_memset(ctx->id_hash_table, 0, sizeof(ctx->id_hash_table));

    return ctx;
}

/**
 * @brief 销毁核心上下文
 * @param ctx 要销毁的上下? */
static void destroy_core_ctx(scheduler_core_ctx_t *ctx)
{
    if (!ctx)
        return;

    /* 销毁所有哈希表节点 */
    for (size_t i = 0; i < HASH_TABLE_BUCKETS; i++) {
        task_hash_node_t *node = ctx->id_hash_table[i];
        while (node) {
            task_hash_node_t *next = node->next;
            AGENTRT_FREE(node);
            node = next;
        }
    }

    /* 销毁任务表互斥?*/
    if (ctx->task_table_lock) {
        agentrt_mutex_free(ctx->task_table_lock);
    }

    /* 清理任务表（任务信息本身由适配器清理） */
    for (uint32_t i = 0; i < ctx->task_count; i++) {
        /* 任务信息结构由适配器负责清理，这里只置?*/
        ctx->task_table[i] = NULL;
    }

    AGENTRT_FREE(ctx);
}

/* ==================== 公共API实现 ==================== */

scheduler_core_ctx_t *scheduler_core_get_ctx(void)
{
    /* W09.4: _Atomic 指针需通过 atomic_load 读取 */
    return atomic_load_explicit(&g_core_ctx, memory_order_acquire);
}

int scheduler_core_init(void)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针，移除 (_Atomic void **) 强制转换 */
    if (atomic_load_explicit(&g_core_ctx, memory_order_acquire)) {
        return 0;
    }

    /* 延迟初始化 init 锁（CAS 保证仅创建一次） */
    if (!atomic_load_explicit(&g_ctx_init_lock, memory_order_acquire)) {
        void *new_lock = agentrt_mutex_create();
        if (!new_lock)
            ATM_RET_ERR(AGENTRT_EINVAL);

        void *expected = NULL;
        /* W09.4: 使用 C11 泛型宏 atomic_compare_exchange_strong_explicit，
         * 避免 atomic_compare_exchange_strong_ptr 的 _Atomic void** 类型不匹配警告。
         * g_ctx_init_lock 是 _Atomic(void *)，泛型宏可正确处理任意 _Atomic 类型。 */
        if (!atomic_compare_exchange_strong_explicit(&g_ctx_init_lock, &expected,
                                                     new_lock, memory_order_seq_cst,
                                                     memory_order_seq_cst)) {
            agentrt_mutex_free(new_lock);
        }
    }

    void *init_lock = atomic_load_explicit(&g_ctx_init_lock, memory_order_acquire);
    agentrt_mutex_lock(init_lock);

    if (atomic_load_explicit(&g_core_ctx, memory_order_acquire)) {
        agentrt_mutex_unlock(init_lock);
        return 0;
    }

    scheduler_core_ctx_t *ctx = create_core_ctx();
    if (!ctx) {
        agentrt_mutex_unlock(init_lock);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    /* W09.4: 使用 atomic_store_explicit 写入 _Atomic 指针 */
    atomic_store_explicit(&g_core_ctx, ctx, memory_order_release);
    atomic_store_explicit(&ctx->initialized, 1, memory_order_release);
    atomic_thread_fence(memory_order_release);

    agentrt_mutex_unlock(init_lock);
    return 0;
}

void scheduler_core_destroy(void)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx)
        return;

    void *init_lock = atomic_load_explicit(&g_ctx_init_lock, memory_order_acquire);
    agentrt_mutex_lock(init_lock);

    /* 在锁内重新读取，确保线程安全 */
    ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    destroy_core_ctx(ctx);
    atomic_store_explicit(&g_core_ctx, (scheduler_core_ctx_t *)NULL, memory_order_release);

    agentrt_mutex_unlock(init_lock);

    agentrt_mutex_free(init_lock);
    atomic_store_explicit(&g_ctx_init_lock, (void *)NULL, memory_order_release);
}

int scheduler_core_is_initialized(void)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    return ctx && (atomic_load_explicit(&ctx->initialized, memory_order_acquire) == 1);
}

uint64_t scheduler_core_fetch_add_task_id(void)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx)
        return 0;
    return atomic_fetch_add_explicit(&ctx->next_task_id, 1, memory_order_seq_cst);
}

void scheduler_core_hash_insert(agentrt_task_id_t id, task_info_core_t *info)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx || !info)
        return;

    size_t bucket = task_hash_core(id);
    task_hash_node_t *node = (task_hash_node_t *)AGENTRT_MALLOC(sizeof(task_hash_node_t));
    if (!node)
        return;

    node->id = id;
    node->task_info = info;
    node->next = ctx->id_hash_table[bucket];
    ctx->id_hash_table[bucket] = node;
}

task_info_core_t *scheduler_core_hash_find(agentrt_task_id_t id)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx) return NULL;

    size_t bucket = task_hash_core(id);
    task_hash_node_t *node = ctx->id_hash_table[bucket];

    while (node) {
        if (node->id == id) {
            return node->task_info;
        }
        node = node->next;
    }

    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}

void scheduler_core_hash_remove(agentrt_task_id_t id)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx)
        return;

    size_t bucket = task_hash_core(id);
    task_hash_node_t *node = ctx->id_hash_table[bucket];
    task_hash_node_t *prev = NULL;

    while (node) {
        if (node->id == id) {
            if (prev) {
                prev->next = node->next;
            } else {
                ctx->id_hash_table[bucket] = node->next;
            }
            AGENTRT_FREE(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

task_info_core_t *scheduler_core_task_info_create(agentrt_task_id_t id, void *(*entry)(void *),
                                                  void *arg, const char *name, int priority)
{

    task_info_core_t *info = (task_info_core_t *)AGENTRT_CALLOC(1, sizeof(task_info_core_t));
    if (!info) return NULL;

    info->id = id;
    info->entry = entry;
    info->arg = arg;
    info->priority = priority;
    info->state = AGENTRT_TASK_STATE_CREATED;

    /* 设置任务名称 */
    if (name) {
AGENTRT_STRNCPY_TERM(info->name, name, sizeof(info->name));
        (info->name)[sizeof(info->name) - 1] = '\0';
    } else {
        snprintf(info->name, sizeof(info->name), "task_%llu", (unsigned long long)id);
    }

    /* 平台特定数据初始化为NULL */
    info->platform_handle = NULL;
    info->platform_data = NULL;

    return info;
}

void scheduler_core_task_info_destroy(task_info_core_t *info)
{
    if (!info)
        return;

    /* 注意：platform_handle和platform_data由平台适配器清?*/
    AGENTRT_FREE(info);
}

int scheduler_core_task_table_add(task_info_core_t *info)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx || !info)
        ATM_RET_ERR(AGENTRT_EINVAL);

    if (ctx->task_count >= TASK_TABLE_CAPACITY) {
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    ctx->task_table[ctx->task_count++] = info;
    return 0;
}

task_info_core_t *scheduler_core_task_table_remove(agentrt_task_id_t id)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx) return NULL;

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        if (ctx->task_table[i] && ctx->task_table[i]->id == id) {
            task_info_core_t *removed = ctx->task_table[i];

            /* 移动最后一个元素到当前位置 */
            ctx->task_table[i] = ctx->task_table[ctx->task_count - 1];
            ctx->task_table[ctx->task_count - 1] = NULL;
            ctx->task_count--;

            return removed;
        }
    }

    return NULL;
}

task_info_core_t *scheduler_core_find_by_platform_handle(void *platform_handle)
{
    /* W09.4: 使用 atomic_load_explicit 读取 _Atomic 指针 */
    scheduler_core_ctx_t *ctx = atomic_load_explicit(&g_core_ctx, memory_order_acquire);
    if (!ctx || !platform_handle) return NULL;

    for (uint32_t i = 0; i < ctx->task_count; i++) {
        if (ctx->task_table[i] &&
            ctx->task_table[i]->platform_handle == platform_handle) {
            return ctx->task_table[i];
        }
    }

    AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
}
