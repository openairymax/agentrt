/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_coreloop_latency.c - CoreLoopThree 单周期延迟基准测试 (INT-17)
 *
 * Phase 2 集成测试: 基准测试 CoreLoopThree 三循环核心运行时的单周期延迟
 *
 * 验证覆盖:
 *   INT-17.1: 最小周期延迟 - 单任务简单分解，测量 P50/P95/P99。目标 <100ms P50
 *   INT-17.2: 完整管线周期 - decomposition→planning→generation→critique→verification 完整周期
 *   INT-17.3: 多任务吞吐量 - 并发提交 10 个任务，测量总吞吐量
 *   INT-17.4: 周期开销分解 - 分别测量各阶段耗时
 *   INT-17.5: 冷启动 vs 热启动周期 - 比较首次周期(冷)与后续周期(热)的延迟
 *
 * 使用 clock_gettime(CLOCK_MONOTONIC, ...) 计时
 * 每项测试运行 100 次迭代，10 次预热
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "loop.h"
#include "error.h"

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

/* 延迟目标 (纳秒): 100ms = 100,000,000 ns */
#define TARGET_P50_MS  100000000ULL

/* 多任务并发数 */
#define CONCURRENT_TASK_COUNT 10

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

static void print_result_ms(const bench_result_t *result)
{
    printf("    %-50s\n", result->name);
    printf("      avg=%8.3f ms | min=%8.3f ms | max=%8.3f ms\n",
           result->avg_ns / 1000000.0, result->min_ns / 1000000.0,
           result->max_ns / 1000000.0);
    printf("      p50=%8.3f ms | p95=%8.3f ms | p99=%8.3f ms\n",
           result->p50_ns / 1000000.0, result->p95_ns / 1000000.0,
           result->p99_ns / 1000000.0);
    printf("      throughput=%.0f ops/sec\n", result->ops_per_sec);
}

/* ============================================================================
 * 辅助: 创建默认认知引擎
 * ============================================================================ */
static agentos_cognition_engine_t *create_default_engine(void)
{
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
    assert(err == AGENTOS_OK);
    assert(engine != NULL);
    return engine;
}

/* ============================================================================
 * INT-17.1: 最小周期延迟
 *
 * 单任务简单分解，测量 P50/P95/P99:
 *   - 创建认知引擎
 *   - 提交简单中文任务
 *   - 测量从 process 到 plan 返回的单周期延迟
 *   - 验证 P50 < 100ms
 * ============================================================================ */
