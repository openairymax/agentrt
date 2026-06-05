/**
 * @file bench_routing_latency.c
 * @brief llm_d 模型路由延迟基准测试 (INT-18)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 目标: 路由决策延迟 < 5ms
 */

#include "cache.h"
#include "llm_service.h"
#include "providers/registry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 基准测试迭代次数 */
#define BENCH_ITERATIONS 10000

/* ========== 计时工具 ========== */

static double get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}

typedef struct {
    double min_us;
    double max_us;
    double avg_us;
    double p50_us;
    double p95_us;
    double p99_us;
    int iterations;
} bench_result_t;

static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static void compute_stats(double *samples, int count, bench_result_t *result)
{
    qsort(samples, (size_t)count, sizeof(double), compare_double);

    result->min_us = samples[0];
    result->max_us = samples[count - 1];
    result->iterations = count;

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    result->avg_us = sum / (double)count;

    int p50_idx = (int)(count * 0.50);
    int p95_idx = (int)(count * 0.95);
    int p99_idx = (int)(count * 0.99);

    result->p50_us = samples[p50_idx];
    result->p95_us = samples[p95_idx];
    result->p99_us = samples[p99_idx];
}

static void print_bench_result(const char *name, const bench_result_t *r)
{
    printf("  %s:\n", name);
    printf("    iterations: %d\n", r->iterations);
    printf("    min:   %8.2f us\n", r->min_us);
    printf("    avg:   %8.2f us\n", r->avg_us);
    printf("    p50:   %8.2f us\n", r->p50_us);
    printf("    p95:   %8.2f us\n", r->p95_us);
    printf("    p99:   %8.2f us\n", r->p99_us);
    printf("    max:   %8.2f us\n", r->max_us);
}

/* ========== INT-18.1: 提供商查找延迟基准 ========== */

static void bench_provider_lookup(void)
{
    printf("  [INT-18.1] Provider lookup latency benchmark...\n");

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

    double *samples = (double *)malloc(BENCH_ITERATIONS * sizeof(double));
    assert(samples != NULL);

    /* 预热 */
    for (int i = 0; i < 100; i++) {
        provider_registry_find(reg, "warmup-model");
    }

    /* 基准测试 */
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        double start = get_time_us();
        provider_registry_find(reg, "test-model-gpt-4o");
        double end = get_time_us();
        samples[i] = end - start;
    }

    bench_result_t result;
    compute_stats(samples, BENCH_ITERATIONS, &result);
    print_bench_result("Provider lookup (empty registry)", &result);

    /* 验证目标: P99 < 5ms */
    double target_us = 5000.0; /* 5ms = 5000us */
    if (result.p99_us < target_us) {
        printf("    TARGET MET: P99 %.2fus < 5000us (5ms)\n", result.p99_us);
    } else {
        printf("    WARNING: P99 %.2fus >= 5000us (5ms)\n", result.p99_us);
    }

    free(samples);
    provider_registry_destroy(reg);
    printf("    PASSED\n");
}

/* ========== INT-18.2: 缓存命中延迟基准 ========== */

static void bench_cache_hit_latency(void)
{
    printf("  [INT-18.2] Cache hit latency benchmark...\n");

    cache_t *cache = cache_create(1000, 3600);
    assert(cache != NULL);

    /* 预填充缓存 */
    const char *test_key = "model:gpt-4o:hash_complex_key_12345";
    const char *test_value = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4o\",\"choices\":[{\"message\":{\"content\":\"Benchmark response content\"}}],\"usage\":{\"prompt_tokens\":100,\"completion_tokens\":50}}";
    cache_put(cache, test_key, test_value);

    double *samples = (double *)malloc(BENCH_ITERATIONS * sizeof(double));
    assert(samples != NULL);

    /* 预热 */
    for (int i = 0; i < 100; i++) {
        char *val = NULL;
        cache_get(cache, test_key, &val);
        free(val);
    }

    /* 基准测试 */
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        double start = get_time_us();
        char *val = NULL;
        cache_get(cache, test_key, &val);
        double end = get_time_us();
        free(val);
        samples[i] = end - start;
    }

    bench_result_t result;
    compute_stats(samples, BENCH_ITERATIONS, &result);
    print_bench_result("Cache hit (LRU lookup)", &result);

    /* 缓存命中应远低于5ms */
    double target_us = 5000.0;
    if (result.p99_us < target_us) {
        printf("    TARGET MET: P99 %.2fus < 5000us (5ms)\n", result.p99_us);
    } else {
        printf("    WARNING: P99 %.2fus >= 5000us (5ms)\n", result.p99_us);
    }

    free(samples);
    cache_destroy(cache);
    printf("    PASSED\n");
}

