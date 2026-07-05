/**
 * @file round_robin_router.c
 * @brief P3.1.2: 轮询路由
 *
 * 在所有满足能力要求的端点中按轮询顺序选择，
 * 确保请求均匀分布到各端点。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "router/router_context.h"

/* ==================== P3.1.2: 轮询路由 ==================== */

/**
 * @brief 轮询路由 — 在所有满足能力的端点中按顺序轮询
 *
 * 使用全局计数器 round_robin_index 确保每次请求
 * 分发到不同的端点，实现负载均衡。
 *
 * @param request 路由请求
 * @param result  路由结果输出
 * @return 0 成功，-1 无可用端点
 */
int route_round_robin(const llm_route_request_t *request,
                       llm_route_result_t *result)
{
    router_ctx_t *ctx = router_ctx_get();
    llm_endpoint_t *eligible[LLM_ROUTER_MAX_ENDPOINTS];
    size_t eligible_count = router_get_eligible(request, eligible,
                                                 LLM_ROUTER_MAX_ENDPOINTS);

    if (eligible_count == 0) {
        AGENTRT_LOG_WARN("C-L02: RoundRobin: no eligible endpoints "
                "(caps=0x%x, preferred=%s, total_endpoints=%zu) "
                "STACK: route_round_robin",
                request->required_caps,
                request->preferred_provider[0] ? request->preferred_provider : "any",
                router_ctx_get()->endpoint_count);
        return -1;
    }

    size_t idx = ctx->round_robin_index % eligible_count;
    ctx->round_robin_index++;

    AGENTRT_LOG_DEBUG("C-L02: RoundRobin: round_robin_index=%zu -> endpoint[%zu/%zu]",
                      ctx->round_robin_index, idx, eligible_count);

    llm_endpoint_t *ep = eligible[idx];
    size_t input_tokens = router_estimate_tokens(request->prompt, request->prompt_len);
    size_t output_tokens = request->max_tokens > 0 ? request->max_tokens : 1024;

    router_fill_result(result, ep, LLM_ROUTE_ROUND_ROBIN, 70,
                       input_tokens, output_tokens);

    /* 降级：下一个轮询端点 */
    if (eligible_count > 1) {
        size_t fallback_idx = (idx + 1) % eligible_count;
        router_set_fallback(result, eligible[fallback_idx]);
        AGENTRT_LOG_DEBUG("C-L02: RoundRobin: fallback set to %s/%s",
                          eligible[fallback_idx]->provider_name,
                          eligible[fallback_idx]->model_name);
    }

    AGENTRT_LOG_INFO("C-L02: RoundRobin: selected %s/%s (round=%zu)",
                     ep->provider_name, ep->model_name,
                     ctx->round_robin_index);

    return 0;
}