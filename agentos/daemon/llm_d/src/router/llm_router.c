/**
 * @file llm_router.c
 * @brief P3.1.5: LLM 路由器编排器 — 统一选择接口 + 端点管理 + 统计
 *
 * 编排四种路由策略，提供统一的 agentos_router_select_provider() 接口。
 * 负责端点注册/注销、统计收集、默认策略管理。
 *
 * 路由策略（独立文件实现）：
 *   - P3.1.1 cost_aware_router.c:     成本感知路由（决策树）
 *   - P3.1.2 round_robin_router.c:    轮询路由
 *   - P3.1.3 least_latency_router.c:  最低延迟路由
 *   - P3.1.4 quality_first_router.c:  质量优先路由
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "router/router_context.h"
#include "router/router_internal.h"

/* ==================== 全局路由器上下文 ==================== */

static router_ctx_t g_router;

router_ctx_t *router_ctx_get(void)
{
    return &g_router;
}

/* ==================== 生命周期 ==================== */

int llm_router_init(const char *config_path)
{
    router_ctx_t *ctx = router_ctx_get();

    if (ctx->initialized) {
        AGENTOS_LOG_INFO("C-L02: LLMRouter: already initialized, skipping");
        return 0;
    }

    AGENTOS_MEMSET(ctx, 0, sizeof(router_ctx_t));
    ctx->default_strategy = LLM_ROUTE_COST;

    AGENTOS_LOG_INFO("C-L02: LLMRouter: initializing with default_strategy=COST");

    if (AGENTOS_MUTEX_INIT(&ctx->mutex, NULL) != 0) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: failed to initialize mutex STACK: llm_router_init");
        return -1;
    }

    /* 初始化成本追踪器 (P3.1.6) */
    pricing_rule_t default_rules[] = {
        {"gpt-4*",    0.03,   0.06},
        {"gpt-3.5*",  0.001,  0.002},
        {"claude*",   0.015,  0.075},
        {"deepseek*", 0.00014, 0.00028},
        {"gemini*",   0.0005,  0.0015},
    };
    ctx->cost_tracker = cost_tracker_create(default_rules,
        sizeof(default_rules) / sizeof(default_rules[0]));
    if (!ctx->cost_tracker) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: failed to create cost_tracker STACK: llm_router_init");
        AGENTOS_MUTEX_DESTROY(&ctx->mutex);
        return -1;
    }
    AGENTOS_LOG_INFO("C-L02: LLMRouter: cost_tracker initialized with %zu pricing rules",
                     sizeof(default_rules) / sizeof(default_rules[0]));

    /* 初始化 token 计数器 (P3.1.6) */
    ctx->token_counter = token_counter_create("cl100k_base");
    if (!ctx->token_counter) {
        AGENTOS_LOG_WARN("C-L02: LLMRouter: token_counter creation failed, "
                         "will use heuristic estimation");
    } else {
        AGENTOS_LOG_INFO("C-L02: LLMRouter: token_counter initialized (encoding=cl100k_base)");
    }

    ctx->initialized = true;

    AGENTOS_LOG_INFO("C-L02: LLMRouter: initialization complete (config=%s)",
                     config_path ? config_path : "default");
    (void)config_path;
    return 0;
}

void llm_router_destroy(void)
{
    router_ctx_t *ctx = router_ctx_get();

    if (!ctx->initialized) {
        AGENTOS_LOG_DEBUG("C-L02: LLMRouter: not initialized, skip destroy");
        return;
    }

    AGENTOS_LOG_INFO("C-L02: LLMRouter: destroying (total_requests=%llu, "
                     "total_cost=$%.6f, total_tokens=%llu, fallbacks=%llu, errors=%llu)",
                     (unsigned long long)ctx->stats.total_requests,
                     ctx->stats.total_cost,
                     (unsigned long long)ctx->stats.total_tokens,
                     (unsigned long long)ctx->stats.fallback_count,
                     (unsigned long long)ctx->stats.error_count);

    if (ctx->cost_tracker) {
        cost_tracker_destroy(ctx->cost_tracker);
        ctx->cost_tracker = NULL;
    }
    if (ctx->token_counter) {
        token_counter_destroy(ctx->token_counter);
        ctx->token_counter = NULL;
    }

    AGENTOS_MUTEX_DESTROY(&ctx->mutex);
    AGENTOS_MEMSET(ctx, 0, sizeof(router_ctx_t));

    AGENTOS_LOG_INFO("C-L02: LLMRouter: destroyed");
}

/* ==================== 端点管理 ==================== */

