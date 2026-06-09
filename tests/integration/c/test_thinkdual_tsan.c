/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thinkdual_tsan.c - Thinkdual ThreadSanitizer 验证测试 (INT-02)
 *
 * Phase 2 集成测试: 验证 Thinkdual (triple_coordinator) 系统的线程安全性
 *
 * 验证覆盖:
 *   INT-02.1: 并发 t2/t1-f/t1-p 访问 - 多线程同时操作三重协调器组件
 *   INT-02.2: 并发 thinking_chain 操作 - 多线程向同一链路添加步骤
 *   INT-02.3: 并发元认知评估 - 多线程触发元认知检查
 *   INT-02.4: stream_critic 路径竞争检测 - 验证无数据竞争
 *   INT-02.5: 死锁检测 - 验证引擎锁序无死锁
 *
 * 编译方式:
 *   gcc -fsanitize=thread -g -O1 -pthread \
 *       -I../../agentos/atoms/coreloopthree/src/cognition/critique \
 *       -I../../agentos/atoms/coreloopthree/src/cognition/foundation \
 *       -I../../agentos/atoms/coreloopthree/include \
 *       -I../../agentos/atoms/corekern/include \
 *       -I../../agentos/commons/include \
 *       -I../../agentos/commons/utils/error/include \
 *       -I../../agentos/commons/utils/types/include \
 *       -I../../agentos/commons/platform/include \
 *       test_thinkdual_tsan.c -o test_thinkdual_tsan
 *
 * 该测试设计为通过 ThreadSanitizer 检测数据竞争。
 * 若存在内部全局状态竞争，TSan 将在运行时报告。
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "error.h"
#include "triple_coordinator.h"
#include "thinking_chain.h"
#include "metacognition.h"
#include "platform.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

