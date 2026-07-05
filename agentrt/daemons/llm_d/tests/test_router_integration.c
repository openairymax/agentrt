/**
 * @file test_router_integration.c
 * @brief P3.16 (ACC-DT17): llm_router 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验证 ACC-DT17 验收标准：llm_router 被集成且路由策略生效。
 *
 * 测试内容：
 *   1. COST 策略选择成本最低的端点
 *   2. ROUND_ROBIN 策略在端点间轮询（负载均衡）
 *   3. service 生命周期集成 — create 初始化 router、destroy 销毁 router、
 *      router 在 service 生命周期内可用且 total_requests 递增
 *
 * 每个测试为独立可执行进程内的函数，通过 init/destroy 保证全局单例隔离。
 *
 * @note 不使用 assert() 执行副作用操作：Release 构建类型定义 NDEBUG，会将
 *       assert(expr) 展开为 ((void)0)，导致 expr 中的函数调用（init/register/
 *       get_stats）根本不执行。所有副作用操作必须用显式 if 检查 + TEST_FAIL。
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "llm_service.h"
#include "router/llm_router.h"

#include <stdio.h>
#include <string.h>

/* ========== 测试统计 ========== */

static int test_count = 0;
static int pass_count = 0;

#define TEST_PASS() do { pass_count++; test_count++; } while (0)
#define TEST_FAIL(msg) do { printf("    FAIL: %s\n", msg); test_count++; } while (0)

/* ========== 辅助：构建端点与请求 ========== */

static llm_endpoint_t make_endpoint(const char *provider, const char *model,
                                    double cost_in, double cost_out,
                                    uint32_t latency_ms, uint32_t caps)
{
    llm_endpoint_t ep;
    memset(&ep, 0, sizeof(ep));
    snprintf(ep.provider_name, sizeof(ep.provider_name), "%s", provider);
    snprintf(ep.model_name, sizeof(ep.model_name), "%s", model);
    ep.enabled = true;
    ep.priority = 0;
    ep.context_window = 8192;
    ep.cost_per_1k_input = cost_in;
    ep.cost_per_1k_output = cost_out;
    ep.avg_latency_ms = latency_ms;
    ep.capabilities = caps;
    return ep;
}

static llm_route_request_t make_request(llm_route_strategy_t strategy,
                                        uint32_t required_caps,
                                        const char *prompt,
                                        uint32_t max_tokens)
{
    llm_route_request_t req;
    memset(&req, 0, sizeof(req));
    req.prompt = prompt ? prompt : "";
    req.prompt_len = prompt ? strlen(prompt) : 0;
    req.required_caps = required_caps;
    req.max_tokens = max_tokens;
    req.max_cost = 0;          /* 不限预算 */
    req.max_latency_ms = 0;    /* 不限延迟 */
    req.strategy = strategy;
    req.preferred_provider[0] = '\0';
    return req;
}

/* ========== TEST 1: COST 策略选最便宜端点 ========== */

