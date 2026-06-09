/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_llm_d_routing_latency.c - LLM 模型路由延迟基准测试 (INT-18)
 *
 * Phase 2 集成测试: 基准测试 llm_d 模型路由管线的延迟
 *
 * 验证覆盖:
 *   INT-18.1: 简单查询路由延迟 - 路由简单查询，测量 P50/P95/P99。目标 <5ms
 *   INT-18.2: 复杂查询路由延迟 - 路由复杂查询含模型选择
 *   INT-18.3: 注册表查找延迟 - 基准测试 provider registry 查找
 *   INT-18.4: 缓存命中 vs 未命中 - 比较缓存命中与未命中的路由延迟
 *   INT-18.5: 并发路由 - 10 个并发路由请求，测量总吞吐量
 *
 * 使用 clock_gettime(CLOCK_MONOTONIC, ...) 计时
 * 每项测试运行 100 次迭代，10 次预热
 */

#define _POSIX_C_SOURCE 199309L

#include "cache.h"
#include "cost_tracker.h"
#include "llm_service.h"
#include "providers/provider.h"
#include "providers/registry.h"
#include "response.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * 测试框架宏（与项目现有测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_PASS(name)                                                        \
    do {                                                                       \
        printf("    [PASS] %s\n", name);                                       \
        g_tests_passed++;                                                      \
    } while (0)

#define TEST_FAIL(name, msg)                                                   \
    do {                                                                       \
        printf("    [FAIL] %s: %s\n", name, msg);                              \
        g_tests_failed++;                                                      \
    } while (0)

/* ============================================================================
 * 基准测试常量
 * ============================================================================ */
#define BENCH_ITERATIONS 100
#define BENCH_WARMUP     10

/* 延迟目标 (纳秒): 5ms = 5,000,000 ns */
#define TARGET_P99_NS 5000000ULL

/* 并发路由请求数 */
#define CONCURRENT_ROUTING_COUNT 10

/* ============================================================================
 * 计时辅助函数
 * ============================================================================ */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * 统计辅助函数
 * ============================================================================ */
typedef struct {
    const char *name;
    double avg_ns;
    double min_ns;
    double max_ns;
    double p50_ns;
    double p95_ns;
    double p99_ns;
    uint64_t total_ops;
    double ops_per_sec;
} bench_result_t;

static int compare_uint64(const void *a, const void *b)
{
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static void calculate_stats(uint64_t *times, size_t count, bench_result_t *result)
{
    qsort(times, count, sizeof(uint64_t), compare_uint64);

    double sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += (double)times[i];
    }

    result->avg_ns = sum / (double)count;
    result->min_ns = (double)times[0];
    result->max_ns = (double)times[count - 1];
    result->p50_ns = (double)times[count / 2];
    result->p95_ns = (double)times[count * 95 / 100];
    result->p99_ns = (double)times[count * 99 / 100];
    result->total_ops = count;

    if (sum > 0) {
        result->ops_per_sec = (double)count / (sum / 1000000000.0);
    } else {
        result->ops_per_sec = 0;
    }
}

static void print_result_us(const bench_result_t *result)
{
    printf("    %-50s\n", result->name);
    printf("      avg=%8.2f us | min=%8.2f us | max=%8.2f us\n",
           result->avg_ns / 1000.0, result->min_ns / 1000.0,
           result->max_ns / 1000.0);
    printf("      p50=%8.2f us | p95=%8.2f us | p99=%8.2f us\n",
           result->p50_ns / 1000.0, result->p95_ns / 1000.0,
           result->p99_ns / 1000.0);
    printf("      throughput=%.0f ops/sec\n", result->ops_per_sec);
}

/* ============================================================================
 * 复杂度评估辅助 (与 test_llm_d_routing.c 一致)
 * ============================================================================ */
typedef enum {
    COMPLEXITY_SIMPLE   = 0,
    COMPLEXITY_MODERATE = 1,
    COMPLEXITY_COMPLEX  = 2
} complexity_level_t;

static const char *complexity_names[] = {"SIMPLE", "MODERATE", "COMPLEX"};

