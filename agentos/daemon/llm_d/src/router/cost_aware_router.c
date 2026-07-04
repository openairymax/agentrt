/**
 * @file cost_aware_router.c
 * @brief P3.1.1: 成本感知路由 — 决策树路由
 *
 * 决策树：
 *   预算检查 → 任务类型判断 → Provider 选择 → 降级链
 *
 * 在所有满足能力要求的端点中选择成本最低的，
 * 同时考虑预算上限和延迟上限约束。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "router/router_context.h"

/* ==================== P3.1.1: 成本感知路由 ==================== */

/**
 * @brief 决策树路由（预算检查 → 任务类型判断 → Provider 选择 → 降级链）
 *
 * 路由决策流程：
 *   1. 筛选满足能力要求的端点
 *   2. 估算输入/输出 token 数
 *   3. 遍历端点，跳过超出预算或延迟上限的
 *   4. 选择成本最低的端点
 *   5. 设置次优端点作为降级
 *
 * @param request 路由请求
 * @param result  路由结果输出
 * @return 0 成功，-1 无可用端点
 */
int route_cost_aware(const llm_route_request_t *request,
                      llm_route_result_t *result)
{
    router_ctx_t *ctx = router_ctx_get();
    (void)ctx;
    llm_endpoint_t *eligible[LLM_ROUTER_MAX_ENDPOINTS];
    size_t eligible_count = router_get_eligible(request, eligible,
                                                 LLM_ROUTER_MAX_ENDPOINTS);

    if (eligible_count == 0) {
        AGENTOS_LOG_WARN("C-L02: CostAware: no eligible endpoints found "
                         "(caps=0x%x, preferred=%s)",
                         request->required_caps,
                         request->preferred_provider[0] ? request->preferred_provider : "any");
        return -1;
    }

    AGENTOS_LOG_DEBUG("C-L02: CostAware: evaluating %zu eligible endpoints", eligible_count);

    size_t input_tokens = router_estimate_tokens(request->prompt, request->prompt_len);
    size_t output_tokens = request->max_tokens > 0 ? request->max_tokens : 1024;

    AGENTOS_LOG_DEBUG("C-L02: CostAware: estimated tokens input=%zu output=%zu "
                      "budget=$%.6f latency_limit=%ums",
                      input_tokens, output_tokens,
                      request->max_cost, request->max_latency_ms);

    /* 决策树：选择成本最低的端点 */
    size_t best_idx = 0;
    double best_cost = INFINITY;
    size_t fallback_idx = 0;
    size_t skipped_budget = 0;
    size_t skipped_latency = 0;

    for (size_t i = 0; i < eligible_count; i++) {
        double cost = router_estimate_cost(eligible[i], input_tokens, output_tokens);

        AGENTOS_LOG_DEBUG("C-L02: CostAware: endpoint[%zu] %s/%s cost=$%.6f "
                          "latency=%ums",
                          i, eligible[i]->provider_name, eligible[i]->model_name,
                          cost, eligible[i]->avg_latency_ms);

        /* 预算检查 */
        if (request->max_cost > 0 && cost > request->max_cost) {
            AGENTOS_LOG_DEBUG("C-L02: CostAware: skipping %s/%s — over budget "
                              "($%.6f > $%.6f)",
                              eligible[i]->provider_name, eligible[i]->model_name,
                              cost, request->max_cost);
            skipped_budget++;
            continue;
        }

        /* 延迟检查 */
        if (request->max_latency_ms > 0 &&
            eligible[i]->avg_latency_ms > request->max_latency_ms) {
            AGENTOS_LOG_DEBUG("C-L02: CostAware: skipping %s/%s — over latency "
                              "(%ums > %ums)",
                              eligible[i]->provider_name, eligible[i]->model_name,
                              eligible[i]->avg_latency_ms, request->max_latency_ms);
            skipped_latency++;
            continue;
        }

        if (cost < best_cost) {
            fallback_idx = best_idx;
            best_idx = i;
            best_cost = cost;
        }
    }

    AGENTOS_LOG_DEBUG("C-L02: CostAware: filtered %zu budget + %zu latency, "
                      "best_cost=$%.6f",
                      skipped_budget, skipped_latency, best_cost);

    if (best_cost == INFINITY) {
        AGENTOS_LOG_WARN("C-L02: CostAware: no endpoint within budget/latency constraints "
                        "(skipped_budget=%zu, skipped_latency=%zu, total_eligible=%zu, "
                        "budget=$%.6f, latency_limit=%ums) STACK: route_cost_aware",
                        skipped_budget, skipped_latency, eligible_count,
                        request->max_cost, request->max_latency_ms);

    /* 预算耗尽警告：所有端点都因超预算被跳过 */
    if (skipped_budget == eligible_count && skipped_budget > 0) {
        AGENTOS_LOG_WARN("C-L02: CostAware: budget exhausted — all %zu eligible "
                         "endpoints exceed budget=$%.6f STACK: route_cost_aware",
                         eligible_count, request->max_cost);
    }
        return -1;
    }

    /* 填充结果 */
    llm_endpoint_t *best = eligible[best_idx];
    router_fill_result(result, best, LLM_ROUTE_COST, 90,
                       input_tokens, output_tokens);

    /* 设置降级 */
    if (eligible_count > 1 && fallback_idx != best_idx) {
        router_set_fallback(result, eligible[fallback_idx]);
        AGENTOS_LOG_DEBUG("C-L02: CostAware: fallback set to %s/%s",
                          eligible[fallback_idx]->provider_name,
                          eligible[fallback_idx]->model_name);
    }

    AGENTOS_LOG_INFO("C-L02: CostAware: selected %s/%s cost=$%.6f latency=%ums",
                     best->provider_name, best->model_name,
                     best_cost, best->avg_latency_ms);

    return 0;
}