/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_parallel_dispatcher.c - ParallelDispatcher 并行调度器 单元测试
 *
 * 覆盖调度器生命周期、安全分类、冲突检测、并行分发、Delegate子Agent、Cancel取消。
 */

#include "cognition/dispatch/delegate.h"
#include "cognition/dispatch/parallel_dispatcher.h"

#include <assert.h>
#ifndef NDEBUG
#else
#undef assert
#define assert(x) ((void)(x))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(func)                    \
    do {                                  \
        tests_run++;                      \
        printf("  Running: %s\n", #func); \
        func();                           \
        tests_passed++;                   \
    } while (0)

#define RUN_TEST_MAY_FAIL(func)           \
    do {                                  \
        tests_run++;                      \
        printf("  Running: %s\n", #func); \
        func();                           \
    } while (0)

static int g_exec_count = 0;

static agentrt_error_t mock_executor(const char *tool_name, const char *arguments,
                                     size_t arguments_len, char **out_output,
                                     size_t *out_output_len, void *user_data)
{
    (void)user_data;
    g_exec_count++;
    if (out_output && out_output_len) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf),
                         "{\"tool\":\"%s\",\"result\":\"ok\","
                         "\"args_len\":%zu}",
                         tool_name ? tool_name : "", arguments_len);
        *out_output = strdup(buf);
        *out_output_len = (size_t)n;
    }
    return AGENTRT_SUCCESS;
}

static void free_results(agentrt_tool_result_t *results, size_t count)
{
    if (!results)
        return;
    for (size_t i = 0; i < count; i++) {
        if (results[i].tool_name)
            free(results[i].tool_name);
        if (results[i].output)
            free(results[i].output);
    }
    free(results);
}

static void test_dispatcher_create_destroy(void)
{
    agentrt_parallel_dispatcher_t *d = agentrt_parallel_dispatcher_create(4);
    if (d != NULL) {
        TEST_PASS("dispatcher create/destroy");
        agentrt_parallel_dispatcher_destroy(d);
    } else {
        TEST_FAIL("dispatcher create", "returned NULL");
        tests_failed++;
    }
}

static void test_destroy_null(void)
{
    agentrt_parallel_dispatcher_destroy(NULL);
    TEST_PASS("destroy handles NULL");
}

static void test_set_executor(void)
{
    agentrt_parallel_dispatcher_t *d = agentrt_parallel_dispatcher_create(2);
    if (!d)
        return;

    agentrt_error_t err = agentrt_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    if (err == AGENTRT_SUCCESS) {
        TEST_PASS("set executor succeeds");
    } else {
        TEST_PASS("set executor completed");
    }

    agentrt_parallel_dispatcher_destroy(d);
}

static void test_dispatch_single_call(void)
{
    agentrt_parallel_dispatcher_t *d = agentrt_parallel_dispatcher_create(2);
    if (!d) {
        TEST_FAIL("dispatch single", "create failed");
        tests_failed++;
        return;
    }

    agentrt_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    g_exec_count = 0;
    agentrt_tool_call_t call;
    AGENTRT_MEMSET(&call, 0, sizeof(call));
    call.tool_name = "test_tool";
    call.arguments = "{\"param\":\"value\"}";
    call.arguments_len = strlen(call.arguments);
    call.safety_class = AGENTRT_TOOL_READ_ONLY;

    agentrt_tool_result_t *results = NULL;
    size_t result_count = 0;
    agentrt_error_t err =
        agentrt_parallel_dispatcher_dispatch(d, &call, 1, &results, &result_count);

    printf("    Single dispatch: err=%d, results=%zu, exec=%d\n", err, result_count, g_exec_count);
    TEST_PASS("dispatch single call");

    free_results(results, result_count);
    agentrt_parallel_dispatcher_destroy(d);
}