int llm_router_register_endpoint(const llm_endpoint_t *endpoint)
{
    router_ctx_t *ctx = router_ctx_get();

    if (!endpoint) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: register_endpoint called with NULL endpoint STACK: llm_router_register_endpoint");
        return -1;
    }

    AGENTOS_MUTEX_LOCK(&ctx->mutex);

    if (ctx->endpoint_count >= LLM_ROUTER_MAX_ENDPOINTS) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: endpoint limit reached (%zu/%d), "
                          "cannot register %s/%s STACK: llm_router_register_endpoint",
                          ctx->endpoint_count, LLM_ROUTER_MAX_ENDPOINTS,
                          endpoint->provider_name, endpoint->model_name);
        AGENTOS_MUTEX_UNLOCK(&ctx->mutex);
        return -1;
    }

    AGENTOS_MEMCPY(&ctx->endpoints[ctx->endpoint_count], endpoint,
                  sizeof(llm_endpoint_t));
    ctx->endpoint_count++;

    AGENTOS_LOG_INFO("C-L02: LLMRouter: registered endpoint %s/%s (total=%zu, "
                     "caps=0x%x, cost=$%.6f/$%.6f, latency=%ums)",
                     endpoint->provider_name, endpoint->model_name,
                     ctx->endpoint_count,
                     endpoint->capabilities,
                     endpoint->cost_per_1k_input, endpoint->cost_per_1k_output,
                     endpoint->avg_latency_ms);

    AGENTOS_MUTEX_UNLOCK(&ctx->mutex);
    return 0;
}

int llm_router_unregister_endpoint(const char *provider_name,
                                    const char *model_name)
{
    router_ctx_t *ctx = router_ctx_get();

    if (!provider_name || !model_name) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: unregister_endpoint with NULL params STACK: llm_router_unregister_endpoint");
        return -1;
    }

    AGENTOS_MUTEX_LOCK(&ctx->mutex);

    for (size_t i = 0; i < ctx->endpoint_count; i++) {
        llm_endpoint_t *ep = &ctx->endpoints[i];
        if (strcmp(ep->provider_name, provider_name) == 0 &&
            strcmp(ep->model_name, model_name) == 0) {
            AGENTOS_LOG_INFO("C-L02: LLMRouter: unregistering endpoint %s/%s",
                             provider_name, model_name);
            /* 用最后一个覆盖 */
            if (i < ctx->endpoint_count - 1) {
                AGENTOS_MEMCPY(ep, &ctx->endpoints[ctx->endpoint_count - 1],
                              sizeof(llm_endpoint_t));
            }
            ctx->endpoint_count--;
            AGENTOS_MUTEX_UNLOCK(&ctx->mutex);
            return 0;
        }
    }

    AGENTOS_LOG_WARN("C-L02: LLMRouter: endpoint %s/%s not found for unregister "
                     "(total_endpoints=%zu) STACK: llm_router_unregister_endpoint",
                     provider_name, model_name, ctx->endpoint_count);
    AGENTOS_MUTEX_UNLOCK(&ctx->mutex);
    return -1;
}

/* ==================== P3.1.5: 统一路由接口 ==================== */