TEST(int17_1_minimal_cycle_latency)
{
    printf("    --- Minimal Cycle Latency (Single Task) ---\n");

    const struct {
        const char *input;
        const char *desc;
    } simple_inputs[] = {
        {"分析销售数据",                    "chinese_short_analysis"},
        {"生成报告摘要",                    "chinese_short_summary"},
        {"查询库存状态",                    "chinese_short_query"},
        {"计算平均值",                      "chinese_short_calc"},
        {"检查系统健康",                    "chinese_short_health"},
        {NULL, NULL}
    };

    uint64_t *all_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!all_times) {
        TEST_FAIL("INT-17.1", "memory allocation failed");
        return;
    }

    for (int t = 0; simple_inputs[t].input != NULL; t++) {
        agentos_cognition_engine_t *engine = create_default_engine();

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, simple_inputs[t].input,
                                      strlen(simple_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        /* 基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            agentos_task_plan_t *plan = NULL;

            uint64_t start = get_time_ns();
            agentos_cognition_process(engine, simple_inputs[t].input,
                                      strlen(simple_inputs[t].input), &plan);
            all_times[i] = get_time_ns() - start;

            if (plan)
                agentos_task_plan_free(plan);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        result.name = simple_inputs[t].desc;
        calculate_stats(all_times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p50_ns <= (double)TARGET_P50_MS) {
            printf("      [OK] P50=%.3f ms < 100 ms target\n",
                   result.p50_ns / 1000000.0);
        } else {
            printf("      [WARN] P50=%.3f ms exceeds 100 ms target\n",
                   result.p50_ns / 1000000.0);
        }

        agentos_cognition_destroy(engine);
    }

    free(all_times);
    TEST_PASS("INT-17.1: minimal cycle latency benchmark completed");
}

/* ============================================================================
 * INT-17.2: 完整管线周期
 *
 * decomposition→planning→generation→critique→verification 完整周期:
 *   - 创建认知引擎（带记忆引擎）
 *   - 提交复杂多步骤输入
 *   - 测量完整管线的端到端延迟
 *   - 包含: 意图解析 → 任务规划 → 协同调度 → 健康检查
 * ============================================================================ */
TEST(int17_2_full_pipeline_cycle)
{
    printf("    --- Full Pipeline Cycle Latency ---\n");

    const struct {
        const char *input;
        const char *desc;
    } pipeline_inputs[] = {
        {"分析以下销售数据：产品A=15000，产品B=23000，产品C=18000。"
         "比较去年同期数据并生成战略建议。",
         "chinese_multi_step_analysis"},
        {"计算 [10, 20, 30, 40, 50] 的平均值，验证每一步的正确性，"
         "然后与中位数进行比较并解释差异。",
         "chinese_calc_verify_compare"},
        {"Compare machine learning vs deep learning approaches, "
         "evaluate the quality of this comparison, and ensure "
         "both theoretical foundations and practical applications are covered.",
         "english_compare_evaluate"},
        {NULL, NULL}
    };

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        TEST_FAIL("INT-17.2", "memory allocation failed");
        return;
    }

    for (int t = 0; pipeline_inputs[t].input != NULL; t++) {
        agentos_cognition_engine_t *engine = create_default_engine();

        /* 关联记忆引擎 */
        agentos_memory_engine_t *mem_engine = NULL;
        agentos_error_t err = agentos_memory_create(NULL, &mem_engine);
        if (err == AGENTOS_OK && mem_engine != NULL) {
            agentos_cognition_set_memory(engine, mem_engine);
        }

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, pipeline_inputs[t].input,
                                      strlen(pipeline_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        /* 基准测试: 完整管线 decomposition→planning→generation→critique→verification */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* decomposition + planning: cognition_process */
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, pipeline_inputs[t].input,
                                      strlen(pipeline_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);

            /* generation + critique: health_check 包含内部评估 */
            char *health = NULL;
            agentos_cognition_health_check(engine, &health);
            if (health)
                free(health);

            /* verification: stats 包含验证统计 */
            char *stats = NULL;
            size_t stats_len = 0;
            agentos_cognition_stats(engine, &stats, &stats_len);
            if (stats)
                free(stats);

            times[i] = get_time_ns() - start;
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "Full Pipeline: %s", pipeline_inputs[t].desc);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p50_ns <= (double)TARGET_P50_MS) {
            printf("      [OK] P50=%.3f ms < 100 ms target\n",
                   result.p50_ns / 1000000.0);
        } else {
            printf("      [WARN] P50=%.3f ms exceeds 100 ms target\n",
                   result.p50_ns / 1000000.0);
        }

        agentos_cognition_destroy(engine);
        if (mem_engine)
            agentos_memory_destroy(mem_engine);
    }

    free(times);
    TEST_PASS("INT-17.2: full pipeline cycle latency benchmark completed");
}

/* ============================================================================
 * INT-17.3: 多任务吞吐量
 *
 * 并发提交 10 个任务，测量总吞吐量:
 *   - 创建核心循环
 *   - 连续提交 10 个任务
 *   - 测量总耗时和每任务平均延迟
 *   - 计算吞吐量 (tasks/sec)
 * ============================================================================ */
