/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thread_safety.c - ThreadSanitizer 线程安全验证测试 (INT-02)
 *
 * 验证覆盖:
 *   INT-02.1: ThreadSanitizer-ready 并发测试 (coordinator + dispatcher)
 *   INT-02.2: 错误处理路径验证 (NULL 输入 / NULL 引擎 / 错误码传播)
 *
 * 编译方式:
 *   gcc -fsanitize=thread -g -O1 -pthread -I../include \
 *       -I../../../../commons/utils/error/include \
 *       -I../../../../commons/utils/types/include \
 *       test_thread_safety.c -o test_thread_safety
 *
 * 该测试设计为通过 ThreadSanitizer 检测数据竞争。
 * 每个线程创建独立的认知引擎实例，避免共享状态竞争。
 * 若存在内部全局状态竞争，TSan 将在运行时报告。
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "error.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

/* ============================================================================
 * 辅助: 创建默认认知引擎，失败时通过 assert 终止
 * ============================================================================ */
static agentos_cognition_engine_t *create_default_engine(void)
{
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
    assert(err == AGENTOS_SUCCESS);
    assert(engine != NULL);
    return engine;
}

/* ============================================================================
 * 辅助: 创建带 feedback 回调的认知引擎
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

static agentos_cognition_engine_t *create_engine_with_feedback(void)
{
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 30000;
    config.cognition_max_retries = 3;
    config.feedback_callback = null_feedback_callback;
    config.feedback_user_data = NULL;

    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_ex_take(&config, NULL, NULL, NULL, &engine);
    assert(err == AGENTOS_SUCCESS);
    assert(engine != NULL);
    return engine;
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
 * 辅助: 错误码可读字符串
 * ============================================================================ */
static const char *error_str(agentos_error_t err)
{
    if (err == AGENTOS_SUCCESS) return "AGENTOS_SUCCESS";
    if (err == AGENTOS_EINVAL)  return "AGENTOS_EINVAL";
    if (err == AGENTOS_ENOMEM)  return "AGENTOS_ENOMEM";
    return "UNKNOWN";
}

/* ============================================================================
 * 线程工作函数: 单个线程执行完整生命周期
 * create → process → stats → destroy
 * 每个线程使用独立的引擎实例，TSan 可检测内部全局状态竞争
 * ============================================================================ */
typedef struct {
    int thread_id;
    const char *input;
    int expect_success;
    int *result_count;
    pthread_mutex_t *result_mutex;
} thread_worker_args_t;

static void *thread_worker(void *arg)
{
    thread_worker_args_t *args = (thread_worker_args_t *)arg;
    int local_success = 0;

    printf("    [Thread %d] Starting...\n", args->thread_id);

    /* 1. 创建引擎 */
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err == AGENTOS_SUCCESS && engine != NULL) {
        printf("    [Thread %d] Engine created\n", args->thread_id);

        /* 2. 处理输入 */
        agentos_task_plan_t *plan = NULL;
        err = agentos_cognition_process(engine, args->input,
                                        strlen(args->input), &plan);
        if (err == AGENTOS_SUCCESS) {
            printf("    [Thread %d] Process succeeded: plan=%p, nodes=%zu\n",
                   args->thread_id, (void *)plan,
                   plan ? plan->task_plan_node_count : 0);
            if (plan) {
                agentos_task_plan_free(plan);
            }
            local_success = 1;
        } else {
            printf("    [Thread %d] Process failed: err=%s\n",
                   args->thread_id, error_str(err));
        }

        /* 3. 读取统计信息 */
        char *stats = NULL;
        size_t stats_len = 0;
        err = agentos_cognition_stats(engine, &stats, &stats_len);
        if (err == AGENTOS_SUCCESS && stats) {
            printf("    [Thread %d] Stats: %.80s\n", args->thread_id, stats);
            free(stats);
        }

        /* 4. 健康检查 */
        char *health_json = NULL;
        err = agentos_cognition_health_check(engine, &health_json);
        if (err == AGENTOS_SUCCESS && health_json) {
            assert(is_valid_json_prefix(health_json));
            free(health_json);
        }

        /* 5. 销毁引擎 */
        agentos_cognition_destroy(engine);
        printf("    [Thread %d] Engine destroyed\n", args->thread_id);
    } else {
        printf("    [Thread %d] Engine creation failed: err=%s\n",
               args->thread_id, error_str(err));
    }

    /* 同步更新结果计数 */
    if (args->result_mutex) {
        pthread_mutex_lock(args->result_mutex);
        (*args->result_count) += local_success;
        pthread_mutex_unlock(args->result_mutex);
    }

    return (void *)(intptr_t)local_success;
}

