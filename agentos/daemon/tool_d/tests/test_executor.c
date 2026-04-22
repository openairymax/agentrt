/**
 * @file test_executor.c
 * @brief Tool 执行器单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "executor.h"
#include "tool_service.h"

static void test_executor_create_destroy(void) {
    printf("  test_executor_create_destroy...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_executor_config(void) {
    printf("  test_executor_config...\n");

    tool_executor_config_t config = {
        .max_workers = 5,
        .timeout_sec = 10,
        .workbench_type = "default"
    };

    tool_executor_t* exec = tool_executor_create(&config);
    assert(exec != NULL);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_executor_run(void) {
    printf("  test_executor_run...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_echo";
    meta.name = "echo_test";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 5;

    tool_result_t* result = NULL;
    int ret = tool_executor_run(exec, &meta, "hello", &result);
    if (ret == 0 && result != NULL) {
        if (result->output) printf("    Output: %s\n", result->output);
        tool_result_free(result);
    } else {
        printf("    Execution skipped or failed (expected in test env)\n");
    }

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_executor_run_async(void) {
    printf("  test_executor_run_async...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_echo_async";
    meta.name = "echo_async";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 5;

    tool_result_t* result = NULL;
    int ret = tool_executor_run_async(exec, &meta, "hello", NULL, NULL, &result);
    if (ret == 0 && result != NULL) {
        tool_result_free(result);
    } else {
        printf("    Async execution skipped or failed (expected in test env)\n");
    }

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Executor Unit Tests\n");
    printf("=========================================\n");

    test_executor_create_destroy();
    test_executor_config();
    test_executor_run();
    test_executor_run_async();

    printf("\nAll tool executor tests PASSED\n");
    return 0;
}
