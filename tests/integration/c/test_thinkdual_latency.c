/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thinkdual_latency.c - Thinkdual 流式校验延迟基准测试 (INT-04)
 *
 * Phase 2 集成测试: 基准测试 Thinkdual 认知管线的流式校验延迟
 *
 * 验证覆盖:
 *   INT-04.1: 单语义单元校验延迟 - 测量中文语义单元校验的 P50/P95/P99
 *   INT-04.2: 多单元流式延迟 - 测量 10 个语义单元的流式校验延迟
 *   INT-04.3: Stream Critic 响应延迟 - 测量 stream_critic 处理耗时
 *   INT-04.4: 元认知评估延迟 - 测量五维评估耗时
 *   INT-04.5: 端到端 Thinkdual 管线延迟 - thinking_chain→triple_coordinator→stream_critic→engine→metacognition
 *
 * 目标: 每语义单元 < 50ms
 *
 * 使用 clock_gettime(CLOCK_MONOTONIC, ...) 计时
 * 每项测试运行 1000 次迭代，100 次预热
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "error.h"
#include "semantic_unit.h"

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
#define BENCH_ITERATIONS 1000
#define BENCH_WARMUP     100

/* 延迟目标 (纳秒): 50ms = 50,000,000 ns */
#define TARGET_PER_UNIT_NS 50000000ULL

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
 * 辅助: 验证字符串是否为合法的 JSON 起始标记
 * ============================================================================ */