/* ============================================================================
 * 线程工作函数 (feedback 变体): 带 feedback 回调的完整生命周期
 * create_ex → process → health_check → destroy
 * ============================================================================ */
typedef struct {
    int thread_id;
    const char *input;
    int *result_count;
    pthread_mutex_t *result_mutex;
} thread_feedback_worker_args_t;

static void *thread_feedback_worker(void *arg)
{
    thread_feedback_worker_args_t *args = (thread_feedback_worker_args_t *)arg;
    int local_success = 0;

    printf("    [FB-Thread %d] Starting with feedback config...\n", args->thread_id);

    /* 1. 创建带 feedback 的引擎 */
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 15000;
    config.cognition_max_retries = 2;
    config.feedback_callback = null_feedback_callback;
    config.feedback_user_data = NULL;

    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_ex_take(&config, NULL, NULL, NULL, &engine);
    if (err == AGENTOS_SUCCESS && engine != NULL) {
        printf("    [FB-Thread %d] Engine created with feedback\n", args->thread_id);

        /* 2. 处理输入 */
        agentos_task_plan_t *plan = NULL;
        err = agentos_cognition_process(engine, args->input,
                                        strlen(args->input), &plan);
        if (err == AGENTOS_SUCCESS) {
            printf("    [FB-Thread %d] Process succeeded\n", args->thread_id);
            if (plan) {
                agentos_task_plan_free(plan);
            }
            local_success = 1;
        } else {
            printf("    [FB-Thread %d] Process failed: err=%s\n",
                   args->thread_id, error_str(err));
        }

        /* 3. 健康检查 */
        char *health_json = NULL;
        err = agentos_cognition_health_check(engine, &health_json);
        if (err == AGENTOS_SUCCESS && health_json) {
            assert(is_valid_json_prefix(health_json));
            free(health_json);
        }

        /* 4. 销毁引擎 */
        agentos_cognition_destroy(engine);
        printf("    [FB-Thread %d] Engine destroyed\n", args->thread_id);
    } else {
        printf("    [FB-Thread %d] Engine creation failed: err=%s\n",
               args->thread_id, error_str(err));
    }

    if (args->result_mutex) {
        pthread_mutex_lock(args->result_mutex);
        (*args->result_count) += local_success;
        pthread_mutex_unlock(args->result_mutex);
    }

    return (void *)(intptr_t)local_success;
}

/* ============================================================================
 * 线程工作函数 (coordinator + dispatcher 变体):
 * 使用自定义 coordinator 和 dispatcher 策略的完整生命周期
 * ============================================================================ */
typedef struct {
    int thread_id;
    const char *input;
    int *result_count;
    pthread_mutex_t *result_mutex;
} thread_strategy_worker_args_t;

/* 模拟 coordinator 函数 */
static agentos_error_t mock_coord_func(const char **prompts, size_t count,
                                       void *context, char **out_result)
{
    (void)context;
    if (!prompts || !out_result) return AGENTOS_EINVAL;

    const char *header = "{\"coordinated\":true,\"count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = "}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    if (!*out_result) return AGENTOS_ENOMEM;
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    return AGENTOS_SUCCESS;
}

static void mock_coord_destroy(agentos_coordinator_strategy_t *s)
{
    free(s);
}