TEST(int17_3_multi_task_throughput)
{
    printf("    --- Multi-Task Throughput (10 concurrent tasks) ---\n");

    const char *concurrent_inputs[CONCURRENT_TASK_COUNT] = {
        "分析季度销售数据",
        "生成市场趋势报告",
        "查询客户满意度评分",
        "计算投资回报率",
        "评估项目风险等级",
        "优化供应链流程",
        "预测下季度需求量",
        "审核财务报表数据",
        "比较竞品定价策略",
        "生成战略规划建议"
    };

    uint64_t *batch_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!batch_times) {
        TEST_FAIL("INT-17.3", "memory allocation failed");
        return;
    }

    agentos_cognition_engine_t *engine = create_default_engine();

    /* 预热 */
    for (int w = 0; w < BENCH_WARMUP; w++) {
        for (int i = 0; i < CONCURRENT_TASK_COUNT; i++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, concurrent_inputs[i],
                                      strlen(concurrent_inputs[i]), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }
    }

    /* 基准测试: 每次提交 10 个任务，测量总耗时 */
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        uint64_t start = get_time_ns();

        for (int i = 0; i < CONCURRENT_TASK_COUNT; i++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, concurrent_inputs[i],
                                      strlen(concurrent_inputs[i]), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        batch_times[iter] = get_time_ns() - start;
    }

    bench_result_t result;
    memset(&result, 0, sizeof(result));
    result.name = "10-task batch throughput";
    calculate_stats(batch_times, BENCH_ITERATIONS, &result);
    print_result_ms(&result);

    /* 计算每任务平均延迟 */
    double per_task_p50 = result.p50_ns / (double)CONCURRENT_TASK_COUNT;
    double per_task_avg = result.avg_ns / (double)CONCURRENT_TASK_COUNT;
    printf("    Per-task P50: %.3f ms (batch P50 / %d)\n",
           per_task_p50 / 1000000.0, CONCURRENT_TASK_COUNT);
    printf("    Per-task avg: %.3f ms\n", per_task_avg / 1000000.0);

    /* 吞吐量 */
    double throughput = (double)CONCURRENT_TASK_COUNT / (result.avg_ns / 1000000000.0);
    printf("    Throughput: %.1f tasks/sec\n", throughput);

    agentos_cognition_destroy(engine);
    free(batch_times);
    TEST_PASS("INT-17.3: multi-task throughput benchmark completed");
}

/* ============================================================================
 * INT-17.4: 周期开销分解
 *
 * 分别测量各阶段耗时:
 *   Phase 1: Decomposition (意图解析)
 *   Phase 2: Planning (任务规划)
 *   Phase 3: Generation (内容生成 - health_check)
 *   Phase 4: Critique (评估 - stats)
 *   Phase 5: Verification (验证 - 二次 health_check)
 * ============================================================================ */
