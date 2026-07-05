/**
 * @file hook_builtin_handlers.c
 * @brief P0.20.1: 内置 Hook 处理器统一注册入口
 *
 * 整合 audit/metrics/trace 三个子处理器的注册与注销，提供单一入口
 * agentrt_hook_register_builtin_handlers() / agentrt_hook_unregister_builtin_handlers()。
 *
 * 调用顺序：
 *   注册：metrics → audit → trace（按 priority 升序，确保执行顺序正确）
 *   注销：trace → audit → metrics（反向，确保清理顺序正确）
 *
 * 注册后 hook entry 总数：12
 *   - metrics: 8（全事件类型，priority=50）
 *   - audit: 2（ON_ERROR + POST_TOOL，priority=80）
 *   - trace: 2（PRE_EXEC + POST_EXEC，priority=90/10）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_builtin_handlers.h"
#include "svc_logger.h"

/* ==================== 子处理器前向声明 ==================== */

/* hook_audit_handler.c */
int  hook_audit_handler_register(void);
void hook_audit_handler_unregister(void);

/* hook_metrics_handler.c */
int  hook_metrics_handler_register(void);
void hook_metrics_handler_unregister(void);

/* hook_trace_handler.c */
int  hook_trace_handler_register(void);
void hook_trace_handler_unregister(void);

/* ==================== 统一注册/注销 ==================== */

int agentrt_hook_register_builtin_handlers(void)
{
    int total_registered = 0;

    /* 注册顺序：metrics → audit → trace
     * 注册顺序不影响执行顺序（执行顺序由 priority 决定），但影响注册日志输出顺序。 */

    if (hook_metrics_handler_register() == 0) {
        total_registered += 8;
    }
    if (hook_audit_handler_register() == 0) {
        total_registered += 2;
    }
    if (hook_trace_handler_register() == 0) {
        total_registered += 2;
    }

    SVC_LOG_INFO("P0.20.1: builtin hook handlers registered (total=%d, "
                 "metrics=8 audit=2 trace=2)", total_registered);
    return 0;
}

void agentrt_hook_unregister_builtin_handlers(void)
{
    /* 注销顺序：trace → audit → metrics（反向清理） */
    hook_trace_handler_unregister();
    hook_audit_handler_unregister();
    hook_metrics_handler_unregister();

    SVC_LOG_INFO("P0.20.1: builtin hook handlers unregistered");
}