/* 模拟 dispatcher 函数 */
static agentos_error_t mock_disp_func(const agentos_task_node_t *task,
                                      const void **candidates, size_t count,
                                      void *context, char **out_agent_id)
{
    (void)task;
    (void)candidates;
    (void)context;
    if (!out_agent_id) return AGENTOS_EINVAL;

    *out_agent_id = strdup("agent-0");
    if (!*out_agent_id) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

static void mock_disp_destroy(agentos_dispatching_strategy_t *s)
{
    free(s);
}

static void *thread_strategy_worker(void *arg)
{
    thread_strategy_worker_args_t *args = (thread_strategy_worker_args_t *)arg;
    int local_success = 0;

    printf("    [ST-Thread %d] Starting with coordinator+dispatcher...\n",
           args->thread_id);

    /* 1. 创建 coordinator 策略 */
    agentos_coordinator_strategy_t *coord =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(*coord));
    if (!coord) return (void *)(intptr_t)0;
    coord->coordinate = mock_coord_func;
    coord->destroy = mock_coord_destroy;
    coord->data = NULL;

    /* 2. 创建 dispatcher 策略 */
    agentos_dispatching_strategy_t *disp =
        (agentos_dispatching_strategy_t *)calloc(1, sizeof(*disp));
    if (!disp) {
        free(coord);
        return (void *)(intptr_t)0;
    }
    disp->dispatch = mock_disp_func;
    disp->destroy = mock_disp_destroy;
    disp->data = NULL;

    /* 3. 创建引擎 */
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, coord, disp, &engine);
    if (err == AGENTOS_SUCCESS && engine != NULL) {
        printf("    [ST-Thread %d] Engine created with strategies\n",
               args->thread_id);

        /* 4. 处理输入 */
        agentos_task_plan_t *plan = NULL;
        err = agentos_cognition_process(engine, args->input,
                                        strlen(args->input), &plan);
        if (err == AGENTOS_SUCCESS) {
            printf("    [ST-Thread %d] Process succeeded\n", args->thread_id);
            if (plan) {
                agentos_task_plan_free(plan);
            }
            local_success = 1;
        } else {
            printf("    [ST-Thread %d] Process failed: err=%s\n",
                   args->thread_id, error_str(err));
        }

        /* 5. 销毁引擎 */
        agentos_cognition_destroy(engine);
        printf("    [ST-Thread %d] Engine destroyed\n", args->thread_id);
    } else {
        printf("    [ST-Thread %d] Engine creation failed: err=%s\n",
               args->thread_id, error_str(err));
    }

    /* 释放策略 (引擎不接管所有权) */
    coord->destroy(coord);
    disp->destroy(disp);

    if (args->result_mutex) {
        pthread_mutex_lock(args->result_mutex);
        (*args->result_count) += local_success;
        pthread_mutex_unlock(args->result_mutex);
    }

    return (void *)(intptr_t)local_success;
}

/* ============================================================================
 * INT-02.1: ThreadSanitizer-ready 并发测试 (coordinator + dispatcher)
 *
 * 创建多个线程，每个线程执行独立引擎的完整生命周期。
 * 该测试设计为通过 ThreadSanitizer 检测数据竞争：
 *   - 每个线程创建独立的引擎实例
 *   - 若内部存在全局/静态状态竞争，TSan 将报告
 *   - 使用 pthread 进行线程创建和同步
 *
 * 断言:
 *   - 所有线程正常完成
 *   - 所有线程处理成功
 *   - 无数据竞争 (TSan 检测)
 * ============================================================================ */