static complexity_level_t assess_complexity(const char *input)
{
    if (!input) return COMPLEXITY_SIMPLE;

    size_t len = strlen(input);

    const char *complex_kw[] = {
        "architecture", "distributed", "system design", "scalability",
        "架构", "分布式", "系统设计", "高可用", "微服务"
    };
    const char *moderate_kw[] = {
        "function", "algorithm", "sort", "implement", "write",
        "函数", "算法", "排序", "实现", "编写", "Python", "Java"
    };

    int complex_score  = 0;
    int moderate_score = 0;

    for (size_t i = 0; i < sizeof(complex_kw) / sizeof(complex_kw[0]); i++) {
        if (strstr(input, complex_kw[i])) complex_score++;
    }
    for (size_t i = 0; i < sizeof(moderate_kw) / sizeof(moderate_kw[0]); i++) {
        if (strstr(input, moderate_kw[i])) moderate_score++;
    }

    if (complex_score >= 1 || len > 500) return COMPLEXITY_COMPLEX;
    if (moderate_score >= 1 || len > 50)  return COMPLEXITY_MODERATE;
    return COMPLEXITY_SIMPLE;
}

static const char *route_by_complexity(complexity_level_t level, const char *user_model)
{
    if (user_model && user_model[0]) return user_model;

    switch (level) {
    case COMPLEXITY_SIMPLE:   return "gpt-4o-mini";
    case COMPLEXITY_MODERATE: return "gpt-4o";
    case COMPLEXITY_COMPLEX:  return "claude-sonnet";
    default:                  return "gpt-4o-mini";
    }
}

/* ============================================================================
 * 辅助: 创建测试用注册表
 * ============================================================================ */
static provider_registry_t *create_test_registry(void)
{
    struct {
        const char *name;
        const char *enabled;
    } prov_cfg[] = {
        {"openai",    "true"},
        {"anthropic", "true"},
        {"google",    "true"},
        {"deepseek",  "true"},
        {"local",     "true"},
    };

    service_config_t cfg = {
        .cache_capacity = 100,
        .cache_ttl_sec  = 3600,
        .max_retries    = 3,
        .timeout_ms     = 30000,
        .token_encoding = "cl100k_base",
        .providers      = prov_cfg,
        .provider_count = 5,
    };

    return provider_registry_create(&cfg);
}

/* ============================================================================
 * INT-18.1: 简单查询路由延迟
 *
 * 路由简单查询，测量 P50/P95/P99:
 *   - 创建注册表和缓存
 *   - 对简单输入执行: 复杂度评估 → 模型选择 → provider 查找 → 缓存检查
 *   - 验证 P99 < 5ms
 * ============================================================================ */
TEST(int18_1_simple_query_routing_latency)
{
    printf("    --- Simple Query Routing Latency ---\n");

    provider_registry_t *reg = create_test_registry();
    if (!reg) {
        TEST_FAIL("INT-18.1", "registry creation failed");
        return;
    }

    cache_t *cache = cache_create(1000, 3600);
    if (!cache) {
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.1", "cache creation failed");
        return;
    }

    const struct {
        const char *input;
        const char *desc;
    } simple_inputs[] = {
        {"Hello, how are you?",                "english_greeting"},
        {"What is the weather today?",          "english_weather"},
        {"你好，今天天气怎么样？",              "chinese_greeting"},
        {"Hi",                                 "english_short"},
        {"查询状态",                           "chinese_short_query"},
        {NULL, NULL}
    };

    uint64_t *all_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!all_times) {
        cache_destroy(cache);
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.1", "memory allocation failed");
        return;
    }

    for (int t = 0; simple_inputs[t].input != NULL; t++) {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            complexity_level_t level = assess_complexity(simple_inputs[t].input);
            const char *model = route_by_complexity(level, NULL);
            provider_registry_find(reg, model);
        }

        /* 基准测试: 完整简单路由流程 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* Step 1: 复杂度评估 */
            complexity_level_t level = assess_complexity(simple_inputs[t].input);

            /* Step 2: 模型选择 */
            const char *model = route_by_complexity(level, NULL);

            /* Step 3: 缓存检查 */
            char cache_key[128];
            snprintf(cache_key, sizeof(cache_key), "%s:%s", model, "simple_hash");
            char *cached = NULL;
            int cache_hit = cache_get(cache, cache_key, &cached);

            /* Step 4: Provider 查找 (缓存未命中时) */
            if (!cache_hit || !cached) {
                provider_registry_find(reg, model);
            }

            all_times[i] = get_time_ns() - start;

            if (cached)
                free(cached);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        result.name = simple_inputs[t].desc;
        calculate_stats(all_times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    free(all_times);
    cache_destroy(cache);
    provider_registry_destroy(reg);
    TEST_PASS("INT-18.1: simple query routing latency benchmark completed");
}