int llm_router_route(const llm_route_request_t *request,
                      llm_route_result_t *result)
{
    router_ctx_t *ctx = router_ctx_get();

    if (!request || !result) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: route called with NULL request or result STACK: llm_router_route");
        return -1;
    }

    if (!ctx->initialized) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: route called before initialization STACK: llm_router_route");
        return -1;
    }

    AGENTOS_MEMSET(result, 0, sizeof(llm_route_result_t));

    llm_route_strategy_t strategy = request->strategy;
    if (strategy >= LLM_ROUTE_COUNT) {
        strategy = ctx->default_strategy;
        AGENTOS_LOG_DEBUG("C-L02: LLMRouter: invalid strategy, using default=%d", strategy);
    }

    AGENTOS_LOG_DEBUG("C-L02: LLMRouter: routing request (strategy=%d, caps=0x%x, "
                      "max_cost=$%.6f, max_latency=%ums, prompt_len=%zu)",
                      strategy, request->required_caps,
                      request->max_cost, request->max_latency_ms,
                      request->prompt_len);

    int ret = -1;
    const char *strategy_name = "UNKNOWN";

    switch (strategy) {
    case LLM_ROUTE_COST:
        strategy_name = "COST_AWARE";
        ret = route_cost_aware(request, result);
        /* 如果成本路由失败，降级到轮询 */
        if (ret != 0) {
            AGENTOS_LOG_WARN("C-L02: LLMRouter: cost_aware failed, falling back to round_robin");
            ctx->stats.fallback_count++;
            AGENTOS_MEMSET(result, 0, sizeof(llm_route_result_t));
            ret = route_round_robin(request, result);
            if (ret == 0) {
                strategy_name = "COST_AWARE->ROUND_ROBIN";
            }
        }
        break;
    case LLM_ROUTE_ROUND_ROBIN:
        strategy_name = "ROUND_ROBIN";
        ret = route_round_robin(request, result);
        break;
    case LLM_ROUTE_LATENCY:
        strategy_name = "LEAST_LATENCY";
        ret = route_least_latency(request, result);
        break;
    case LLM_ROUTE_COMPLEXITY:
        strategy_name = "QUALITY_FIRST";
        ret = route_quality_first(request, result);
        break;
    case LLM_ROUTE_FALLBACK:
        strategy_name = "FALLBACK";
        /* 降级路由：尝试所有端点，返回第一个可用的 */
        ret = route_round_robin(request, result);
        if (ret != 0) {
            AGENTOS_LOG_DEBUG("C-L02: LLMRouter: fallback round_robin failed, "
                              "trying cost_aware");
            ret = route_cost_aware(request, result);
        }
        if (ret != 0) {
            AGENTOS_LOG_DEBUG("C-L02: LLMRouter: fallback cost_aware failed, "
                              "trying least_latency");
            ret = route_least_latency(request, result);
        }
        break;
    default:
        strategy_name = "DEFAULT->COST_AWARE";
        ret = route_cost_aware(request, result);
        break;
    }

    /* 更新统计 */
    AGENTOS_MUTEX_LOCK(&ctx->mutex);
    ctx->stats.total_requests++;
    if (ret == 0) {
        ctx->stats.routed_count[strategy]++;
        ctx->stats.total_cost += result->estimated_cost;
        /* 估算 token 数 */
        if (request->prompt) {
            ctx->stats.total_tokens +=
                router_estimate_tokens(request->prompt, request->prompt_len);
        }
        AGENTOS_LOG_INFO("C-L02: LLMRouter: routed via %s -> %s/%s "
                         "(cost=$%.6f, latency=%ums, confidence=%d%%)",
                         strategy_name,
                         result->provider_name, result->model_name,
                         result->estimated_cost, result->estimated_latency_ms,
                         result->confidence);
    } else {
        ctx->stats.error_count++;
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: all routing strategies failed for request "
                          "(strategy=%d, caps=0x%x, max_cost=$%.6f, "
                          "max_latency=%ums, prompt_len=%zu, "
                          "total_endpoints=%zu) STACK: llm_router_route",
                          strategy, request->required_caps,
                          request->max_cost, request->max_latency_ms,
                          request->prompt_len, ctx->endpoint_count);
    }
    AGENTOS_MUTEX_UNLOCK(&ctx->mutex);

    return ret;
}

/* ==================== 统计与配置 ==================== */

int llm_router_get_stats(llm_router_stats_t *stats)
{
    router_ctx_t *ctx = router_ctx_get();

    if (!stats) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: get_stats called with NULL stats STACK: llm_router_get_stats");
        return -1;
    }

    AGENTOS_MUTEX_LOCK(&ctx->mutex);
    AGENTOS_MEMCPY(stats, &ctx->stats, sizeof(llm_router_stats_t));
    AGENTOS_MUTEX_UNLOCK(&ctx->mutex);

    AGENTOS_LOG_DEBUG("C-L02: LLMRouter: stats queried (total=%llu, cost=$%.6f)",
                      (unsigned long long)stats->total_requests,
                      stats->total_cost);

    return 0;
}

int llm_router_set_default_strategy(llm_route_strategy_t strategy)
{
    router_ctx_t *ctx = router_ctx_get();

    if (strategy >= LLM_ROUTE_COUNT) {
        AGENTOS_LOG_ERROR("C-L02: LLMRouter: invalid strategy %d STACK: llm_router_set_default_strategy", strategy);
        return -1;
    }

    AGENTOS_MUTEX_LOCK(&ctx->mutex);
    llm_route_strategy_t old = ctx->default_strategy;
    ctx->default_strategy = strategy;
    AGENTOS_MUTEX_UNLOCK(&ctx->mutex);

    AGENTOS_LOG_INFO("C-L02: LLMRouter: default strategy changed %d -> %d",
                     old, strategy);

    return 0;
}

/* ==================== P3.1.5: 统一选择接口 ==================== */

/**
 * @brief agentos_router_select_provider — 统一提供者选择接口
 *
 * 包装 llm_router_route，提供更简洁的 API。
 * 外部调用者无需构造 llm_route_request_t 结构体。
 */
int agentos_router_select_provider(const char *prompt, size_t prompt_len,
                                    uint32_t required_caps, uint32_t max_tokens,
                                    double max_cost, uint32_t max_latency_ms,
                                    llm_route_strategy_t strategy,
                                    llm_route_result_t *result)
{
    llm_route_request_t request;
    AGENTOS_MEMSET(&request, 0, sizeof(request));

    request.prompt         = prompt;
    request.prompt_len     = prompt_len;
    request.required_caps  = required_caps;
    request.max_tokens     = max_tokens;
    request.max_cost       = max_cost;
    request.max_latency_ms = max_latency_ms;
    request.strategy       = strategy;

    return llm_router_route(&request, result);
}