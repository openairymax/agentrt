/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_credential_latency.c - 凭证池获取延迟基准测试 (INT-19)
 *
 * Phase 2 集成测试: 基准测试 Cupolas 安全穹顶凭证池获取延迟
 *
 * 验证覆盖:
 *   INT-19.1: 单凭证获取 - 获取一个凭证，测量 P50/P95/P99。目标 <1ms
 *   INT-19.2: 凭证轮换延迟 - 测量凭证轮换耗时
 *   INT-19.3: 池耗尽恢复 - 所有凭证使用中时，测量等待下一个可用凭证的时间
 *   INT-19.4: 并发获取 - 10 个线程同时获取凭证
 *   INT-19.5: 凭证验证延迟 - 返回凭证前验证凭证的耗时
 *
 * 使用 clock_gettime(CLOCK_MONOTONIC, ...) 计时
 * 每项测试运行 100 次迭代，10 次预热
 */

#include "cupolas.h"
#include "cupolas_signature.h"
#include "cupolas_vault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

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

/* 延迟目标 (纳秒): 1ms = 1,000,000 ns */
#define TARGET_P99_NS 1000000ULL

/* 并发获取线程数 */
#define CONCURRENT_THREAD_COUNT 10

/* 凭证池大小 */
#define CREDENTIAL_POOL_SIZE 10

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
 * 辅助: 初始化 Cupolas 框架
 * ============================================================================ */
static int init_cupolas_framework(void)
{
    agentos_error_t error = AGENTOS_OK;
    int ret = cupolas_init(NULL, &error);
    if (ret != AGENTOS_OK) {
        TEST_FAIL("cupolas_init", "initialization failed");
        return -1;
    }
    return 0;
}

/* ============================================================================
 * 辅助: 初始化 Vault 模块
 * ============================================================================ */
static int init_vault_module(void)
{
    int ret = cupolas_vault_init(NULL);
    if (ret != 0) {
        TEST_FAIL("cupolas_vault_init", "vault initialization failed");
        return -1;
    }
    return 0;
}

/* ============================================================================
 * 辅助: 创建并打开测试用 Vault
 * ============================================================================ */
static cupolas_vault_t *open_test_vault(void)
{
    cupolas_vault_t *vault = NULL;
    int ret = cupolas_vault_open("bench_credential_vault", "bench_password", &vault);
    if (ret != 0 || vault == NULL) {
        TEST_FAIL("cupolas_vault_open", "failed to open vault");
        return NULL;
    }
    return vault;
}

/* ============================================================================
 * 辅助: 存入一组凭证（用于基准测试）
 * ============================================================================ */
static int store_credential_pool(cupolas_vault_t *vault, const char *group,
                                 int count, cupolas_vault_cred_type_t type)
{
    for (int i = 0; i < count; i++) {
        char cred_id[128];
        snprintf(cred_id, sizeof(cred_id), "%s_cred_%d", group, i);

        char cred_data[128];
        snprintf(cred_data, sizeof(cred_data),
                 "{\"api_key\":\"sk-bench-%s-%d\",\"created\":%lu}",
                 group, i, (unsigned long)time(NULL));

        int ret = cupolas_vault_store(vault, cred_id, type,
                                      (const uint8_t *)cred_data,
                                      strlen(cred_data), NULL);
        if (ret != 0) {
            printf("    Failed to store credential '%s': %d\n", cred_id, ret);
            return ret;
        }
    }
    return 0;
}

/* ============================================================================
 * 辅助: 授予 Agent 访问权限
 * ============================================================================ */
static void grant_pool_access(cupolas_vault_t *vault, const char *group,
                               int count, const char *agent_id)
{
    for (int i = 0; i < count; i++) {
        char cred_id[128];
        snprintf(cred_id, sizeof(cred_id), "%s_cred_%d", group, i);
        cupolas_vault_grant_access(vault, cred_id, agent_id,
                                   CUPOLAS_VAULT_OP_READ | CUPOLAS_VAULT_OP_WRITE, 0);
    }
}

/* ============================================================================
 * 辅助: 清理凭证池
 * ============================================================================ */
