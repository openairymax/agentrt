/**
 * @file test_cognition_example.c
 * @brief Cognition 引擎测试示例（使用新的断言宏）
 * @version 1.0.0.9
 * @date 2026-04-04
 * 
 * 本文件演示如何使用 test_macros.h 中的断言宏来编写 C 单元测试。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_macros.h"

/**
 * @brief 测试示例：空指针检查
 */
void test_null_pointer_check() {
    TEST_CASE_START(test_null_pointer_check);
    
    void *ptr = NULL;
    TEST_ASSERT_NULL(ptr, "指针应该为 NULL");
    
    ptr = malloc(100);
    TEST_ASSERT_NOT_NULL(ptr, "malloc 应该返回非 NULL 指针");
    
    if (ptr != NULL) {
        free(ptr);
    }
    
    TEST_CASE_END();
}

/**
 * @brief 测试示例：整数比较
 */
void test_integer_comparison() {
    TEST_CASE_START(test_integer_comparison);
    
    int expected = 42;
    int actual = 42;
    TEST_ASSERT_EQUAL_INT(expected, actual, "值应该相等");
    
    int result = 20 + 22;
    TEST_ASSERT_EQUAL_INT(42, result, "计算结果应该为 42");
    
    TEST_CASE_END();
}

/**
 * @brief 测试示例：字符串比较
 */
void test_string_comparison() {
    TEST_CASE_START(test_string_comparison);
    
    const char *expected = "AgentOS";
    char actual[20];
    strcpy(actual, "AgentOS");
    
    TEST_ASSERT_EQUAL_STRING(expected, actual, "字符串应该相等");
    
    TEST_CASE_END();
}

/**
 * @brief 测试示例：布尔值检查
 */
void test_boolean_check() {
    TEST_CASE_START(test_boolean_check);
    
    bool condition = true;
    TEST_ASSERT_TRUE(condition, "条件应该为真");
    
    condition = false;
    TEST_ASSERT_FALSE(condition, "条件应该为假");
    
    TEST_CASE_END();
}

/**
 * @brief 测试示例：错误码检查
 */
void test_error_code_check() {
    TEST_CASE_START(test_error_code_check);
    
    // 模拟成功的情况
    int success_code = 0;
    TEST_ASSERT_SUCCESS(success_code, "操作应该成功");
    
    // 模拟失败的情况
    int error_code = -1;
    TEST_ASSERT_FAILED(error_code, "操作应该失败");
    
    TEST_CASE_END();
}

/**
 * @brief 主测试函数
 */
int main(int argc, char *argv[]) {
    printf("============================================================\n");
    printf("AgentOS Cognition 引擎单元测试\n");
    printf("使用 test_macros.h 断言框架\n");
    printf("============================================================\n");
    
    // 运行所有测试
    RUN_TEST(test_null_pointer_check);
    RUN_TEST(test_integer_comparison);
    RUN_TEST(test_string_comparison);
    RUN_TEST(test_boolean_check);
    RUN_TEST(test_error_code_check);
    
    // 打印统计结果
    PRINT_TEST_STATS();
    
    // 返回测试结果
    return TESTS_PASSED() ? EXIT_SUCCESS : EXIT_FAILURE;
}
