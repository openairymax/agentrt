/**
 * @file test_macros.h
 * @brief AgentOS C 单元测试断言宏定义
 * @version 1.0.0.9
 * @date 2026-04-04
 * 
 * 提供简洁易用的 C 测试断言宏，替代手动 printf 检查。
 * 参考 CMockery2 和 Unity 测试框架设计。
 */

#ifndef __AGENTOS_TEST_MACROS_H__
#define __AGENTOS_TEST_MACROS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief 测试结果统计结构体
 */
typedef struct {
    int passed;
    int failed;
    int total;
} TestStats;

/**
 * @brief 全局测试结果统计
 */
static TestStats g_test_stats = {0, 0, 0};

/**
 * @brief 断言宏 - 检查条件是否为真
 * 
 * @param condition 要检查的条件
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_TRUE(condition, message) \
    do { \
        g_test_stats.total++; \
        if (condition) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s (条件不成立)\n", message); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查条件是否为假
 * 
 * @param condition 要检查的条件
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_FALSE(condition, message) \
    TEST_ASSERT_TRUE(!(condition), message)

/**
 * @brief 断言宏 - 检查指针不为 NULL
 * 
 * @param ptr 要检查的指针
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT_TRUE((ptr) != NULL, message)

/**
 * @brief 断言宏 - 检查指针为 NULL
 * 
 * @param ptr 要检查的指针
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT_TRUE((ptr) == NULL, message)

/**
 * @brief 断言宏 - 检查两个整数相等
 * 
 * @param expected 期望值
 * @param actual 实际值
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_EQUAL_INT(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s (期望=%d, 实际=%d)\n", message, (int)(expected), (int)(actual)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望：%d\n", (int)(expected)); \
            fprintf(stderr, "   实际：%d\n", (int)(actual)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查两个字符串相等
 * 
 * @param expected 期望字符串
 * @param actual 实际字符串
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_EQUAL_STRING(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if (strcmp((expected), (actual)) == 0) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s (值=\"%s\")\n", message, (expected)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望：\"%s\"\n", (expected)); \
            fprintf(stderr, "   实际：\"%s\"\n", (actual)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查返回值是否成功
 * 
 * @param err_code 错误码
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_SUCCESS(err_code, message) \
    TEST_ASSERT_EQUAL_INT(0, (err_code), message)

/**
 * @brief 断言宏 - 检查返回值是否失败
 * 
 * @param err_code 错误码
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_FAILED(err_code, message) \
    TEST_ASSERT_TRUE((err_code) != 0, message)

/**
 * @brief 测试用例开始宏
 * 
 * @param test_name 测试用例名称
 */
#define TEST_CASE_START(test_name) \
    printf("\n"); \
    printf("============================================================\n"); \
    printf("测试用例：%s\n", #test_name); \
    printf("============================================================\n")

/**
 * @brief 测试用例结束宏
 */
#define TEST_CASE_END() \
    printf("\n")

/**
 * @brief 打印测试统计结果
 */
#define PRINT_TEST_STATS() \
    do { \
        printf("\n"); \
        printf("============================================================\n"); \
        printf("测试统计结果\n"); \
        printf("============================================================\n"); \
        printf("总测试数：%d\n", g_test_stats.total); \
        printf("通过数：  %d\n", g_test_stats.passed); \
        printf("失败数：  %d\n", g_test_stats.failed); \
        printf("通过率：  %.2f%%\n", \
               g_test_stats.total > 0 ? \
               (float)g_test_stats.passed / g_test_stats.total * 100.0f : 0.0f); \
        printf("============================================================\n"); \
        \
        if (g_test_stats.failed > 0) { \
            printf("❌ 测试失败！\n"); \
        } else if (g_test_stats.total > 0) { \
            printf("✅ 所有测试通过！\n"); \
        } else { \
            printf("⚠️  未执行任何测试\n"); \
        } \
        printf("\n"); \
    } while(0)

/**
 * @brief 检查测试是否全部通过
 * 
 * @return true 全部通过
 * @return false 有失败
 */
#define TESTS_PASSED() (g_test_stats.failed == 0 && g_test_stats.total > 0)

/**
 * @brief 重置测试统计
 */
#define RESET_TEST_STATS() \
    do { \
        g_test_stats.passed = 0; \
        g_test_stats.failed = 0; \
        g_test_stats.total = 0; \
    } while(0)

/**
 * @brief 测试运行函数宏
 * 
 * @param test_func 测试函数名
 */
#define RUN_TEST(test_func) \
    do { \
        printf("\n>>> 运行测试：%s\n", #test_func); \
        test_func(); \
    } while(0)

#endif /* __AGENTOS_TEST_MACROS_H__ */
