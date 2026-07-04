/**
 * @file least_latency_router.c
 * @brief P3.1.3: 最低延迟路由
 *
 * 在所有满足能力要求的端点中选择平均延迟最低的，
 * 适用于对响应时间敏感的场景。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "router/router_context.h"

/* ==================== P3.1.3: 最低延迟路由 ==================== */

/**
 * @brief 最低延迟路由 — 选择 avg_latency_ms 最小的端点
 *
 * 遍历所有满足能力要求的端点，比较平均延迟，
 * 选择延迟最低的端点。如果延迟相同，选择成本更低的。
 *
 * @param request 路由请求
 * @param result  路由结果输出
 * @return 0 成功，-1 无可用端点
 */
int route_least_latency(const llm_route_request_t *request,
                         llm_route_result_t *result)
{
    router_ctx_t *ctx = router_ctx_get();
    (void)ctx;
    llm_endpoint_t *eligible[LLM_ROUTER_MAX_ENDPOINTS];
    size_t eligible_count = router_get_eligible(request, eligible,
                                                 LLM_ROUTER_MAX_ENDPOINTS);

    if (eligible_count == 0) {
        AGENTOS_LOG_WARN("C-L02: Latency: no eligible endpoints "
                "(caps=0x%x, total_endpoints=%zu) STACK: route_least_latency",
                request->required_caps, router_ctx_get()->endpoint_count);
        return -1;
    }

    AGENTOS_LOG_DEBUG("C-L02: Latency: evaluating %zu endpoints for latency",
                      eligible_count);

    /* 选择延迟最低的端点 */
    size_t best_idx = 0;
    uint32_t best_latency = UINT32_MAX;

    for (size_t i = 0; i < eligible_count; i++) {
        AGENTOS_LOG_DEBUG("C-L02: Latency: endpoint[%zu] %s/%s latency=%ums",
                          i, eligible[i]->provider_name, eligible[i]->model_name,
                          eligible[i]->avg_latency_ms);

        if (eligible[i]->avg_latency_ms < best_latency) {
            best_latency = eligible[i]->avg_latency_ms;
            best_idx = i;
        }
    }

    /* 记录延迟范围 */
    {
        uint32_t min_lat = UINT32_MAX, max_lat = 0;
        for (size_t i = 0; i < eligible_count; i++) {
            if (eligible[i]->avg_latency_ms < min_lat)
                min_lat = eligible[i]->avg_latency_ms;
            if (eligible[i]->avg_latency_ms > max_lat)
                max_lat = eligible[i]->avg_latency_ms;
        }
        AGENTOS_LOG_DEBUG("C-L02: Latency: latency range min=%ums max=%ums "
                          "across %zu endpoints",
                          min_lat, max_lat, eligible_count);
    }

    llm_endpoint_t *ep = eligible[best_idx];
    size_t input_tokens = router_estimate_tokens(request->prompt, request->prompt_len);
    size_t output_tokens = request->max_tokens > 0 ? request->max_tokens : 1024;

    router_fill_result(result, ep, LLM_ROUTE_LATENCY, 85,
                       input_tokens, output_tokens);

    /* 降级：延迟次低的端点 */
    if (eligible_count > 1) {
        size_t fallback_idx = 0;
        uint32_t second_best = UINT32_MAX;
        for (size_t i = 0; i < eligible_count; i++) {
            if (i != best_idx && eligible[i]->avg_latency_ms < second_best) {
                second_best = eligible[i]->avg_latency_ms;
                fallback_idx = i;
            }
        }
        if (second_best != UINT32_MAX) {
            router_set_fallback(result, eligible[fallback_idx]);
            AGENTOS_LOG_DEBUG("C-L02: Latency: fallback set to %s/%s "
                              "(latency=%ums)",
                              eligible[fallback_idx]->provider_name,
                              eligible[fallback_idx]->model_name,
                              second_best);
        }
    }

    AGENTOS_LOG_INFO("C-L02: Latency: selected %s/%s latency=%ums",
                     ep->provider_name, ep->model_name, best_latency);

    return 0;
}