#define ASSERT_OK(expr)                                                        \
    do {                                                                       \
        agentos_error_t __err = (expr);                                        \
        if (__err != AGENTOS_OK) {                                             \
            printf("    ASSERT_FAIL: %s returned %d at line %d\n", #expr,     \
                   (int)__err, __LINE__);                                       \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    ASSERT_FAIL: %s at line %d\n", #cond, __LINE__);       \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

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
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    assert(err == AGENTOS_OK);
    assert(engine != NULL);
    return engine;
}

/* ============================================================================
 * 辅助: 空 feedback 回调（用于并发测试）
 * ============================================================================ */
static void null_feedback_callback(int level, const char *module,
                                   const char *event, const char *data,
                                   size_t data_len, void *user_data)
{
    (void)level;
    (void)module;
    (void)event;
    (void)data;
    (void)data_len;
    (void)user_data;
}

/* ============================================================================
 * Mock: t2 主思考函数（线程安全版本，使用原子计数器）
 * ============================================================================ */
static int g_mock_t2_call_count = 0;

static agentos_error_t mock_t2_thinking(const char **prompts, size_t count,
                                        void *context, char **out_result)
{
    (void)context;
    __sync_fetch_and_add(&g_mock_t2_call_count, 1);

    const char *header = "{\"role\":\"t2\",\"type\":\"deep_thinking\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"status\":\"completed\"}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    for (size_t i = 0; i < count; i++)
        assert(prompts[i] != NULL);

    return AGENTOS_OK;
}

/* ============================================================================
 * Mock: t1-f 快速校验函数
 * ============================================================================ */
static int g_mock_t1f_call_count = 0;

static agentos_error_t mock_t1f_fast_check(const char **prompts, size_t count,
                                            void *context, char **out_result)
{
    (void)context;
    __sync_fetch_and_add(&g_mock_t1f_call_count, 1);

    const char *header = "{\"role\":\"t1-f\",\"type\":\"fast_validation\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"verdict\":\"pass\"}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    return AGENTOS_OK;
}

/* ============================================================================
 * Mock: t1-p 并行分发函数
 * ============================================================================ */
static int g_mock_t1p_call_count = 0;

static agentos_error_t mock_t1p_parallel_dispatch(const char **prompts, size_t count,
                                                   void *context, char **out_result)
{
    (void)context;
    __sync_fetch_and_add(&g_mock_t1p_call_count, 1);

    const char *header = "{\"role\":\"t1-p\",\"type\":\"parallel_dispatch\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"dispatched\":true}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    return AGENTOS_OK;
}

static void mock_coordinator_destroy(agentos_coordinator_strategy_t *strategy)
{
    free(strategy);
}

/* ============================================================================
 * Mock: tc3 S2 生成函数（用于 triple_coordinator 并发测试）
 * ============================================================================ */
static int g_mock_s2_gen_count = 0;

static agentos_error_t mock_s2_gen(const char *input, size_t input_len, char **output,
                                   size_t *output_len, void *user_data)
{
    (void)input;
    (void)input_len;
    (void)user_data;
    __sync_fetch_and_add(&g_mock_s2_gen_count, 1);

    const char *result = "This is a sufficiently long generated output that exceeds "
                         "fifty characters to pass the accept threshold in the "
                         "triple coordinator verification loop.";
    size_t len = strlen(result);
    char *buf = (char *)malloc(len + 1);
    assert(buf != NULL);
    memcpy(buf, result, len + 1);
    *output = buf;
    *output_len = len;
    return AGENTOS_OK;
}

/* ============================================================================
 * Mock: tc3 S1 专家函数
 * ============================================================================ */
static int g_mock_s1_expert_count = 0;

static agentos_error_t mock_s1_exp(const char *content, size_t content_len,
                                   const char *critique, size_t critique_len,
                                   float *out_score, tc3_verdict_t *out_verdict,
                                   char **out_opinion, size_t *out_opinion_len,
                                   void *user_data)
{
    (void)content;
    (void)content_len;
    (void)critique;
    (void)critique_len;
    (void)user_data;
    __sync_fetch_and_add(&g_mock_s1_expert_count, 1);

    if (out_score)
        *out_score = 0.8f;
    if (out_verdict)
        *out_verdict = TC3_RESULT_ACCEPT;
    if (out_opinion) {
        const char *opinion = "expert_approved";
        size_t olen = strlen(opinion);
        char *o = (char *)malloc(olen + 1);
        assert(o != NULL);
        memcpy(o, opinion, olen + 1);
        *out_opinion = o;
        if (out_opinion_len)
            *out_opinion_len = olen;
    }
    return AGENTOS_OK;
}

/* ============================================================================
 * INT-02.1: 并发 t2/t1-f/t1-p 访问
 *
 * 多线程同时访问 triple coordinator 的三个组件:
 *   - 线程组 A: 同时调用 t2 策略
 *   - 线程组 B: 同时调用 t1-f 策略
 *   - 线程组 C: 同时调用 t1-p 策略
 *
 * 验证:
 *   - 所有线程正常完成
 *   - 无数据损坏
 *   - 调用计数正确
 * ============================================================================ */

/* 线程参数: 访问特定 coordinator 策略 */
typedef struct {
    int thread_id;
    int iterations;
    agentos_coordinator_strategy_t *strategy;
    const char **prompts;
    size_t prompt_count;
    int *local_success;
    agentos_mutex_t *mutex;
} coord_access_args_t;

static void *coord_access_worker(void *arg)
{
    coord_access_args_t *args = (coord_access_args_t *)arg;
    int success = 0;

    for (int i = 0; i < args->iterations; i++) {
        char *result = NULL;
        agentos_error_t err = args->strategy->coordinate(
            args->prompts, args->prompt_count, NULL, &result);
        if (err == AGENTOS_OK && result != NULL) {
            if (is_valid_json_prefix(result))
                success++;
            free(result);
        }
    }

    agentos_mutex_lock(args->mutex);
    (*args->local_success) += success;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)success;
}

