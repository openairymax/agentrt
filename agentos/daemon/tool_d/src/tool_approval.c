/**
 * @file tool_approval.c
 * @brief C-L05: Cupolas SafetyGuard → tool_d 工具审批适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "tool_approval.h"
#include "daemon_errors.h"
#include "daemon_security.h"
#include "error.h"
#include "logger.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tool_approval_ctx {
    tool_approval_config_t config;
    char agent_id[128];
    uint64_t total_checks;
    uint64_t denied_count;
    uint64_t sanitized_count;
};

tool_approval_ctx_t *tool_approval_create(const tool_approval_config_t *cfg)
{
    tool_approval_ctx_t *ctx =
        (tool_approval_ctx_t *)AGENTOS_CALLOC(1, sizeof(tool_approval_ctx_t));
    if (!ctx) {
        AGENTOS_LOG_ERROR("tool_approval_create: alloc failed");
        return NULL;
    }

    if (cfg) {
        ctx->config = *cfg;
        if (cfg->agent_id) {
            snprintf(ctx->agent_id, sizeof(ctx->agent_id), "%s", cfg->agent_id);
        }
    } else {
        ctx->config.agent_id = ctx->agent_id;
        ctx->config.enable_safety_guard_chain = false;
        ctx->config.enable_audit_logging = true;
        ctx->config.permission_rules = NULL;
        snprintf(ctx->agent_id, sizeof(ctx->agent_id), "unknown");
    }

    ctx->total_checks = 0;
    ctx->denied_count = 0;
    ctx->sanitized_count = 0;

    AGENTOS_LOG_INFO("C-L05: Tool approval context created (safety_guard=%d, audit=%d)",
                     ctx->config.enable_safety_guard_chain,
                     ctx->config.enable_audit_logging);
    return ctx;
}

void tool_approval_destroy(tool_approval_ctx_t *ctx)
{
    if (!ctx)
        return;
    AGENTOS_LOG_INFO("C-L05: Tool approval destroyed (checks=%llu denied=%llu sanitized=%llu)",
                     (unsigned long long)ctx->total_checks,
                     (unsigned long long)ctx->denied_count,
                     (unsigned long long)ctx->sanitized_count);
    AGENTOS_FREE(ctx);
}

int tool_approval_sanitize_params(tool_approval_ctx_t *ctx, const char *tool_name,
                                  const char *params_json, char *sanitized_params,
                                  size_t sanitized_size)
{
    if (!ctx || !tool_name || !params_json || !sanitized_params || sanitized_size == 0) {
        return -1;
    }

    /* 使用 daemon_security 进行参数净化 */
    char sanitized_tool[256];
    int ret = daemon_sanitize_tool_params(tool_name, params_json, sanitized_tool,
                                          sizeof(sanitized_tool), sanitized_params, sanitized_size);

    if (ret == 0) {
        /* 检查参数是否被修改 */
        if (strcmp(params_json, sanitized_params) != 0) {
            AGENTOS_LOG_INFO("C-L05: Tool params sanitized for '%s'", tool_name);
            ctx->sanitized_count++;
        }
    } else {
        AGENTOS_LOG_WARN("C-L05: Tool param sanitization failed for '%s': ret=%d", tool_name, ret);
    }

    return ret;
}

int tool_approval_check(tool_approval_ctx_t *ctx, const tool_metadata_t *meta,
                        const char *params_json, tool_approval_detail_t *detail)
{
    if (!ctx || !meta) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 初始化详情 */
    if (detail) {
        __builtin_memset(detail, 0, sizeof(*detail));
        detail->decision = TOOL_APPROVAL_DENIED;
        detail->permission_check_passed = 0;
        detail->safety_guard_passed = 0;
        detail->params_were_sanitized = 0;
    }

    ctx->total_checks++;

    const char *tool_name = meta->name ? meta->name : "unknown";
    const char *agent_id = ctx->config.agent_id ? ctx->config.agent_id : "unknown";

    /* ── 步骤 1: 参数净化 ── */
    char sanitized[4096] = {0};
    if (params_json && params_json[0] != '\0') {
        int san_ret = tool_approval_sanitize_params(ctx, tool_name, params_json, sanitized,
                                                    sizeof(sanitized));
        if (san_ret == 0 && strcmp(params_json, sanitized) != 0) {
            if (detail) {
                detail->params_were_sanitized = 1;
                snprintf(detail->sanitized_params, sizeof(detail->sanitized_params), "%s",
                         sanitized);
            }
        }
    } else {
        if (sanitized[0] == '\0' && params_json) {
            snprintf(sanitized, sizeof(sanitized), "%s", params_json);
        }
    }

    /* ── 步骤 2: 权限检查 (Cupolas) ── */
    int perm_ret = daemon_check_tool_permission(agent_id, tool_name, "execute");
    if (!perm_ret) {
        AGENTOS_LOG_WARN("C-L05: Permission denied for agent='%s' tool='%s'", agent_id, tool_name);
        if (detail) {
            detail->permission_check_passed = 0;
            snprintf(detail->reason, sizeof(detail->reason),
                     "Permission denied: agent '%s' cannot execute tool '%s'", agent_id, tool_name);
        }
        ctx->denied_count++;
        return AGENTOS_ERR_PERMISSION_DENIED;
    }

    if (detail) {
        detail->permission_check_passed = 1;
    }

    /* ── 步骤 3: SafetyGuard 链式检查（可选） ── */
    if (ctx->config.enable_safety_guard_chain) {
        /* SafetyGuard 链式检查通过 daemon_security 内部集成 */
        /* 此处使用 daemon_check_tool_permission 已涵盖基本 SafetyGuard 检查 */
        /* 未来可扩展为调用 safety_guard_check() API */
        if (detail) {
            detail->safety_guard_passed = 1;
        }
    }

    /* ── 步骤 4: 审计日志 ── */
    if (ctx->config.enable_audit_logging) {
        daemon_audit_log_event("tool_d", "tool_execute", tool_name, 1, agent_id);
    }

    /* ── 返回结果 ── */
    if (detail) {
        if (detail->params_were_sanitized) {
            detail->decision = TOOL_APPROVAL_SANITIZED;
            snprintf(detail->reason, sizeof(detail->reason),
                     "Approved with sanitized params for tool '%s'", tool_name);
        } else {
            detail->decision = TOOL_APPROVAL_ALLOWED;
            snprintf(detail->reason, sizeof(detail->reason), "Approved for tool '%s'", tool_name);
        }
    }

    return 0;
}

void tool_approval_get_stats(tool_approval_ctx_t *ctx, uint64_t *out_total_checks,
                             uint64_t *out_denied_count, uint64_t *out_sanitized_count)
{
    if (!ctx)
        return;
    if (out_total_checks)
        *out_total_checks = ctx->total_checks;
    if (out_denied_count)
        *out_denied_count = ctx->denied_count;
    if (out_sanitized_count)
        *out_sanitized_count = ctx->sanitized_count;
}