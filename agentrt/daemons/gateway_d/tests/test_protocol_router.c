/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file test_protocol_router.c
 * @brief 网关协议路由器统计计数验证测试
 *
 * 验证 record_proto_stats 重构后，gw_proto_router_route 的统计逻辑正确。
 * 覆盖场景：
 *   - 5 种协议类型（MCP / A2A / OpenAI / JSON-RPC / Unknown）在"handler 存在"和"handler 不存在"
 *     两条路径下的统计计数
 *   - total_requests / route_errors 全局计数器
 */

#include "gateway_protocol_router.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 当独立编译时，error.h 可能未通过头文件链传递到本文件 */
#ifndef AGENTRT_ERR_NOT_FOUND
#define AGENTRT_ERR_NOT_FOUND       (-6)
#endif
#ifndef AGENTRT_ERR_INVALID_PARAM
#define AGENTRT_ERR_INVALID_PARAM   (-2)
#endif

/* ==================== Mock handler ==================== */

static int mock_handler(const char *method, const char *path,
                        const char *body_json, char **response_json,
                        void *user_data)
{
    (void)method;
    (void)path;
    (void)body_json;
    (void)user_data;

    /* 返回成功，让调用者可以观察 route_errors 不变 */
    if (response_json)
        *response_json = NULL;
    return 0;
}

/* ==================== 测试用例 ==================== */

/* 协议类型枚举值 — 与头文件 GW_PROTO_DETECT_xxx 对齐 */
static const int TEST_PROTO_TYPES[] = {
    GW_PROTO_DETECT_MCP,      /* 1 */
    GW_PROTO_DETECT_A2A,      /* 2 */
    GW_PROTO_DETECT_OPENAI,   /* 3 */
    GW_PROTO_DETECT_JSONRPC,  /* 4 */
    GW_PROTO_DETECT_UNKNOWN   /* 0 — default 分支 */
};
static const int NUM_PROTO_TYPES = 5;

static const char *proto_label(gw_proto_detect_result_t t)
{
    switch (t) {
    case GW_PROTO_DETECT_MCP:     return "MCP";
    case GW_PROTO_DETECT_A2A:     return "A2A";
    case GW_PROTO_DETECT_OPENAI:  return "OpenAI";
    case GW_PROTO_DETECT_JSONRPC: return "JSON-RPC";
    default:                      return "Unknown";
    }
}

/**
 * @brief 测试场景A: handler 存在时的统计计数
 *
 * 注册 mock handler → 对每种协议调用 route → 验证各计数器为 1
 */
static void test_stats_with_handler(void)
{
    printf("\n--- Scenario A: route with handler ---\n");

    gw_proto_router_t *router = gw_proto_router_create();
    assert(router != NULL);

    /* 注册 5 种协议的 mock handler */
    for (int i = 0; i < NUM_PROTO_TYPES; i++) {
        gw_proto_detect_result_t pt = (gw_proto_detect_result_t)TEST_PROTO_TYPES[i];
        int rc = gw_proto_router_register(router, pt, mock_handler, NULL);
        assert(rc == 0);
        (void)rc;
        printf("  registered handler for %s\n", proto_label(pt));
    }

    /* 对每种协议各路由一次 */
    for (int i = 0; i < NUM_PROTO_TYPES; i++) {
        gw_proto_detect_result_t pt = (gw_proto_detect_result_t)TEST_PROTO_TYPES[i];
        char *resp = NULL;
        int rc = gw_proto_router_route(router, pt, "GET", "/test", "{}", &resp);
        /* Unknown 没有 handler，但注册了，所以应该成功 */
        printf("  route(%s) => %d\n", proto_label(pt), rc);
    }

    /* 读取统计 */
    gw_proto_router_stats_t stats;
    int rc = gw_proto_router_get_stats(router, &stats);
    assert(rc == 0);
    (void)rc;

    printf("  total_requests=%llu\n", (unsigned long long)stats.total_requests);
    assert(stats.total_requests == (uint64_t)NUM_PROTO_TYPES && "total_requests should be 5");

    /* 各协议计数为 1 */
    assert(stats.mcp_requests      == 1 && "mcp_requests should be 1");
    assert(stats.a2a_requests      == 1 && "a2a_requests should be 1");
    assert(stats.openai_requests   == 1 && "openai_requests should be 1");
    assert(stats.jsonrpc_requests  == 1 && "jsonrpc_requests should be 1");
    assert(stats.unknown_requests  == 1 && "unknown_requests should be 1");
    assert(stats.route_errors      == 0 && "route_errors should be 0 (handler found)");

    printf("  >>> PASSED\n");

    gw_proto_router_destroy(router);
}

/**
 * @brief 测试场景B: handler 不存在的统计计数
 *
 * 不注册任何 handler → 每次 route 应走 if(!handler) 分支 →
 * route_errors 递增 + proto 计数器递增
 */