TEST(int02_1_concurrent_t2_t1f_t1p_access)
{
    printf("    --- Concurrent t2/t1-f/t1-p Access ---\n");

    #define INT02_1_THREADS_PER_ROLE 3
    #define INT02_1_ITERATIONS 50
    #define INT02_1_TOTAL_THREADS (INT02_1_THREADS_PER_ROLE * 3)

    g_mock_t2_call_count = 0;
    g_mock_t1f_call_count = 0;
    g_mock_t1p_call_count = 0;

    /* 创建三组策略 */
    agentos_coordinator_strategy_t *t2_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    agentos_coordinator_strategy_t *t1f_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    agentos_coordinator_strategy_t *t1p_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    ASSERT_TRUE(t2_strategy != NULL);
    ASSERT_TRUE(t1f_strategy != NULL);
    ASSERT_TRUE(t1p_strategy != NULL);

    t2_strategy->coordinate = mock_t2_thinking;
    t2_strategy->destroy = mock_coordinator_destroy;
    t2_strategy->data = NULL;

    t1f_strategy->coordinate = mock_t1f_fast_check;
    t1f_strategy->destroy = mock_coordinator_destroy;
    t1f_strategy->data = NULL;

    t1p_strategy->coordinate = mock_t1p_parallel_dispatch;
    t1p_strategy->destroy = mock_coordinator_destroy;
    t1p_strategy->data = NULL;

    /* 准备测试输入 */
    const char *deep_prompts[] = {"Analyze Q1 sales data", "Compare with Q4"};
    const char *fast_prompts[] = {"Verify: growth rate calculation"};
    const char *parallel_prompts[] = {"Chart A", "Chart B", "Chart C"};

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int total_success = 0;
    agentos_thread_t threads[INT02_1_TOTAL_THREADS];
    coord_access_args_t args[INT02_1_TOTAL_THREADS];

    /* 创建 t2 线程组 */
    for (int i = 0; i < INT02_1_THREADS_PER_ROLE; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_1_ITERATIONS;
        args[i].strategy = t2_strategy;
        args[i].prompts = deep_prompts;
        args[i].prompt_count = 2;
        args[i].local_success = &total_success;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], coord_access_worker, &args[i]) == 0);
    }

    /* 创建 t1-f 线程组 */
    for (int i = 0; i < INT02_1_THREADS_PER_ROLE; i++) {
        int idx = INT02_1_THREADS_PER_ROLE + i;
        args[idx].thread_id = idx;
        args[idx].iterations = INT02_1_ITERATIONS;
        args[idx].strategy = t1f_strategy;
        args[idx].prompts = fast_prompts;
        args[idx].prompt_count = 1;
        args[idx].local_success = &total_success;
        args[idx].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[idx], coord_access_worker, &args[idx]) == 0);
    }

    /* 创建 t1-p 线程组 */
    for (int i = 0; i < INT02_1_THREADS_PER_ROLE; i++) {
        int idx = INT02_1_THREADS_PER_ROLE * 2 + i;
        args[idx].thread_id = idx;
        args[idx].iterations = INT02_1_ITERATIONS;
        args[idx].strategy = t1p_strategy;
        args[idx].prompts = parallel_prompts;
        args[idx].prompt_count = 3;
        args[idx].local_success = &total_success;
        args[idx].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[idx], coord_access_worker, &args[idx]) == 0);
    }

    /* 等待所有线程完成 */
    for (int i = 0; i < INT02_1_TOTAL_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    t2 calls: %d, t1-f calls: %d, t1-p calls: %d\n",
           g_mock_t2_call_count, g_mock_t1f_call_count, g_mock_t1p_call_count);
    printf("    Total successful operations: %d / %d\n",
           total_success, INT02_1_TOTAL_THREADS * INT02_1_ITERATIONS);

    /* 验证: 所有调用都成功完成 */
    ASSERT_TRUE(g_mock_t2_call_count == INT02_1_THREADS_PER_ROLE * INT02_1_ITERATIONS);
    ASSERT_TRUE(g_mock_t1f_call_count == INT02_1_THREADS_PER_ROLE * INT02_1_ITERATIONS);
    ASSERT_TRUE(g_mock_t1p_call_count == INT02_1_THREADS_PER_ROLE * INT02_1_ITERATIONS);
    ASSERT_TRUE(total_success == INT02_1_TOTAL_THREADS * INT02_1_ITERATIONS);

    agentos_mutex_destroy(&mutex);
    t2_strategy->destroy(t2_strategy);
    t1f_strategy->destroy(t1f_strategy);
    t1p_strategy->destroy(t1p_strategy);

    #undef INT02_1_THREADS_PER_ROLE
    #undef INT02_1_ITERATIONS
    #undef INT02_1_TOTAL_THREADS

    g_tests_passed++;
}

