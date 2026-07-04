/**
 * @file hook_trace_handler.c
 * @brief P0.20.1: Trace Hook 处理器 — 执行边界 span 记录（OpenTelemetry 预备）
 *
 * 注册到 PRE_EXEC + POST_EXEC 两种事件类型，记录 trace span 的开始与结束，
 * 为 OpenTelemetry span 导出做准备。trace_id 跨进程贯穿（L2 规范 §6）。
 *
 * Trace span 记录格式（结构化 SVC_LOG_INFO）：
 *   - PRE_EXEC：SPAN_START|op=...|trace=...|sess=...|ts=...
 *   - POST_EXEC：SPAN_END|op=...|trace=...|sess=...|ts=...
 *
 * 配对关系：同一 trace_id 的 PRE_EXEC 与 POST_EXEC 构成一个完整 span，
 * 可通过时间戳差计算 span 持续时间。
 *
 * 决策：CONTINUE（trace 不干预流程，仅记录）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_builtin_handlers.h"
#include "hook_registry.h"
#include "svc_logger.h"

/* ==================== Trace 回调 ==================== */

/**
 * @brief PRE_EXEC trace 回调 — 记录 span 开始
 *
 * priority=90（最先执行），确保 trace span 在其他 hook 之前开始记录，
 * 使 span 持续时间尽可能覆盖完整的执行过程（含其他 hook 的耗时）。
 */
static hook_decision_t hook_trace_pre_exec_callback(hook_context_t *ctx)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    SVC_LOG_INFO("TRACE|SPAN_START|op=%s|src=%s|trace=%s|sess=%s|ts=%llu",
                 ctx->operation ? ctx->operation : "unknown",
                 ctx->source_daemon ? ctx->source_daemon : "unknown",
                 ctx->trace_id[0] ? ctx->trace_id : "-",
                 ctx->session_id[0] ? ctx->session_id : "-",
                 (unsigned long long)ctx->timestamp_ns);
    return HOOK_DECISION_CONTINUE;
}

/**
 * @brief POST_EXEC trace 回调 — 记录 span 结束
 *
 * priority=10（最后执行），确保 trace span 在其他 hook 之后结束记录，
 * 使 span 持续时间覆盖完整的执行过程。
 */
static hook_decision_t hook_trace_post_exec_callback(hook_context_t *ctx)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    SVC_LOG_INFO("TRACE|SPAN_END|op=%s|src=%s|trace=%s|sess=%s|ts=%llu",
                 ctx->operation ? ctx->operation : "unknown",
                 ctx->source_daemon ? ctx->source_daemon : "unknown",
                 ctx->trace_id[0] ? ctx->trace_id : "-",
                 ctx->session_id[0] ? ctx->session_id : "-",
                 (unsigned long long)ctx->timestamp_ns);
    return HOOK_DECISION_CONTINUE;
}

/* ==================== 注册/注销 ==================== */

#define TRACE_HANDLER_PRE_EXEC_NAME  "trace_handler_pre_exec"
#define TRACE_HANDLER_POST_EXEC_NAME "trace_handler_post_exec"

int hook_trace_handler_register(void)
{
    /* PRE_EXEC：span 开始，priority=90（最先执行） */
    if (agentos_hook_register(TRACE_HANDLER_PRE_EXEC_NAME,
                               HOOK_TYPE_PRE_EXEC,
                               hook_trace_pre_exec_callback,
                               NULL,
                               90,
                               true) != 0) {
        SVC_LOG_WARN("hook_trace: failed to register PRE_EXEC handler (may already exist)");
    }

    /* POST_EXEC：span 结束，priority=10（最后执行） */
    if (agentos_hook_register(TRACE_HANDLER_POST_EXEC_NAME,
                               HOOK_TYPE_POST_EXEC,
                               hook_trace_post_exec_callback,
                               NULL,
                               10,
                               true) != 0) {
        SVC_LOG_WARN("hook_trace: failed to register POST_EXEC handler (may already exist)");
    }

    return 0;
}

void hook_trace_handler_unregister(void)
{
    agentos_hook_unregister(TRACE_HANDLER_PRE_EXEC_NAME);
    agentos_hook_unregister(TRACE_HANDLER_POST_EXEC_NAME);
}
