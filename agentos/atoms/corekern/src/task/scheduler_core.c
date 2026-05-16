/**
 * @file scheduler_core.c
 * @brief 调度器核心层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "scheduler_core.h"
#include "mem.h"
#include <stdlib.h>
#include <stdio.h>

/* Unified base library compatibility layer */
#include "atomic_compat.h"
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

/* ==================== 静态全局状态 ==================== */

static scheduler_core_ctx_t* g_core_ctx = NULL;

static void* g_ctx_init_lock = NULL;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 创建并初始化核心上下�? * @return 新创建的上下文，失败返回NULL
 */
static scheduler_core_ctx_t* create_core_ctx(void) {
    scheduler_core_ctx_t* ctx = (scheduler_core_ctx_t*)AGENTOS_CALLOC(1, sizeof(scheduler_core_ctx_t));
    if (!ctx) return NULL;

    /* 创建任务表互斥锁 */
    ctx->task_table_lock = agentos_mutex_create();
    if (!ctx->task_table_lock) {
        AGENTOS_FREE(ctx);
        return NULL;
    }

    /* 初始化原子状态 */
    atomic_init(&ctx->initialized, 0);
    ctx->next_task_id = 1;
    ctx->task_count = 0;

    /* 清零哈希�?*/
    memset(ctx->id_hash_table, 0, sizeof(ctx->id_hash_table));

    return ctx;
}

/**
 * @brief 销毁核心上下文
 * @param ctx 要销毁的上下�? */
static void destroy_core_ctx(scheduler_core_ctx_t* ctx) {
    if (!ctx) return;

    /* 销毁所有哈希表节点 */
    for (size_t i = 0; i < HASH_TABLE_BUCKETS; i++) {
        task_hash_node_t* node = ctx->id_hash_table[i];
        while (node) {
            task_hash_node_t* next = node->next;
            AGENTOS_FREE(node);
            node = next;
        }
    }

    /* 销毁任务表互斥�?*/
    if (ctx->task_table_lock) {
        agentos_mutex_free(ctx->task_table_lock);
    }

    /* 清理任务表（任务信息本身由适配器清理） */
    for (uint32_t i = 0; i < ctx->task_count; i++) {
        /* 任务信息结构由适配器负责清理，这里只置�?*/
        ctx->task_table[i] = NULL;
    }

    AGENTOS_FREE(ctx);
}

/* ==================== 公共API实现 ==================== */

scheduler_core_ctx_t* scheduler_core_get_ctx(void) {
    return g_core_ctx;
}

int scheduler_core_init(void) {
    fprintf(stderr, "[DBG] scheduler_core_init: entry, g_core_ctx=%p\n", (void*)g_core_ctx); fflush(stderr);
    if (atomic_load_ptr((void* volatile*)&g_core_ctx, memory_order_acquire)) {
        fprintf(stderr, "[DBG] scheduler_core_init: already initialized, returning 0\n"); fflush(stderr);
        return 0;
    }

    fprintf(stderr, "[DBG] scheduler_core_init: checking g_ctx_init_lock=%p\n", (void*)g_ctx_init_lock); fflush(stderr);
    if (!g_ctx_init_lock) {
        fprintf(stderr, "[DBG] scheduler_core_init: creating mutex\n"); fflush(stderr);
        void* new_lock = agentos_mutex_create();
        fprintf(stderr, "[DBG] scheduler_core_init: mutex created=%p\n", new_lock); fflush(stderr);
        if (!new_lock) return -1;

        void* expected = NULL;
        if (!atomic_compare_exchange_strong_ptr(
                (void* volatile*)&g_ctx_init_lock, &expected, new_lock,
                memory_order_seq_cst, memory_order_seq_cst)) {
            fprintf(stderr, "[DBG] scheduler_core_init: CAS failed, freeing mutex\n"); fflush(stderr);
            agentos_mutex_free(new_lock);
        }
    }

    fprintf(stderr, "[DBG] scheduler_core_init: locking g_ctx_init_lock\n"); fflush(stderr);
    agentos_mutex_lock(g_ctx_init_lock);
    fprintf(stderr, "[DBG] scheduler_core_init: locked\n"); fflush(stderr);

    if (atomic_load_ptr((void* volatile*)&g_core_ctx, memory_order_acquire)) {
        fprintf(stderr, "[DBG] scheduler_core_init: already initialized (under lock), returning 0\n"); fflush(stderr);
        agentos_mutex_unlock(g_ctx_init_lock);
        return 0;
    }

    fprintf(stderr, "[DBG] scheduler_core_init: creating core ctx\n"); fflush(stderr);
    g_core_ctx = create_core_ctx();
    fprintf(stderr, "[DBG] scheduler_core_init: core ctx=%p\n", (void*)g_core_ctx); fflush(stderr);
    if (!g_core_ctx) {
        agentos_mutex_unlock(g_ctx_init_lock);
        return -1;
    }

    fprintf(stderr, "[DBG] scheduler_core_init: before atomic_store\n"); fflush(stderr);
    atomic_store_explicit(&g_core_ctx->initialized, 1, memory_order_release);
    fprintf(stderr, "[DBG] scheduler_core_init: after atomic_store\n"); fflush(stderr);
    /* atomic_thread_fence(memory_order_release); -- temporarily disabled for debugging */
    write(2, "[DBG] fence skipped\n", 20);

    fprintf(stderr, "[DBG] scheduler_core_init: unlocking\n"); fflush(stderr);
    agentos_mutex_unlock(g_ctx_init_lock);
    fprintf(stderr, "[DBG] scheduler_core_init: done, returning 0\n"); fflush(stderr);
    return 0;
}