static void test_cost_strategy_selects_cheapest(void)
{
    printf("  [ACC-DT17.1] COST strategy selects cheapest endpoint...\n");

    /* 显式 if 检查（非 assert）— Release/NDEBUG 下 assert 会被消除 */
    if (llm_router_init(NULL) != 0) {
        TEST_FAIL("llm_router_init failed");
        return;
    }

    /* 注册 3 个端点，能力相同但成本递减：
     *   gpt-4o     : 0.030   / 0.060    (最贵)
     *   gpt-3.5    : 0.001   / 0.002    (中等)
     *   deepseek   : 0.00014 / 0.00028  (最便宜) */
    const uint32_t caps = LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING;
    llm_endpoint_t e1 = make_endpoint("openai",   "gpt-4o",      0.030,   0.060,   1200, caps);
    llm_endpoint_t e2 = make_endpoint("openai",   "gpt-3.5",     0.001,   0.002,   1000, caps);
    llm_endpoint_t e3 = make_endpoint("deepseek", "deepseek-v3", 0.00014, 0.00028,  900, caps);
    if (llm_router_register_endpoint(&e1) != 0 ||
        llm_router_register_endpoint(&e2) != 0 ||
        llm_router_register_endpoint(&e3) != 0) {
        TEST_FAIL("register_endpoint failed");
        llm_router_destroy();
        return;
    }

    /* 路由前读取统计，确认 router 已就绪且 total_requests==0 */
    llm_router_stats_t pre_stats;
    if (llm_router_get_stats(&pre_stats) != 0) {
        TEST_FAIL("llm_router_get_stats failed (pre-route)");
        llm_router_destroy();
        return;
    }
    if (pre_stats.total_requests != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "expected pre-route total_requests==0, got %llu",
                 (unsigned long long)pre_stats.total_requests);
        TEST_FAIL(msg);
        llm_router_destroy();
        return;
    }

    llm_route_request_t req = make_request(LLM_ROUTE_COST,
                                           LLM_CAP_CHAT | LLM_CAP_COMPLETION,
                                           "hello world this is a routing test", 100);
    llm_route_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = llm_router_route(&req, &result);
    if (rc != 0) {
        TEST_FAIL("COST route returned non-zero");
        llm_router_destroy();
        return;
    }

    printf("    COST routed -> %s/%s (cost=$%.6f, strategy=%d)\n",
           result.provider_name, result.model_name,
           result.estimated_cost, (int)result.strategy_used);

    /* 主验证点：COST 策略应选最便宜的 deepseek-v3 */
    if (strcmp(result.model_name, "deepseek-v3") != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "expected deepseek-v3, got %s", result.model_name);
        TEST_FAIL(msg);
        llm_router_destroy();
        return;
    }

    /* 额外验证：统计已记录至少 1 次请求 */
    llm_router_stats_t stats;
    if (llm_router_get_stats(&stats) != 0) {
        TEST_FAIL("llm_router_get_stats failed (post-route)");
        llm_router_destroy();
        return;
    }
    if (stats.total_requests < 1) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "expected post-route total_requests>=1, got %llu",
                 (unsigned long long)stats.total_requests);
        TEST_FAIL(msg);
        llm_router_destroy();
        return;
    }

    TEST_PASS();
    printf("    PASSED (cheapest endpoint selected, total_requests=%llu)\n",
           (unsigned long long)stats.total_requests);

    llm_router_destroy();
}

/* ========== TEST 2: ROUND_ROBIN 轮询 ========== */

static void test_round_robin_cycles(void)
{
    printf("  [ACC-DT17.2] ROUND_ROBIN cycles through endpoints...\n");

    if (llm_router_init(NULL) != 0) {
        TEST_FAIL("llm_router_init failed");
        return;
    }

    const uint32_t caps = LLM_CAP_CHAT | LLM_CAP_COMPLETION;
    llm_endpoint_t ea = make_endpoint("openai",    "model-a", 0.001, 0.002, 1000, caps);
    llm_endpoint_t eb = make_endpoint("anthropic", "model-b", 0.002, 0.004, 1100, caps);
    if (llm_router_register_endpoint(&ea) != 0 ||
        llm_router_register_endpoint(&eb) != 0) {
        TEST_FAIL("register_endpoint failed");
        llm_router_destroy();
        return;
    }

    /* 路由前确认 router 就绪 */
    llm_router_stats_t pre_stats;
    if (llm_router_get_stats(&pre_stats) != 0) {
        TEST_FAIL("llm_router_get_stats failed (pre-route)");
        llm_router_destroy();
        return;
    }
    if (pre_stats.total_requests != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "expected pre-route total_requests==0, got %llu",
                 (unsigned long long)pre_stats.total_requests);
        TEST_FAIL(msg);
        llm_router_destroy();
        return;
    }

    llm_route_request_t req = make_request(LLM_ROUTE_ROUND_ROBIN, caps, "rotate", 50);

    /* 路由 4 次，收集 model 名（独立进程内 round_robin_index 从 0 开始） */
    char models[4][64];
    for (int i = 0; i < 4; i++) {
        llm_route_result_t result;
        memset(&result, 0, sizeof(result));
        int rc = llm_router_route(&req, &result);
        if (rc != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "ROUND_ROBIN route %d returned non-zero", i);
            TEST_FAIL(msg);
            llm_router_destroy();
            return;
        }
        snprintf(models[i], sizeof(models[i]), "%s", result.model_name);
        printf("    round %d -> %s/%s\n", i, result.provider_name, result.model_name);
    }

    /* 验证轮询：model[0]==model[2], model[1]==model[3], model[0]!=model[1] */
    if (strcmp(models[0], models[2]) == 0 &&
        strcmp(models[1], models[3]) == 0 &&
        strcmp(models[0], models[1]) != 0) {
        TEST_PASS();
        printf("    PASSED (alternated %s <-> %s)\n", models[0], models[1]);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "expected alternation, got %s,%s,%s,%s",
                 models[0], models[1], models[2], models[3]);
        TEST_FAIL(msg);
    }

    llm_router_destroy();
}

