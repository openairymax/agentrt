/**
 * @file hook_executor.c
 * @brief P2.1.2: Hook 执行器实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hook_executor.h"
#include "hook_timeout.h"
#include "memory_compat.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==================== 决策聚合 ==================== */

hook_decision_t hook_executor_merge_decision(hook_decision_t current,
                                              hook_decision_t new_dec)
{
    /* ABORT 优先级最高，一旦 ABORT 不再改变 */
    if (current == HOOK_DECISION_ABORT || new_dec == HOOK_DECISION_ABORT)
        return HOOK_DECISION_ABORT;

    /* RETRY 次之 */
    if (current == HOOK_DECISION_RETRY || new_dec == HOOK_DECISION_RETRY)
        return HOOK_DECISION_RETRY;

    /* MODIFY */
    if (current == HOOK_DECISION_MODIFY || new_dec == HOOK_DECISION_MODIFY)
        return HOOK_DECISION_MODIFY;

    /* SKIP */
    if (current == HOOK_DECISION_SKIP || new_dec == HOOK_DECISION_SKIP)
        return HOOK_DECISION_SKIP;

    return HOOK_DECISION_CONTINUE;
}

/* ==================== 上下文序列化 ==================== */

int hook_executor_ctx_to_json(const hook_context_t *ctx, char *buf, size_t bufsize)
{
    if (!ctx || !buf || bufsize == 0) return -1;

    const char *type_names[] = {
        "PRE_EXEC", "POST_EXEC", "PRE_LLM", "POST_LLM",
        "PRE_TOOL", "POST_TOOL", "ON_ERROR", "ON_MEMORY_EVOLVE"
    };

    const char *type_str = (ctx->type < HOOK_TYPE_COUNT)
        ? type_names[ctx->type] : "UNKNOWN";

    return snprintf(buf, bufsize,
        "{"
        "\"type\":\"%s\","
        "\"hook_name\":\"%s\","
        "\"source_daemon\":\"%s\","
        "\"operation\":\"%s\","
        "\"input_data_len\":%zu,"
        "\"output_data_len\":%zu,"
        "\"timestamp_ns\":%llu,"
        "\"trace_id\":\"%s\","
        "\"session_id\":\"%s\""
        "}",
        type_str,
        ctx->hook_name ? ctx->hook_name : "",
        ctx->source_daemon ? ctx->source_daemon : "",
        ctx->operation ? ctx->operation : "",
        ctx->input_data_len,
        ctx->output_data_len,
        (unsigned long long)ctx->timestamp_ns,
        ctx->trace_id,
        ctx->session_id);
}

/* ==================== 单个 Hook 执行 ==================== */

