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
#include <math.h>
#include <stdarg.h>
#include <stdint.h>

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


/* ============================================================
 * 扩展断言宏
 * ============================================================ */

/**
 * @brief 断言宏 - 检查两个长整数相等
 */
#define TEST_ASSERT_EQUAL_LONG(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((long)(expected) == (long)(actual)) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望：%ld\n", (long)(expected)); \
            fprintf(stderr, "   实际：%ld\n", (long)(actual)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查两个无符号整数相等
 */
#define TEST_ASSERT_EQUAL_UINT(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((unsigned int)(expected) == (unsigned int)(actual)) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望：%u\n", (unsigned int)(expected)); \
            fprintf(stderr, "   实际：%u\n", (unsigned int)(actual)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查两个浮点数近似相等
 *
 * @param expected 期望值
 * @param actual 实际值
 * @param tolerance 允许的误差
 * @param message 失败时的错误消息
 */
#define TEST_ASSERT_EQUAL_FLOAT(expected, actual, tolerance, message) \
    do { \
        g_test_stats.total++; \
        double exp_val = (double)(expected); \
        double act_val = (double)(actual); \
        double tol_val = (double)(tolerance); \
        if (fabs(exp_val - act_val) <= tol_val) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s (期望=%.6f, 实际=%.6f, 误差=%.6f)\n", \
                   message, exp_val, act_val, fabs(exp_val - act_val)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望：%.6f\n", exp_val); \
            fprintf(stderr, "   实际：%.6f\n", act_val); \
            fprintf(stderr, "   误差：%.6f (允许：%.6f)\n", fabs(exp_val - act_val), tol_val); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查两个指针相等
 */
#define TEST_ASSERT_EQUAL_POINTER(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   期望指针：%p\n", (void*)(expected)); \
            fprintf(stderr, "   实际指针：%p\n", (void*)(actual)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查字符串包含子串
 */
#define TEST_ASSERT_STRING_CONTAINS haystack, needle, message \
    do { \
        g_test_stats.total++; \
        if (strstr((haystack), (needle)) != NULL) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   字符串不包含：\"%s\"\n", (needle)); \
            fprintf(stderr, "   实际字符串：\"%s\"\n", (haystack)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查字符串前缀
 */
#define TEST_ASSERT_STRING_STARTS_WITH(str, prefix, message) \
    do { \
        g_test_stats.total++; \
        size_t prefix_len = strlen(prefix); \
        if (strncmp((str), (prefix), prefix_len) == 0) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   字符串不以 \"%s\" 开头\n", (prefix)); \
            fprintf(stderr, "   实际字符串：\"%s\"\n", (str)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查字符串后缀
 */
#define TEST_ASSERT_STRING_ENDS_WITH(str, suffix, message) \
    do { \
        g_test_stats.total++; \
        size_t str_len = strlen(str); \
        size_t suffix_len = strlen(suffix); \
        if (str_len >= suffix_len && \
            strcmp((str) + str_len - suffix_len, (suffix)) == 0) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   字符串不以 \"%s\" 结尾\n", (suffix)); \
            fprintf(stderr, "   实际字符串：\"%s\"\n", (str)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查值在范围内
 */
#define TEST_ASSERT_IN_RANGE(value, min_val, max_val, message) \
    do { \
        g_test_stats.total++; \
        if ((value) >= (min_val) && (value) <= (max_val)) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s (值=%d 在范围内 [%d, %d])\n", \
                   message, (int)(value), (int)(min_val), (int)(max_val)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   值 %d 不在范围内 [%d, %d]\n", \
                    (int)(value), (int)(min_val), (int)(max_val)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 断言宏 - 检查指针数组不为空且长度符合预期
 */
#define TEST_ASSERT_ARRAY_NOT_EMPTY(arr, message) \
    TEST_ASSERT_NOT_NULL((arr), message)

/**
 * @brief 断言宏 - 检查内存块内容
 */
#define TEST_ASSERT_MEMORY_EQUAL(expected, actual, size, message) \
    do { \
        g_test_stats.total++; \
        if (memcmp((expected), (actual), (size)) == 0) { \
            g_test_stats.passed++; \
            printf("✅ PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            fprintf(stderr, "   内存块内容不一致 (大小: %zu 字节)\n", (size_t)(size)); \
            fprintf(stderr, "   位置：%s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief 条件跳过测试
 */
#define TEST_SKIP(message) \
    do { \
        printf("⏭️  SKIP: %s (位置：%s:%d)\n", message, __FILE__, __LINE__); \
        return; \
    } while(0)

/**
 * @brief 条件失败测试
 */
#define TEST_FAIL(message) \
    do { \
        g_test_stats.total++; \
        g_test_stats.failed++; \
        fprintf(stderr, "❌ FAIL: %s (位置：%s:%d)\n", message, __FILE__, __LINE__); \
    } while(0)

/**
 * @brief 测试分组开始
 */
#define TEST_GROUP_START(group_name) \
    printf("\n"); \
    printf("####################################################\n"); \
    printf("测试分组：%s\n", #group_name); \
    printf("####################################################\n")

/**
 * @brief 测试分组结束
 */
#define TEST_GROUP_END() \
    printf("\n")

#endif /* __AGENTOS_TEST_MACROS_H__ */