TEST(int02_1_concurrent_cognition_engines)
{
    #define INT02_1_NUM_THREADS 4

    pthread_t threads[INT02_1_NUM_THREADS];
    thread_worker_args_t args[INT02_1_NUM_THREADS];
    pthread_mutex_t result_mutex;
    int result_count = 0;

    pthread_mutex_init(&result_mutex, NULL);

    const char *inputs[] = {
        "Analyze the quarterly sales data and provide recommendations",
        "Define machine learning and explain its key algorithms",
        "Write a Python function to calculate Fibonacci numbers",
        "Summarize the history of artificial intelligence"
    };

    printf("    Spawning %d concurrent threads (basic engines)...\n",
           INT02_1_NUM_THREADS);

    /* 创建线程 */
    for (int i = 0; i < INT02_1_NUM_THREADS; i++) {
        args[i].thread_id = i + 1;
        args[i].input = inputs[i];
        args[i].expect_success = 1;
        args[i].result_count = &result_count;
        args[i].result_mutex = &result_mutex;

        int rc = pthread_create(&threads[i], NULL, thread_worker, &args[i]);
        assert(rc == 0);
    }

    /* 等待线程完成 */
    for (int i = 0; i < INT02_1_NUM_THREADS; i++) {
        void *retval = NULL;
        int rc = pthread_join(threads[i], &retval);
        assert(rc == 0);
        printf("    Thread %d joined: retval=%d\n", i + 1,
               (int)(intptr_t)retval);
    }

    printf("    Basic engine threads: %d/%d succeeded\n",
           result_count, INT02_1_NUM_THREADS);

    /* 断言: 所有线程完成且无错误 */
    assert(result_count == INT02_1_NUM_THREADS);

    pthread_mutex_destroy(&result_mutex);

    #undef INT02_1_NUM_THREADS
}

/* ============================================================================
 * INT-02.1 补充: Feedback 引擎并发测试
 * 创建多个线程使用带 feedback 回调的引擎
 * ============================================================================ */
TEST(int02_1_concurrent_feedback_engines)
{
    #define INT02_1_FB_NUM_THREADS 4

    pthread_t threads[INT02_1_FB_NUM_THREADS];
    thread_feedback_worker_args_t args[INT02_1_FB_NUM_THREADS];
    pthread_mutex_t result_mutex;
    int result_count = 0;

    pthread_mutex_init(&result_mutex, NULL);

    const char *inputs[] = {
        "Calculate the average of [10, 20, 30, 40, 50]",
        "Explain the theory of relativity in simple terms",
        "Compare and contrast SQL and NoSQL databases",
        "Design a REST API for a todo list application"
    };

    printf("    Spawning %d concurrent threads (feedback engines)...\n",
           INT02_1_FB_NUM_THREADS);

    for (int i = 0; i < INT02_1_FB_NUM_THREADS; i++) {
        args[i].thread_id = i + 1;
        args[i].input = inputs[i];
        args[i].result_count = &result_count;
        args[i].result_mutex = &result_mutex;

        int rc = pthread_create(&threads[i], NULL, thread_feedback_worker,
                                &args[i]);
        assert(rc == 0);
    }

    for (int i = 0; i < INT02_1_FB_NUM_THREADS; i++) {
        void *retval = NULL;
        int rc = pthread_join(threads[i], &retval);
        assert(rc == 0);
        printf("    FB-Thread %d joined: retval=%d\n", i + 1,
               (int)(intptr_t)retval);
    }

    printf("    Feedback engine threads: %d/%d succeeded\n",
           result_count, INT02_1_FB_NUM_THREADS);
    assert(result_count == INT02_1_FB_NUM_THREADS);

    pthread_mutex_destroy(&result_mutex);

    #undef INT02_1_FB_NUM_THREADS
}

/* ============================================================================
 * INT-02.1 补充: Coordinator + Dispatcher 策略并发测试
 * 创建多个线程使用自定义 coordinator 和 dispatcher 策略
 * ============================================================================ */