hook_decision_t hook_executor_run_one(const hook_entry_t *entry,
                                       hook_context_t *ctx,
                                       uint64_t *out_duration_ns)
{
    if (!entry || !ctx) {
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    hook_decision_t decision = HOOK_DECISION_CONTINUE;

    switch (entry->impl_type) {
    case HOOK_IMPL_CALLBACK: {
        if (!entry->callback) break;

        /* 带超时保护执行 */
        decision = hook_timeout_run(entry, ctx, 0, out_duration_ns);
        break;
    }
    case HOOK_IMPL_SHELL: {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int exit_code = hook_executor_run_shell(entry->script_path, ctx);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        switch (exit_code) {
        case 0: decision = HOOK_DECISION_CONTINUE; break;
        case 1: decision = HOOK_DECISION_SKIP;     break;
        case 2: decision = HOOK_DECISION_RETRY;    break;
        case 3: decision = HOOK_DECISION_ABORT;    break;
        case 4: decision = HOOK_DECISION_MODIFY;   break;
        default: decision = HOOK_DECISION_CONTINUE; break;
        }
        break;
    }
    case HOOK_IMPL_PYTHON: {
        /* Python 脚本通过 shell 执行（python3 script.py） */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        char cmd[HOOK_HOOK_PATH_MAX_LEN + 32];
        snprintf(cmd, sizeof(cmd), "python3 %s", entry->script_path);
        /* 简化：直接调用 shell 执行 */
        int exit_code = hook_executor_run_shell(cmd, ctx);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        switch (exit_code) {
        case 0: decision = HOOK_DECISION_CONTINUE; break;
        case 1: decision = HOOK_DECISION_SKIP;     break;
        case 2: decision = HOOK_DECISION_RETRY;    break;
        case 3: decision = HOOK_DECISION_ABORT;    break;
        case 4: decision = HOOK_DECISION_MODIFY;   break;
        default: decision = HOOK_DECISION_CONTINUE; break;
        }
        break;
    }
    case HOOK_IMPL_WEBHOOK: {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int result = hook_executor_run_webhook(entry->script_path, ctx);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        decision = (result == 0) ? HOOK_DECISION_CONTINUE : HOOK_DECISION_ABORT;
        break;
    }
    default:
        if (out_duration_ns) *out_duration_ns = 0;
        break;
    }

    /* 更新统计 */
    uint64_t duration = out_duration_ns ? *out_duration_ns : 0;
    hook_registry_update_stats(entry->name, decision, duration);

    return decision;
}

/* ==================== Hook 链执行 ==================== */

hook_decision_t hook_executor_run(hook_context_t *ctx, hook_exec_mode_t mode)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    /* 获取该类型的所有已启用 Hook */
    hook_entry_t *entries[HOOK_REGISTRY_MAX];
    size_t count = 0;

    if (hook_registry_get_by_type(ctx->type, entries,
                                   HOOK_REGISTRY_MAX, &count) != 0) {
        return HOOK_DECISION_CONTINUE;
    }

    if (count == 0)
        return HOOK_DECISION_CONTINUE;

    hook_decision_t final_decision = HOOK_DECISION_CONTINUE;

    /* 顺序执行 */
    for (size_t i = 0; i < count; i++) {
        uint64_t duration_ns = 0;
        hook_decision_t decision = hook_executor_run_one(entries[i], ctx,
                                                          &duration_ns);

        final_decision = hook_executor_merge_decision(final_decision, decision);

        /* 如果 ABORT，立即停止执行后续 Hook */
        if (final_decision == HOOK_DECISION_ABORT)
            break;
    }

    (void)mode; /* 并行模式在 Phase 3 实现 */

    return final_decision;
}

/* ==================== Shell 执行 ==================== */

int hook_executor_run_shell(const char *script_path, hook_context_t *ctx)
{
    if (!script_path || !ctx) return -1;

    /* 序列化上下文为 JSON */
    char json_buf[4096];
    int json_len = hook_executor_ctx_to_json(ctx, json_buf, sizeof(json_buf));
    if (json_len < 0) return -1;

    /* 构建命令 */
    char cmd[HOOK_HOOK_PATH_MAX_LEN + 64];
    snprintf(cmd, sizeof(cmd),
             "AGENTOS_HOOK_CONTEXT='%s' /bin/sh %s 2>/dev/null",
             json_buf, script_path);

    int exit_code = system(cmd);
    return WEXITSTATUS(exit_code);
}

/* ==================== Webhook 执行 ==================== */

int hook_executor_run_webhook(const char *url, hook_context_t *ctx)
{
    if (!url || !ctx) return -1;

    /* 序列化上下文为 JSON */
    char json_buf[4096];
    int json_len = hook_executor_ctx_to_json(ctx, json_buf, sizeof(json_buf));
    if (json_len < 0) return -1;

    /* 使用 curl 发送 POST 请求 */
    char cmd[HOOK_HOOK_PATH_MAX_LEN + 8192];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST -H 'Content-Type: application/json' "
             "-d '%s' --max-time 5 '%s' > /dev/null 2>&1",
             json_buf, url);

    int exit_code = system(cmd);
    return WEXITSTATUS(exit_code);
}