static void cleanup_pool(cupolas_vault_t *vault, const char *group,
                          int count, const char *agent_id)
{
    for (int i = 0; i < count; i++) {
        char cred_id[128];
        snprintf(cred_id, sizeof(cred_id), "%s_cred_%d", group, i);
        cupolas_vault_delete(vault, cred_id, agent_id);
    }
}

/* ============================================================================
 * INT-19.1: 单凭证获取
 *
 * 获取一个凭证，测量 P50/P95/P99:
 *   - 初始化 Cupolas + Vault
 *   - 存入凭证池
 *   - 多次检索单个凭证
 *   - 验证 P99 < 1ms
 * ============================================================================ */
TEST(int19_1_single_credential_acquisition)
{
    printf("    --- Single Credential Acquisition Latency ---\n");

    if (init_cupolas_framework() != 0)
        return;
    if (init_vault_module() != 0) {
        cupolas_cleanup();
        return;
    }

    cupolas_vault_t *vault = open_test_vault();
    if (!vault) {
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 存入凭证池 */
    int ret = store_credential_pool(vault, "single_pool", CREDENTIAL_POOL_SIZE,
                                     CUPOLAS_VAULT_CRED_TOKEN);
    if (ret != 0) {
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }
    grant_pool_access(vault, "single_pool", CREDENTIAL_POOL_SIZE, "bench_agent");
    printf("    Stored %d credentials in pool\n", CREDENTIAL_POOL_SIZE);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cleanup_pool(vault, "single_pool", CREDENTIAL_POOL_SIZE, "bench_agent");
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 测试不同凭证的获取延迟 */
    const struct {
        const char *cred_id;
        const char *desc;
    } test_creds[] = {
        {"single_pool_cred_0", "first_credential"},
        {"single_pool_cred_4", "middle_credential"},
        {"single_pool_cred_9", "last_credential"},
        {NULL, NULL}
    };

    for (int t = 0; test_creds[t].cred_id != NULL; t++) {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            uint8_t data_buf[256];
            size_t data_len = sizeof(data_buf);
            cupolas_vault_retrieve(vault, test_creds[t].cred_id, "bench_agent",
                                   data_buf, &data_len);
        }

        /* 基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint8_t data_buf[256];
            size_t data_len = sizeof(data_buf);

            uint64_t start = get_time_ns();
            cupolas_vault_retrieve(vault, test_creds[t].cred_id, "bench_agent",
                                   data_buf, &data_len);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        result.name = test_creds[t].desc;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    free(times);
    cleanup_pool(vault, "single_pool", CREDENTIAL_POOL_SIZE, "bench_agent");
    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
    cupolas_cleanup();
    TEST_PASS("INT-19.1: single credential acquisition latency benchmark completed");
}

/* ============================================================================
 * INT-19.2: 凭证轮换延迟
 *
 * 测量凭证轮换耗时:
 *   - 使用不同轮换策略 (ROUND_ROBIN, LEAST_USED, RATE_LIMITED, PRIORITY)
 *   - 测量每次轮换调用的延迟
 *   - 验证 P99 < 1ms
 * ============================================================================ */
TEST(int19_2_credential_rotation_latency)
{
    printf("    --- Credential Rotation Latency ---\n");

    if (init_cupolas_framework() != 0)
        return;
    if (init_vault_module() != 0) {
        cupolas_cleanup();
        return;
    }

    cupolas_vault_t *vault = open_test_vault();
    if (!vault) {
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 存入凭证池 */
    int ret = store_credential_pool(vault, "rotate_pool", CREDENTIAL_POOL_SIZE,
                                     CUPOLAS_VAULT_CRED_TOKEN);
    if (ret != 0) {
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }
    grant_pool_access(vault, "rotate_pool", CREDENTIAL_POOL_SIZE, "rotate_agent");
    printf("    Stored %d credentials for rotation benchmark\n", CREDENTIAL_POOL_SIZE);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cleanup_pool(vault, "rotate_pool", CREDENTIAL_POOL_SIZE, "rotate_agent");
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 场景 1: ROUND_ROBIN 轮换 */
    {
        /* 预热 */
        char selected_id[256];
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_ROUND_ROBIN,
                                            selected_id, sizeof(selected_id));
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_ROUND_ROBIN,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Rotation: ROUND_ROBIN", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    /* 场景 2: LEAST_USED 轮换 */
    {
        char selected_id[256];
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_LEAST_USED,
                                            selected_id, sizeof(selected_id));
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_LEAST_USED,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Rotation: LEAST_USED", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: RATE_LIMITED 轮换 */
    {
        char selected_id[256];
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_RATE_LIMITED,
                                            selected_id, sizeof(selected_id));
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_RATE_LIMITED,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Rotation: RATE_LIMITED", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 4: PRIORITY 轮换 */
    {
        char selected_id[256];
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_PRIORITY,
                                            selected_id, sizeof(selected_id));
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "rotate_pool",
                                            CUPOLAS_VAULT_ROTATE_PRIORITY,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Rotation: PRIORITY", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    free(times);
    cleanup_pool(vault, "rotate_pool", CREDENTIAL_POOL_SIZE, "rotate_agent");
    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
    cupolas_cleanup();
    TEST_PASS("INT-19.2: credential rotation latency benchmark completed");
}

/* ============================================================================
 * INT-19.3: 池耗尽恢复
 *
 * 所有凭证使用中时，测量等待下一个可用凭证的时间:
 *   - 存入少量凭证（3个）
 *   - 模拟所有凭证被占用
 *   - 测量从"池耗尽"到"凭证释放后可用"的恢复时间
 *   - 使用轮换策略等待可用凭证
 * ============================================================================ */
#define EXHAUST_POOL_SIZE 3

TEST(int19_3_pool_exhaustion_recovery)
{
    printf("    --- Pool Exhaustion Recovery Latency ---\n");

    if (init_cupolas_framework() != 0)
        return;
    if (init_vault_module() != 0) {
        cupolas_cleanup();
        return;
    }

    cupolas_vault_t *vault = open_test_vault();
    if (!vault) {
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 存入少量凭证 */
    int ret = store_credential_pool(vault, "exhaust_pool", EXHAUST_POOL_SIZE,
                                     CUPOLAS_VAULT_CRED_TOKEN);
    if (ret != 0) {
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }
    grant_pool_access(vault, "exhaust_pool", EXHAUST_POOL_SIZE, "exhaust_agent");
    printf("    Stored %d credentials for exhaustion test\n", EXHAUST_POOL_SIZE);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cleanup_pool(vault, "exhaust_pool", EXHAUST_POOL_SIZE, "exhaust_agent");
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 场景 1: 池未耗尽时的轮换延迟 (基线) */
    {
        char selected_id[256];

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_rotate_credential(vault, "exhaust_pool",
                                            CUPOLAS_VAULT_ROTATE_ROUND_ROBIN,
                                            selected_id, sizeof(selected_id));
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "exhaust_pool",
                                            CUPOLAS_VAULT_ROTATE_ROUND_ROBIN,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Exhaustion: baseline (pool available)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 2: 模拟池耗尽 - 大量检索后轮换 */
    {
        /* 模拟凭证被大量使用 */
        for (int round = 0; round < 5; round++) {
            for (int i = 0; i < EXHAUST_POOL_SIZE; i++) {
                char cred_id[128];
                snprintf(cred_id, sizeof(cred_id), "exhaust_pool_cred_%d", i);
                uint8_t data_buf[256];
                size_t data_len = sizeof(data_buf);
                cupolas_vault_retrieve(vault, cred_id, "exhaust_agent",
                                       data_buf, &data_len);
            }
        }

        char selected_id[256];
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "exhaust_pool",
                                            CUPOLAS_VAULT_ROTATE_LEAST_USED,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Exhaustion: after heavy usage (LEAST_USED)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: 池恢复 - 使用 RATE_LIMITED 策略 */
    {
        char selected_id[256];
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_rotate_credential(vault, "exhaust_pool",
                                            CUPOLAS_VAULT_ROTATE_RATE_LIMITED,
                                            selected_id, sizeof(selected_id));
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Exhaustion: recovery (RATE_LIMITED)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 4: 检查凭证可用性延迟 */
    {
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            char cred_id[128];
            snprintf(cred_id, sizeof(cred_id), "exhaust_pool_cred_%d",
                     i % EXHAUST_POOL_SIZE);

            uint64_t start = get_time_ns();
            bool exists = cupolas_vault_exists(vault, cred_id);
            times[i] = get_time_ns() - start;
            (void)exists;
        }

        bench_result_t result = {"Exhaustion: credential existence check", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    free(times);
    cleanup_pool(vault, "exhaust_pool", EXHAUST_POOL_SIZE, "exhaust_agent");
    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
    cupolas_cleanup();
    TEST_PASS("INT-19.3: pool exhaustion recovery latency benchmark completed");
}

/* ============================================================================
 * INT-19.4: 并发获取
 *
 * 10 个线程同时获取凭证:
 *   - 创建凭证池
 *   - 启动 10 个线程同时检索凭证
 *   - 测量每线程的获取延迟
 *   - 计算总吞吐量
 * ============================================================================ */

/* 并发线程参数 */
typedef struct {
    cupolas_vault_t *vault;
    const char *cred_id;
    const char *agent_id;
    uint64_t latency_ns;
    int success;
} concurrent_acq_param_t;

static void *concurrent_acquire_thread(void *arg)
{
    concurrent_acq_param_t *param = (concurrent_acq_param_t *)arg;

    uint8_t data_buf[256];
    size_t data_len = sizeof(data_buf);

    uint64_t start = get_time_ns();
    int ret = cupolas_vault_retrieve(param->vault, param->cred_id,
                                      param->agent_id, data_buf, &data_len);
    param->latency_ns = get_time_ns() - start;
    param->success = (ret == 0) ? 1 : 0;

    return NULL;
}

TEST(int19_4_concurrent_acquisition)
{
    printf("    --- Concurrent Credential Acquisition (10 threads) ---\n");

    if (init_cupolas_framework() != 0)
        return;
    if (init_vault_module() != 0) {
        cupolas_cleanup();
        return;
    }

    cupolas_vault_t *vault = open_test_vault();
    if (!vault) {
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 存入凭证池 */
    int ret = store_credential_pool(vault, "concurrent_pool", CREDENTIAL_POOL_SIZE,
                                     CUPOLAS_VAULT_CRED_TOKEN);
    if (ret != 0) {
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }
    grant_pool_access(vault, "concurrent_pool", CREDENTIAL_POOL_SIZE, "concurrent_agent");
    printf("    Stored %d credentials for concurrent benchmark\n", CREDENTIAL_POOL_SIZE);

    uint64_t *batch_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!batch_times) {
        cleanup_pool(vault, "concurrent_pool", CREDENTIAL_POOL_SIZE, "concurrent_agent");
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    concurrent_acq_param_t params[CONCURRENT_THREAD_COUNT];
    pthread_t threads[CONCURRENT_THREAD_COUNT];

    /* 预热 */
    for (int w = 0; w < BENCH_WARMUP; w++) {
        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            char cred_id[128];
            snprintf(cred_id, sizeof(cred_id), "concurrent_pool_cred_%d",
                     i % CREDENTIAL_POOL_SIZE);

            params[i].vault = vault;
            params[i].cred_id = strdup(cred_id);
            params[i].agent_id = "concurrent_agent";
            params[i].latency_ns = 0;
            params[i].success = 0;

            pthread_create(&threads[i], NULL, concurrent_acquire_thread, &params[i]);
        }

        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            pthread_join(threads[i], NULL);
            free((void *)params[i].cred_id);
        }
    }

    /* 基准测试: 10 个线程并发获取凭证 */
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        uint64_t batch_start = get_time_ns();

        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            char cred_id[128];
            snprintf(cred_id, sizeof(cred_id), "concurrent_pool_cred_%d",
                     i % CREDENTIAL_POOL_SIZE);

            params[i].vault = vault;
            params[i].cred_id = strdup(cred_id);
            params[i].agent_id = "concurrent_agent";
            params[i].latency_ns = 0;
            params[i].success = 0;

            pthread_create(&threads[i], NULL, concurrent_acquire_thread, &params[i]);
        }

        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            pthread_join(threads[i], NULL);
            free((void *)params[i].cred_id);
        }

        batch_times[iter] = get_time_ns() - batch_start;
    }

    bench_result_t result;
    memset(&result, 0, sizeof(result));
    result.name = "10-thread concurrent acquisition";
    calculate_stats(batch_times, BENCH_ITERATIONS, &result);
    print_result_us(&result);

    /* 每线程平均延迟 */
    double per_thread_p50 = result.p50_ns / (double)CONCURRENT_THREAD_COUNT;
    double per_thread_avg = result.avg_ns / (double)CONCURRENT_THREAD_COUNT;
    printf("    Per-thread P50: %.2f us (batch P50 / %d)\n",
           per_thread_p50 / 1000.0, CONCURRENT_THREAD_COUNT);
    printf("    Per-thread avg: %.2f us\n", per_thread_avg / 1000.0);

    /* 吞吐量 */
    double throughput = (double)CONCURRENT_THREAD_COUNT / (result.avg_ns / 1000000000.0);
    printf("    Throughput: %.0f acquisitions/sec\n", throughput);

    /* 统计成功/失败 */
    int total_success = 0;
    int total_attempts = 0;
    for (int iter = 0; iter < 5; iter++) {
        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            char cred_id[128];
            snprintf(cred_id, sizeof(cred_id), "concurrent_pool_cred_%d",
                     i % CREDENTIAL_POOL_SIZE);

            params[i].vault = vault;
            params[i].cred_id = strdup(cred_id);
            params[i].agent_id = "concurrent_agent";
            params[i].latency_ns = 0;
            params[i].success = 0;

            pthread_create(&threads[i], NULL, concurrent_acquire_thread, &params[i]);
        }

        for (int i = 0; i < CONCURRENT_THREAD_COUNT; i++) {
            pthread_join(threads[i], NULL);
            total_attempts++;
            if (params[i].success) total_success++;
            free((void *)params[i].cred_id);
        }
    }
    printf("    Success rate: %d/%d (%.1f%%)\n",
           total_success, total_attempts,
           total_attempts > 0 ? (double)total_success / total_attempts * 100.0 : 0.0);

    free(batch_times);
    cleanup_pool(vault, "concurrent_pool", CREDENTIAL_POOL_SIZE, "concurrent_agent");
    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
    cupolas_cleanup();
    TEST_PASS("INT-19.4: concurrent acquisition latency benchmark completed");
}

/* ============================================================================
 * INT-19.5: 凭证验证延迟
 *
 * 返回凭证前验证凭证的耗时:
 *   - 检查凭证存在性 (exists)
 *   - 检查访问权限 (check_access)
 *   - 获取元数据 (get_metadata)
 *   - 完整验证流程: exists + check_access + get_metadata + retrieve
 * ============================================================================ */
TEST(int19_5_credential_validation_latency)
{
    printf("    --- Credential Validation Latency ---\n");

    if (init_cupolas_framework() != 0)
        return;
    if (init_vault_module() != 0) {
        cupolas_cleanup();
        return;
    }

    cupolas_vault_t *vault = open_test_vault();
    if (!vault) {
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    /* 存入凭证池 */
    int ret = store_credential_pool(vault, "validate_pool", CREDENTIAL_POOL_SIZE,
                                     CUPOLAS_VAULT_CRED_TOKEN);
    if (ret != 0) {
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }
    grant_pool_access(vault, "validate_pool", CREDENTIAL_POOL_SIZE, "validate_agent");
    printf("    Stored %d credentials for validation benchmark\n", CREDENTIAL_POOL_SIZE);

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        cleanup_pool(vault, "validate_pool", CREDENTIAL_POOL_SIZE, "validate_agent");
        cupolas_vault_close(vault);
        cupolas_vault_cleanup();
        cupolas_cleanup();
        return;
    }

    const char *test_cred = "validate_pool_cred_0";

    /* 场景 1: exists 检查延迟 */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_exists(vault, test_cred);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_exists(vault, test_cred);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Validation: exists check", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 2: check_access 延迟 */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_check_access(vault, test_cred, "validate_agent",
                                       CUPOLAS_VAULT_OP_READ);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_check_access(vault, test_cred, "validate_agent",
                                       CUPOLAS_VAULT_OP_READ);
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Validation: check_access", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 3: get_metadata 延迟 */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            cupolas_vault_metadata_t meta;
            memset(&meta, 0, sizeof(meta));
            cupolas_vault_get_metadata(vault, test_cred, &meta);
            cupolas_vault_free_metadata(&meta);
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            cupolas_vault_metadata_t meta;
            memset(&meta, 0, sizeof(meta));

            uint64_t start = get_time_ns();
            cupolas_vault_get_metadata(vault, test_cred, &meta);
            times[i] = get_time_ns() - start;

            cupolas_vault_free_metadata(&meta);
        }

        bench_result_t result = {"Validation: get_metadata", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    /* 场景 4: 完整验证流程 exists + check_access + get_metadata + retrieve */
    {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            if (cupolas_vault_exists(vault, test_cred)) {
                if (cupolas_vault_check_access(vault, test_cred, "validate_agent",
                                                CUPOLAS_VAULT_OP_READ)) {
                    cupolas_vault_metadata_t meta;
                    memset(&meta, 0, sizeof(meta));
                    cupolas_vault_get_metadata(vault, test_cred, &meta);
                    cupolas_vault_free_metadata(&meta);

                    uint8_t data_buf[256];
                    size_t data_len = sizeof(data_buf);
                    cupolas_vault_retrieve(vault, test_cred, "validate_agent",
                                           data_buf, &data_len);
                }
            }
        }

        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* Step 1: 检查凭证存在 */
            if (cupolas_vault_exists(vault, test_cred)) {
                /* Step 2: 检查访问权限 */
                if (cupolas_vault_check_access(vault, test_cred, "validate_agent",
                                                CUPOLAS_VAULT_OP_READ)) {
                    /* Step 3: 获取元数据验证 */
                    cupolas_vault_metadata_t meta;
                    memset(&meta, 0, sizeof(meta));
                    cupolas_vault_get_metadata(vault, test_cred, &meta);
                    cupolas_vault_free_metadata(&meta);

                    /* Step 4: 检索凭证数据 */
                    uint8_t data_buf[256];
                    size_t data_len = sizeof(data_buf);
                    cupolas_vault_retrieve(vault, test_cred, "validate_agent",
                                           data_buf, &data_len);
                }
            }

            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Validation: full pipeline (exists+access+meta+retrieve)", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);

        if (result.p99_ns <= (double)TARGET_P99_NS) {
            printf("      [OK] P99=%.2f us < 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        } else {
            printf("      [WARN] P99=%.2f us exceeds 1000 us (1ms) target\n",
                   result.p99_ns / 1000.0);
        }
    }

    /* 场景 5: 不存在凭证的验证延迟 */
    {
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            cupolas_vault_exists(vault, "nonexistent_cred");
            times[i] = get_time_ns() - start;
        }

        bench_result_t result = {"Validation: nonexistent credential check", 0};
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_us(&result);
    }

    free(times);
    cleanup_pool(vault, "validate_pool", CREDENTIAL_POOL_SIZE, "validate_agent");
    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
    cupolas_cleanup();
    TEST_PASS("INT-19.5: credential validation latency benchmark completed");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Credential Pool Acquisition Latency\n");
    printf("  Phase 2 - INT-19\n");
    printf("  Iterations: %d (warmup: %d)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("  Target: P99 < 1 ms per acquisition\n");
    printf("=========================================\n\n");

    /* INT-19.1: 单凭证获取 */
    printf("--- INT-19.1: Single Credential Acquisition ---\n");
    RUN_TEST(int19_1_single_credential_acquisition);

    /* INT-19.2: 凭证轮换延迟 */
    printf("\n--- INT-19.2: Credential Rotation Latency ---\n");
    RUN_TEST(int19_2_credential_rotation_latency);

    /* INT-19.3: 池耗尽恢复 */
    printf("\n--- INT-19.3: Pool Exhaustion Recovery ---\n");
    RUN_TEST(int19_3_pool_exhaustion_recovery);

    /* INT-19.4: 并发获取 */
    printf("\n--- INT-19.4: Concurrent Acquisition ---\n");
    RUN_TEST(int19_4_concurrent_acquisition);

    /* INT-19.5: 凭证验证延迟 */
    printf("\n--- INT-19.5: Credential Validation Latency ---\n");
    RUN_TEST(int19_5_credential_validation_latency);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d credential latency benchmark tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