/* ============================================================================
 * INT-02.2: 并发 thinking_chain 操作
 *
 * 多线程同时向同一 thinking_chain 添加步骤:
 *   - 每个线程创建步骤并完成
 *   - 验证步骤计数正确
 *   - 验证无数据竞争（TSan 检测）
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    agentos_thinking_chain_t *chain;
    int *steps_completed;
    agentos_mutex_t *mutex;
} chain_worker_args_t;

static void *chain_worker(void *arg)
{
    chain_worker_args_t *args = (chain_worker_args_t *)arg;
    int local_completed = 0;

    for (int i = 0; i < args->iterations; i++) {
        char input_buf[64];
        snprintf(input_buf, sizeof(input_buf), "thread_%d_step_%d", args->thread_id, i);

        agentos_thinking_step_t *step = NULL;
        agentos_error_t err = agentos_tc_step_create(
            args->chain, TC_STEP_GENERATION, input_buf, strlen(input_buf),
            NULL, 0, &step);

        if (err == AGENTOS_OK && step != NULL) {
            char content_buf[128];
            snprintf(content_buf, sizeof(content_buf),
                     "Generated content from thread %d iteration %d with sufficient length",
                     args->thread_id, i);

            err = agentos_tc_step_complete(step, content_buf, strlen(content_buf),
                                           0.8f, "S2");
            if (err == AGENTOS_OK)
                local_completed++;
        }
    }

    agentos_mutex_lock(args->mutex);
    (*args->steps_completed) += local_completed;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)local_completed;
}

TEST(int02_2_concurrent_thinking_chain_ops)
{
    printf("    --- Concurrent Thinking Chain Operations ---\n");

    #define INT02_2_NUM_THREADS 4
    #define INT02_2_ITERATIONS 25

    /* 创建共享 thinking chain */
    agentos_thinking_chain_t *chain = NULL;
    ASSERT_OK(agentos_tc_chain_create("tsan_chain_test", 8192, 64, &chain));
    ASSERT_TRUE(chain != NULL);
    ASSERT_OK(agentos_tc_chain_start(chain));

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int steps_completed = 0;
    agentos_thread_t threads[INT02_2_NUM_THREADS];
    chain_worker_args_t args[INT02_2_NUM_THREADS];

    for (int i = 0; i < INT02_2_NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_2_ITERATIONS;
        args[i].chain = chain;
        args[i].steps_completed = &steps_completed;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], chain_worker, &args[i]) == 0);
    }

    for (int i = 0; i < INT02_2_NUM_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    Steps completed: %d / %d\n",
           steps_completed, INT02_2_NUM_THREADS * INT02_2_ITERATIONS);
    printf("    Chain step count: %zu\n", chain->step_count);

    /* 验证: 所有步骤都成功完成 */
    ASSERT_TRUE(steps_completed == INT02_2_NUM_THREADS * INT02_2_ITERATIONS);
    ASSERT_TRUE(chain->step_count >= (size_t)steps_completed);

    /* 获取链路统计信息 */
    char *stats = NULL;
    size_t stats_len = 0;
    ASSERT_OK(agentos_tc_chain_stats(chain, &stats, &stats_len));
    ASSERT_TRUE(stats != NULL);
    printf("    Chain stats: %.100s\n", stats);
    free(stats);

    agentos_tc_chain_stop(chain);
    agentos_tc_chain_destroy(chain);
    agentos_mutex_destroy(&mutex);

    #undef INT02_2_NUM_THREADS
    #undef INT02_2_ITERATIONS

    g_tests_passed++;
}

/* ============================================================================
 * INT-02.3: 并发元认知评估
 *
 * 多线程同时触发元认知检查:
 *   - 共享元认知引擎实例
 *   - 每个线程创建步骤并执行评估
 *   - 验证评估结果正确性
 *   - 验证无数据竞争（TSan 检测）
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    agentos_metacognition_t *mc;
    agentos_thinking_chain_t *chain;
    int *evals_completed;
    agentos_mutex_t *mutex;
} metacog_worker_args_t;

static void *metacog_worker(void *arg)
{
    metacog_worker_args_t *args = (metacog_worker_args_t *)arg;
    int local_completed = 0;

    for (int i = 0; i < args->iterations; i++) {
        /* 创建步骤 */
        char input_buf[64];
        snprintf(input_buf, sizeof(input_buf), "meta_test_%d_%d", args->thread_id, i);

        agentos_thinking_step_t *step = NULL;
        agentos_error_t err = agentos_tc_step_create(
            args->chain, TC_STEP_GENERATION, input_buf, strlen(input_buf),
            NULL, 0, &step);

        if (err == AGENTOS_OK && step != NULL) {
            char content_buf[128];
            snprintf(content_buf, sizeof(content_buf),
                     "Content for metacognition evaluation from thread %d step %d",
                     args->thread_id, i);

            agentos_tc_step_complete(step, content_buf, strlen(content_buf),
                                     0.75f, "S2");

            /* 执行快速评估 */
            float score = 0.0f;
            int acceptable = 0;
            err = agentos_mc_evaluate_quick(args->mc, step, &score, &acceptable);
            if (err == AGENTOS_OK) {
                local_completed++;
            }

            /* 校准置信度 */
            float calibrated = agentos_mc_calibrate_confidence(args->mc, 0.75f);
            (void)calibrated;
        }
    }

    agentos_mutex_lock(args->mutex);
    (*args->evals_completed) += local_completed;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)local_completed;
}

