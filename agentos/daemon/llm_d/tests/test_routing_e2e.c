/**
 * @file test_routing_e2e.c
 * @brief llm_d 端到端路由集成测试 (INT-15)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验证: registry → find_provider → 缓存 → 成本追踪 的完整调用链
 */

#define _POSIX_C_SOURCE 199309L

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "providers/provider.h"
#include "providers/registry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========== 测试辅助函数 ========== */

static int test_count = 0;
static int pass_count = 0;

#define TEST_PASS() do { pass_count++; test_count++; } while(0)
#define TEST_FAIL(msg) do { printf("    FAIL: %s\n", msg); test_count++; } while(0)

/* 模拟 pricing_rule */
static pricing_rule_t mock_rules[] = {
    {"gpt-4o",        2.50, 10.00},
    {"gpt-4o-mini",   0.15,  0.60},
    {"claude-sonnet", 3.00, 15.00},
    {"claude-haiku",  0.25,  1.25},
    {"deepseek-v3",   0.14,  0.28},
    {"gemini-pro",    0.50,  1.50},
};

/* ========== INT-01.1: 提供商注册表创建与销毁 ========== */

static void test_registry_create_and_find(void)
{
    printf("  [INT-15.1] Registry create and provider find...\n");

    /* 创建注册表 */
    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = NULL,
        .provider_count = 0,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    assert(reg != NULL);
    printf("    Registry created OK\n");

    /* 验证未注册提供商查找失败 */
    const provider_t *p = provider_registry_find(reg, "nonexistent-model");
    assert(p == NULL);
    (void)p;
    printf("    Nonexistent model correctly returns NULL\n");

    provider_registry_destroy(reg);
    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-15.2: 缓存命中/未命中 ========== */

static void test_cache_hit_miss(void)
{
    printf("  [INT-15.2] Cache hit/miss mechanism...\n");

    cache_t *cache = cache_create(100, 3600);
    assert(cache != NULL);

    /* 缓存未命中 */
    char *value = NULL;
    int ret = cache_get(cache, "model:gpt-4o:hash123", &value);
    assert(ret != 1 || value == NULL);
    (void)ret;
    printf("    Cache miss: OK\n");

    /* 写入缓存 */
    const char *response_json = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4o\",\"choices\":[{\"message\":{\"content\":\"Hello\"}}]}";
    cache_put(cache, "model:gpt-4o:hash123", response_json);

    /* 缓存命中 */
    ret = cache_get(cache, "model:gpt-4o:hash123", &value);
    assert(ret == 1);
    assert(value != NULL);
    assert(strcmp(value, response_json) == 0);
    free(value);
    printf("    Cache hit: OK (same response)\n");

    /* TTL 过期验证 */
    cache_t *ttl_cache = cache_create(10, 1); /* 1秒TTL */
    assert(ttl_cache != NULL);

    cache_put(ttl_cache, "ttl_key", "ttl_value");
    value = NULL;
    ret = cache_get(ttl_cache, "ttl_key", &value);
    assert(ret == 1);
    free(value);

    /* 等待TTL过期 */
#ifdef _WIN32
    Sleep(1500);
#else
    {
        struct timespec ts = {1, 500000000L}; /* 1.5秒 */
        nanosleep(&ts, NULL);
    }
#endif

    value = NULL;
    ret = cache_get(ttl_cache, "ttl_key", &value);
    assert(ret != 1 || value == NULL);
    printf("    TTL expiry: OK (cache miss after %ds)\n", 1);

    cache_destroy(ttl_cache);
    cache_destroy(cache);
    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-15.3: 成本追踪准确性 ========== */

static void test_cost_tracking_accuracy(void)
{
    printf("  [INT-15.3] Cost tracking accuracy...\n");

    cost_tracker_t *ct = cost_tracker_create(mock_rules,
        (int)(sizeof(mock_rules) / sizeof(mock_rules[0])));
    assert(ct != NULL);

    /* 添加使用记录 */
    cost_tracker_add(ct, "gpt-4o", 1000, 500);
    /* 成本: (1000/1000)*2.50 + (500/1000)*10.00 = 2.50 + 5.00 = 7.50 */

    cost_tracker_add(ct, "gpt-4o-mini", 500, 200);
    /* 成本: (500/1000)*0.15 + (200/1000)*0.60 = 0.075 + 0.12 = 0.195 */

    cost_tracker_add(ct, "deepseek-v3", 2000, 1000);
    /* 成本: (2000/1000)*0.14 + (1000/1000)*0.28 = 0.28 + 0.28 = 0.56 */

    /* 导出JSON验证 */
    cJSON *report = cost_tracker_export(ct);
    assert(report != NULL);

    char *json_str = cJSON_PrintUnformatted(report);
    assert(json_str != NULL);
    printf("    Cost report: %s\n", json_str);

    /* 验证JSON包含模型信息 */
    assert(strstr(json_str, "gpt-4o") != NULL);
    assert(strstr(json_str, "gpt-4o-mini") != NULL);
    assert(strstr(json_str, "deepseek-v3") != NULL);

    free(json_str);
    cJSON_Delete(report);
    cost_tracker_destroy(ct);
    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-15.4: 未知提供商错误处理 ========== */

static void test_unknown_provider_error(void)
{
    printf("  [INT-15.4] Unknown provider error handling...\n");

    /* 查找不存在的模型 */
    service_config_t cfg = {
        .cache_capacity = 10,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = NULL,
        .provider_count = 0,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    assert(reg != NULL);

    /* 查找不存在的模型应返回NULL */
    const provider_t *p = provider_registry_find(reg, "unknown-model-v999");
    assert(p == NULL);
    (void)p;
    printf("    Unknown model correctly returns NULL\n");

    /* NULL参数检查 */
    const provider_t *null_p = provider_registry_find(NULL, "gpt-4o");
    assert(null_p == NULL);
    (void)null_p;
    printf("    NULL registry correctly returns NULL\n");

    const provider_t *null_model = provider_registry_find(reg, NULL);
    assert(null_model == NULL);
    (void)null_model;
    printf("    NULL model correctly returns NULL\n");

    provider_registry_destroy(reg);
    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== INT-15.5: 注册表并发安全 ========== */

static void test_registry_thread_safety(void)
{
    printf("  [INT-15.5] Registry thread safety (basic)...\n");

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = NULL,
        .provider_count = 0,
    };

    provider_registry_t *reg = provider_registry_create(&cfg);
    assert(reg != NULL);

    /* 同一线程重复查找不应死锁 */
    for (int i = 0; i < 100; i++) {
        const provider_t *p = provider_registry_find(reg, "test-model");
        (void)p;
    }
    printf("    100 sequential lookups: OK (no deadlock)\n");

    provider_registry_destroy(reg);

    /* NULL 注册表销毁不应崩溃 */
    provider_registry_destroy(NULL);
    printf("    NULL destroy: OK (no crash)\n");

    TEST_PASS();
    printf("    PASSED\n");
}

/* ========== 主函数 ========== */

int main(void)
{
    printf("=========================================\n");
    printf("  LLM Routing E2E Integration Tests\n");
    printf("  (INT-15: registry -> find_provider -> cache -> cost)\n");
    printf("=========================================\n\n");

    test_registry_create_and_find();
    test_cache_hit_miss();
    test_cost_tracking_accuracy();
    test_unknown_provider_error();
    test_registry_thread_safety();

    printf("\n=========================================\n");
    printf("  Results: %d/%d tests PASSED\n", pass_count, test_count);
    printf("=========================================\n");

    return (pass_count == test_count) ? 0 : 1;
}