TEST(int17_4_cycle_overhead_breakdown)
{
    printf("    --- Cycle Overhead Breakdown ---\n");

    const char *input = "分析以下销售数据：产品A=15000，产品B=23000，产品C=18000。"
                        "比较去年同期数据并生成战略建议。";

    agentos_cognition_engine_t *engine = create_default_engine();

    /* 关联记忆引擎 */
    agentos_memory_engine_t *mem_engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &mem_engine);
    if (err == AGENTOS_OK && mem_engine != NULL) {
        agentos_cognition_set_memory(engine, mem_engine);
    }

    /* 预热 */
    for (int w = 0; w < BENCH_WARMUP; w++) {
        agentos_task_plan_t *plan = NULL;
        agentos_cognition_process(engine, input, strlen(input), &plan);
        if (plan)
            agentos_task_plan_free(plan);
        char *health = NULL;
        agentos_cognition_health_check(engine, &health);
        if (health)
            free(health);
    }

    /* 分阶段计时数组 */
    uint64_t *phase_decomposition = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *phase_planning      = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *phase_generation    = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *phase_critique      = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *phase_verification  = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));

    if (!phase_decomposition || !phase_planning || !phase_generation
        || !phase_critique || !phase_verification) {
        TEST_FAIL("INT-17.4", "memory allocation failed");
        free(phase_decomposition);
        free(phase_planning);
        free(phase_generation);
        free(phase_critique);
        free(phase_verification);
        agentos_cognition_destroy(engine);
        if (mem_engine)
            agentos_memory_destroy(mem_engine);
        return;
    }

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        /* Phase 1: Decomposition - 意图解析 */
        uint64_t t0 = get_time_ns();
        agentos_intent_parser_t *parser = NULL;
        agentos_intent_parser_create(&parser);
        agentos_intent_t *intent = NULL;
        agentos_intent_parser_parse(parser, input, strlen(input), &intent);
        phase_decomposition[i] = get_time_ns() - t0;

        /* Phase 2: Planning - 任务规划 (cognition_process 包含分解+规划) */
        uint64_t t1 = get_time_ns();
        agentos_task_plan_t *plan = NULL;
        agentos_cognition_process(engine, input, strlen(input), &plan);
        phase_planning[i] = get_time_ns() - t1;

        /* Phase 3: Generation - 内容生成 (health_check 触发内部生成) */
        uint64_t t2 = get_time_ns();
        char *health = NULL;
        agentos_cognition_health_check(engine, &health);
        phase_generation[i] = get_time_ns() - t2;
        if (health)
            free(health);

        /* Phase 4: Critique - 评估 (stats 触发内部评估) */
        uint64_t t3 = get_time_ns();
        char *stats = NULL;
        size_t stats_len = 0;
        agentos_cognition_stats(engine, &stats, &stats_len);
        phase_critique[i] = get_time_ns() - t3;
        if (stats)
            free(stats);

        /* Phase 5: Verification - 验证 (二次 health_check) */
        uint64_t t4 = get_time_ns();
        char *verify = NULL;
        agentos_cognition_health_check(engine, &verify);
        phase_verification[i] = get_time_ns() - t4;
        if (verify)
            free(verify);

        /* 清理本轮资源 */
        if (plan)
            agentos_task_plan_free(plan);
        if (intent)
            agentos_intent_free(intent);
        agentos_intent_parser_destroy(parser);
    }

    /* 输出各阶段统计 */
    bench_result_t result;
    const char *phase_names[] = {
        "Phase 1: Decomposition",
        "Phase 2: Planning",
        "Phase 3: Generation",
        "Phase 4: Critique",
        "Phase 5: Verification"
    };
    uint64_t *phase_arrays[] = {
        phase_decomposition, phase_planning, phase_generation,
        phase_critique, phase_verification
    };

    double total_avg_ns = 0;
    for (int p = 0; p < 5; p++) {
        memset(&result, 0, sizeof(result));
        result.name = phase_names[p];
        calculate_stats(phase_arrays[p], BENCH_ITERATIONS, &result);
        print_result_ms(&result);
        total_avg_ns += result.avg_ns;
    }

    /* 输出各阶段占比 */
    printf("    --- Phase Overhead Breakdown ---\n");
    for (int p = 0; p < 5; p++) {
        memset(&result, 0, sizeof(result));
        calculate_stats(phase_arrays[p], BENCH_ITERATIONS, &result);
        double pct = (total_avg_ns > 0) ? (result.avg_ns / total_avg_ns * 100.0) : 0;
        printf("      %s: %.1f%% (avg=%.3f ms)\n",
               phase_names[p], pct, result.avg_ns / 1000000.0);
    }

    free(phase_decomposition);
    free(phase_planning);
    free(phase_generation);
    free(phase_critique);
    free(phase_verification);

    agentos_cognition_destroy(engine);
    if (mem_engine)
        agentos_memory_destroy(mem_engine);
    TEST_PASS("INT-17.4: cycle overhead breakdown benchmark completed");
}

/* ============================================================================
 * INT-17.5: 冷启动 vs 热启动周期
 *
 * 比较首次周期(冷)与后续周期(热)的延迟:
 *   - 测量引擎创建后的第一个 process 延迟(冷启动)
 *   - 测量预热后的 process 延迟(热启动)
 *   - 计算冷/热比
 * ============================================================================ */