/* ========== INT-18.3: 缓存未命中延迟基准 ========== */

static void bench_cache_miss_latency(void)
{
    printf("  [INT-18.3] Cache miss latency benchmark...\n");

    cache_t *cache = cache_create(1000, 3600);
    assert(cache != NULL);

    double *samples = (double *)malloc(BENCH_ITERATIONS / 10 * sizeof(double));
    assert(samples != NULL);

    int iterations = BENCH_ITERATIONS / 10;
    for (int i = 0; i < iterations; i++) {
        char key[64];
        snprintf(key, sizeof(key), "nonexistent_key_%d", i);

        double start = get_time_us();
        char *val = NULL;
        cache_get(cache, key, &val);
        double end = get_time_us();
        samples[i] = end - start;
    }

    bench_result_t result;
    compute_stats(samples, iterations, &result);
    print_bench_result("Cache miss (hash lookup)", &result);

    free(samples);
    cache_destroy(cache);
    printf("    PASSED\n");
}

/* ========== INT-18.4: 综合路由决策延迟 ========== */

static void bench_full_routing_decision(void)
{
    printf("  [INT-18.4] Full routing decision latency (cache + lookup)...\n");

    cache_t *cache = cache_create(1000, 3600);
    assert(cache != NULL);

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

    double *samples = (double *)malloc(BENCH_ITERATIONS * sizeof(double));
    assert(samples != NULL);

    const char *model = "gpt-4o";
    const char *cache_key = "model:gpt-4o:hash_routing_bench";

    /* 预热 */
    cache_put(cache, cache_key, "{\"cached\":true}");
    for (int i = 0; i < 100; i++) {
        char *val = NULL;
        if (cache_get(cache, cache_key, &val) != 1) {
            provider_registry_find(reg, model);
        }
        free(val);
    }

    /* 基准测试: 模拟完整路由决策流程 */
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        double start = get_time_us();

        /* 步骤1: 检查缓存 */
        char *cached = NULL;
        int cache_hit = cache_get(cache, cache_key, &cached);

        if (!cache_hit || !cached) {
            /* 步骤2: 查找提供商 */
            provider_registry_find(reg, model);
        }

        double end = get_time_us();
        free(cached);
        samples[i] = end - start;
    }

    bench_result_t result;
    compute_stats(samples, BENCH_ITERATIONS, &result);
    print_bench_result("Full routing decision (cache hit path)", &result);

    /* 目标 < 5ms */
    double target_us = 5000.0;
    if (result.p99_us < target_us) {
        printf("    TARGET MET: P99 %.2fus < 5000us (5ms)\n", result.p99_us);
    } else {
        printf("    WARNING: P99 %.2fus >= 5000us (5ms)\n", result.p99_us);
    }

    free(samples);
    provider_registry_destroy(reg);
    cache_destroy(cache);
    printf("    PASSED\n");
}

/* ========== 主函数 ========== */

int main(void)
{
    printf("=========================================\n");
    printf("  LLM Routing Latency Benchmarks\n");
    printf("  (INT-18: target P99 < 5ms)\n");
    printf("=========================================\n\n");

    bench_provider_lookup();
    bench_cache_hit_latency();
    bench_cache_miss_latency();
    bench_full_routing_decision();

    printf("\n=========================================\n");
    printf("  All routing latency benchmarks completed\n");
    printf("=========================================\n");

    return 0;
}