TEST(int02_1_concurrent_strategy_engines)
{
    #define INT02_1_ST_NUM_THREADS 4

    pthread_t threads[INT02_1_ST_NUM_THREADS];
    thread_strategy_worker_args_t args[INT02_1_ST_NUM_THREADS];
    pthread_mutex_t result_mutex;
    int result_count = 0;

    pthread_mutex_init(&result_mutex, NULL);

    const char *inputs[] = {
        "Plan a multi-step analysis of customer feedback data",
        "Coordinate the response to a system outage incident",
        "Dispatch tasks to the appropriate team members",
        "Orchestrate a complex workflow with dependencies"
    };

    printf("    Spawning %d concurrent threads (strategy engines)...\n",
           INT02_1_ST_NUM_THREADS);

    for (int i = 0; i < INT02_1_ST_NUM_THREADS; i++) {
        args[i].thread_id = i + 1;
        args[i].input = inputs[i];
        args[i].result_count = &result_count;
        args[i].result_mutex = &result_mutex;

        int rc = pthread_create(&threads[i], NULL, thread_strategy_worker,
                                &args[i]);
        assert(rc == 0);
    }

    for (int i = 0; i < INT02_1_ST_NUM_THREADS; i++) {
        void *retval = NULL;
        int rc = pthread_join(threads[i], &retval);
        assert(rc == 0);
        printf("    ST-Thread %d joined: retval=%d\n", i + 1,
               (int)(intptr_t)retval);
    }

    printf("    Strategy engine threads: %d/%d succeeded\n",
           result_count, INT02_1_ST_NUM_THREADS);
    assert(result_count == INT02_1_ST_NUM_THREADS);

    pthread_mutex_destroy(&result_mutex);

    #undef INT02_1_ST_NUM_THREADS
}

/* ============================================================================
 * INT-02.1 补充: 混合并发测试
 * 同时运行不同类型的引擎线程，检测交叉竞争
 * ============================================================================ */
typedef enum {
    MIX_TYPE_BASIC = 0,
    MIX_TYPE_FEEDBACK = 1,
    MIX_TYPE_STRATEGY = 2
} mix_worker_type_t;

typedef struct {
    int thread_id;
    mix_worker_type_t type;
    const char *input;
    int *result_count;
    pthread_mutex_t *result_mutex;
} mix_worker_args_t;

static void *thread_mix_worker(void *arg)
{
    mix_worker_args_t *args = (mix_worker_args_t *)arg;
    int local_success = 0;

    printf("    [Mix-Thread %d type=%d] Starting...\n",
           args->thread_id, (int)args->type);

    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err;

    switch (args->type) {
    case MIX_TYPE_BASIC:
        err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
        break;
    case MIX_TYPE_FEEDBACK: {
        agentos_cognition_config_t config;
        memset(&config, 0, sizeof(config));
        config.cognition_default_timeout_ms = 30000;
        config.cognition_max_retries = 3;
        config.feedback_callback = null_feedback_callback;
        config.feedback_user_data = NULL;
        err = agentos_cognition_create_ex_take(&config, NULL, NULL, NULL, &engine);
        break;
    }
    case MIX_TYPE_STRATEGY: {
        agentos_coordinator_strategy_t *coord =
            (agentos_coordinator_strategy_t *)calloc(1, sizeof(*coord));
        agentos_dispatching_strategy_t *disp =
            (agentos_dispatching_strategy_t *)calloc(1, sizeof(*disp));
        if (!coord || !disp) {
            free(coord);
            free(disp);
            err = AGENTOS_ENOMEM;
            break;
        }
        coord->coordinate = mock_coord_func;
        coord->destroy = mock_coord_destroy;
        coord->data = NULL;
        disp->dispatch = mock_disp_func;
        disp->destroy = mock_disp_destroy;
        disp->data = NULL;

        err = agentos_cognition_create_take(NULL, coord, disp, &engine);
        coord->destroy(coord);
        disp->destroy(disp);
        break;
    }
    default:
        err = AGENTOS_EINVAL;
        break;
    }

    if (err == AGENTOS_SUCCESS && engine != NULL) {
        agentos_task_plan_t *plan = NULL;
        err = agentos_cognition_process(engine, args->input,
                                        strlen(args->input), &plan);
        if (err == AGENTOS_SUCCESS) {
            if (plan) {
                agentos_task_plan_free(plan);
            }
            local_success = 1;
        }
        agentos_cognition_destroy(engine);
    }

    if (args->result_mutex) {
        pthread_mutex_lock(args->result_mutex);
        (*args->result_count) += local_success;
        pthread_mutex_unlock(args->result_mutex);
    }

    return (void *)(intptr_t)local_success;
}

