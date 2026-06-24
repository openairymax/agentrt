/**
 * @file hook_timeout.c
 * @brief P2.1.4: Hook 执行超时保护实现
 *
 * 使用 pthread 线程 + pthread_timedjoin_np 实现超时控制。
 * 回调在独立线程中执行，超时后通过 pthread_cancel 终止。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hook_timeout.h"
#include "hook_registry.h"
#include "memory_compat.h"
#include "sync_compat.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

/* ==================== 超时管理器内部结构 ==================== */

#define HOOK_TIMEOUT_MAX_ENTRIES  128  /**< 最大超时条目数 */

/**
 * @brief 超时回调执行参数
 */
typedef struct {
    const hook_entry_t *entry;     /**< Hook 条目 */
    hook_context_t *ctx;           /**< Hook 上下文 */
    hook_decision_t result;        /**< 执行结果 */
    bool finished;                 /**< 是否完成 */
    agentos_mutex_t mutex;         /**< 结果保护锁 */
    agentos_cond_t cond;           /**< 完成信号 */
} timeout_exec_arg_t;

/**
 * @brief 全局超时管理器
 */
static struct {
    hook_timeout_entry_t entries[HOOK_TIMEOUT_MAX_ENTRIES];
    size_t count;
    uint32_t default_timeout_ms;
    agentos_mutex_t mutex;
    bool initialized;
} g_timeout_mgr;

/* ==================== 生命周期 ==================== */

int hook_timeout_manager_init(uint32_t default_timeout_ms)
{
    if (g_timeout_mgr.initialized)
        return 0;

    AGENTOS_MEMSET(&g_timeout_mgr, 0, sizeof(g_timeout_mgr));

    g_timeout_mgr.default_timeout_ms = (default_timeout_ms > 0)
        ? default_timeout_ms : HOOK_TIMEOUT_DEFAULT_MS;

    if (AGENTOS_MUTEX_INIT(&g_timeout_mgr.mutex, NULL) != 0) {
        return -1;
    }

    g_timeout_mgr.initialized = true;
    return 0;
}

void hook_timeout_manager_destroy(void)
{
    if (!g_timeout_mgr.initialized)
        return;

    AGENTOS_MUTEX_DESTROY(&g_timeout_mgr.mutex);
    AGENTOS_MEMSET(&g_timeout_mgr, 0, sizeof(g_timeout_mgr));
    g_timeout_mgr.initialized = false;
}

/* ==================== 超时配置 ==================== */

int hook_timeout_set(const char *hook_name, uint32_t timeout_ms)
{
    if (!hook_name || !g_timeout_mgr.initialized)
        return -1;

    if (timeout_ms > 0 && timeout_ms < HOOK_TIMEOUT_MIN_MS)
        timeout_ms = HOOK_TIMEOUT_MIN_MS;
    if (timeout_ms > HOOK_TIMEOUT_MAX_MS)
        timeout_ms = HOOK_TIMEOUT_MAX_MS;

    AGENTOS_MUTEX_LOCK(&g_timeout_mgr.mutex);

    /* 查找现有条目 */
    for (size_t i = 0; i < g_timeout_mgr.count; i++) {
        if (strcmp(g_timeout_mgr.entries[i].hook_name, hook_name) == 0) {
            g_timeout_mgr.entries[i].timeout_ms = timeout_ms;
            g_timeout_mgr.entries[i].enabled = true;
            AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
            return 0;
        }
    }

    /* 添加新条目 */
    if (g_timeout_mgr.count >= HOOK_TIMEOUT_MAX_ENTRIES) {
        AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
        return -1;
    }

    hook_timeout_entry_t *entry = &g_timeout_mgr.entries[g_timeout_mgr.count];
    AGENTOS_STRNCPY_TERM(entry->hook_name, hook_name, sizeof(entry->hook_name));
    entry->timeout_ms = timeout_ms;
    entry->timeout_count = 0;
    entry->enabled = true;
    g_timeout_mgr.count++;

    AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
    return 0;
}

uint32_t hook_timeout_get(const char *hook_name)
{
    if (!hook_name || !g_timeout_mgr.initialized)
        return g_timeout_mgr.default_timeout_ms;

    AGENTOS_MUTEX_LOCK(&g_timeout_mgr.mutex);

    for (size_t i = 0; i < g_timeout_mgr.count; i++) {
        if (strcmp(g_timeout_mgr.entries[i].hook_name, hook_name) == 0) {
            uint32_t timeout = g_timeout_mgr.entries[i].timeout_ms;
            AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
            return timeout > 0 ? timeout : g_timeout_mgr.default_timeout_ms;
        }
    }

    AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
    return g_timeout_mgr.default_timeout_ms;
}

int hook_timeout_get_count(const char *hook_name)
{
    if (!hook_name || !g_timeout_mgr.initialized)
        return -1;

    AGENTOS_MUTEX_LOCK(&g_timeout_mgr.mutex);

    for (size_t i = 0; i < g_timeout_mgr.count; i++) {
        if (strcmp(g_timeout_mgr.entries[i].hook_name, hook_name) == 0) {
            int count = (int)g_timeout_mgr.entries[i].timeout_count;
            AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
            return count;
        }
    }

    AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
    return -1;
}