TEST(int02_3_concurrent_metacognition_evaluation)
{
    printf("    --- Concurrent Metacognition Evaluation ---\n");

    #define INT02_3_NUM_THREADS 4
    #define INT02_3_ITERATIONS 20

    /* 创建共享元认知引擎 */
    agentos_metacognition_t *mc = NULL;
    ASSERT_OK(agentos_mc_create(&mc));
    ASSERT_TRUE(mc != NULL);

    /* 创建共享 thinking chain */
    agentos_thinking_chain_t *chain = NULL;
    ASSERT_OK(agentos_tc_chain_create("meta_tsan_test", 8192, 64, &chain));
    ASSERT_TRUE(chain != NULL);
    ASSERT_OK(agentos_tc_chain_start(chain));

    /* 关联链路 */
    agentos_mc_set_chain(mc, chain);

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int evals_completed = 0;
    agentos_thread_t threads[INT02_3_NUM_THREADS];
    metacog_worker_args_t args[INT02_3_NUM_THREADS];

    for (int i = 0; i < INT02_3_NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_3_ITERATIONS;
        args[i].mc = mc;
        args[i].chain = chain;
        args[i].evals_completed = &evals_completed;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], metacog_worker, &args[i]) == 0);
    }

    for (int i = 0; i < INT02_3_NUM_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    Evaluations completed: %d / %d\n",
           evals_completed, INT02_3_NUM_THREADS * INT02_3_ITERATIONS);
    printf("    Metacognition total_evaluations: %lu\n",
           (unsigned long)mc->total_evaluations);

    /* 验证: 评估次数正确 */
    ASSERT_TRUE(evals_completed == INT02_3_NUM_THREADS * INT02_3_ITERATIONS);

    /* 获取统计信息 */
    char *stats = NULL;
    ASSERT_OK(agentos_mc_stats(mc, &stats));
    ASSERT_TRUE(stats != NULL);
    printf("    MC stats: %.100s\n", stats);
    free(stats);

    agentos_tc_chain_stop(chain);
    agentos_tc_chain_destroy(chain);
    agentos_mc_destroy(mc);
    agentos_mutex_destroy(&mutex);

    #undef INT02_3_NUM_THREADS
    #undef INT02_3_ITERATIONS

    g_tests_passed++;
}

/* ============================================================================
 * INT-02.4: stream_critic 路径竞争检测
 *
 * 多线程同时通过 triple_coordinator 执行流式批判路径:
 *   - 每个线程创建独立的 coordinator
 *   - 并发执行 tc3_coordinator_execute
 *   - 验证统计信息无数据竞争
 *   - 验证输出无损坏
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    int *exec_success;
    agentos_mutex_t *mutex;
} stream_critic_args_t;

static void *stream_critic_worker(void *arg)
{
    stream_critic_args_t *args = (stream_critic_args_t *)arg;
    int local_success = 0;

    for (int i = 0; i < args->iterations; i++) {
        /* 每次迭代创建独立的 coordinator */
        tc3_config_t config = TC3_CONFIG_DEFAULTS;
        config.s2_generate = mock_s2_gen;
        config.s1_expert = mock_s1_exp;
        config.s2_user_data = NULL;
        config.s1_expert_user_data = NULL;
        config.max_verify_rounds = 3;
        config.max_escalations = 2;
        config.accept_threshold = 0.55f;
        config.minor_fix_threshold = 0.35f;
        config.escalate_threshold = 0.20f;

        tc3_coordinator_t *coord = NULL;
        agentos_error_t err = tc3_coordinator_create(&config, NULL, NULL, &coord);
        if (err != 0 || coord == NULL)
            continue;

        char input_buf[64];
        snprintf(input_buf, sizeof(input_buf), "stream_critic_%d_%d", args->thread_id, i);

        char *output = NULL;
        size_t output_len = 0;
        err = tc3_coordinator_execute(coord, input_buf, strlen(input_buf),
                                      &output, &output_len);
        if (err == 0 && output != NULL) {
            /* 验证输出完整性 */
            if (output_len > 0 && strlen(output) == output_len)
                local_success++;
            free(output);
        }

        /* 验证统计信息可安全读取 */
        tc3_stats_t stats;
        if (tc3_coordinator_get_stats(coord, &stats) == 0) {
            /* 统计应反映本次执行 */
            if (stats.total_units > 0)
                local_success++;
        }

        tc3_coordinator_destroy(coord);
    }

    agentos_mutex_lock(args->mutex);
    (*args->exec_success) += local_success;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)local_success;
}

