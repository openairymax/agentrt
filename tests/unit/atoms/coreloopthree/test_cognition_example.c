/**
 * @file test_cognition_example.c
 * @brief Cognition 引擎测试示例（使用新的断言宏）
 * @version 0.1.0
 * @date 2026-04-04
 *
 * 本文件演示如何使用 test_macros.h 中的断言宏来编写 C 单元测试。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_macros.h"

void test_null_pointer_check() {
    TEST_CASE_START(test_null_pointer_check);

    void *ptr = NULL;
    TEST_ASSERT_NULL(ptr, "pointer should be NULL");

    ptr = AGENTRT_MALLOC(100);
    TEST_ASSERT_NOT_NULL(ptr, "AGENTRT_MALLOC should return non-NULL");

    if (ptr != NULL) {
        AGENTRT_FREE(ptr);
    }

    TEST_CASE_END();
}

void test_integer_comparison() {
    TEST_CASE_START(test_integer_comparison);

    int expected = 42;
    int actual = 42;
    TEST_ASSERT_EQUAL_INT(expected, actual, "values should be equal");

    int result = 20 + 22;
    TEST_ASSERT_EQUAL_INT(42, result, "calculation result should be 42");

    TEST_CASE_END();
}

void test_string_comparison() {
    TEST_CASE_START(test_string_comparison);

    const char *expected = "AgentOS";
    char actual[20];
    strcpy(actual, "AgentOS");

    TEST_ASSERT_EQUAL_STRING(expected, actual, "strings should be equal");

    TEST_CASE_END();
}

void test_boolean_check() {
    TEST_CASE_START(test_boolean_check);

    bool condition = true;
    TEST_ASSERT_TRUE(condition, "condition should be true");

    condition = false;
    TEST_ASSERT_FALSE(condition, "condition should be false");

    TEST_CASE_END();
}

void test_error_code_check() {
    TEST_CASE_START(test_error_code_check);

    int success_code = 0;
    TEST_ASSERT_SUCCESS(success_code, "operation should succeed");

    int error_code = -1;
    TEST_ASSERT_FAILED(error_code, "operation should fail");

    TEST_CASE_END();
}

int main(int argc, char *argv[]) {
    printf("============================================================\n");
    printf("AgentOS Cognition Engine Unit Tests\n");
    printf("Using test_macros.h assertion framework\n");
    printf("============================================================\n");

    RUN_TEST(test_null_pointer_check);
    RUN_TEST(test_integer_comparison);
    RUN_TEST(test_string_comparison);
    RUN_TEST(test_boolean_check);
    RUN_TEST(test_error_code_check);

    PRINT_TEST_STATS();

    return TESTS_PASSED() ? EXIT_SUCCESS : EXIT_FAILURE;
}