/* ========== TEST 3: service 生命周期集成 ========== */

static void test_service_lifecycle_integration(void)
{
    printf("  [ACC-DT17.3] service lifecycle integrates router...\n");

    /* llm_service_create 内部调用 llm_router_init (见 service.c P3.16 ACC-DT17) */
    llm_service_t *svc = llm_service_create(NULL);
    if (svc == NULL) {
        TEST_FAIL("llm_service_create returned NULL");
        return;
    }

    /* create 后 router 应已初始化，stats 可访问且 total_requests==0
     * （空 registry → register_router_endpoints 注册 0 个端点） */
    llm_router_stats_t stats;
    if (llm_router_get_stats(&stats) != 0) {
        TEST_FAIL("llm_router_get_stats failed after service create");
        llm_service_destroy(svc);
        return;
    }
    if (stats.total_requests != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "expected total_requests==0 after create, got %llu",
                 (unsigned long long)stats.total_requests);
        TEST_FAIL(msg);
        llm_service_destroy(svc);
        return;
    }
    printf("    after create: router stats accessible, total_requests=0\n");

    /* 注册一个端点并路由，验证 router 在 service 生命周期内可用且 total_requests 递增 */
    const uint32_t caps = LLM_CAP_CHAT | LLM_CAP_COMPLETION;
    llm_endpoint_t ep = make_endpoint("openai", "gpt-4o", 0.03, 0.06, 1200, caps);
    if (llm_router_register_endpoint(&ep) != 0) {
        TEST_FAIL("register_endpoint failed within service lifecycle");
        llm_service_destroy(svc);
        return;
    }

    llm_route_request_t req = make_request(LLM_ROUTE_COST, caps, "integration probe", 100);
    llm_route_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = llm_router_route(&req, &result);
    if (rc != 0) {
        TEST_FAIL("router_route failed within service lifecycle");
        llm_service_destroy(svc);
        return;
    }

    /* 验证 total_requests 递增（显式 if 检查 — 非 assert） */
    if (llm_router_get_stats(&stats) != 0) {
        TEST_FAIL("llm_router_get_stats failed after route");
        llm_service_destroy(svc);
        return;
    }
    if (stats.total_requests >= 1) {
        printf("    after route: total_requests=%llu (incremented)\n",
               (unsigned long long)stats.total_requests);
        TEST_PASS();
        printf("    PASSED (router live within service lifecycle)\n");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "total_requests did not increment (got %llu)",
                 (unsigned long long)stats.total_requests);
        TEST_FAIL(msg);
    }

    /* llm_service_destroy 内部调用 llm_router_destroy (见 service.c P3.16 ACC-DT17) */
    llm_service_destroy(svc);
}

/* ========== 主函数 ========== */

int main(void)
{
    printf("=========================================\n");
    printf("  P3.16 (ACC-DT17): llm_router Integration Tests\n");
    printf("=========================================\n\n");

    test_cost_strategy_selects_cheapest();
    test_round_robin_cycles();
    test_service_lifecycle_integration();

    printf("\n=========================================\n");
    printf("  Results: %d/%d tests PASSED\n", pass_count, test_count);
    printf("=========================================\n");

    return (pass_count == test_count) ? 0 : 1;
}
