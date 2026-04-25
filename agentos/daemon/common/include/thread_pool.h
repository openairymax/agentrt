/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file thread_pool.h
 * @brief Daemon通用工作线程池
 *
 * 提供固定大小的工作线程池，支持任务提交、优雅关闭。
 * 所有daemon服务(gateway_d/llm_d/tool_d等)共享此基础设施。
 */

#ifndef AGENTOS_DAEMON_THREAD_POOL_H
#define AGENTOS_DAEMON_THREAD_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 类型定义 ========== */

typedef struct thread_pool_s thread_pool_t;

typedef void (*thread_task_fn_t)(void* arg);

/* ========== 配置 ========== */

typedef struct {
    uint32_t min_threads;
    uint32_t max_threads;
    uint32_t queue_size;
    uint32_t idle_timeout_ms;
} thread_pool_config_t;

/* ========== 生命周期 ========== */

thread_pool_t* thread_pool_create(const thread_pool_config_t* config);

void thread_pool_destroy(thread_pool_t* pool);

int thread_pool_submit(thread_pool_t* pool,
                       thread_task_fn_t task,
                       void* arg);

/* ========== 查询 ========== */

uint32_t thread_pool_active_count(thread_pool_t* pool);

uint32_t thread_pool_pending_count(thread_pool_t* pool);

bool thread_pool_is_running(thread_pool_t* pool);

/* ========== 默认配置 ========== */

static inline void thread_pool_get_default_config(thread_pool_config_t* cfg) {
    cfg->min_threads = 2;
    cfg->max_threads = 8;
    cfg->queue_size = 256;
    cfg->idle_timeout_ms = 30000;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_THREAD_POOL_H */