int hook_timeout_reset_count(const char *hook_name)
{
    if (!hook_name || !g_timeout_mgr.initialized)
        return -1;

    AGENTOS_MUTEX_LOCK(&g_timeout_mgr.mutex);

    for (size_t i = 0; i < g_timeout_mgr.count; i++) {
        if (strcmp(g_timeout_mgr.entries[i].hook_name, hook_name) == 0) {
            g_timeout_mgr.entries[i].timeout_count = 0;
            AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
            return 0;
        }
    }

    AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
    return -1;
}

/* ==================== 超时回调线程 ==================== */

static void *timeout_thread_func(void *arg)
{
    timeout_exec_arg_t *exec = (timeout_exec_arg_t *)arg;
    if (!exec || !exec->entry || !exec->ctx) {
        if (exec) {
            exec->finished = true;
            AGENTOS_COND_SIGNAL(&exec->cond);
        }
        return NULL;
    }

    /* 执行 C 回调 */
    if (exec->entry->callback) {
        exec->result = exec->entry->callback(exec->ctx);
    } else {
        exec->result = HOOK_DECISION_CONTINUE;
    }

    /* 通知调用者 */
    AGENTOS_MUTEX_LOCK(&exec->mutex);
    exec->finished = true;
    AGENTOS_COND_SIGNAL(&exec->cond);
    AGENTOS_MUTEX_UNLOCK(&exec->mutex);

    return NULL;
}

/* ==================== 带超时的执行 ==================== */

hook_decision_t hook_timeout_run(const hook_entry_t *entry,
                                  hook_context_t *ctx,
                                  uint32_t timeout_ms,
                                  uint64_t *out_duration_ns)
{
    if (!entry || !ctx) {
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    /* 如果 Hook 是 CALLBACK 类型，直接调用（无超时开销） */
    if (entry->impl_type != HOOK_IMPL_CALLBACK || !entry->callback) {
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    /* 确定超时时间 */
    uint32_t effective_timeout = (timeout_ms > 0)
        ? timeout_ms : hook_timeout_get(entry->name);

    /* 初始化执行参数 */
    timeout_exec_arg_t exec;
    AGENTOS_MEMSET(&exec, 0, sizeof(exec));
    exec.entry = entry;
    exec.ctx = ctx;
    exec.result = HOOK_DECISION_CONTINUE;
    exec.finished = false;

    if (AGENTOS_MUTEX_INIT(&exec.mutex, NULL) != 0 ||
        AGENTOS_COND_INIT(&exec.cond, NULL) != 0) {
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    /* 创建线程执行回调 */
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    pthread_t thread;
    int ret = pthread_create(&thread, NULL, timeout_thread_func, &exec);
    if (ret != 0) {
        AGENTOS_MUTEX_DESTROY(&exec.mutex);
        AGENTOS_COND_DESTROY(&exec.cond);
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    /* 等待线程完成或超时 */
    AGENTOS_MUTEX_LOCK(&exec.mutex);

    bool timed_out = false;
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_sec += effective_timeout / 1000;
    abs_time.tv_nsec += (effective_timeout % 1000) * 1000000;
    if (abs_time.tv_nsec >= 1000000000) {
        abs_time.tv_sec += 1;
        abs_time.tv_nsec -= 1000000000;
    }

    while (!exec.finished && !timed_out) {
        int wait_ret = AGENTOS_COND_TIMEDWAIT(&exec.cond, &exec.mutex,
                                               (int)effective_timeout);
        if (wait_ret != 0) {
            /* 超时 */
            timed_out = true;
            break;
        }
    }

    hook_decision_t result;
    if (timed_out) {
        /* 超时：取消线程并增加超时计数器 */
        AGENTOS_MUTEX_UNLOCK(&exec.mutex);
        pthread_cancel(thread);
        pthread_join(thread, NULL);

        result = HOOK_DECISION_ABORT;

        /* 更新超时计数 */
        if (g_timeout_mgr.initialized) {
            AGENTOS_MUTEX_LOCK(&g_timeout_mgr.mutex);
            for (size_t i = 0; i < g_timeout_mgr.count; i++) {
                if (strcmp(g_timeout_mgr.entries[i].hook_name, entry->name) == 0) {
                    g_timeout_mgr.entries[i].timeout_count++;
                    break;
                }
            }
            AGENTOS_MUTEX_UNLOCK(&g_timeout_mgr.mutex);
        }
    } else {
        result = exec.result;
        AGENTOS_MUTEX_UNLOCK(&exec.mutex);
        pthread_join(thread, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    if (out_duration_ns) {
        *out_duration_ns = (uint64_t)(end_time.tv_sec - start_time.tv_sec) * 1000000000ULL
                         + (uint64_t)(end_time.tv_nsec - start_time.tv_nsec);
    }

    AGENTOS_MUTEX_DESTROY(&exec.mutex);
    AGENTOS_COND_DESTROY(&exec.cond);

    return result;
}