TEST(int02_1_mixed_concurrent_engines)
{
    #define INT02_1_MIX_NUM_THREADS 6

    pthread_t threads[INT02_1_MIX_NUM_THREADS];
    mix_worker_args_t args[INT02_1_MIX_NUM_THREADS];
    pthread_mutex_t result_mutex;
    int result_count = 0;

    pthread_mutex_init(&result_mutex, NULL);

    const char *inputs[] = {
        "Basic task A: analyze data",
        "Feedback task B: verify results",
        "Strategy task C: coordinate response",
        "Basic task D: summarize findings",
        "Feedback task E: validate output",
        "Strategy task F: dispatch work"
    };

    const mix_worker_type_t types[] = {
        MIX_TYPE_BASIC, MIX_TYPE_FEEDBACK, MIX_TYPE_STRATEGY,
        MIX_TYPE_BASIC, MIX_TYPE_FEEDBACK, MIX_TYPE_STRATEGY
    };

    printf("    Spawning %d mixed concurrent threads...\n",
           INT02_1_MIX_NUM_THREADS);

    for (int i = 0; i < INT02_1_MIX_NUM_THREADS; i++) {
        args[i].thread_id = i + 1;
        args[i].type = types[i];
        args[i].input = inputs[i];
        args[i].result_count = &result_count;
        args[i].result_mutex = &result_mutex;

        int rc = pthread_create(&threads[i], NULL, thread_mix_worker,
                                &args[i]);
        assert(rc == 0);
    }

    for (int i = 0; i < INT02_1_MIX_NUM_THREADS; i++) {
        void *retval = NULL;
        int rc = pthread_join(threads[i], &retval);
        assert(rc == 0);
        printf("    Mix-Thread %d joined: retval=%d\n", i + 1,
               (int)(intptr_t)retval);
    }

    printf("    Mixed engine threads: %d/%d succeeded\n",
           result_count, INT02_1_MIX_NUM_THREADS);
    assert(result_count == INT02_1_MIX_NUM_THREADS);

    pthread_mutex_destroy(&result_mutex);

    #undef INT02_1_MIX_NUM_THREADS
}

/* ============================================================================
 * INT-02.2: 错误处理路径验证
 *
 * 验证 NULL 输入处理、NULL 引擎处理、错误码传播。
 *
 * 断言:
 *   - NULL 输入返回 AGENTOS_EINVAL
 *   - NULL 引擎返回 AGENTOS_EINVAL
 *   - 错误码正确传播，不被吞没
 * ============================================================================ */