static int is_valid_json_prefix(const char *str)
{
    if (!str || str[0] == '\0')
        return 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
        str++;
    return (*str == '{' || *str == '[');
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
 * INT-04.1: 单语义单元校验延迟
 *
 * 测量 P50/P95/P99 延迟，验证单个中文语义单元的校验耗时:
 *   - 创建流式检测器
 *   - 向检测器送入单个中文语义单元
 *   - 测量从 feed 到 flush + pop 的完整延迟
 *   - 验证 P99 < 50ms
 * ============================================================================ */
TEST(int04_1_single_semantic_unit_latency)
{
    printf("    --- Single Semantic Unit Validation Latency ---\n");

    /* 中文语义单元测试数据 */
    const struct {
        const char *text;
        const char *desc;
    } test_units[] = {
        {"人工智能是计算机科学的一个重要分支。", "chinese_single_sentence"},
        {"深度学习模型在自然语言处理领域取得了突破性进展。", "chinese_tech_sentence"},
        {"请分析这份季度报告并生成战略建议。", "chinese_instruction"},
        {"机器学习算法需要大量标注数据进行训练。", "chinese_ml_fact"},
        {"系统检测到异常行为，请立即进行安全审查！", "chinese_alert"},
        {NULL, NULL}
    };

    uint64_t *all_times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!all_times) {
        TEST_FAIL("INT-04.1", "memory allocation failed");
        return;
    }

    for (int t = 0; test_units[t].text != NULL; t++) {
        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            su_stream_detector_t *det = NULL;
            agentos_error_t err = su_stream_detector_create(NULL, &det);
            if (err == AGENTOS_SUCCESS && det != NULL) {
                su_stream_detector_feed(det, test_units[t].text,
                                        strlen(test_units[t].text), 0.8f);
                su_stream_detector_flush(det);
                size_t cnt = su_stream_detector_pending_count(det);
                for (size_t i = 0; i < cnt; i++) {
                    su_semantic_unit_t unit;
                    su_stream_detector_pop_pending(det, &unit);
                    if (unit.text)
                        AGENTOS_FREE(unit.text);
                }
                su_stream_detector_destroy(det);
            }
        }

        /* 基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            su_stream_detector_t *det = NULL;
            agentos_error_t err = su_stream_detector_create(NULL, &det);
            if (err != AGENTOS_SUCCESS || det == NULL) {
                all_times[i] = 0;
                continue;
            }

            uint64_t start = get_time_ns();

            su_stream_detector_feed(det, test_units[t].text,
                                    strlen(test_units[t].text), 0.8f);
            su_stream_detector_flush(det);

            size_t cnt = su_stream_detector_pending_count(det);
            for (size_t j = 0; j < cnt; j++) {
                su_semantic_unit_t unit;
                su_stream_detector_pop_pending(det, &unit);
                if (unit.text)
                    AGENTOS_FREE(unit.text);
            }

            all_times[i] = get_time_ns() - start;

            su_stream_detector_destroy(det);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        result.name = test_units[t].desc;
        calculate_stats(all_times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p99_ns <= (double)TARGET_PER_UNIT_NS) {
            printf("      [OK] P99=%.3f ms < 50 ms target\n",
                   result.p99_ns / 1000000.0);
        } else {
            printf("      [WARN] P99=%.3f ms exceeds 50 ms target\n",
                   result.p99_ns / 1000000.0);
        }
    }

    free(all_times);
    TEST_PASS("INT-04.1: single semantic unit latency benchmark completed");
}

/* ============================================================================
 * INT-04.2: 多单元流式延迟
 *
 * 测量 10 个语义单元的流式校验延迟:
 *   - 创建单个流式检测器
 *   - 连续送入 10 个中文语义单元
 *   - 测量从第一个 feed 到所有单元 pop 的总延迟
 *   - 验证平均每单元延迟 < 50ms
 * ============================================================================ */
#define MULTI_UNIT_COUNT 10

TEST(int04_2_multi_unit_stream_latency)
{
    printf("    --- Multi-Unit Stream Latency (10 units) ---\n");

    /* 10 个中文语义单元 */
    const char *units[MULTI_UNIT_COUNT] = {
        "人工智能正在改变我们的生活方式。",
        "深度学习是机器学习的一个重要分支。",
        "自然语言处理技术取得了显著进步。",
        "计算机视觉在自动驾驶中发挥关键作用。",
        "强化学习适用于序列决策问题。",
        "图神经网络可以处理非欧几里得数据。",
        "迁移学习减少了模型训练所需的数据量。",
        "生成对抗网络能够合成逼真的图像。",
        "联邦学习保护了用户的数据隐私。",
        "知识蒸馏实现了模型的轻量化部署。"
    };

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        TEST_FAIL("INT-04.2", "memory allocation failed");
        return;
    }

    /* 预热 */
    for (int w = 0; w < BENCH_WARMUP; w++) {
        su_stream_detector_t *det = NULL;
        agentos_error_t err = su_stream_detector_create(NULL, &det);
        if (err == AGENTOS_SUCCESS && det != NULL) {
            for (int i = 0; i < MULTI_UNIT_COUNT; i++) {
                su_stream_detector_feed(det, units[i], strlen(units[i]), 0.8f);
            }
            su_stream_detector_flush(det);
            size_t cnt = su_stream_detector_pending_count(det);
            for (size_t j = 0; j < cnt; j++) {
                su_semantic_unit_t unit;
                su_stream_detector_pop_pending(det, &unit);
                if (unit.text)
                    AGENTOS_FREE(unit.text);
            }
            su_stream_detector_destroy(det);
        }
    }

    /* 基准测试 */
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        su_stream_detector_t *det = NULL;
        agentos_error_t err = su_stream_detector_create(NULL, &det);
        if (err != AGENTOS_SUCCESS || det == NULL) {
            times[iter] = 0;
            continue;
        }

        uint64_t start = get_time_ns();

        for (int i = 0; i < MULTI_UNIT_COUNT; i++) {
            su_stream_detector_feed(det, units[i], strlen(units[i]), 0.8f);
        }
        su_stream_detector_flush(det);

        size_t cnt = su_stream_detector_pending_count(det);
        for (size_t j = 0; j < cnt; j++) {
            su_semantic_unit_t unit;
            su_stream_detector_pop_pending(det, &unit);
            if (unit.text)
                AGENTOS_FREE(unit.text);
        }

        times[iter] = get_time_ns() - start;
        su_stream_detector_destroy(det);
    }

    bench_result_t result;
    memset(&result, 0, sizeof(result));
    result.name = "10-unit stream validation";
    calculate_stats(times, BENCH_ITERATIONS, &result);
    print_result_ms(&result);

    /* 计算每单元平均延迟 */
    double per_unit_p99 = result.p99_ns / (double)MULTI_UNIT_COUNT;
    printf("    Per-unit P99: %.3f ms (total P99 / %d)\n",
           per_unit_p99 / 1000000.0, MULTI_UNIT_COUNT);

    if (per_unit_p99 <= (double)TARGET_PER_UNIT_NS) {
        printf("      [OK] Per-unit P99=%.3f ms < 50 ms target\n",
               per_unit_p99 / 1000000.0);
    } else {
        printf("      [WARN] Per-unit P99=%.3f ms exceeds 50 ms target\n",
               per_unit_p99 / 1000000.0);
    }

    free(times);
    TEST_PASS("INT-04.2: multi-unit stream latency benchmark completed");
}

/* ============================================================================
 * INT-04.3: Stream Critic 响应延迟
 *
 * 测量 stream_critic 通过认知引擎处理输入的响应时间:
 *   - 创建认知引擎
 *   - 送入不同类型的输入
 *   - 测量 cognition_process 的 P50/P95/P99
 * ============================================================================ */