void scheduler_core_destroy(void) {
    if (!g_core_ctx) return;

    agentos_mutex_lock(g_ctx_init_lock);

    destroy_core_ctx(g_core_ctx);
    g_core_ctx = NULL;

    agentos_mutex_unlock(g_ctx_init_lock);

    agentos_mutex_free(g_ctx_init_lock);
    g_ctx_init_lock = NULL;
}

int scheduler_core_is_initialized(void) {
    return g_core_ctx && (atomic_load_explicit(&g_core_ctx->initialized, memory_order_acquire) == 1);
}

uint64_t scheduler_core_fetch_add_task_id(void) {
    if (!g_core_ctx) return 0;
    return atomic_fetch_add_explicit(&g_core_ctx->next_task_id, 1, memory_order_seq_cst);
}

void scheduler_core_hash_insert(agentos_task_id_t id, task_info_core_t* info) {
    if (!g_core_ctx || !info) return;

    size_t bucket = task_hash_core(id);
    task_hash_node_t* node = (task_hash_node_t*)AGENTOS_MALLOC(sizeof(task_hash_node_t));
    if (!node) return;

    node->id = id;
    node->task_info = info;
    node->next = g_core_ctx->id_hash_table[bucket];
    g_core_ctx->id_hash_table[bucket] = node;
}

task_info_core_t* scheduler_core_hash_find(agentos_task_id_t id) {
    if (!g_core_ctx) return NULL;

    size_t bucket = task_hash_core(id);
    task_hash_node_t* node = g_core_ctx->id_hash_table[bucket];

    while (node) {
        if (node->id == id) {
            return node->task_info;
        }
        node = node->next;
    }

    return NULL;
}

void scheduler_core_hash_remove(agentos_task_id_t id) {
    if (!g_core_ctx) return;

    size_t bucket = task_hash_core(id);
    task_hash_node_t* node = g_core_ctx->id_hash_table[bucket];
    task_hash_node_t* prev = NULL;

    while (node) {
        if (node->id == id) {
            if (prev) {
                prev->next = node->next;
            } else {
                g_core_ctx->id_hash_table[bucket] = node->next;
            }
            AGENTOS_FREE(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

task_info_core_t* scheduler_core_task_info_create(
    agentos_task_id_t id,
    void* (*entry)(void*),
    void* arg,
    const char* name,
    int priority) {

    task_info_core_t* info = (task_info_core_t*)AGENTOS_CALLOC(1, sizeof(task_info_core_t));
    if (!info) return NULL;

    info->id = id;
    info->entry = entry;
    info->arg = arg;
    info->priority = priority;
    info->state = AGENTOS_TASK_STATE_CREATED;

    /* 设置任务名称 */
    if (name) {
        strncpy(info->name, name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
    } else {
        snprintf(info->name, sizeof(info->name), "task_%llu", (unsigned long long)id);
    }

    /* 平台特定数据初始化为NULL */
    info->platform_handle = NULL;
    info->platform_data = NULL;

    return info;
}

void scheduler_core_task_info_destroy(task_info_core_t* info) {
    if (!info) return;

    /* 注意：platform_handle和platform_data由平台适配器清�?*/
    AGENTOS_FREE(info);
}

int scheduler_core_task_table_add(task_info_core_t* info) {
    if (!g_core_ctx || !info) return -1;

    if (g_core_ctx->task_count >= TASK_TABLE_CAPACITY) {
        return -1;  /* 表已�?*/
    }

    g_core_ctx->task_table[g_core_ctx->task_count++] = info;
    return 0;
}

task_info_core_t* scheduler_core_task_table_remove(agentos_task_id_t id) {
    if (!g_core_ctx) return NULL;

    for (uint32_t i = 0; i < g_core_ctx->task_count; i++) {
        if (g_core_ctx->task_table[i] && g_core_ctx->task_table[i]->id == id) {
            task_info_core_t* removed = g_core_ctx->task_table[i];

            /* 移动最后一个元素到当前位置 */
            g_core_ctx->task_table[i] = g_core_ctx->task_table[g_core_ctx->task_count - 1];
            g_core_ctx->task_table[g_core_ctx->task_count - 1] = NULL;
            g_core_ctx->task_count--;

            return removed;
        }
    }

    return NULL;
}

task_info_core_t* scheduler_core_find_by_platform_handle(void* platform_handle) {
    if (!g_core_ctx || !platform_handle) return NULL;

    for (uint32_t i = 0; i < g_core_ctx->task_count; i++) {
        if (g_core_ctx->task_table[i] && g_core_ctx->task_table[i]->platform_handle == platform_handle) {
            return g_core_ctx->task_table[i];
        }
    }

    return NULL;
}
