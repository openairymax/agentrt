/**
 * @file hook_builtin_handlers.h
 * @brief P0.20.1: 生产 Hook 处理器 — 审计/metrics/trace 内置注册
 *
 * 提供 3 个生产级 Hook 处理器，覆盖 Airymax 运行时关键事件的可观测性：
 *
 *   1. **审计 Hook (hook_audit_handler)**
 *      - 注册类型：ON_ERROR + POST_TOOL
 *      - 功能：将错误事件与工具调用事件记录到审计日志（结构化 SVC_LOG_INFO），
 *        满足 L3 安全治理规范"所有安全相关操作必须留痕"要求。
 *      - 决策：CONTINUE（审计不干预流程）
 *
 *   2. **Metrics Hook (hook_metrics_handler)**
 *      - 注册类型：全部 8 种（PRE_EXEC/POST_EXEC/PRE_LLM/POST_LLM/
 *        PRE_TOOL/POST_TOOL/ON_ERROR/ON_MEMORY_EVOLVE）
 *      - 功能：原子计数器按事件类型分类累计，提供 JSON 格式导出 API
 *        agentrt_hook_metrics_dump()，为 Prometheus 导出做准备。
 *      - 决策：CONTINUE（metrics 不干预流程）
 *
 *   3. **Trace Hook (hook_trace_handler)**
 *      - 注册类型：PRE_EXEC + POST_EXEC（执行边界）
 *      - 功能：记录 trace_id + operation + timestamp，为 OpenTelemetry span
 *        导出做准备。trace_id 跨进程贯穿（L2 规范 §6）。
 *      - 决策：CONTINUE（trace 不干预流程）
 *
 * 所有处理器均为 C 回调（HOOK_IMPL_CALLBACK），回调内不执行阻塞操作（< 10ms，
 * 遵循 hook_service.h 契约）。处理器在 agentrt_hook_init() 之后通过
 * agentrt_hook_register_builtin_handlers() 注册，在 agentrt_hook_shutdown()
 * 之前通过 agentrt_hook_unregister_builtin_handlers() 注销。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTRT_HOOK_BUILTIN_HANDLERS_H
#define AGENTRT_HOOK_BUILTIN_HANDLERS_H

#include "hook_service.h"
#include "agentrt_hook.h"   /* agentrt_hook_register/unregister/trigger 等 inline API */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 内置处理器注册 ==================== */

/**
 * @brief 注册全部内置生产 Hook 处理器（审计/metrics/trace）
 *
 * 在 agentrt_hook_init() 成功后调用。注册 12 个 hook entry：
 *   - audit_handler_error    → ON_ERROR（priority=80）
 *   - audit_handler_tool     → POST_TOOL（priority=80）
 *   - metrics_handler_*      → 8 种类型（priority=50，轻量计数）
 *   - trace_handler_pre_exec → PRE_EXEC（priority=90，最先执行）
 *   - trace_handler_post_exec→ POST_EXEC（priority=10，最后执行）
 *
 * 幂等：重复调用安全（重名注册返回 -3，已注册的保持不变）。
 *
 * @return 0 成功，-1 未初始化，-3 重名冲突（部分已注册）
 */
int agentrt_hook_register_builtin_handlers(void);

/**
 * @brief 注销全部内置生产 Hook 处理器
 *
 * 在 agentrt_hook_shutdown() 之前调用。幂等。
 */
void agentrt_hook_unregister_builtin_handlers(void);

/* ==================== Metrics 导出 API ==================== */

/**
 * @brief 导出 metrics 为 JSON 格式
 *
 * 输出格式：
 * @code
 * {"hook_metrics":{
 *   "PRE_EXEC":123,"POST_EXEC":120,"PRE_LLM":80,"POST_LLM":80,
 *   "PRE_TOOL":43,"POST_TOOL":43,"ON_ERROR":3,"ON_MEMORY_EVOLVE":15
 * }}
 * @endcode
 *
 * @param buf     输出缓冲区
 * @param bufsize 缓冲区大小
 * @return 写入字节数（不含 '\0'），-1 缓冲区不足或参数无效
 */
int agentrt_hook_metrics_dump(char *buf, size_t bufsize);

/**
 * @brief 获取指定事件类型的累计计数
 *
 * @param type Hook 事件类型（0..HOOK_TYPE_COUNT-1）
 * @return 累计计数；type 越界返回 0
 */
uint64_t agentrt_hook_metrics_get_count(hook_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_HOOK_BUILTIN_HANDLERS_H */
