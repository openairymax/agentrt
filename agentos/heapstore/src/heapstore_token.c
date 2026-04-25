/**
 * @file heapstore_token.c
 * @brief AgentOS heapstore Token 计数实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 实现 Token 使用统计和监控功能，
 *       符合 ARCHITECTURAL_PRINCIPLES.md 中的 E-2 可观测性原则。
 */

#include "heapstore_token.h"
#include "heapstore.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
#endif

/* ==================== 常量定义 ==================== */

#define MAX_TASK_ID_LEN 128
#define MAX_BUDGET_ENTRIES 1024

/* ==================== 内部状态 ==================== */

static int g_token_initialized = 0;

static atomic_uint_fast64_t g_total_prompt_tokens = 0;
static atomic_uint_fast64_t g_total_completion_tokens = 0;
static atomic_uint_fast64_t g_total_system_tokens = 0;
static atomic_uint_fast64_t g_total_user_tokens = 0;
static atomic_uint_fast64_t g_tokens_saved_by_cache = 0;
static atomic_uint_fast64_t g_total_write_ops = 0;
static atomic_uint_fast64_t g_total_read_ops = 0;
static atomic_uint_fast64_t g_total_batch_ops = 0;
static atomic_uint_fast64_t g_last_operation_time = 0;

#ifdef _WIN32
static CRITICAL_SECTION g_token_mutex;
#else
static pthread_mutex_t g_token_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* ==================== 内部数据结构和函数 ==================== */

typedef struct {
    char task_id[MAX_TASK_ID_LEN];
    heapstore_token_budget_t budget;
    atomic_uint_fast64_t used_tokens;
    int active;
} task_budget_entry_t;

static task_budget_entry_t g_budget_table[MAX_BUDGET_ENTRIES];
static int g_budget_count = 0;

#ifdef _WIN32
static void token_mutex_init(void) {
    InitializeCriticalSection(&g_token_mutex);
}

static void token_mutex_destroy(void) {
    DeleteCriticalSection(&g_token_mutex);
}

static void token_mutex_lock(void) {
    EnterCriticalSection(&g_token_mutex);
}

static void token_mutex_unlock(void) {
    LeaveCriticalSection(&g_token_mutex);
}
#else
static void token_mutex_init(void) {
    pthread_mutex_init(&g_token_mutex, NULL);
}

static void token_mutex_destroy(void) {
    pthread_mutex_destroy(&g_token_mutex);
}

static void token_mutex_lock(void) {
    pthread_mutex_lock(&g_token_mutex);
}

static void token_mutex_unlock(void) {
    pthread_mutex_unlock(&g_token_mutex);
}
#endif

static int find_budget_entry(const char* task_id) {
    for (int i = 0; i < MAX_BUDGET_ENTRIES; i++) {
        if (g_budget_table[i].active &&
            strncmp(g_budget_table[i].task_id, task_id, MAX_TASK_ID_LEN - 1) == 0) {
            return i;
        }
    }
    return -1;
}

static int allocate_budget_entry(void) {
    for (int i = 0; i < MAX_BUDGET_ENTRIES; i++) {
        if (!g_budget_table[i].active) {
            return i;
        }
    }
    return -1;
}

/* ==================== 公共 API 实现 ==================== */

