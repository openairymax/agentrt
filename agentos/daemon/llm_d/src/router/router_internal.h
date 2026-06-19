/**
 * @file router_internal.h
 * @brief 路由器内部接口 — 各路由策略函数声明
 *
 * 由 llm_router.c（编排器）调用各路由策略实现。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_LLM_ROUTER_INTERNAL_H
#define AGENTOS_LLM_ROUTER_INTERNAL_H

#include "router/llm_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── P3.1.1: 成本感知路由 ── */
int route_cost_aware(const llm_route_request_t *request,
                      llm_route_result_t *result);

/* ── P3.1.2: 轮询路由 ── */
int route_round_robin(const llm_route_request_t *request,
                       llm_route_result_t *result);

/* ── P3.1.3: 最低延迟路由 ── */
int route_least_latency(const llm_route_request_t *request,
                         llm_route_result_t *result);

/* ── P3.1.4: 质量优先路由 ── */
int route_quality_first(const llm_route_request_t *request,
                         llm_route_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_ROUTER_INTERNAL_H */