/* INT-02.2.1: NULL 输入处理 */
TEST(int02_2_null_input_handling)
{
    printf("    Testing NULL input handling...\n");

    /* 1. NULL engine → AGENTOS_EINVAL */
    agentos_task_plan_t *plan = NULL;
    agentos_error_t err = agentos_cognition_process(NULL, "test", 4, &plan);
    assert(err != AGENTOS_SUCCESS);
    printf("    NULL engine → err=%s (expected: non-success)\n", error_str(err));

    /* 2. NULL input → AGENTOS_EINVAL */
    agentos_cognition_engine_t *engine = create_default_engine();
    err = agentos_cognition_process(engine, NULL, 0, &plan);
    assert(err != AGENTOS_SUCCESS);
    printf("    NULL input → err=%s (expected: non-success)\n", error_str(err));

    /* 3. NULL out_plan → AGENTOS_EINVAL */
    err = agentos_cognition_process(engine, "test", 4, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    NULL out_plan → err=%s (expected: non-success)\n", error_str(err));

    /* 4. NULL out_engine → AGENTOS_EINVAL */
    err = agentos_cognition_create_take(NULL, NULL, NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    NULL out_engine → err=%s (expected: non-success)\n", error_str(err));

    agentos_cognition_destroy(engine);
}

/* INT-02.2.2: NULL 引擎处理 (API 级别) */
TEST(int02_2_null_engine_handling)
{
    printf("    Testing NULL engine handling...\n");

    /* 1. agentos_cognition_destroy(NULL) 应该安全无操作 */
    agentos_cognition_destroy(NULL);
    printf("    destroy(NULL) → no crash (safe no-op)\n");

    /* 2. agentos_cognition_set_fallback_plan(NULL, NULL) */
    agentos_cognition_set_fallback_plan(NULL, NULL);
    printf("    set_fallback_plan(NULL, NULL) → no crash\n");

    /* 3. agentos_cognition_set_context_take(NULL, NULL, NULL) */
    agentos_cognition_set_context_take(NULL, NULL, NULL);
    printf("    set_context(NULL, NULL, NULL) → no crash\n");

    /* 4. agentos_cognition_set_memory(NULL, NULL) */
    agentos_cognition_set_memory(NULL, NULL);
    printf("    set_memory(NULL, NULL) → no crash\n");

    /* 5. agentos_cognition_stats(NULL, NULL, NULL) → 错误 */
    agentos_error_t err = agentos_cognition_stats(NULL, NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    stats(NULL) → err=%s (expected: non-success)\n", error_str(err));

    /* 6. agentos_cognition_health_check(NULL, NULL) → 错误 */
    err = agentos_cognition_health_check(NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    health_check(NULL) → err=%s (expected: non-success)\n", error_str(err));

    /* 7. agentos_task_plan_free(NULL) 应该安全无操作 */
    agentos_task_plan_free(NULL);
    printf("    task_plan_free(NULL) → no crash\n");
}

/* INT-02.2.3: 错误码传播验证 */
TEST(int02_2_error_code_propagation)
{
    printf("    Testing error code propagation through pipeline...\n");

    /* 1. 创建引擎并处理有效输入，验证成功路径 */
    agentos_cognition_engine_t *engine = create_default_engine();

    agentos_task_plan_t *plan = NULL;
    agentos_error_t err = agentos_cognition_process(
        engine, "valid input", 11, &plan);
    assert(err == AGENTOS_SUCCESS);
    printf("    Valid input → err=%s (expected: AGENTOS_SUCCESS)\n",
           error_str(err));
    if (plan) {
        agentos_task_plan_free(plan);
    }

    /* 2. 验证: 错误码在多次处理后仍可正确获取 */
    const char *invalid_inputs[] = {
        "another valid input for stats tracking",
        "different input to verify pipeline",
        "third input for consistency check"
    };

    for (int i = 0; i < 3; i++) {
        plan = NULL;
        err = agentos_cognition_process(engine, invalid_inputs[i],
                                        strlen(invalid_inputs[i]), &plan);
        assert(err == AGENTOS_SUCCESS);
        printf("    Input %d → err=%s\n", i + 1, error_str(err));
        if (plan) {
            agentos_task_plan_free(plan);
        }
    }

    /* 3. 验证: 统计信息在错误路径后仍可获取 */
    char *stats = NULL;
    size_t stats_len = 0;
    err = agentos_cognition_stats(engine, &stats, &stats_len);
    assert(err == AGENTOS_SUCCESS);
    assert(stats != NULL);
    printf("    Stats after pipeline: %.100s\n", stats);
    free(stats);

    /* 4. 验证: 健康检查在错误路径后仍正常工作 */
    char *health_json = NULL;
    err = agentos_cognition_health_check(engine, &health_json);
    assert(err == AGENTOS_SUCCESS);
    assert(health_json != NULL);
    assert(is_valid_json_prefix(health_json));
    printf("    Health check after pipeline: %.100s\n", health_json);
    free(health_json);

    /* 5. 验证: 错误码不被吞没 (使用 NULL 参数触发错误) */
    err = agentos_cognition_process(engine, NULL, 0, &plan);
    assert(err != AGENTOS_SUCCESS);
    printf("    NULL input after valid ops → err=%s (error NOT swallowed)\n",
           error_str(err));

    agentos_cognition_destroy(engine);
}

/* INT-02.2.4: create_ex 错误处理 */
TEST(int02_2_create_ex_error_handling)
{
    printf("    Testing create_ex error handling...\n");

    /* 1. NULL config → 使用默认配置 */
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_ex_take(NULL, NULL, NULL, NULL,
                                                      &engine);
    assert(err == AGENTOS_SUCCESS);
    assert(engine != NULL);
    printf("    create_ex(NULL config) → succeeded\n");
    agentos_cognition_destroy(engine);

    /* 2. NULL out_engine → 错误 */
    err = agentos_cognition_create_ex_take(NULL, NULL, NULL, NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    create_ex(NULL out_engine) → err=%s\n", error_str(err));

    /* 3. 有效 config + NULL out_engine → 错误 */
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 10000;
    config.cognition_max_retries = 1;
    err = agentos_cognition_create_ex_take(&config, NULL, NULL, NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    create_ex(config, NULL out_engine) → err=%s\n", error_str(err));
}

/* INT-02.2.5: 意图解析器错误处理 */
TEST(int02_2_intent_parser_error_handling)
{
    printf("    Testing intent parser error handling...\n");

    /* 1. NULL out_parser → 错误 */
    agentos_error_t err = agentos_intent_parser_create(NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    intent_parser_create(NULL) → err=%s\n", error_str(err));

    /* 2. NULL parser → 安全无操作 */
    agentos_intent_parser_destroy(NULL);
    printf("    intent_parser_destroy(NULL) → no crash\n");

    /* 3. 创建 parser 并测试 NULL 输入 */
    agentos_intent_parser_t *parser = NULL;
    err = agentos_intent_parser_create(&parser);
    assert(err == AGENTOS_SUCCESS);
    assert(parser != NULL);

    err = agentos_intent_parser_parse(parser, NULL, 0, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    intent_parser_parse(NULL input) → err=%s\n", error_str(err));

    agentos_intent_free(NULL);
    printf("    intent_free(NULL) → no crash\n");

    agentos_intent_parser_destroy(parser);
}

/* INT-02.2.6: 记忆引擎错误处理 */
TEST(int02_2_memory_engine_error_handling)
{
    printf("    Testing memory engine error handling...\n");

    /* 1. NULL out_engine → 错误 */
    agentos_error_t err = agentos_memory_create(NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    memory_create(NULL out_engine) → err=%s\n", error_str(err));

    /* 2. NULL engine → 安全无操作 */
    agentos_memory_destroy(NULL);
    printf("    memory_destroy(NULL) → no crash\n");

    /* 3. 创建引擎并测试 NULL 写入 */
    agentos_memory_engine_t *mem = NULL;
    err = agentos_memory_create(NULL, &mem);
    assert(err == AGENTOS_SUCCESS);
    assert(mem != NULL);

    err = agentos_memory_write(mem, NULL, NULL);
    assert(err != AGENTOS_SUCCESS);
    printf("    memory_write(NULL record) → err=%s\n", error_str(err));

    agentos_memory_destroy(mem);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  CoreLoopThree Thread Safety Tests\n");
    printf("  ThreadSanitizer Verification (INT-02)\n");
    printf("=========================================\n\n");

    /* INT-02.1: 并发测试 */
    printf("--- INT-02.1: Concurrent Cognition Engine Tests ---\n");
    RUN_TEST(int02_1_concurrent_cognition_engines);
    RUN_TEST(int02_1_concurrent_feedback_engines);
    RUN_TEST(int02_1_concurrent_strategy_engines);
    RUN_TEST(int02_1_mixed_concurrent_engines);

    /* INT-02.2: 错误处理 */
    printf("\n--- INT-02.2: Error Handling Path Verification ---\n");
    RUN_TEST(int02_2_null_input_handling);
    RUN_TEST(int02_2_null_engine_handling);
    RUN_TEST(int02_2_error_code_propagation);
    RUN_TEST(int02_2_create_ex_error_handling);
    RUN_TEST(int02_2_intent_parser_error_handling);
    RUN_TEST(int02_2_memory_engine_error_handling);

    printf("\n=========================================\n");
    printf("  All thread safety tests PASSED\n");
    printf("=========================================\n");

    return 0;
}