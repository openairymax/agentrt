/**
 * @file hook_interceptor.c
 * @brief P2.1.3 / P2.1a: 拦截型 Hook 实现 — SafetyGuard 安全链集成
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hook_interceptor.h"
#include "memory_compat.h"

#include <string.h>

/* ==================== 默认配置 ==================== */

static hook_interceptor_config_t g_interceptor_config = {
    .enable_safety_guard       = true,
    .enable_permission_check   = true,
    .enable_audit_log          = true,
    .enable_parameter_sanitize = true,
    .max_guard_timeout_ms      = 5000,
};

static bool g_initialized = false;

/* ==================== 生命周期 ==================== */

int hook_interceptor_init(const hook_interceptor_config_t *config)
{
    if (config) {
        AGENTOS_MEMCPY(&g_interceptor_config, config, sizeof(hook_interceptor_config_t));
    }
    g_initialized = true;
    return 0;
}

void hook_interceptor_destroy(void)
{
    g_initialized = false;
}

/* ==================== 配置 ==================== */

int hook_interceptor_get_config(hook_interceptor_config_t *config)
{
    if (!config || !g_initialized) return -1;
    AGENTOS_MEMCPY(config, &g_interceptor_config, sizeof(hook_interceptor_config_t));
    return 0;
}

int hook_interceptor_set_config(const hook_interceptor_config_t *config)
{
    if (!config || !g_initialized) return -1;
    AGENTOS_MEMCPY(&g_interceptor_config, config, sizeof(hook_interceptor_config_t));
    return 0;
}

/* ==================== 事件映射 ==================== */

int hook_interceptor_ctx_to_safety_event(const hook_context_t *ctx,
                                          safety_event_t *event)
{
    if (!ctx || !event) return -1;

    AGENTOS_MEMSET(event, 0, sizeof(safety_event_t));

    /* 根据 Hook 类型映射 SafetyGuard 事件类型 */
    switch (ctx->type) {
    case HOOK_TYPE_PRE_EXEC:
    case HOOK_TYPE_POST_EXEC:
        event->type = SAFETY_EVENT_EXECUTION_START;
        break;
    case HOOK_TYPE_PRE_TOOL:
    case HOOK_TYPE_POST_TOOL:
        event->type = SAFETY_EVENT_ACCESS_REQUEST;
        break;
    case HOOK_TYPE_PRE_LLM:
    case HOOK_TYPE_POST_LLM:
        event->type = SAFETY_EVENT_DATA_FLOW;
        break;
    case HOOK_TYPE_ON_ERROR:
        event->type = SAFETY_EVENT_VIOLATION_DETECTED;
        break;
    case HOOK_TYPE_ON_MEMORY_EVOLVE:
        event->type = SAFETY_EVENT_RESOURCE_ALLOCATE;
        break;
    default:
        event->type = SAFETY_EVENT_ACCESS_REQUEST;
        break;
    }

    /* 设置事件属性 */
    event->timestamp = ctx->timestamp_ns;
    event->flags = 0;

    if (ctx->source_daemon) {
        AGENTOS_STRNCPY_TERM(event->subject, ctx->source_daemon,
                             sizeof(event->subject));
    }

    if (ctx->operation) {
        AGENTOS_STRNCPY_TERM(event->action, ctx->operation,
                             sizeof(event->action));
    }

    if (ctx->hook_name) {
        AGENTOS_STRNCPY_TERM(event->resource, ctx->hook_name,
                             sizeof(event->resource));
    }

    return 0;
}

/* ==================== 决策映射 ==================== */

hook_decision_t hook_interceptor_map_decision(safety_decision_t sg_decision)
{
    switch (sg_decision) {
    case SAFETY_DECISION_ALLOW:
        return HOOK_DECISION_CONTINUE;
    case SAFETY_DECISION_DENY:
        return HOOK_DECISION_ABORT;
    case SAFETY_DECISION_CONDITIONAL:
        return HOOK_DECISION_MODIFY;
    case SAFETY_DECISION_DEFER:
        return HOOK_DECISION_SKIP;
    case SAFETY_DECISION_ABORT:
        return HOOK_DECISION_ABORT;
    default:
        return HOOK_DECISION_CONTINUE;
    }
}

/* ==================== 拦截检查 ==================== */

hook_decision_t hook_interceptor_check(hook_context_t *ctx)
{
    if (!ctx || !g_initialized)
        return HOOK_DECISION_CONTINUE;

    if (!g_interceptor_config.enable_safety_guard)
        return HOOK_DECISION_CONTINUE;

    /* 将 Hook 上下文转换为 SafetyGuard 事件 */
    safety_event_t event;
    if (hook_interceptor_ctx_to_safety_event(ctx, &event) != 0) {
        return HOOK_DECISION_CONTINUE;
    }

    /* 创建 SafetyGuard 上下文 */
    safety_guard_context_t *guard_ctx = safety_guard_create();
    if (!guard_ctx) {
        return HOOK_DECISION_CONTINUE;
    }

    /* 执行 SafetyGuard 检查链 */
    safety_result_t *results = NULL;
    size_t result_count = 0;

    safety_guard_check_chain(guard_ctx, &event, &results, &result_count);

    /* 聚合所有 Guard 的决策 */
    safety_decision_t final_decision = SAFETY_DECISION_ALLOW;

    for (size_t i = 0; i < result_count; i++) {
        /* 最严格的决策优先 */
        if (results[i].decision == SAFETY_DECISION_ABORT ||
            results[i].decision == SAFETY_DECISION_DENY) {
            final_decision = results[i].decision;
            break;
        }
        if (results[i].decision == SAFETY_DECISION_CONDITIONAL &&
            final_decision == SAFETY_DECISION_ALLOW) {
            final_decision = SAFETY_DECISION_CONDITIONAL;
        }
        if (results[i].decision == SAFETY_DECISION_DEFER &&
            final_decision == SAFETY_DECISION_ALLOW) {
            final_decision = SAFETY_DECISION_DEFER;
        }
    }

    /* 释放 SafetyGuard 资源 */
    if (results) {
        AGENTOS_FREE(results);
    }
    safety_guard_destroy(guard_ctx);

    /* 映射回 Hook 决策 */
    return hook_interceptor_map_decision(final_decision);
}