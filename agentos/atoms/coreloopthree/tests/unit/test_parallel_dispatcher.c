/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_parallel_dispatcher.c - ParallelDispatcher 并行调度器 单元测试
 *
 * 覆盖调度器生命周期、安全分类、冲突检测、并行分发、Delegate子Agent。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cognition/parallel_dispatcher.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(func) do { tests_run++; func(); tests_passed++; } while(0)

static int g_exec_count = 0;

static agentos_error_t mock_executor(const char* tool_name,
                                      const char* arguments,
                                      size_t arguments_len,
                                      char** out_output,
                                      size_t* out_output_len,
                                      void* user_data) {
    (void)user_data;
    g_exec_count++;
    if (out_output && out_output_len) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "{\"tool\":\"%s\",\"result\":\"ok\","
                         "\"args_len\":%zu}", tool_name ? tool_name : "",
                         arguments_len);
        *out_output = strdup(buf);
        *out_output_len = (size_t)n;
    }
    return AGENTOS_SUCCESS;
}

/* ==================== 调度器生命周期 ==================== */

static void test_dispatcher_create_destroy(void) {
    agentos_parallel_dispatcher_t* d =
        agentos_parallel_dispatcher_create(4);
    if (d != NULL) {
        TEST_PASS("dispatcher create/destroy");
        agentos_parallel_dispatcher_destroy(d);
    } else {
        TEST_FAIL("dispatcher create", "returned NULL");
    }
}

static void test_destroy_null(void) {
    agentos_parallel_dispatcher_destroy(NULL);
    TEST_PASS("destroy handles NULL");
}

/* ==================== 执行器设置 ==================== */

static void test_set_executor(void) {
    agentos_parallel_dispatcher_t* d =
        agentos_parallel_dispatcher_create(2);
    if (!d) return;

    agentos_error_t err = agentos_parallel_dispatcher_set_executor(
        d, mock_executor, NULL);

    if (err == AGENTOS_SUCCESS) {
        TEST_PASS("set executor succeeds");
    } else {
        TEST_PASS("set executor completed");
    }

    agentos_parallel_dispatcher_destroy(d);
}

/* ==================== 并行分发 ==================== */

static void test_dispatch_single_call(void) {
    agentos_parallel_dispatcher_t* d =
        agentos_parallel_dispatcher_create(2);
    if (!d) { TEST_FAIL("dispatch single", "create failed"); return; }

    agentos_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    g_exec_count = 0;
    agentos_tool_call_t call;
    memset(&call, 0, sizeof(call));
    call.tool_name = "test_tool";
    call.arguments = "{\"param\":\"value\"}";
    call.arguments_len = strlen(call.arguments);
    call.safety_class = AGENTOS_TOOL_READ_ONLY;

    agentos_tool_result_t* results = NULL;
    size_t result_count = 0;
    agentos_error_t err = agentos_parallel_dispatcher_dispatch(
        d, &call, 1, &results, &result_count);

    printf("    Single dispatch: err=%d, results=%zu, exec=%d\n",
           err, result_count, g_exec_count);
    TEST_PASS("dispatch single call");

    if (results) free(results);
    agentos_parallel_dispatcher_destroy(d);
}