static void test_stats_without_handler(void)
{
    printf("\n--- Scenario B: route without handler ---\n");

    gw_proto_router_t *router = gw_proto_router_create();
    assert(router != NULL);

    /* 不注册 handler，直接路由 */
    const int N = 3; /* 对每种协议调 3 次 */
    for (int i = 0; i < NUM_PROTO_TYPES; i++) {
        gw_proto_detect_result_t pt = (gw_proto_detect_result_t)TEST_PROTO_TYPES[i];
        for (int j = 0; j < N; j++) {
            char *resp = NULL;
            int rc = gw_proto_router_route(router, pt, "GET", "/test", "{}", &resp);
            assert(rc == AGENTRT_ERR_NOT_FOUND);
            (void)rc;
        }
    }

    gw_proto_router_stats_t stats;
    gw_proto_router_get_stats(router, &stats);

    printf("  total_requests=%llu\n", (unsigned long long)stats.total_requests);
    printf("  route_errors=%llu\n", (unsigned long long)stats.route_errors);

    assert(stats.total_requests == (uint64_t)(NUM_PROTO_TYPES * N) && "total=15");
    assert(stats.route_errors   == (uint64_t)(NUM_PROTO_TYPES * N) && "errors=15");

    assert(stats.mcp_requests      == (uint64_t)N && "mcp=3");
    assert(stats.a2a_requests      == (uint64_t)N && "a2a=3");
    assert(stats.openai_requests   == (uint64_t)N && "openai=3");
    assert(stats.jsonrpc_requests  == (uint64_t)N && "jsonrpc=3");
    assert(stats.unknown_requests  == (uint64_t)N && "unknown=3");

    printf("  >>> PASSED\n");

    gw_proto_router_destroy(router);
}

/**
 * @brief 测试场景C: 混合场景 — handler 存在 + 不存在混合
 *
 * 只注册 MCP 和 A2A 的 handler，验证:
 *   MCP/A2A 走正常路径，route_errors=0
 *   OpenAI/JSONRPC/Unknown 走 404 路径，route_errors 递增
 */
static void test_stats_mixed(void)
{
    printf("\n--- Scenario C: mixed (MCP+A2A registered, others unregistered) ---\n");

    gw_proto_router_t *router = gw_proto_router_create();
    assert(router != NULL);

    /* 只注册 MCP 和 A2A */
    gw_proto_router_register(router, GW_PROTO_DETECT_MCP, mock_handler, NULL);
    gw_proto_router_register(router, GW_PROTO_DETECT_A2A, mock_handler, NULL);

    /* 各路由一次 */
    for (int i = 0; i < NUM_PROTO_TYPES; i++) {
        gw_proto_detect_result_t pt = (gw_proto_detect_result_t)TEST_PROTO_TYPES[i];
        char *resp = NULL;
        gw_proto_router_route(router, pt, "GET", "/test", "{}", &resp);
    }

    gw_proto_router_stats_t stats;
    gw_proto_router_get_stats(router, &stats);

    printf("  mcp_requests=%llu  a2a_requests=%llu  openai_requests=%llu  "
           "jsonrpc_requests=%llu  unknown_requests=%llu\n",
           (unsigned long long)stats.mcp_requests,
           (unsigned long long)stats.a2a_requests,
           (unsigned long long)stats.openai_requests,
           (unsigned long long)stats.jsonrpc_requests,
           (unsigned long long)stats.unknown_requests);
    printf("  route_errors=%llu\n", (unsigned long long)stats.route_errors);

    assert(stats.total_requests   == (uint64_t)NUM_PROTO_TYPES && "total=5");
    assert(stats.mcp_requests     == 1);
    assert(stats.a2a_requests     == 1);
    assert(stats.openai_requests  == 1);
    assert(stats.jsonrpc_requests == 1);
    assert(stats.unknown_requests == 1);
    /* MCP 和 A2A 有 handler，OpenAI/JSONRPC/Unknown 没有 → 3 个错误 */
    assert(stats.route_errors     == 3 && "3 unregistered = 3 errors");

    printf("  >>> PASSED\n");

    gw_proto_router_destroy(router);
}

/** @brief 验证空指针入参保护 */
static void test_invalid_params(void)
{
    printf("\n--- Scenario D: invalid params ---\n");

    gw_proto_router_t *router = gw_proto_router_create();
    assert(router != NULL);

    int rc;

    rc = gw_proto_router_route(NULL, GW_PROTO_DETECT_MCP, "GET", "/t", "{}", NULL);
    assert(rc == AGENTRT_ERR_INVALID_PARAM);

    rc = gw_proto_router_route(router, GW_PROTO_DETECT_MCP, NULL, "/t", "{}", NULL);
    assert(rc == AGENTRT_ERR_INVALID_PARAM);
    (void)rc;

    printf("  >>> PASSED\n");

    gw_proto_router_destroy(router);
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("\n=== Gateway Protocol Router Stats Tests ===\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("Test file: %s\n\n", __FILE__);

    test_invalid_params();
    test_stats_with_handler();
    test_stats_without_handler();
    test_stats_mixed();

    printf("\n=== All tests passed ===\n\n");
    return 0;
}