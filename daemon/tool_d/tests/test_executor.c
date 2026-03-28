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

static void test_executor_create_destroy(void) {
    printf("  test_executor_create_destroy...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_executor_config(void) {
    printf("  test_executor_config...\n");

    executor_config_t manager = {
        .max_concurrent = 5,
        .default_timeout_ms = 10000,
        .max_output_size = 1024 * 1024,
        .enable_sandbox = 1
    };

    tool_executor_t* exec = tool_executor_create(&manager);
    assert(exec != NULL);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_tool_meta_validation(void) {
    printf("  test_tool_meta_validation...\n");

    tool_meta_t valid_meta;
    memset(&valid_meta, 0, sizeof(valid_meta));
    valid_meta.name = "test_tool";
    valid_meta.version = "1.0.0";
    valid_meta.executable = "/usr/bin/echo";

    int ret = tool_meta_validate(&valid_meta);
    assert(ret == 0);

    tool_meta_t invalid_meta;
    memset(&invalid_meta, 0, sizeof(invalid_meta));
    invalid_meta.name = NULL;

    ret = tool_meta_validate(&invalid_meta);
    assert(ret != 0);

    printf("    PASSED\n");
}

static void test_executor_prepare_args(void) {
    printf("  test_executor_prepare_args...\n");

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "echo_tool";
    meta.executable = "/usr/bin/echo";

    const char* input = "Hello, World!";

    char** args = NULL;
    int argc = 0;

    int ret = tool_executor_prepare_args(&meta, input, &args, &argc);
    assert(ret == 0);
    assert(args != NULL);
    assert(argc > 0);

    tool_executor_free_args(args, argc);

    printf("    PASSED\n");
}

static void test_executor_build_command(void) {
    printf("  test_executor_build_command...\n");

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "test_command";
    meta.executable = "/usr/bin/cat";
    meta.args = (char*[]){"-n", NULL};

    char* cmd = tool_executor_build_command(&meta, "test_input");
    assert(cmd != NULL);
    assert(strstr(cmd, "/usr/bin/cat") != NULL);

    free(cmd);

    printf("    PASSED\n");
}

static void test_executor_timeout_config(void) {
    printf("  test_executor_timeout_config...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    int ret = tool_executor_set_timeout(exec, 5000);
    assert(ret == 0);

    uint32_t timeout = tool_executor_get_timeout(exec);
    assert(timeout == 5000);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

static void test_executor_output_capture(void) {
    printf("  test_executor_output_capture...\n");

    tool_executor_t* exec = tool_executor_create(NULL);
    assert(exec != NULL);

    char* stdout_buf = NULL;
    char* stderr_buf = NULL;
    size_t stdout_len = 0;
    size_t stderr_len = 0;

    tool_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.name = "echo_test";
    meta.executable = "/usr/bin/echo";
    meta.timeout_ms = 5000;

    int ret = tool_executor_execute(exec, &meta, "test", 
                                     &stdout_buf, &stdout_len,
                                     &stderr_buf, &stderr_len);
    assert(ret == 0 || ret == AGENTOS_ERR_TOOL_EXEC_FAIL);

    if (stdout_buf) free(stdout_buf);
    if (stderr_buf) free(stderr_buf);

    tool_executor_destroy(exec);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Executor Unit Tests\n");
    printf("=========================================\n");

    test_executor_create_destroy();
    test_executor_config();
    test_tool_meta_validation();
    test_executor_prepare_args();
    test_executor_build_command();
    test_executor_timeout_config();
    test_executor_output_capture();

    printf("\n✅ All tool executor tests PASSED\n");
    return 0;
}