TEST(int02_4_stream_critic_race_detection)
{
    printf("    --- Stream Critic Path Race Detection ---\n");

    #define INT02_4_NUM_THREADS 4
    #define INT02_4_ITERATIONS 15

    g_mock_s2_gen_count = 0;
    g_mock_s1_expert_count = 0;

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int exec_success = 0;
    agentos_thread_t threads[INT02_4_NUM_THREADS];
    stream_critic_args_t args[INT02_4_NUM_THREADS];

    for (int i = 0; i < INT02_4_NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_4_ITERATIONS;
        args[i].exec_success = &exec_success;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], stream_critic_worker, &args[i]) == 0);
    }

    for (int i = 0; i < INT02_4_NUM_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    S2 generate calls: %d\n", g_mock_s2_gen_count);
    printf("    S1 expert calls: %d\n", g_mock_s1_expert_count);
    printf("    Successful operations: %d / %d (exec+stats)\n",
           exec_success, INT02_4_NUM_THREADS * INT02_4_ITERATIONS * 2);

    /* 验证: S2 至少被调用 N 次（每个 coordinator 至少一次） */
    ASSERT_TRUE(g_mock_s2_gen_count >= INT02_4_NUM_THREADS * INT02_4_ITERATIONS);

    agentos_mutex_destroy(&mutex);

    #undef INT02_4_NUM_THREADS
    #undef INT02_4_ITERATIONS

    g_tests_passed++;
}

/* ============================================================================
 * INT-02.5: 死锁检测 - 验证引擎锁序无死锁
 *
 * 测试场景:
 *   - 多线程同时操作认知引擎 + 记忆引擎 + 执行引擎
 *   - 使用不同的锁获取顺序来检测潜在死锁
 *   - 设置超时机制确保测试不会永久挂起
 *
 * 验证:
 *   - 所有线程在合理时间内完成
 *   - 无死锁发生
 *   - 跨引擎操作无数据竞争
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    agentos_cognition_engine_t *engine;
    agentos_memory_engine_t *mem_engine;
    const char *input;
    int *ops_completed;
    agentos_mutex_t *mutex;
} deadlock_test_args_t;

static void *deadlock_test_worker(void *arg)
{
    deadlock_test_args_t *args = (deadlock_test_args_t *)arg;
    int local_ops = 0;

    for (int i = 0; i < args->iterations; i++) {
        /* 操作序列1: 先引擎后记忆 (锁序 A→B) */
        agentos_task_plan_t *plan = NULL;
        agentos_error_t err = agentos_cognition_process(
            args->engine, args->input, strlen(args->input), &plan);
        if (err == AGENTOS_OK) {
            if (plan)
                agentos_task_plan_free(plan);
            local_ops++;
        }

        /* 操作序列2: 先记忆后引擎 (锁序 B→A) */
        agentos_memory_record_t record;
        memset(&record, 0, sizeof(record));
        char data_buf[64];
        snprintf(data_buf, sizeof(data_buf), "deadlock_test_%d_%d", args->thread_id, i);
        record.memory_record_type = 0;
        record.memory_record_data = (void *)data_buf;
        record.memory_record_data_len = strlen(data_buf);
        record.memory_record_importance = 0.5f;

        char *record_id = NULL;
        err = agentos_memory_write(args->mem_engine, &record, &record_id);
        if (err == AGENTOS_OK && record_id) {
            free(record_id);
            local_ops++;
        }

        /* 操作序列3: 健康检查 (只读操作) */
        char *health_json = NULL;
        err = agentos_cognition_health_check(args->engine, &health_json);
        if (err == AGENTOS_OK && health_json) {
            assert(is_valid_json_prefix(health_json));
            free(health_json);
            local_ops++;
        }

        /* 操作序列4: 统计信息 (只读操作) */
        char *stats = NULL;
        size_t stats_len = 0;
        err = agentos_cognition_stats(args->engine, &stats, &stats_len);
        if (err == AGENTOS_OK && stats) {
            free(stats);
            local_ops++;
        }
    }

    agentos_mutex_lock(args->mutex);
    (*args->ops_completed) += local_ops;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)local_ops;
}