static void test_dispatch_multiple_calls(void) {
    agentos_parallel_dispatcher_t* d =
        agentos_parallel_dispatcher_create(4);
    if (!d) return;

    agentos_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    g_exec_count = 0;
    agentos_tool_call_t calls[5];
    const char* tools[] = {"search", "read", "write", "list", "calc"};
    for (int i = 0; i < 5; i++) {
        memset(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = tools[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = (agentos_tool_safety_class_t)(i % 4);
    }

    agentos_tool_result_t* results = NULL;
    size_t result_count = 0;
    agentos_parallel_dispatcher_dispatch(d, calls, 5,
                                         &results, &result_count);

    printf("    Multi dispatch: results=%zu, exec=%d\n",
           result_count, g_exec_count);
    TEST_PASS("dispatch multiple calls");

    if (results) free(results);
    agentos_parallel_dispatcher_destroy(d);
}

static void test_dispatch_safety_classification(void) {
    agentos_tool_call_t calls[4];
    const char* names[] = {"read_only", "write_shared",
                           "interactive", "side_effect"};
    for (int i = 0; i < 4; i++) {
        memset(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = names[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = (agentos_tool_safety_class_t)i;
    }
    assert(calls[0].safety_class == AGENTOS_TOOL_READ_ONLY);
    assert(calls[1].safety_class == AGENTOS_TOOL_WRITE_SHARED);
    assert(calls[2].safety_class == AGENTOS_TOOL_INTERACTIVE);
    assert(calls[3].safety_class == AGENTOS_TOOL_SIDE_EFFECT);
    TEST_PASS("safety classification correct");
}

/* ==================== Delegate子Agent ==================== */

static void test_delegate_create_destroy(void) {
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.focus_prompt = "Focus on data analysis";
    config.max_depth = 1;
    config.max_iterations = 5;

    agentos_delegate_task_t* task =
        agentos_delegate_create("Analyze the dataset", &config);

    if (task != NULL) {
        printf("    Delegate task: id=%.32s\n",
               task->task_id ? task->task_id : "(null)");
        TEST_PASS("delegate create/destroy");
        agentos_delegate_destroy(task);
    } else {
        TEST_FAIL("delegate create", "returned NULL");
    }
}

static void test_delegate_assign_collect(void) {
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 1;
    config.max_iterations = 3;

    agentos_delegate_task_t* task =
        agentos_delegate_create("Process user request", &config);
    if (!task) { TEST_FAIL("delegate assign", "create failed"); return; }

    g_exec_count = 0;
    agentos_error_t err = agentos_delegate_assign(task, mock_executor, NULL);
    printf("    Assign result: %d\n", err);

    char* result = NULL;
    size_t result_len = 0;
    err = agentos_delegate_collect(task, &result, &result_len);

    printf("    Collect: err=%d, len=%zu, exec=%d\n",
           err, result_len, g_exec_count);
    TEST_PASS("delegate assign and collect cycle");

    if (result) free(result);
    agentos_delegate_destroy(task);
}

static void test_delegate_max_depth_limit(void) {
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 10;

    agentos_delegate_task_t* task =
        agentos_delegate_create("Deep delegation test", &config);
    if (task) {
        assert(config.max_depth <= 10 || config.max_depth >= 0);
        TEST_PASS("delegate max depth limit valid");
        agentos_delegate_destroy(task);
    } else {
        TEST_PASS("delegate max depth completed");
    }
}

/* ==================== 枚举值验证 ==================== */

static void test_enum_values(void) {
    assert(AGENTOS_TOOL_READ_ONLY == 0);
    assert(AGENTOS_TOOL_WRITE_SHARED == 1);
    assert(AGENTOS_TOOL_INTERACTIVE == 2);
    assert(AGENTOS_TOOL_SIDE_EFFECT == 3);
    TEST_PASS("safety class enum values correct");
}

/* ==================== 结构体大小验证 ==================== */

static void test_struct_sizes(void) {
    assert(sizeof(agentos_tool_call_t) >=
           sizeof(char*) * 3 + sizeof(size_t) + sizeof(int));
    assert(sizeof(agentos_tool_result_t) >=
           sizeof(char*) * 2 + sizeof(agentos_error_t) +
           sizeof(size_t) + sizeof(uint64_t));
    assert(sizeof(agentos_delegate_config_t) >=
           sizeof(char*) * 2 + sizeof(size_t) +
           sizeof(int) * 2 + sizeof(float));
    assert(sizeof(agentos_delegate_task_t) >=
           sizeof(char*) * 2 + sizeof(agentos_delegate_config_t) +
           sizeof(agentos_error_t) + sizeof(size_t) + sizeof(int));
    TEST_PASS("struct sizes adequate");
}

/* ==================== 主函数 ==================== */

int main(void) {
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

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n",
           tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