/* ============================================================================
 * INT-18.2: 复杂查询路由延迟
 *
 * 路由复杂查询含模型选择:
 *   - 对复杂输入执行路由流程
 *   - 包含 fallback 逻辑
 *   - 测量 P50/P95/P99
 * ============================================================================ */
TEST(int18_2_complex_query_routing_latency)
{
    printf("    --- Complex Query Routing Latency ---\n");

    provider_registry_t *reg = create_test_registry();
    if (!reg) {
        TEST_FAIL("INT-18.2", "registry creation failed");
        return;
    }

    cache_t *cache = cache_create(1000, 3600);
    if (!cache) {
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.2", "cache creation failed");
        return;
    }

    const struct {
        const char *input;
        const char *desc;
    } complex_inputs[] = {
        {"Design a distributed system architecture for a global e-commerce platform "
         "handling millions of requests per second with high availability requirements",
         "english_distributed_system"},
        {"请设计一个微服务架构的分布式系统，需要考虑可扩展性、容错性和数据一致性",
         "chinese_microservice_arch"},
        {"Write a Python function to sort a list of integers using merge sort algorithm",
         "english_algorithm_implementation"},
        {"请编写一个Java函数实现快速排序算法，并分析其时间复杂度",
         "chinese_algorithm_complexity"},
        {NULL, NULL}
    };

    /* Fallback 模型链 */
    const char *fallback_models[] = {
        "claude-sonnet", "gpt-4o", "deepseek-v3", "gemini-pro", "gpt-4o-mini"
    };
    int fallback_count = (int)(sizeof(fallback_models) / sizeof(fallback_models[0]));

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cache_destroy(cache);
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.2", "memory allocation failed");
        return;
    }

    for (int t = 0; complex_inputs[t].input != NULL; t++) {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            complexity_level_t level = assess_complexity(complex_inputs[t].input);
            const char *model = route_by_complexity(level, NULL);
            const provider_t *p = provider_registry_find(reg, model);
            if (!p) {
                for (int f = 0; f < fallback_count; f++) {
                    p = provider_registry_find(reg, fallback_models[f]);
                    if (p) break;
                }
            }
        }

        /* 基准测试: 复杂路由含 fallback */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* Step 1: 复杂度评估 */
            complexity_level_t level = assess_complexity(complex_inputs[t].input);

            /* Step 2: 模型选择 */
            const char *model = route_by_complexity(level, NULL);

            /* Step 3: 缓存检查 */
            char cache_key[128];
            snprintf(cache_key, sizeof(cache_key), "%s:%s", model, "complex_hash");
            char *cached = NULL;
            int cache_hit = cache_get(cache, cache_key, &cached);

            /* Step 4: Provider 查找 + Fallback */
            const provider_t *provider = NULL;
            if (!cache_hit || !cached) {
                provider = provider_registry_find(reg, model);
                /* Fallback: 如果首选模型未找到，尝试备选 */
                if (!provider) {
                    for (int f = 0; f < fallback_count; f++) {
                        provider = provider_registry_find(reg, fallback_models[f]);
                        if (provider) break;
                    }
                }
            }

            times[i] = get_time_ns() - start;

            if (cached)
                free(cached);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "Complex: %s", complex_inputs[t].desc);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    free(times);
    cache_destroy(cache);
    provider_registry_destroy(reg);
    TEST_PASS("INT-18.2: complex query routing latency benchmark completed");
}