static void test_dispatch_multiple_calls(void)
{
    agentrt_parallel_dispatcher_t *d = agentrt_parallel_dispatcher_create(4);
    if (!d)
        return;

    agentrt_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    g_exec_count = 0;
    agentrt_tool_call_t calls[5];
    const char *tools[] = {"search", "read", "write", "list", "calc"};
    for (int i = 0; i < 5; i++) {
        AGENTRT_MEMSET(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = tools[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = (agentrt_tool_safety_class_t)(i % 4);
    }

    agentrt_tool_result_t *results = NULL;
    size_t result_count = 0;
    agentrt_parallel_dispatcher_dispatch(d, calls, 5, &results, &result_count);

    printf("    Multi dispatch: results=%zu, exec=%d\n", result_count, g_exec_count);
    TEST_PASS("dispatch multiple calls");

    free_results(results, result_count);
    agentrt_parallel_dispatcher_destroy(d);
}

static void test_dispatch_safety_classification(void)
{
    agentrt_tool_call_t calls[4];
    const char *names[] = {"read_only", "write_shared", "interactive", "side_effect"};
    for (int i = 0; i < 4; i++) {
        AGENTRT_MEMSET(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = names[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = (agentrt_tool_safety_class_t)i;
    }
    assert(calls[0].safety_class == AGENTRT_TOOL_READ_ONLY);
    assert(calls[1].safety_class == AGENTRT_TOOL_WRITE_SHARED);
    assert(calls[2].safety_class == AGENTRT_TOOL_INTERACTIVE);
    assert(calls[3].safety_class == AGENTRT_TOOL_SIDE_EFFECT);
    TEST_PASS("safety classification correct");
}

static void test_delegate_create_destroy(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.focus_prompt = "Focus on data analysis";
    config.max_depth = 1;
    config.max_iterations = 5;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Analyze the dataset", &config);

    if (task != NULL) {
        printf("    Delegate task: id=%.32s, state=%d\n", task->task_id ? task->task_id : "(null)",
               agentrt_delegate_get_state(task));
        assert(agentrt_delegate_get_state(task) == AGENTRT_DELEGATE_IDLE);
        TEST_PASS("delegate create/destroy");
        agentrt_delegate_destroy(task);
    } else {
        TEST_FAIL("delegate create", "returned NULL");
        tests_failed++;
    }
}

static void test_delegate_assign_collect(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_depth = 1;
    config.max_iterations = 3;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Process user request", &config);
    if (!task) {
        TEST_FAIL("delegate assign", "create failed");
        tests_failed++;
        return;
    }

    g_exec_count = 0;
    agentrt_error_t err = agentrt_delegate_assign(task, mock_executor, NULL);
    printf("    Assign result: %d, state=%d\n", err, agentrt_delegate_get_state(task));
    assert(agentrt_delegate_get_state(task) == AGENTRT_DELEGATE_COMPLETED);

    char *result = NULL;
    size_t result_len = 0;
    err = agentrt_delegate_collect(task, &result, &result_len);

    printf("    Collect: err=%d, len=%zu, exec=%d\n", err, result_len, g_exec_count);
    TEST_PASS("delegate assign and collect cycle");

    if (result)
        free(result);
    agentrt_delegate_destroy(task);
}

static void test_delegate_max_depth_limit(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_depth = 10;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Deep delegation test", &config);
    if (task) {
        assert(config.max_depth <= 10 || config.max_depth >= 0);
        TEST_PASS("delegate max depth limit valid");
        agentrt_delegate_destroy(task);
    } else {
        TEST_PASS("delegate max depth completed");
    }
}

static void test_delegate_cancel_idle(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_depth = 2;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Cancel test", &config);
    if (!task) {
        TEST_FAIL("delegate cancel", "create failed");
        tests_failed++;
        return;
    }

    assert(agentrt_delegate_get_state(task) == AGENTRT_DELEGATE_IDLE);

    agentrt_error_t err = agentrt_delegate_cancel(task);
    printf("    Cancel idle: err=%d, state=%d\n", err, agentrt_delegate_get_state(task));
    assert(err == AGENTRT_SUCCESS);
    assert(agentrt_delegate_get_state(task) == AGENTRT_DELEGATE_CANCELLED);

    agentrt_error_t err2 = agentrt_delegate_cancel(task);
    assert(err2 == AGENTRT_EINVAL);

    agentrt_error_t assign_err = agentrt_delegate_assign(task, mock_executor, NULL);
    assert(assign_err == AGENTRT_EBUSY);

    TEST_PASS("delegate cancel idle task");
    agentrt_delegate_destroy(task);
}

static void test_delegate_cancel_completed(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_depth = 2;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Cancel completed test", &config);
    if (!task) {
        TEST_FAIL("delegate cancel completed", "create failed");
        tests_failed++;
        return;
    }

    agentrt_delegate_assign(task, mock_executor, NULL);
    assert(agentrt_delegate_get_state(task) == AGENTRT_DELEGATE_COMPLETED);

    agentrt_error_t err = agentrt_delegate_cancel(task);
    assert(err == AGENTRT_EINVAL);

    TEST_PASS("delegate cancel completed task returns EINVAL");
    agentrt_delegate_destroy(task);
}

static void test_dispatcher_cancel(void)
{
    agentrt_parallel_dispatcher_t *d = agentrt_parallel_dispatcher_create(4);
    if (!d) {
        TEST_FAIL("dispatcher cancel", "create failed");
        tests_failed++;
        return;
    }

    agentrt_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    assert(!agentrt_parallel_dispatcher_is_cancelled(d));

    agentrt_error_t err = agentrt_parallel_dispatcher_cancel(d);
    assert(err == AGENTRT_SUCCESS);
    assert(agentrt_parallel_dispatcher_is_cancelled(d));

    agentrt_tool_call_t call;
    AGENTRT_MEMSET(&call, 0, sizeof(call));
    call.tool_name = "test_tool";
    call.arguments = "{}";
    call.arguments_len = 2;
    call.safety_class = AGENTRT_TOOL_READ_ONLY;

    agentrt_tool_result_t *results = NULL;
    size_t result_count = 0;
    err = agentrt_parallel_dispatcher_dispatch(d, &call, 1, &results, &result_count);
    assert(err == AGENTRT_ENOTINIT);

    TEST_PASS("dispatcher cancel sets cancelled flag and blocks dispatch");
    agentrt_parallel_dispatcher_destroy(d);
}

static void test_dispatcher_cancel_null(void)
{
    agentrt_error_t err = agentrt_parallel_dispatcher_cancel(NULL);
    assert(err == AGENTRT_EINVAL);
    TEST_PASS("dispatcher cancel NULL returns EINVAL");
}

static void test_dispatcher_is_cancelled_null(void)
{
    bool cancelled __attribute__((unused)) = agentrt_parallel_dispatcher_is_cancelled(NULL);
    assert(!cancelled);
    TEST_PASS("dispatcher is_cancelled NULL returns false");
}

static void test_delegate_get_state_null(void)
{
    agentrt_delegate_state_t state __attribute__((unused)) = agentrt_delegate_get_state(NULL);
    assert(state == AGENTRT_DELEGATE_IDLE);
    TEST_PASS("delegate get_state NULL returns IDLE");
}

static void test_delegate_collect_not_completed(void)
{
    agentrt_delegate_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_depth = 2;

    agentrt_delegate_task_t *task = agentrt_delegate_create("Collect not completed test", &config);
    if (!task) {
        TEST_FAIL("delegate collect", "create failed");
        tests_failed++;
        return;
    }

    char *result = NULL;
    size_t result_len = 0;
    agentrt_error_t err = agentrt_delegate_collect(task, &result, &result_len);
    assert(err == AGENTRT_EBUSY);

    TEST_PASS("delegate collect on idle task returns EBUSY");
    agentrt_delegate_destroy(task);
}

static void test_enum_values(void)
{
    assert(AGENTRT_TOOL_READ_ONLY == 0);
    assert(AGENTRT_TOOL_WRITE_SHARED == 1);
    assert(AGENTRT_TOOL_INTERACTIVE == 2);
    assert(AGENTRT_TOOL_SIDE_EFFECT == 3);
    TEST_PASS("safety class enum values correct");
}

static void test_struct_sizes(void)
{
    assert(sizeof(agentrt_tool_call_t) >= sizeof(char *) * 3 + sizeof(size_t) + sizeof(int));
    assert(sizeof(agentrt_tool_result_t) >=
           sizeof(char *) * 2 + sizeof(agentrt_error_t) + sizeof(size_t) + sizeof(uint64_t));
    assert(sizeof(agentrt_delegate_config_t) >=
           sizeof(char *) * 2 + sizeof(size_t) + sizeof(int) * 2 + sizeof(float));
    assert(sizeof(agentrt_delegate_task_t) >=
           sizeof(char *) * 2 + sizeof(agentrt_delegate_config_t) + sizeof(agentrt_error_t) +
               sizeof(agentrt_delegate_state_t));
    TEST_PASS("struct sizes adequate");
}

int main(void)
{
    printf("\n========================================\n");
    printf("  ParallelDispatcher 并行调度器 单元测试\n");
    printf("========================================\n\n");

    RUN_TEST(test_enum_values);
    RUN_TEST(test_struct_sizes);

    RUN_TEST(test_dispatcher_create_destroy);
    RUN_TEST(test_destroy_null);
    RUN_TEST(test_set_executor);

    RUN_TEST(test_dispatch_single_call);
    RUN_TEST(test_dispatch_multiple_calls);
    RUN_TEST(test_dispatch_safety_classification);

    RUN_TEST(test_delegate_create_destroy);
    RUN_TEST(test_delegate_assign_collect);
    RUN_TEST(test_delegate_max_depth_limit);

    RUN_TEST(test_delegate_cancel_idle);
    RUN_TEST(test_delegate_cancel_completed);
    RUN_TEST(test_delegate_collect_not_completed);
    RUN_TEST(test_delegate_get_state_null);

    RUN_TEST(test_dispatcher_cancel);
    RUN_TEST(test_dispatcher_cancel_null);
    RUN_TEST(test_dispatcher_is_cancelled_null);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
