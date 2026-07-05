/**
 * @file hook_metrics_handler.c
 * @brief P0.20.1: Metrics Hook 处理器 — 全事件类型原子计数与 JSON 导出
 *
 * 注册到全部 8 种事件类型，使用 C11 _Atomic 原子计数器按类型分类累计，
 * 提供 agentrt_hook_metrics_dump() JSON 导出 API，为 Prometheus 导出做准备。
 *
 * 设计要点：
 *   - 原子操作：C11 <stdatomic.h> _Atomic uint64_t，无锁，线程安全
 *   - 轻量级：回调仅执行一次原子递增（< 100ns），远低于 10ms 契约上限
 *   - 不干预流程：始终返回 CONTINUE
 *   - JSON 导出：紧凑格式，适合 Prometheus text exposition parser 消费
 *
 * JSON 导出格式：
 * @code
 * {"hook_metrics":{
 *   "PRE_EXEC":123,"POST_EXEC":120,"PRE_LLM":80,"POST_LLM":80,
 *   "PRE_TOOL":43,"POST_TOOL":43,"ON_ERROR":3,"ON_MEMORY_EVOLVE":15
 * }}
 * @endcode
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_builtin_handlers.h"
#include "hook_registry.h"
#include "svc_logger.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* ==================== 原子计数器 ==================== */

/* 按 hook_type_t 索引的原子计数器数组（HOOK_TYPE_COUNT=8）。
 * 使用 C11 _Atomic 保证线程安全，无需锁。
 * 初始化为零（静态存储期），符合 metrics 语义。 */
static _Atomic uint64_t g_metrics_counts[HOOK_TYPE_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

/* 事件类型名称（用于 JSON 导出，与 hook_service.h enum 顺序一致） */
static const char *const g_type_names[HOOK_TYPE_COUNT] = {
    "PRE_EXEC",       /* 0 */
    "POST_EXEC",      /* 1 */
    "PRE_LLM",        /* 2 */
    "POST_LLM",       /* 3 */
    "PRE_TOOL",       /* 4 */
    "POST_TOOL",      /* 5 */
    "ON_ERROR",       /* 6 */
    "ON_MEMORY_EVOLVE" /* 7 */
};

/* ==================== Metrics 回调 ==================== */

/**
 * @brief 通用 metrics 回调 — 按事件类型原子递增计数器
 *
 * 注册到所有 8 种事件类型，通过 ctx->type 区分。回调仅执行一次原子递增
 * （约 5-20ns on x86_64），远低于 10ms 契约上限。
 */
static hook_decision_t hook_metrics_callback(hook_context_t *ctx)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;
    if (ctx->type >= 0 && ctx->type < HOOK_TYPE_COUNT) {
        atomic_fetch_add_explicit(&g_metrics_counts[ctx->type], 1,
                                  memory_order_relaxed);
    }
    return HOOK_DECISION_CONTINUE;
}

/* ==================== 导出 API ==================== */

uint64_t agentrt_hook_metrics_get_count(hook_type_t type)
{
    if (type < 0 || type >= HOOK_TYPE_COUNT) return 0;
    return atomic_load_explicit(&g_metrics_counts[type], memory_order_relaxed);
}

int agentrt_hook_metrics_dump(char *buf, size_t bufsize)
{
    if (!buf || bufsize == 0) return -1;

    /* 最坏情况：8 × ("PRE_MEMORY_EVOLVE":18446744073709551615,) ≈ 320 字节 + 头尾 */
    static const char header[] = "{\"hook_metrics\":{";
    static const char footer[] = "}}";
    const size_t header_len = sizeof(header) - 1;
    const size_t footer_len = sizeof(footer) - 1;

    if (bufsize < header_len + footer_len + 32) return -1;

    size_t pos = 0;
    __builtin_memcpy(buf + pos, header, header_len);
    pos += header_len;

    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        uint64_t count = atomic_load_explicit(&g_metrics_counts[i],
                                               memory_order_relaxed);
        /* 每个条目格式："NAME":COUNT */
        int n = snprintf(buf + pos, bufsize - pos - footer_len,
                         "%s\"%s\":%llu",
                         (i > 0) ? "," : "",
                         g_type_names[i],
                         (unsigned long long)count);
        if (n < 0 || (size_t)n >= bufsize - pos - footer_len) {
            return -1; /* 缓冲区不足 */
        }
        pos += (size_t)n;
    }

    __builtin_memcpy(buf + pos, footer, footer_len);
    pos += footer_len;
    buf[pos] = '\0';
    return (int)pos;
}

/* ==================== 注册/注销 ==================== */

/* 8 种事件类型的注册名前缀 */
#define METRICS_HANDLER_NAME_PREFIX "metrics_handler_"

int hook_metrics_handler_register(void)
{
    /* 注册 8 个 entry，每个用不同 name 但相同 callback。
     * name 格式：metrics_handler_<type_name>（小写），如 metrics_handler_pre_exec。
     * 用 ctx->type 在回调内区分事件类型，避免 8 个几乎相同的回调函数。 */
    static const char *const type_suffixes[HOOK_TYPE_COUNT] = {
        "pre_exec", "post_exec", "pre_llm", "post_llm",
        "pre_tool", "post_tool", "on_error", "on_memory_evolve"
    };

    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "%s%s", METRICS_HANDLER_NAME_PREFIX,
                 type_suffixes[i]);

        if (agentrt_hook_register(name,
                                   (hook_type_t)i,
                                   hook_metrics_callback,
                                   NULL,   /* user_data */
                                   50,     /* priority — 中等，不与 audit(80)/trace(90) 冲突 */
                                   true) != 0) {
            SVC_LOG_WARN("hook_metrics: failed to register %s (may already exist)", name);
            /* 幂等：重名不视为致命错误 */
        }
    }
    return 0;
}

void hook_metrics_handler_unregister(void)
{
    static const char *const type_suffixes[HOOK_TYPE_COUNT] = {
        "pre_exec", "post_exec", "pre_llm", "post_llm",
        "pre_tool", "post_tool", "on_error", "on_memory_evolve"
    };

    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "%s%s", METRICS_HANDLER_NAME_PREFIX,
                 type_suffixes[i]);
        agentrt_hook_unregister(name);
    }
}