heapstore_error_t heapstore_token_init(void) {
    if (g_token_initialized) {
        return heapstore_SUCCESS;
    }

    token_mutex_init();
    memset(g_budget_table, 0, sizeof(g_budget_table));
    g_budget_count = 0;

    atomic_init(&g_total_prompt_tokens, 0);
    atomic_init(&g_total_completion_tokens, 0);
    atomic_init(&g_total_system_tokens, 0);
    atomic_init(&g_total_user_tokens, 0);
    atomic_init(&g_tokens_saved_by_cache, 0);
    atomic_init(&g_total_write_ops, 0);
    atomic_init(&g_total_read_ops, 0);
    atomic_init(&g_total_batch_ops, 0);
    atomic_init(&g_last_operation_time, 0);

    g_token_initialized = 1;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_shutdown(void) {
    if (!g_token_initialized) {
        return heapstore_SUCCESS;
    }

    token_mutex_lock();
    memset(g_budget_table, 0, sizeof(g_budget_table));
    g_budget_count = 0;
    token_mutex_unlock();

    token_mutex_destroy();
    g_token_initialized = 0;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_record(
    heapstore_token_type_t type,
    uint64_t count,
    heapstore_token_operation_t operation
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (count == 0) {
        return heapstore_SUCCESS;
    }

    switch (type) {
        case HEAPSTORE_TOKEN_TYPE_PROMPT:
            atomic_fetch_add(&g_total_prompt_tokens, count);
            break;
        case HEAPSTORE_TOKEN_TYPE_COMPLETION:
            atomic_fetch_add(&g_total_completion_tokens, count);
            break;
        case HEAPSTORE_TOKEN_TYPE_SYSTEM:
            atomic_fetch_add(&g_total_system_tokens, count);
            break;
        case HEAPSTORE_TOKEN_TYPE_USER:
            atomic_fetch_add(&g_total_user_tokens, count);
            break;
        case HEAPSTORE_TOKEN_TYPE_CACHE_HIT:
            atomic_fetch_add(&g_tokens_saved_by_cache, count);
            break;
        default:
            break;
    }

    switch (operation) {
        case HEAPSTORE_TOKEN_OP_WRITE:
            atomic_fetch_add(&g_total_write_ops, 1);
            break;
        case HEAPSTORE_TOKEN_OP_READ:
            atomic_fetch_add(&g_total_read_ops, 1);
            break;
        case HEAPSTORE_TOKEN_OP_BATCH:
            atomic_fetch_add(&g_total_batch_ops, 1);
            break;
        default:
            break;
    }

    atomic_store(&g_last_operation_time, (uint64_t)time(NULL));
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_get_stats(heapstore_token_stats_t* out_stats) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!out_stats) {
        return heapstore_ERR_INVALID_PARAM;
    }

    uint64_t total_ops = atomic_load(&g_total_write_ops) +
        atomic_load(&g_total_read_ops) +
        atomic_load(&g_total_batch_ops);

    out_stats->total_prompt_tokens = atomic_load(&g_total_prompt_tokens);
    out_stats->total_completion_tokens = atomic_load(&g_total_completion_tokens);
    out_stats->total_system_tokens = atomic_load(&g_total_system_tokens);
    out_stats->total_user_tokens = atomic_load(&g_total_user_tokens);
    out_stats->tokens_saved_by_cache = atomic_load(&g_tokens_saved_by_cache);
    out_stats->total_write_operations = atomic_load(&g_total_write_ops);
    out_stats->total_read_operations = atomic_load(&g_total_read_ops);
    out_stats->total_batch_operations = atomic_load(&g_total_batch_ops);
    out_stats->last_operation_time = atomic_load(&g_last_operation_time);

    uint64_t total_tokens = out_stats->total_prompt_tokens +
        out_stats->total_completion_tokens +
        out_stats->total_system_tokens +
        out_stats->total_user_tokens;

    if (total_ops > 0) {
        out_stats->average_tokens_per_operation = (double)total_tokens / (double)total_ops;
    } else {
        out_stats->average_tokens_per_operation = 0.0;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_reset_stats(void) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    token_mutex_lock();
    atomic_store(&g_total_prompt_tokens, 0);
    atomic_store(&g_total_completion_tokens, 0);
    atomic_store(&g_total_system_tokens, 0);
    atomic_store(&g_total_user_tokens, 0);
    atomic_store(&g_tokens_saved_by_cache, 0);
    atomic_store(&g_total_write_ops, 0);
    atomic_store(&g_total_read_ops, 0);
    atomic_store(&g_total_batch_ops, 0);
    atomic_store(&g_last_operation_time, 0);
    token_mutex_unlock();

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_set_budget(
    const char* task_id,
    const heapstore_token_budget_t* budget
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!task_id || !budget) {
        return heapstore_ERR_INVALID_PARAM;
    }

    token_mutex_lock();

    int entry_idx = find_budget_entry(task_id);
    if (entry_idx < 0) {
        entry_idx = allocate_budget_entry();
        if (entry_idx < 0) {
            token_mutex_unlock();
            return heapstore_ERR_OUT_OF_MEMORY;
        }
        g_budget_count++;
    }

    task_budget_entry_t* entry = &g_budget_table[entry_idx];
    strncpy(entry->task_id, task_id, MAX_TASK_ID_LEN - 1);
    entry->task_id[MAX_TASK_ID_LEN - 1] = '\0';
    entry->budget = *budget;
    atomic_store(&entry->used_tokens, 0);
    entry->active = 1;

    token_mutex_unlock();
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_check_budget(
    const char* task_id,
    uint64_t requested_tokens,
    bool* allowed
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!task_id || !allowed) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *allowed = true;

    token_mutex_lock();

    int entry_idx = find_budget_entry(task_id);
    if (entry_idx >= 0) {
        task_budget_entry_t* entry = &g_budget_table[entry_idx];
        if (entry->budget.enable_budget_enforcement) {
            uint64_t used = atomic_load(&entry->used_tokens);
            uint64_t max_tokens = entry->budget.max_tokens_per_task;

            if (used + requested_tokens > max_tokens) {
                *allowed = false;
            }

            uint64_t used_percent = max_tokens > 0 ? (used * 100) / max_tokens : 0;
            if (used_percent >= entry->budget.critical_threshold_percent) {
                *allowed = false;
            } else if (used_percent >= entry->budget.warning_threshold_percent) {
                *allowed = true;
            }
        }
    }

    token_mutex_unlock();
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_try_consume(
    const char* task_id,
    uint64_t requested_tokens,
    bool* consumed
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!task_id || !consumed) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *consumed = true;

    token_mutex_lock();

    int entry_idx = find_budget_entry(task_id);
    if (entry_idx >= 0) {
        task_budget_entry_t* entry = &g_budget_table[entry_idx];
        if (entry->budget.enable_budget_enforcement) {
            uint64_t used = atomic_load(&entry->used_tokens);
            uint64_t max_tokens = entry->budget.max_tokens_per_task;

            if (used + requested_tokens > max_tokens) {
                *consumed = false;
            } else {
                atomic_store(&entry->used_tokens, used + requested_tokens);
                *consumed = true;
            }
        }
    }

    token_mutex_unlock();
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_refund(
    const char* task_id,
    uint64_t refund_tokens
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!task_id || refund_tokens == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    token_mutex_lock();

    int entry_idx = find_budget_entry(task_id);
    if (entry_idx >= 0) {
        task_budget_entry_t* entry = &g_budget_table[entry_idx];
        uint64_t used = atomic_load(&entry->used_tokens);
        if (refund_tokens > used) {
            atomic_store(&entry->used_tokens, 0);
        } else {
            atomic_store(&entry->used_tokens, used - refund_tokens);
        }
    }

    token_mutex_unlock();
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_token_get_task_usage(
    const char* task_id,
    uint64_t* out_used
) {
    if (!g_token_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!task_id || !out_used) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *out_used = 0;

    token_mutex_lock();
    int entry_idx = find_budget_entry(task_id);
    if (entry_idx >= 0) {
        *out_used = atomic_load(&g_budget_table[entry_idx].used_tokens);
    }
    token_mutex_unlock();

    return heapstore_SUCCESS;
}

const char* heapstore_token_type_to_string(heapstore_token_type_t type) {
    static const char* type_strings[] = {
        [HEAPSTORE_TOKEN_TYPE_PROMPT] = "prompt",
        [HEAPSTORE_TOKEN_TYPE_COMPLETION] = "completion",
        [HEAPSTORE_TOKEN_TYPE_SYSTEM] = "system",
        [HEAPSTORE_TOKEN_TYPE_USER] = "user",
        [HEAPSTORE_TOKEN_TYPE_CACHE_HIT] = "cache_hit",
        [HEAPSTORE_TOKEN_TYPE_TOTAL] = "total"
    };

    if (type >= 0 && type <= HEAPSTORE_TOKEN_TYPE_TOTAL) {
        return type_strings[type];
    }
    return "unknown";
}

const char* heapstore_token_op_to_string(heapstore_token_operation_t operation) {
    static const char* op_strings[] = {
        [HEAPSTORE_TOKEN_OP_WRITE] = "write",
        [HEAPSTORE_TOKEN_OP_READ] = "read",
        [HEAPSTORE_TOKEN_OP_BATCH] = "batch"
    };

    if (operation >= 0 && operation <= HEAPSTORE_TOKEN_OP_BATCH) {
        return op_strings[operation];
    }
    return "unknown";
}