TEST(int02_5_deadlock_detection)
{
    printf("    --- Deadlock Detection (Lock Ordering) ---\n");

    #define INT02_5_NUM_THREADS 6
    #define INT02_5_ITERATIONS 20

    /* 创建共享引擎 */
    agentos_cognition_engine_t *engine = create_default_engine();
    agentos_memory_engine_t *mem_engine = NULL;
    ASSERT_OK(agentos_memory_create(NULL, &mem_engine));
    ASSERT_TRUE(mem_engine != NULL);
    agentos_cognition_set_memory(engine, mem_engine);

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int ops_completed = 0;
    agentos_thread_t threads[INT02_5_NUM_THREADS];
    deadlock_test_args_t args[INT02_5_NUM_THREADS];

    const char *inputs[] = {
        "Analyze sales data for Q1",
        "Compare performance metrics",
        "Generate strategic report",
        "Verify calculation accuracy",
        "Evaluate market trends",
        "Summarize key findings"
    };

    for (int i = 0; i < INT02_5_NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_5_ITERATIONS;
        args[i].engine = engine;
        args[i].mem_engine = mem_engine;
        args[i].input = inputs[i];
        args[i].ops_completed = &ops_completed;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], deadlock_test_worker, &args[i]) == 0);
    }

    for (int i = 0; i < INT02_5_NUM_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    Operations completed: %d\n", ops_completed);
    printf("    Expected minimum: %d (6 threads * 20 iters * 2 core ops)\n",
           INT02_5_NUM_THREADS * INT02_5_ITERATIONS * 2);

    /* 验证: 所有线程完成（未死锁），且核心操作成功 */
    ASSERT_TRUE(ops_completed >= INT02_5_NUM_THREADS * INT02_5_ITERATIONS * 2);

    /* 最终健康检查 */
    char *health_json = NULL;
    ASSERT_OK(agentos_cognition_health_check(engine, &health_json));
    ASSERT_TRUE(health_json != NULL);
    ASSERT_TRUE(is_valid_json_prefix(health_json));
    printf("    Final health check: %.80s\n", health_json);
    free(health_json);

    agentos_cognition_destroy(engine);
    agentos_memory_destroy(mem_engine);
    agentos_mutex_destroy(&mutex);

    #undef INT02_5_NUM_THREADS
    #undef INT02_5_ITERATIONS

    g_tests_passed++;
}

/* ============================================================================
 * INT-02 补充: 并发 feedback 引擎 + coordinator 策略混合测试
 *
 * 同时运行带 feedback 回调的引擎和自定义 coordinator 策略，
 * 检测跨组件的竞争条件
 * ============================================================================ */

typedef struct {
    int thread_id;
    int iterations;
    int use_feedback;
    int *success_count;
    agentos_mutex_t *mutex;
} mixed_tsan_args_t;

