/**
 * @file test_string_utils.c
 * @brief 字符串工具模块单元测试
 *
 * 测试字符串操作的安全性、正确性和边界条件
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../utils/test_framework.h"
#include "../../agentos/commons/utils/string/include/string_compat.h"

/**
 * @test 测试安全字符串复制
 */
static void test_safe_strcpy(void** state) {
    char dest[20];
    int result;
    
    // 正常情况
    result = safe_strcpy(dest, sizeof(dest), "Hello");
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("Hello", dest);
    
    // 边界情况 - 恰好合适
    result = safe_strcpy(dest, sizeof(dest), "1234567890123456789");
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_INT_EQUAL(19, (int)strlen(dest));
    
    // 边界情况 - 超出缓冲区
    result = safe_strcpy(dest, sizeof(dest), "This string is too long for the buffer");
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
    
    // NULL输入
    result = safe_strcpy(NULL, sizeof(dest), "test");
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
    
    result = safe_strcpy(dest, sizeof(dest), NULL);
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
}

/**
 * @test 测试安全字符串拼接
 */
static void test_safe_strcat(void** state) {
    char dest[30] = "Hello";
    int result;
    
    // 正常拼接
    result = safe_strcat(dest, sizeof(dest), ", World!");
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("Hello, World!", dest);
    
    // 边界情况 - 接近满
    strncpy(dest, "12345678901234567890", sizeof(dest) - 1);
    dest[sizeof(dest) - 1] = '\0';
    result = safe_strcat(dest, sizeof(dest), "X");
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_INT_EQUAL(21, (int)strlen(dest));
    
    // 超出缓冲区
    result = safe_strcat(dest, sizeof(dest), "This is way too long");
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
}

/**
 * @test 测试安全内存复制
 */
static void test_safe_memcpy(void** state) {
    char src[] = "Test data";
    char dest[20];
    int result;
    
    // 正常复制
    result = safe_memcpy(dest, sizeof(dest), src, strlen(src) + 1);
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_STRING_EQUAL(src, dest);
    
    // 部分复制
    result = safe_memcpy(dest, sizeof(dest), src, 5);
    AGENTOS_TEST_ASSERT_SUCCESS(result);
    AGENTOS_TEST_ASSERT_INT_EQUAL(0, memcmp(dest, src, 5));
    
    // 目标缓冲区不足
    result = safe_memcpy(dest, 5, src, strlen(src) + 1);
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
    
    // NULL指针
    result = safe_memcpy(NULL, sizeof(dest), src, 10);
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
    
    result = safe_memcpy(dest, sizeof(dest), NULL, 10);
    AGENTOS_TEST_ASSERT_FALSE(result == 0);
}

/**
 * @test 测试安全整数运算
 */
static void test_safe_int_operations(void** state) {
    int result;
    
    // 安全加法
    AGENTOS_TEST_ASSERT_SUCCESS(safe_add_int(100, 200, &result));
    AGENTOS_TEST_ASSERT_INT_EQUAL(300, result);
    
    // 加法溢出检测
    AGENTOS_TEST_ASSERT_FALSE(safe_add_int(INT_MAX, 1, &result) == 0);
    
    // 减法溢出检测（通过加法模拟）
    AGENTOS_TEST_ASSERT_FALSE(safe_add_int(INT_MIN, -1, &result) == 0);
    
    // 安全乘法
    AGENTOS_TEST_ASSERT_SUCCESS(safe_mul_int(100, 100, &result));
    AGENTOS_TEST_ASSERT_INT_EQUAL(10000, result);
    
    // 乘法溢出检测
    AGENTOS_TEST_ASSERT_FALSE(safe_mul_int(INT_MAX, 2, &result) == 0);
}

/**
 * @test 测试安全size_t运算
 */
static void test_safe_size_operations(void** state) {
    size_t result;
    
    // 安全加法
    AGENTOS_TEST_ASSERT_SUCCESS(safe_add_size(1000, 2000, &result));
    AGENTOS_TEST_ASSERT_TRUE(result == 3000);
    
    // 加法溢出检测
    AGENTOS_TEST_ASSERT_FALSE(safe_add_size(SIZE_MAX, 1, &result) == 0);
    
    // 安全乘法
    AGENTOS_TEST_ASSERT_SUCCESS(safe_mul_size(1024, 1024, &result));
    AGENTOS_TEST_ASSERT_TRUE(result == 1048576);
    
    // 乘法溢出检测
    AGENTOS_TEST_ASSERT_FALSE(safe_mul_size(SIZE_MAX, 2, &result) == 0);
}

/**
 * @test 测试数组访问安全检查
 */