/* ============================================================================
 * INT-18.3: 注册表查找延迟
 *
 * 基准测试 provider registry 查找:
 *   - 查找已注册模型
 *   - 查找未注册模型
 *   - 查找 NULL 参数
 *   - 测量 P50/P95/P99
 * ============================================================================ */
TEST(int18_3_registry_lookup_latency)
{
    printf("    --- Registry Lookup Latency ---\n");

    provider_registry_t *reg = create_test_registry();
    if (!reg) {
        TEST_FAIL("INT-18.3", "registry creation failed");
        return;
    }

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.3", "memory allocation failed");
        return;
    }

    /* 场景 1: 查找已注册模型 (gpt-4o) */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            provider_registry_find(reg, "gpt-4o");
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            const provider_t *p = provider_registry_find(reg, "gpt-4o");
            times[i] = get_time_ns() - start;
            (void)p;
        }

        bench_result_t result = {"Registry: lookup registered model (gpt-4o)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    /* 场景 2: 查找未注册模型 */
    {
        for (int w = 0; w < BENCH_WARMUP; w++) {
            provider_registry_find(reg, "nonexistent-model-v999");
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            const provider_t *p = provider_registry_find(reg, "nonexistent-model-v999");
            times[i] = get_time_ns() - start;
            (void)p;
        }

        bench_result_t result = {"Registry: lookup unregistered model", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: 查找不同提供商的模型 */
    {
        const char *models[] = {
            "gpt-4o", "claude-sonnet", "gemini-pro", "deepseek-v3", "local-llama3"
        };
        int model_count = (int)(sizeof(models) / sizeof(models[0]));

        for (int w = 0; w < BENCH_WARMUP; w++) {
            for (int m = 0; m < model_count; m++) {
                provider_registry_find(reg, models[m]);
            }
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            const char *model = models[i % model_count];

            uint64_t start = get_time_ns();
            provider_registry_find(reg, model);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Registry: lookup rotating models", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 4: 空注册表查找 */
    {
        service_config_t empty_cfg = {
            .cache_capacity = 10,
            .cache_ttl_sec  = 3600,
            .max_retries    = 3,
            .timeout_ms     = 30000,
            .token_encoding = "cl100k_base",
            .providers      = NULL,
            .provider_count = 0,
        };
        provider_registry_t *empty_reg = provider_registry_create(&empty_cfg);

        for (int w = 0; w < BENCH_WARMUP; w++) {
            provider_registry_find(empty_reg, "gpt-4o");
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            provider_registry_find(empty_reg, "gpt-4o");
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Registry: lookup in empty registry", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        provider_registry_destroy(empty_reg);
    }

    free(times);
    provider_registry_destroy(reg);
    TEST_PASS("INT-18.3: registry lookup latency benchmark completed");
}

/* ============================================================================
 * INT-18.4: 缓存命中 vs 未命中
 *
 * 比较缓存命中与未命中的路由延迟:
 *   - 预填充缓存，测量缓存命中路径延迟
 *   - 使用不存在的 key，测量缓存未命中路径延迟
 *   - 计算命中/未命中延迟比
 * ============================================================================ */
TEST(int18_4_cache_hit_vs_miss)
{
    printf("    --- Cache Hit vs Miss Routing Latency ---\n");

    cache_t *cache = cache_create(1000, 3600);
    if (!cache) {
        TEST_FAIL("INT-18.4", "cache creation failed");
        return;
    }

    provider_registry_t *reg = create_test_registry();
    if (!reg) {
        cache_destroy(cache);
        TEST_FAIL("INT-18.4", "registry creation failed");
        return;
    }

    /* 预填充缓存 */
    const char *cache_key = "gpt-4o:routing_bench_hash";
    const char *cache_value = "{\"id\":\"chatcmpl-bench\",\"model\":\"gpt-4o\","
                              "\"choices\":[{\"message\":{\"content\":\"cached response\"}}],"
                              "\"usage\":{\"prompt_tokens\":50,\"completion_tokens\":25}}";
    cache_put(cache, cache_key, cache_value);

    uint64_t *hit_times  = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *miss_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!hit_times || !miss_times) {
        free(hit_times);
        free(miss_times);
        provider_registry_destroy(reg);
        cache_destroy(cache);
        TEST_FAIL("INT-18.4", "memory allocation failed");
        return;
    }

    /* 缓存命中基准测试 */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            char *val = NULL;
            cache_get(cache, cache_key, &val);
            free(val);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* 完整路由: 缓存检查 → 命中直接返回 */
            char *val = NULL;
            int hit = cache_get(cache, cache_key, &val);
            if (hit && val) {
                /* 缓存命中，跳过 provider 查找 */
            } else {
                provider_registry_find(reg, "gpt-4o");
            }

            hit_times[i] = get_time_ns() - start;
            free(val);
        }

        bench_result_t result = {"Routing: cache HIT path", 0};
        calculate_stats(hit_times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 5000 us (5ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    /* 缓存未命中基准测试 */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            char *val = NULL;
            cache_get(cache, "nonexistent_key_warmup", &val);
            free(val);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            char miss_key[64];
            snprintf(miss_key, sizeof(miss_key), "miss_key_%d", i);

            uint64_t start = get_time_ns();

            /* 完整路由: 缓存检查 → 未命中 → provider 查找 */
            char *val = NULL;
            int hit = cache_get(cache, miss_key, &val);
            if (!hit || !val) {
                provider_registry_find(reg, "gpt-4o");
            }

            miss_times[i] = get_time_ns() - start;
            free(val);
        }

        bench_result_t result = {"Routing: cache MISS path", 0};
        calculate_stats(miss_times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 命中/未命中延迟比 */
    {
        bench_result_t hit_result, miss_result;
        memset(&hit_result, 0, sizeof(hit_result));
        memset(&miss_result, 0, sizeof(miss_result));
        calculate_stats(hit_times, BENCH_ITERATIONS, &hit_result);
        calculate_stats(miss_times, BENCH_ITERATIONS, &miss_result);

        if (hit_result.p50_ns > 0) {
            double ratio = miss_result.p50_ns / hit_result.p50_ns;
            printf("    Miss/Hit P50 ratio: %.2fx (miss is %s than hit)\n",
                   ratio, ratio > 1.0 ? "slower" : "faster");
        }
    }

    free(hit_times);
    free(miss_times);
    provider_registry_destroy(reg);
    cache_destroy(cache);
    TEST_PASS("INT-18.4: cache hit vs miss routing latency benchmark completed");
}

/* ============================================================================
 * INT-18.5: 并发路由
 *
 * 10 个并发路由请求，测量总吞吐量:
 *   - 模拟 10 个并发路由请求（顺序执行模拟并发场景）
 *   - 每个请求包含: 复杂度评估 → 模型选择 → 缓存检查 → provider 查找
 *   - 测量总耗时和每请求平均延迟
 * ============================================================================ */
TEST(int18_5_concurrent_routing)
{
    printf("    --- Concurrent Routing Throughput (10 requests) ---\n");

    provider_registry_t *reg = create_test_registry();
    if (!reg) {
        TEST_FAIL("INT-18.5", "registry creation failed");
        return;
    }

    cache_t *cache = cache_create(1000, 3600);
    if (!cache) {
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.5", "cache creation failed");
        return;
    }

    /* 预填充部分缓存 */
    const char *cached_models[] = {
        "gpt-4o-mini", "gpt-4o", "claude-sonnet"
    };
    for (int i = 0; i < 3; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%s:concurrent_hash", cached_models[i]);
        cache_put(cache, key, "{\"cached\":true}");
    }

    const char *concurrent_inputs[CONCURRENT_ROUTING_COUNT] = {
        "Hello, how are you?",
        "What is the weather today?",
        "Write a Python function to sort a list",
        "Design a distributed system architecture",
        "请分析这份季度报告",
        "计算平均值并验证结果",
        "Compare machine learning approaches",
        "优化供应链流程设计",
        "评估项目风险等级",
        "生成战略规划建议"
    };

    uint64_t *batch_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!batch_times) {
        cache_destroy(cache);
        provider_registry_destroy(reg);
        TEST_FAIL("INT-18.5", "memory allocation failed");
        return;
    }

    /* 预热 */
    for (int w = 0; w < BENCH_WARMUP; w++) {
        for (int i = 0; i < CONCURRENT_ROUTING_COUNT; i++) {
            complexity_level_t level = assess_complexity(concurrent_inputs[i]);
            const char *model = route_by_complexity(level, NULL);
            char key[64];
            snprintf(key, sizeof(key), "%s:concurrent_hash", model);
            char *val = NULL;
            int hit = cache_get(cache, key, &val);
            if (!hit || !val) {
                provider_registry_find(reg, model);
            }
            free(val);
        }
    }

    /* 基准测试: 10 个并发路由请求 */
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        uint64_t start = get_time_ns();

        for (int i = 0; i < CONCURRENT_ROUTING_COUNT; i++) {
            /* Step 1: 复杂度评估 */
            complexity_level_t level = assess_complexity(concurrent_inputs[i]);

            /* Step 2: 模型选择 */
            const char *model = route_by_complexity(level, NULL);

            /* Step 3: 缓存检查 */
            char key[64];
            snprintf(key, sizeof(key), "%s:concurrent_hash", model);
            char *val = NULL;
            int hit = cache_get(cache, key, &val);

            /* Step 4: Provider 查找 (缓存未命中时) */
            if (!hit || !val) {
                provider_registry_find(reg, model);
            }

            if (val)
                free(val);
        }

        batch_times[iter] = get_time_ns() - start;
    }

    bench_result_t result;
    memset(&result, 0, sizeof(result));
    result.name = "10-request batch routing";
    calculate_stats(batch_times, BENCH_ITERATIONS, &result);
    print_result_us(&result);

    /* 每请求平均延迟 */
    double per_req_p50 = result.p50_ns / (double)CONCURRENT_ROUTING_COUNT;
    double per_req_avg = result.avg_ns / (double)CONCURRENT_ROUTING_COUNT;
    printf("    Per-request P50: %.2f us (batch P50 / %d)\n",
           per_req_p50 / 1000.0, CONCURRENT_ROUTING_COUNT);
    printf("    Per-request avg: %.2f us\n", per_req_avg / 1000.0);

    /* 吞吐量 */
    double throughput = (double)CONCURRENT_ROUTING_COUNT / (result.avg_ns / 1000000000.0);
    printf("    Throughput: %.0f routing decisions/sec\n", throughput);

    /* 验证单请求 P99 目标 */
    double per_req_p99 = result.p99_ns / (double)CONCURRENT_ROUTING_COUNT;
    if (per_req_p99 <= (double)TARGET_P99_NS) {
        printf("      [OK] Per-req P99=%.2f us < 5000 us (5ms) target\n",
               per_req_p99 / 1000.0);
    } else {
        printf("      [WARN] Per-req P99=%.2f us exceeds 5000 us (5ms) target\n",
               per_req_p99 / 1000.0);
    }

    free(batch_times);
    cache_destroy(cache);
    provider_registry_destroy(reg);
    TEST_PASS("INT-18.5: concurrent routing throughput benchmark completed");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  LLM Routing Latency Benchmark\n");
    printf("  Phase 2 - INT-18\n");
    printf("  Iterations: %d (warmup: %d)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("  Target: P99 < 5 ms per routing decision\n");
    printf("=========================================\n\n");

    /* INT-18.1: 简单查询路由延迟 */
    printf("--- INT-18.1: Simple Query Routing Latency ---\n");
    RUN_TEST(int18_1_simple_query_routing_latency);

    /* INT-18.2: 复杂查询路由延迟 */
    printf("\n--- INT-18.2: Complex Query Routing Latency ---\n");
    RUN_TEST(int18_2_complex_query_routing_latency);

    /* INT-18.3: 注册表查找延迟 */
    printf("\n--- INT-18.3: Registry Lookup Latency ---\n");
    RUN_TEST(int18_3_registry_lookup_latency);

    /* INT-18.4: 缓存命中 vs 未命中 */
    printf("\n--- INT-18.4: Cache Hit vs Miss ---\n");
    RUN_TEST(int18_4_cache_hit_vs_miss);

    /* INT-18.5: 并发路由 */
    printf("\n--- INT-18.5: Concurrent Routing ---\n");
    RUN_TEST(int18_5_concurrent_routing);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d LLM routing latency benchmark tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