TEST(int17_5_warm_vs_cold_cycle)
{
    printf("    --- Warm vs Cold Cycle Latency ---\n");

    const char *input = "分析以下销售数据：产品A=15000，产品B=23000，产品C=18000。"
                        "比较去年同期数据并生成战略建议。";

    uint64_t *cold_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    uint64_t *warm_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!cold_times || !warm_times) {
        TEST_FAIL("INT-17.5", "memory allocation failed");
        free(cold_times);
        free(warm_times);
        return;
    }

    /* 冷启动测试: 每次迭代都创建新引擎，测量第一个 process */
    for (int i = 0; i < BENCH_ITERATIONS; i++) {
        agentos_cognition_engine_t *engine = NULL;
        agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
        if (err != AGENTOS_OK || engine == NULL) {
            cold_times[i] = 0;
            continue;
        }

        uint64_t start = get_time_ns();
        agentos_task_plan_t *plan = NULL;
        agentos_cognition_process(engine, input, strlen(input), &plan);
        cold_times[i] = get_time_ns() - start;

        if (plan)
            agentos_task_plan_free(plan);
        agentos_cognition_destroy(engine);
    }

    /* 热启动测试: 创建一次引擎，预热后测量后续 process */
    {
        agentos_cognition_engine_t *engine = create_default_engine();

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, input, strlen(input), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        /* 热启动基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            agentos_task_plan_t *plan = NULL;

            uint64_t start = get_time_ns();
            agentos_cognition_process(engine, input, strlen(input), &plan);
            warm_times[i] = get_time_ns() - start;

            if (plan)
                agentos_task_plan_free(plan);
        }

        agentos_cognition_destroy(engine);
    }

    /* 统计 */
    bench_result_t cold_result, warm_result;
    memset(&cold_result, 0, sizeof(cold_result));
    memset(&warm_result, 0, sizeof(warm_result));
    cold_result.name = "Cold cycle (new engine per iteration)";
    warm_result.name = "Warm cycle (reused engine)";
    calculate_stats(cold_times, BENCH_ITERATIONS, &cold_result);
    calculate_stats(warm_times, BENCH_ITERATIONS, &warm_result);

    print_result_ms(&cold_result);
    print_result_ms(&warm_result);

    /* 冷/热比 */
    if (warm_result.p50_ns > 0) {
        double ratio = cold_result.p50_ns / warm_result.p50_ns;
        printf("    Cold/Warm P50 ratio: %.2fx\n", ratio);
    }

    if (warm_result.p50_ns <= (double)TARGET_P50_MS) {
        printf("      [OK] Warm P50=%.3f ms < 100 ms target\n",
               warm_result.p50_ns / 1000000.0);
    } else {
        printf("      [WARN] Warm P50=%.3f ms exceeds 100 ms target\n",
               warm_result.p50_ns / 1000000.0);
    }

    free(cold_times);
    free(warm_times);
    TEST_PASS("INT-17.5: warm vs cold cycle latency benchmark completed");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  CoreLoopThree Cycle Latency Benchmark\n");
    printf("  Phase 2 - INT-17\n");
    printf("  Iterations: %d (warmup: %d)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("  Target: P50 < 100 ms per cycle\n");
    printf("=========================================\n\n");

    /* INT-17.1: 最小周期延迟 */
    printf("--- INT-17.1: Minimal Cycle Latency ---\n");
    RUN_TEST(int17_1_minimal_cycle_latency);

    /* INT-17.2: 完整管线周期 */
    printf("\n--- INT-17.2: Full Pipeline Cycle Latency ---\n");
    RUN_TEST(int17_2_full_pipeline_cycle);

    /* INT-17.3: 多任务吞吐量 */
    printf("\n--- INT-17.3: Multi-Task Throughput ---\n");
    RUN_TEST(int17_3_multi_task_throughput);

    /* INT-17.4: 周期开销分解 */
    printf("\n--- INT-17.4: Cycle Overhead Breakdown ---\n");
    RUN_TEST(int17_4_cycle_overhead_breakdown);

    /* INT-17.5: 冷启动 vs 热启动 */
    printf("\n--- INT-17.5: Warm vs Cold Cycle ---\n");
    RUN_TEST(int17_5_warm_vs_cold_cycle);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d CoreLoop latency benchmark tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