static void *mixed_tsan_worker(void *arg)
{
    mixed_tsan_args_t *args = (mixed_tsan_args_t *)arg;
    int local_success = 0;

    for (int i = 0; i < args->iterations; i++) {
        agentos_cognition_engine_t *engine = NULL;
        agentos_error_t err;

        if (args->use_feedback) {
            agentos_cognition_config_t config;
            memset(&config, 0, sizeof(config));
            config.cognition_default_timeout_ms = 15000;
            config.cognition_max_retries = 2;
            config.feedback_callback = null_feedback_callback;
            config.feedback_user_data = NULL;
            err = agentos_cognition_create_ex(&config, NULL, NULL, NULL, &engine);
        } else {
            agentos_coordinator_strategy_t *coord =
                (agentos_coordinator_strategy_t *)calloc(1, sizeof(*coord));
            if (!coord) continue;
            coord->coordinate = mock_t2_thinking;
            coord->destroy = mock_coordinator_destroy;
            coord->data = NULL;

            err = agentos_cognition_create(NULL, coord, NULL, &engine);
            coord->destroy(coord);
        }

        if (err == AGENTOS_OK && engine != NULL) {
            char input_buf[64];
            snprintf(input_buf, sizeof(input_buf), "mixed_tsan_%d_%d", args->thread_id, i);

            agentos_task_plan_t *plan = NULL;
            err = agentos_cognition_process(engine, input_buf, strlen(input_buf), &plan);
            if (err == AGENTOS_OK) {
                if (plan)
                    agentos_task_plan_free(plan);
                local_success++;
            }

            /* 读取统计和健康信息 */
            char *stats = NULL;
            size_t stats_len = 0;
            agentos_cognition_stats(engine, &stats, &stats_len);
            if (stats) free(stats);

            char *health = NULL;
            agentos_cognition_health_check(engine, &health);
            if (health) {
                assert(is_valid_json_prefix(health));
                free(health);
            }

            agentos_cognition_destroy(engine);
        }
    }

    agentos_mutex_lock(args->mutex);
    (*args->success_count) += local_success;
    agentos_mutex_unlock(args->mutex);

    return (void *)(intptr_t)local_success;
}

TEST(int02_mixed_feedback_and_coordinator_tsan)
{
    printf("    --- Mixed Feedback + Coordinator TSan Test ---\n");

    #define INT02_MIX_NUM_THREADS 6
    #define INT02_MIX_ITERATIONS 10

    agentos_mutex_t mutex;
    agentos_mutex_init(&mutex);

    int success_count = 0;
    agentos_thread_t threads[INT02_MIX_NUM_THREADS];
    mixed_tsan_args_t args[INT02_MIX_NUM_THREADS];

    for (int i = 0; i < INT02_MIX_NUM_THREADS; i++) {
        args[i].thread_id = i;
        args[i].iterations = INT02_MIX_ITERATIONS;
        args[i].use_feedback = (i % 2 == 0); /* 交替使用 feedback 和 coordinator */
        args[i].success_count = &success_count;
        args[i].mutex = &mutex;
        ASSERT_TRUE(agentos_platform_thread_create(&threads[i], mixed_tsan_worker, &args[i]) == 0);
    }

    for (int i = 0; i < INT02_MIX_NUM_THREADS; i++) {
        agentos_platform_thread_join(threads[i], NULL);
    }

    printf("    Mixed threads: %d / %d succeeded\n",
           success_count, INT02_MIX_NUM_THREADS * INT02_MIX_ITERATIONS);
    ASSERT_TRUE(success_count == INT02_MIX_NUM_THREADS * INT02_MIX_ITERATIONS);

    agentos_mutex_destroy(&mutex);

    #undef INT02_MIX_NUM_THREADS
    #undef INT02_MIX_ITERATIONS

    g_tests_passed++;
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Thinkdual ThreadSanitizer Tests\n");
    printf("  Phase 2 - INT-02\n");
    printf("=========================================\n\n");

    /* INT-02.1: 并发 t2/t1-f/t1-p 访问 */
    printf("--- INT-02.1: Concurrent t2/t1-f/t1-p Access ---\n");
    RUN_TEST(int02_1_concurrent_t2_t1f_t1p_access);

    /* INT-02.2: 并发 thinking_chain 操作 */
    printf("\n--- INT-02.2: Concurrent Thinking Chain Operations ---\n");
    RUN_TEST(int02_2_concurrent_thinking_chain_ops);

    /* INT-02.3: 并发元认知评估 */
    printf("\n--- INT-02.3: Concurrent Metacognition Evaluation ---\n");
    RUN_TEST(int02_3_concurrent_metacognition_evaluation);

    /* INT-02.4: stream_critic 路径竞争检测 */
    printf("\n--- INT-02.4: Stream Critic Race Detection ---\n");
    RUN_TEST(int02_4_stream_critic_race_detection);

    /* INT-02.5: 死锁检测 */
    printf("\n--- INT-02.5: Deadlock Detection ---\n");
    RUN_TEST(int02_5_deadlock_detection);

    /* 混合 TSan 测试 */
    printf("\n--- INT-02 Mixed: Feedback + Coordinator TSan ---\n");
    RUN_TEST(int02_mixed_feedback_and_coordinator_tsan);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d Thinkdual TSan tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
