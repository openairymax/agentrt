/**
 * @file service.c
 * @brief Hook 服务实现 — Phase 2 完整版
 *
 * 使用 hook_registry、hook_executor、hook_timeout、hook_interceptor
 * 模块实现完整的 Hook 生命周期管理。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_service.h"
#include "hook_registry.h"
#include "hook_executor.h"
#include "hook_interceptor.h"
#include "hook_timeout.h"
#include "memory_compat.h"

#include <string.h>

/* ==================== 服务 API ==================== */

int hook_service_register(const hook_registration_t *reg)
{
    if (!reg || !reg->name)
        return -1;

    /* 转换为 hook_entry_t */
    hook_entry_t entry;
    AGENTRT_MEMSET(&entry, 0, sizeof(entry));

    AGENTRT_STRNCPY_TERM(entry.name, reg->name, sizeof(entry.name));
    entry.type       = reg->type;
    entry.impl_type  = HOOK_IMPL_CALLBACK;
    entry.callback   = reg->callback;
    entry.user_data  = reg->user_data;
    entry.priority   = reg->priority;
    entry.enabled    = reg->enabled;

    return hook_registry_register(&entry);
}

int hook_service_unregister(const char *name)
{
    return hook_registry_unregister(name);
}

hook_decision_t hook_service_fire(hook_context_t *ctx)
{
    if (!ctx)
        return HOOK_DECISION_CONTINUE;

    /* P2.1a: 在 PRE_TOOL / PRE_EXEC Hook 触发前执行拦截检查 */
    if (ctx->type == HOOK_TYPE_PRE_TOOL || ctx->type == HOOK_TYPE_PRE_EXEC) {
        hook_decision_t interceptor_decision = hook_interceptor_check(ctx);
        if (interceptor_decision != HOOK_DECISION_CONTINUE) {
            return interceptor_decision;
        }
    }

    /* 执行 Hook 链 */
    return hook_executor_run(ctx, HOOK_EXEC_MODE_SEQUENTIAL);
}

int hook_service_get_stats(const char *name, hook_stats_t *stats)
{
    return hook_registry_get_stats(name, stats);
}

int hook_service_set_enabled(const char *name, bool enabled)
{
    return hook_registry_set_enabled(name, enabled);
}