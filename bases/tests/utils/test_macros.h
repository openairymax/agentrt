/**
 * @file test_macros.h
 * @brief AgentOS C 测试宏工具库
 * @details 提供统一的测试宏，减少样板代码，提高测试代码可维护性
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TEST_MACROS_H
#define AGENTOS_TEST_MACROS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 测试断言宏
 * ============================================================ */

/**
 * @brief 断言条件为真
 * @param condition 条件表达式
 * @param message 失败消息
 */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ ASSERT FAILED: %s\n   File: %s, Line: %d\n   Message: %s\n", \
                    #condition, __FILE__, __LINE__, message); \
            return -1; \
        } \
    } while (0)

/**
 * @brief 断言两个值相等
 * @param expected 期望值
 * @param actual 实际值
 */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "❌ ASSERT EQUAL FAILED:\n   Expected: %lld\n   Actual: %lld\n   File: %s, Line: %d\n", \
                    (long long)(expected), (long long)(actual), __FILE__, __LINE__); \
            return -1; \
        } \
    } while (0)

/**
 * @brief 断言字符串相等
 * @param expected 期望字符串
 * @param actual 实际字符串
 */
#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "❌ ASSERT STRING EQUAL FAILED:\n   Expected: \"%s\"\n   Actual: \"%s\"\n   File: %s, Line: %d\n", \
                    (expected), (actual), __FILE__, __LINE__); \
            return -1; \
        } \
    } while (0)

/**
 * @brief 断言指针不为空
 * @param ptr 指针
 */
#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "❌ ASSERT NOT NULL FAILED: %s is NULL\n   File: %s, Line: %d\n", \
                    #ptr, __FILE__, __LINE__); \
            return -1; \
        } \
    } while (0)

/**
 * @brief 断言指针为空
 * @param ptr 指针
 */
#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "❌ ASSERT NULL FAILED: %s is not NULL\n   File: %s, Line: %d\n", \
                    #ptr, __FILE__, __LINE__); \
            return -1; \
        } \
    } while (0)

/* ============================================================
 * 引擎创建/销毁测试宏
 * ============================================================ */

/**
 * @brief 测试引擎创建和销毁
 * @param engine_type 引擎类型
 * @param create_func 创建函数
 * @param destroy_func 销毁函数
 * @param ... create_func 的额外参数
 */
#define TEST_ENGINE_CREATE_DESTROY(engine_type, create_func, destroy_func, ...) \
    do { \
        printf("Testing " #engine_type " create/destroy...\n"); \
        engine_type* engine = NULL; \
        agentos_error_t err = create_func(__VA_ARGS__, &engine); \
        TEST_ASSERT_EQUAL(AGENTOS_SUCCESS, err); \
        TEST_ASSERT_NOT_NULL(engine); \
        err = destroy_func(engine); \
        TEST_ASSERT_EQUAL(AGENTOS_SUCCESS, err); \
        printf("✓ " #engine_type " create/destroy test passed\n"); \
    } while (0)

/**
 * @brief 测试引擎创建失败场景
 * @param engine_type 引擎类型
 * @param create_func 创建函数
 * @param expected_error 期望的错误码
 * @param ... create_func 的参数
 */
#define TEST_ENGINE_CREATE_FAILURE(engine_type, create_func, expected_error, ...) \
    do { \
        printf("Testing " #engine_type " create failure...\n"); \
        engine_type* engine = NULL; \
        agentos_error_t err = create_func(__VA_ARGS__, &engine); \
        TEST_ASSERT_EQUAL(expected_error, err); \
        TEST_ASSERT_NULL(engine); \
        printf("✓ " #engine_type " create failure test passed\n"); \
    } while (0)

/* ============================================================
 * 测试运行器宏
 * ============================================================ */

/**
 * @brief 运行单个测试函数
 * @param test_func 测试函数名
 */
#define RUN_TEST(test_func) \
    do { \
        printf("\n▶ Running %s...\n", #test_func); \
        int result = test_func(); \
        if (result == 0) { \
            printf("✅ %s PASSED\n", #test_func); \
            passed_tests++; \
        } else { \
            printf("❌ %s FAILED\n", #test_func); \
            failed_tests++; \
        } \
        total_tests++; \
    } while (0)

/**
 * @brief 测试套件开始
 */
#define TEST_SUITE_BEGIN(suite_name) \
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "="\n"); \
    printf("Test Suite: %s\n", suite_name); \
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "="\n\n"); \
    int total_tests = 0; \
    int passed_tests = 0; \
    int failed_tests = 0

/**
 * @brief 测试套件结束
 */
#define TEST_SUITE_END() \
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "="\n"); \
    printf("Test Results: %d total, %d passed, %d failed\n", total_tests, passed_tests, failed_tests); \
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "="\n"); \
    return (failed_tests > 0) ? 1 : 0

/* ============================================================
 * 性能测试宏
 * ============================================================ */

/**
 * @brief 开始性能计时
 */
#define PERF_BEGIN() \
    clock_t __perf_start = clock()

/**
 * @brief 结束性能计时并打印结果
 * @param operation_name 操作名称
 * @param iterations 迭代次数
 */
#define PERF_END(operation_name, iterations) \
    do { \
        clock_t __perf_end = clock(); \
        double __perf_elapsed = (double)(__perf_end - __perf_start) / CLOCKS_PER_SEC; \
        double __perf_ops_per_sec = (iterations) / __perf_elapsed; \
        printf("⏱ %s: %.3f sec (%.0f ops/sec)\n", operation_name, __perf_elapsed, __perf_ops_per_sec); \
    } while (0)

/* ============================================================
 * 内存测试宏
 * ============================================================ */

/**
 * @brief 检查内存泄漏
 * @param check_func 内存检查函数
 */
#define TEST_CHECK_MEMORY_LEAKS(check_func) \
    do { \
        int leaks = check_func(); \
        if (leaks > 0) { \
            fprintf(stderr, "⚠ Memory leaks detected: %d\n", leaks); \
        } else { \
            printf("✓ No memory leaks detected\n"); \
        } \
    } while (0)

/* ============================================================
 * 测试数据生成宏
 * ============================================================ */

/**
 * @brief 生成测试字符串
 * @param var 变量名
 * @param len 长度
 */
#define TEST_GENERATE_STRING(var, len) \
    char var[len + 1]; \
    for (int __i = 0; __i < len; __i++) { \
        var[__i] = 'A' + (__i % 26); \
    } \
    var[len] = '\0'

/**
 * @brief 生成测试缓冲区
 * @param var 变量名
 * @param size 大小
 * @param value 填充值
 */
#define TEST_GENERATE_BUFFER(var, size, value) \
    unsigned char var[size]; \
    memset(var, value, size)

#endif /* AGENTOS_TEST_MACROS_H */