static void test_array_access_safety(void** state) {
    int array[10] = {0};
    
    // 有效索引
    AGENTOS_TEST_ASSERT_TRUE(is_safe_array_access(5, 10));
    AGENTOS_TEST_ASSERT_TRUE(is_safe_array_access(0, 10));
    AGENTOS_TEST_ASSERT_TRUE(is_safe_array_access(9, 10));
    
    // 无效索引
    AGENTOS_TEST_ASSERT_FALSE(is_safe_array_access(10, 10));
    AGENTOS_TEST_ASSERT_FALSE(is_safe_array_access(100, 10));
    
    // 空数组
    AGENTOS_TEST_ASSERT_FALSE(is_safe_array_access(0, 0));
}

/**
 * @test 测试指针偏移安全检查
 */
static void test_ptr_offset_safety(void** state) {
    char buffer[100] = {0};
    
    // 有效偏移
    AGENTOS_TEST_ASSERT_TRUE(is_safe_ptr_offset(buffer, 50, 100));
    AGENTOS_TEST_ASSERT_TRUE(is_safe_ptr_offset(buffer, 99, 100));
    AGENTOS_TEST_ASSERT_TRUE(is_safe_ptr_offset(buffer, 0, 100));
    
    // 无效偏移
    AGENTOS_TEST_ASSERT_FALSE(is_safe_ptr_offset(buffer, 100, 100));
    AGENTOS_TEST_ASSERT_FALSE(is_safe_ptr_offset(buffer, 101, 100));
    
    // NULL指针
    AGENTOS_TEST_ASSERT_FALSE(is_safe_ptr_offset(NULL, 50, 100));
}

/**
 * @test 测试类型转换安全函数
 */
static void test_type_conversion_safety(void** state) {
    int result;
    size_t size_result;
    
    // int到size_t转换
    AGENTOS_TEST_ASSERT_SUCCESS(safe_int_to_size(42, &size_result));
    AGENTOS_TEST_ASSERT_TRUE(size_result == 42);
    
    // 负数转换失败
    AGENTOS_TEST_ASSERT_FALSE(safe_int_to_size(-1, &size_result) == 0);
    
    // size_t到int转换
    AGENTOS_TEST_ASSERT_SUCCESS(safe_size_to_int((size_t)42, &result));
    AGENTOS_TEST_ASSERT_INT_EQUAL(42, result);
    
    // 大数转换失败
    AGENTOS_TEST_ASSERT_FALSE(safe_size_to_int((size_t)(INT_MAX) + 1, &result) == 0);
    
    // double到int转换
    AGENTOS_TEST_ASSERT_SUCCESS(safe_double_to_int(3.14, &result));
    AGENTOS_TEST_ASSERT_INT_EQUAL(3, result);
    
    // 超范围转换失败
    AGENTOS_TEST_ASSERT_FALSE(safe_double_to_int((double)INT_MAX + 1.0, &result) == 0);
}

/**
 * @test 测试安全strlen函数
 */
static void test_safe_strlen(void** state) {
    AGENTOS_TEST_ASSERT_INT_EQUAL(0, (int)safe_strlen(NULL));
    AGENTOS_TEST_ASSERT_INT_EQUAL(0, (int)safe_strlen(""));
    AGENTOS_TEST_ASSERT_INT_EQUAL(5, (int)safe_strlen("Hello"));
    AGENTOS_TEST_ASSERT_INT_EQUAL(11, (int)safe_strlen("Hello World"));
}

/**
 * @test 测试safe_strcmp函数
 */
static void test_safe_strcmp(void** state) {
    // 两个NULL
    AGENTOS_TEST_ASSERT_INT_EQUAL(0, safe_strcmp(NULL, NULL));
    
    // 一个NULL
    AGENTOS_TEST_ASSERT_TRUE(safe_strcmp(NULL, "test") < 0);
    AGENTOS_TEST_ASSERT_TRUE(safe_strcmp("test", NULL) > 0);
    
    // 正常比较
    AGENTOS_TEST_ASSERT_INT_EQUAL(0, safe_strcmp("abc", "abc"));
    AGENTOS_TEST_ASSERT_TRUE(safe_strcmp("abc", "abd") < 0);
    AGENTOS_TEST_ASSERT_TRUE(safe_strcmp("abc", "abb") > 0);
}

/**
 * @brief 运行所有字符串工具测试
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_safe_strcpy),
        cmocka_unit_test(test_safe_strcat),
        cmocka_unit_test(test_safe_memcpy),
        cmocka_unit_test(test_safe_int_operations),
        cmocka_unit_test(test_safe_size_operations),
        cmocka_unit_test(test_array_access_safety),
        cmocka_unit_test(test_ptr_offset_safety),
        cmocka_unit_test(test_type_conversion_safety),
        cmocka_unit_test(test_safe_strlen),
        cmocka_unit_test(test_safe_strcmp),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
