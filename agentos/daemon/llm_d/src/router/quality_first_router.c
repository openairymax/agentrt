/**
 * @file quality_first_router.c
 * @brief P3.1.4: 质量优先路由
 *
 * 按优先级排序（priority 越大越优先），
 * 同优先级按上下文窗口降序排列，
 * 选择质量最高的端点。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "router/router_context.h"

/* ==================== P3.1.4: 质量优先路由 ==================== */

/**
 * @brief 质量优先路由 — 按优先级和上下文窗口排序选择最佳端点
 *
 * 排序规则：
 *   1. priority 越大越优先
 *   2. 同优先级时 context_window 越大越优先
 *
 * 适用于需要高质量输出的场景（如代码生成、复杂推理）。
 *
 * @param request 路由请求
 * @param result  路由结果输出
 * @return 0 成功，-1 无可用端点
 */
int route_quality_first(const llm_route_request_t *request,
                         llm_route_result_t *result)
{
    router_ctx_t *ctx = router_ctx_get();
    (void)ctx;
    llm_endpoint_t *eligible[LLM_ROUTER_MAX_ENDPOINTS];
    size_t eligible_count = router_get_eligible(request, eligible,
                                                 LLM_ROUTER_MAX_ENDPOINTS);

    if (eligible_count == 0) {
        AGENTOS_LOG_WARN("C-L02: Quality: no eligible endpoints "
                "(caps=0x%x, total_endpoints=%zu) STACK: route_quality_first",
                request->required_caps, router_ctx_get()->endpoint_count);
        return -1;
    }

    AGENTOS_LOG_DEBUG("C-L02: Quality: sorting %zu endpoints by quality",
                      eligible_count);

    /* 按优先级排序（priority 越大越优先，相同时按 context_window 降序） */
    for (size_t i = 0; i < eligible_count; i++) {
        for (size_t j = i + 1; j < eligible_count; j++) {
            bool should_swap = false;
            if (eligible[i]->priority < eligible[j]->priority) {
                should_swap = true;
            } else if (eligible[i]->priority == eligible[j]->priority &&
                       eligible[i]->context_window < eligible[j]->context_window) {
                should_swap = true;
            }
            if (should_swap) {
                llm_endpoint_t *tmp = eligible[i];
                eligible[i] = eligible[j];
                eligible[j] = tmp;
            }
        }
    }

    /* 日志：打印排序后的端点列表 */
    for (size_t i = 0; i < eligible_count; i++) {
        AGENTOS_LOG_DEBUG("C-L02: Quality: rank[%zu] %s/%s priority=%d "
                          "context=%u",
                          i, eligible[i]->provider_name, eligible[i]->model_name,
                          eligible[i]->priority, eligible[i]->context_window);
    }

    /* 记录优先级范围 */
    AGENTOS_LOG_DEBUG("C-L02: Quality: priority range max=%d min=%d "
                      "across %zu endpoints",
                      eligible[0]->priority,
                      eligible[eligible_count - 1]->priority,
                      eligible_count);

    llm_endpoint_t *ep = eligible[0];
    size_t input_tokens = router_estimate_tokens(request->prompt, request->prompt_len);
    size_t output_tokens = request->max_tokens > 0 ? request->max_tokens : 1024;

    router_fill_result(result, ep, LLM_ROUTE_COMPLEXITY, 95,
                       input_tokens, output_tokens);

    /* 降级：次优端点 */
    if (eligible_count > 1) {
        router_set_fallback(result, eligible[1]);
        AGENTOS_LOG_DEBUG("C-L02: Quality: fallback set to %s/%s "
                          "(priority=%d)",
                          eligible[1]->provider_name, eligible[1]->model_name,
                          eligible[1]->priority);
    }

    AGENTOS_LOG_INFO("C-L02: Quality: selected %s/%s priority=%d "
                     "context=%u",
                     ep->provider_name, ep->model_name,
                     ep->priority, ep->context_window);

    return 0;
}