/**
 * @file hook_audit_handler.c
 * @brief P0.20.1: 审计 Hook 处理器 — 错误事件与工具调用事件审计留痕
 *
 * 注册到 ON_ERROR + POST_TOOL 两种事件类型，将关键事件记录到审计日志
 * （结构化 SVC_LOG_INFO），满足 L3 安全治理规范"所有安全相关操作必须留痕"。
 *
 * 审计记录字段：
 *   - timestamp_ns：事件时间戳（纳秒，CLOCK_REALTIME 对齐）
 *   - event_type：事件类型（ON_ERROR/POST_TOOL）
 *   - source_daemon：来源 daemon（如 coreloopthree）
 *   - operation：操作名称（如 cognition_process_fail / tool_execute）
 *   - trace_id：追踪 ID（跨进程贯穿，L2 规范 §6）
 *   - session_id：会话 ID
 *   - input_data_len：输入数据长度（不记录数据本身，避免敏感信息泄漏）
 *
 * 决策：CONTINUE（审计不干预流程，仅留痕）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_builtin_handlers.h"
#include "hook_registry.h"
#include "svc_logger.h"

#include <stdio.h>

/* ==================== 审计回调 ==================== */

/**
 * @brief ON_ERROR 审计回调 — 错误事件留痕
 *
 * 记录错误事件到审计日志，包含 operation/trace_id/source_daemon，
 * 供事后追溯与合规审计使用。
 */
static hook_decision_t hook_audit_on_error_callback(hook_context_t *ctx)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    SVC_LOG_INFO("AUDIT|ON_ERROR|op=%s|src=%s|trace=%s|sess=%s|in_len=%zu",
                 ctx->operation ? ctx->operation : "unknown",
                 ctx->source_daemon ? ctx->source_daemon : "unknown",
                 ctx->trace_id[0] ? ctx->trace_id : "-",
                 ctx->session_id[0] ? ctx->session_id : "-",
                 ctx->input_data_len);
    return HOOK_DECISION_CONTINUE;
}

/**
 * @brief POST_TOOL 审计回调 — 工具调用事件留痕
 *
 * 记录工具调用完成事件到审计日志，满足"工具调用必须审计"合规要求。
 * 不记录工具输入/输出数据本身（避免敏感信息泄漏），仅记录长度用于容量审计。
 */
static hook_decision_t hook_audit_post_tool_callback(hook_context_t *ctx)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    SVC_LOG_INFO("AUDIT|POST_TOOL|op=%s|src=%s|trace=%s|sess=%s|in_len=%zu|out_len=%zu",
                 ctx->operation ? ctx->operation : "unknown",
                 ctx->source_daemon ? ctx->source_daemon : "unknown",
                 ctx->trace_id[0] ? ctx->trace_id : "-",
                 ctx->session_id[0] ? ctx->session_id : "-",
                 ctx->input_data_len,
                 ctx->output_data_len);
    return HOOK_DECISION_CONTINUE;
}

/* ==================== 注册/注销 ==================== */

/* 审计 handler 注册名（全局唯一，用于注销时查找） */
#define AUDIT_HANDLER_ERROR_NAME "audit_handler_error"
#define AUDIT_HANDLER_TOOL_NAME  "audit_handler_tool"

int hook_audit_handler_register(void)
{
    /* ON_ERROR：错误事件审计，priority=80（高优先级，先于 trace 记录） */
    if (agentrt_hook_register(AUDIT_HANDLER_ERROR_NAME,
                               HOOK_TYPE_ON_ERROR,
                               hook_audit_on_error_callback,
                               NULL,   /* user_data */
                               80,     /* priority */
                               true) != 0) {
        SVC_LOG_WARN("hook_audit: failed to register ON_ERROR handler (may already exist)");
        /* 不返回错误：重名是幂等场景，部分已注册可接受 */
    }

    /* POST_TOOL：工具调用审计，priority=80 */
    if (agentrt_hook_register(AUDIT_HANDLER_TOOL_NAME,
                               HOOK_TYPE_POST_TOOL,
                               hook_audit_post_tool_callback,
                               NULL,
                               80,
                               true) != 0) {
        SVC_LOG_WARN("hook_audit: failed to register POST_TOOL handler (may already exist)");
    }

    return 0;
}

void hook_audit_handler_unregister(void)
{
    agentrt_hook_unregister(AUDIT_HANDLER_ERROR_NAME);
    agentrt_hook_unregister(AUDIT_HANDLER_TOOL_NAME);
}