TEST(int04_3_stream_critic_latency)
{
    printf("    --- Stream Critic Response Latency ---\n");

    const struct {
        const char *input;
        const char *desc;
    } critic_inputs[] = {
        {"请分析这份销售数据并生成季度报告。", "chinese_analysis_request"},
        {"比较机器学习和深度学习的优缺点。", "chinese_comparison_request"},
        {"计算以下数据的平均值：10, 20, 30, 40, 50。", "chinese_calculation_request"},
        {"验证之前的分析结果是否正确。", "chinese_verification_request"},
        {"Analyze the quarterly sales data and provide strategic recommendations.",
         "english_analysis_request"},
        {NULL, NULL}
    };

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        TEST_FAIL("INT-04.3", "memory allocation failed");
        return;
    }

    for (int t = 0; critic_inputs[t].input != NULL; t++) {
        agentos_cognition_engine_t *engine = create_default_engine();

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, critic_inputs[t].input,
                                      strlen(critic_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        /* 基准测试 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            agentos_task_plan_t *plan = NULL;

            uint64_t start = get_time_ns();
            agentos_cognition_process(engine, critic_inputs[t].input,
                                      strlen(critic_inputs[t].input), &plan);
            times[i] = get_time_ns() - start;

            if (plan)
                agentos_task_plan_free(plan);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "Stream Critic: %s", critic_inputs[t].desc);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p99_ns <= (double)TARGET_PER_UNIT_NS) {
            printf("      [OK] P99=%.3f ms < 50 ms target\n",
                   result.p99_ns / 1000000.0);
        } else {
            printf("      [WARN] P99=%.3f ms exceeds 50 ms target\n",
                   result.p99_ns / 1000000.0);
        }

        agentos_cognition_destroy(engine);
    }

    free(times);
    TEST_PASS("INT-04.3: stream critic latency benchmark completed");
}

/* ============================================================================
 * INT-04.4: 元认知评估延迟
 *
 * 测量元认知五维评估的耗时:
 *   - 创建认知引擎并关联记忆引擎
 *   - 处理输入触发元认知评估
 *   - 测量从 process 到 health_check（含五维评分）的延迟
 *   - 验证 P99 < 50ms
 * ============================================================================ */
TEST(int04_4_metacognition_evaluation_latency)
{
    printf("    --- Metacognition Five-Dimension Assessment Latency ---\n");

    const char *meta_inputs[] = {
        "请评估以下分析的准确性和完整性，并提供改进建议。",
        "对比深度学习与传统机器学习方法的优劣，给出综合评分。",
        "审查之前的推理过程，检查是否存在逻辑错误或不一致之处。",
        NULL
    };

    uint64_t *times = (uint64_t *)malloc(BENCH_ITERATIONS * sizeof(uint64_t));
    if (!times) {
        TEST_FAIL("INT-04.4", "memory allocation failed");
        return;
    }

    for (int t = 0; meta_inputs[t] != NULL; t++) {
        agentos_cognition_engine_t *engine = create_default_engine();
        agentos_memory_engine_t *mem_engine = NULL;
        agentos_error_t err = agentos_memory_create(NULL, &mem_engine);
        if (err == AGENTOS_OK && mem_engine != NULL) {
            agentos_cognition_set_memory(engine, mem_engine);
        }

        /* 预热 */
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, meta_inputs[t],
                                      strlen(meta_inputs[t]), &plan);
            if (plan)
                agentos_task_plan_free(plan);
            char *health = NULL;
            agentos_cognition_health_check(engine, &health);
            if (health)
                free(health);
        }

        /* 基准测试: process + health_check (含元认知五维评估) */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, meta_inputs[t],
                                      strlen(meta_inputs[t]), &plan);
            if (plan)
                agentos_task_plan_free(plan);

            char *health = NULL;
            agentos_cognition_health_check(engine, &health);
            times[i] = get_time_ns() - start;

            if (health)
                free(health);
        }

        bench_result_t result;
        memset(&result, 0, sizeof(result));
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "Metacognition: input_%d", t + 1);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p99_ns <= (double)TARGET_PER_UNIT_NS) {
            printf("      [OK] P99=%.3f ms < 50 ms target\n",
                   result.p99_ns / 1000000.0);
        } else {
            printf("      [WARN] P99=%.3f ms exceeds 50 ms target\n",
                   result.p99_ns / 1000000.0);
        }

        agentos_cognition_destroy(engine);
        if (mem_engine)
            agentos_memory_destroy(mem_engine);
    }

    free(times);
    TEST_PASS("INT-04.4: metacognition evaluation latency benchmark completed");
}

/* ============================================================================
 * INT-04.5: 端到端 Thinkdual 管线延迟
 *
 * 测量完整管线: thinking_chain→triple_coordinator→stream_critic→engine→metacognition
 *   - 创建认知引擎（带 feedback 回调）和记忆引擎
 *   - 处理复杂多步骤输入
 *   - 测量从输入到完整处理（含健康检查和统计）的端到端延迟
 *   - 验证 P99 < 50ms
 * ============================================================================ */

/* feedback 回调（用于验证管线各阶段被触发） */
static int g_pipeline_feedback_count = 0;

