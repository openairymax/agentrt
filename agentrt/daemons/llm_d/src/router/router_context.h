/**
 * @file router_context.h
 * @brief 路由器共享上下文 — 全局状态、辅助函数、端点管理
 *
 * 所有路由器实现文件共享此头文件，避免循环依赖。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#ifndef AGENTRT_LLM_ROUTER_CONTEXT_H
#define AGENTRT_LLM_ROUTER_CONTEXT_H

#include "router/llm_router.h"
#include "cost_tracker.h"
#include "token_counter.h"
#include "memory_compat.h"
#include "sync_compat.h"
#include "logging_compat.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define LLM_ROUTER_MAX_ENDPOINTS 64
#define LLM_ROUTER_MAX_FALLBACK  3

/* ==================== 全局路由器状态 ==================== */

typedef struct {
    llm_endpoint_t endpoints[LLM_ROUTER_MAX_ENDPOINTS];
    size_t endpoint_count;
    llm_route_strategy_t default_strategy;
    llm_router_stats_t stats;
    cost_tracker_t *cost_tracker;
    token_counter_t *token_counter;
    agentrt_mutex_t mutex;  /* platform.h 的 mutex 类型（通过 types.h→platform.h 包含）*/
    bool initialized;

    /* P3.1.2: 轮询计数器 */
    size_t round_robin_index;
} router_ctx_t;

/**
 * @brief 获取全局路由器上下文
 */
router_ctx_t *router_ctx_get(void);

/* ==================== 共享辅助函数 ==================== */

/**
 * @brief 估算请求的 token 数
 */
static inline size_t router_estimate_tokens(const char *prompt, size_t prompt_len)
{
    router_ctx_t *ctx = router_ctx_get();
    if (!ctx->token_counter || !prompt)
        return prompt_len / 4;  /* 粗略估算：4 字符 ≈ 1 token */

    return token_counter_count(ctx->token_counter, prompt);
}

/**
 * @brief 计算端点成本
 */
static inline double router_estimate_cost(const llm_endpoint_t *ep,
                                           size_t input_tokens,
                                           size_t output_tokens)
{
    return (ep->cost_per_1k_input * input_tokens / 1000.0) +
           (ep->cost_per_1k_output * output_tokens / 1000.0);
}

/**
 * @brief 检查端点是否满足能力要求
 */
static inline bool router_has_capabilities(const llm_endpoint_t *ep,
                                            uint32_t required_caps)
{
    return (ep->capabilities & required_caps) == required_caps;
}

/**
 * @brief 获取能力匹配的端点列表
 */
static inline size_t router_get_eligible(const llm_route_request_t *request,
                                          llm_endpoint_t **out_array,
                                          size_t max_count)
{
    router_ctx_t *ctx = router_ctx_get();
    size_t count = 0;
    for (size_t i = 0; i < ctx->endpoint_count && count < max_count; i++) {
        llm_endpoint_t *ep = &ctx->endpoints[i];
        if (!ep->enabled) continue;
        if (!router_has_capabilities(ep, request->required_caps)) continue;
        if (request->preferred_provider[0] &&
            strcmp(ep->provider_name, request->preferred_provider) != 0)
            continue;
        out_array[count++] = ep;
    }
    return count;
}

/**
 * @brief 填充路由结果的基础字段
 */
static inline void router_fill_result(llm_route_result_t *result,
                                       const llm_endpoint_t *ep,
                                       llm_route_strategy_t strategy,
                                       int confidence,
                                       size_t input_tokens,
                                       size_t output_tokens)
{
    AGENTRT_STRNCPY_TERM(result->provider_name, ep->provider_name,
                         sizeof(result->provider_name));
    AGENTRT_STRNCPY_TERM(result->model_name, ep->model_name,
                         sizeof(result->model_name));
    AGENTRT_STRNCPY_TERM(result->endpoint, ep->endpoint,
                         sizeof(result->endpoint));
    result->estimated_cost = router_estimate_cost(ep, input_tokens, output_tokens);
    result->estimated_latency_ms = ep->avg_latency_ms;
    result->strategy_used = strategy;
    result->confidence = confidence;
}

/**
 * @brief 设置降级端点
 */
static inline void router_set_fallback(llm_route_result_t *result,
                                        const llm_endpoint_t *fallback)
{
    if (!fallback) return;
    AGENTRT_STRNCPY_TERM(result->fallback_provider,
                         fallback->provider_name,
                         sizeof(result->fallback_provider));
    AGENTRT_STRNCPY_TERM(result->fallback_model,
                         fallback->model_name,
                         sizeof(result->fallback_model));
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_LLM_ROUTER_CONTEXT_H */