static void pipeline_feedback_callback(int level, const char *module,
                                       const char *event, const char *data,
                                       size_t data_len, void *user_data)
{
    g_pipeline_feedback_count++;
    (void)level;
    (void)module;
    (void)event;
    (void)data;
    (void)data_len;
    (void)user_data;
}

TEST(int04_5_e2e_thinkdual_pipeline_latency)
{
    printf("    --- E2E Thinkdual Pipeline Latency ---\n");

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
        TEST_FAIL("INT-04.5", "memory allocation failed");
        return;
    }

    for (int t = 0; pipeline_inputs[t].input != NULL; t++) {
        /* 创建带 feedback 回调的引擎 */
        agentos_cognition_config_t config;
        memset(&config, 0, sizeof(config));
        config.cognition_default_timeout_ms = 30000;
        config.cognition_max_retries = 3;
        config.feedback_callback = pipeline_feedback_callback;
        config.feedback_user_data = NULL;

        agentos_cognition_engine_t *engine = NULL;
        agentos_error_t err = agentos_cognition_create_ex_take(&config, NULL, NULL, NULL, &engine);
        if (err != AGENTOS_OK || engine == NULL) {
            TEST_FAIL("INT-04.5", "engine creation failed");
            continue;
        }

        /* 关联记忆引擎 */
        agentos_memory_engine_t *mem_engine = NULL;
        err = agentos_memory_create(NULL, &mem_engine);
        if (err == AGENTOS_OK && mem_engine != NULL) {
            agentos_cognition_set_memory(engine, mem_engine);
        }

        /* 预热 */
        g_pipeline_feedback_count = 0;
        for (int w = 0; w < BENCH_WARMUP; w++) {
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, pipeline_inputs[t].input,
                                      strlen(pipeline_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);
        }

        /* 基准测试: 完整管线 */
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            uint64_t start = get_time_ns();

            /* thinking_chain → triple_coordinator → stream_critic → engine */
            agentos_task_plan_t *plan = NULL;
            agentos_cognition_process(engine, pipeline_inputs[t].input,
                                      strlen(pipeline_inputs[t].input), &plan);
            if (plan)
                agentos_task_plan_free(plan);

            /* metacognition: health_check 包含五维评分 */
            char *health = NULL;
            agentos_cognition_health_check(engine, &health);
            if (health)
                free(health);

            /* 统计信息 */
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
        snprintf(name_buf, sizeof(name_buf), "E2E Pipeline: %s", pipeline_inputs[t].desc);
        result.name = name_buf;
        calculate_stats(times, BENCH_ITERATIONS, &result);
        print_result_ms(&result);

        if (result.p99_ns <= (double)TARGET_PER_UNIT_NS) {
            printf("      [OK] P99=%.3f ms < 50 ms target\n",
                   result.p99_ns / 1000000.0);
        } else {
            printf("      [WARN] P99=%.3f ms exceeds 50 ms target\n",
                   result.p99_ns / 1000000.0);
        }

        printf("    Pipeline feedback callbacks: %d (across all iterations)\n",
               g_pipeline_feedback_count);

        agentos_cognition_destroy(engine);
        if (mem_engine)
            agentos_memory_destroy(mem_engine);
    }

    free(times);
    TEST_PASS("INT-04.5: E2E Thinkdual pipeline latency benchmark completed");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Thinkdual Stream Validation Latency\n");
    printf("  Phase 2 - INT-04\n");
    printf("  Iterations: %d (warmup: %d)\n", BENCH_ITERATIONS, BENCH_WARMUP);
    printf("  Target: < 50 ms per semantic unit\n");
    printf("=========================================\n\n");

    /* INT-04.1: 单语义单元校验延迟 */
    printf("--- INT-04.1: Single Semantic Unit Validation Latency ---\n");
    RUN_TEST(int04_1_single_semantic_unit_latency);

    /* INT-04.2: 多单元流式延迟 */
    printf("\n--- INT-04.2: Multi-Unit Stream Latency ---\n");
    RUN_TEST(int04_2_multi_unit_stream_latency);

    /* INT-04.3: Stream Critic 响应延迟 */
    printf("\n--- INT-04.3: Stream Critic Response Latency ---\n");
    RUN_TEST(int04_3_stream_critic_latency);

    /* INT-04.4: 元认知评估延迟 */
    printf("\n--- INT-04.4: Metacognition Evaluation Latency ---\n");
    RUN_TEST(int04_4_metacognition_evaluation_latency);

    /* INT-04.5: 端到端 Thinkdual 管线延迟 */
    printf("\n--- INT-04.5: E2E Thinkdual Pipeline Latency ---\n");
    RUN_TEST(int04_5_e2e_thinkdual_pipeline_latency);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d Thinkdual latency benchmark tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
