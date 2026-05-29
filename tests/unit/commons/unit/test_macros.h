/**
 * @file test_macros.h
 * @brief AgentOS C 单元测试断言宏定义
 * @version 0.1.0
 * @date 2026-04-04
 *
 * 提供简洁易用的 C 测试断言宏，替代手动 printf 检查。
 * 参考 CMocka 和 Unity 测试框架设计。
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

typedef struct {
    int passed;
    int failed;
    int total;
} TestStats;

static TestStats g_test_stats = {0, 0, 0};

#define TEST_ASSERT_TRUE(condition, message) \
    do { \
        g_test_stats.total++; \
        if (condition) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s (condition false)\n", message); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_FALSE(condition, message) \
    TEST_ASSERT_TRUE(!(condition), message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT_TRUE((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT_TRUE((ptr) == NULL, message)

#define TEST_ASSERT_EQUAL_INT(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed++; \
            printf("  PASS: %s (expected=%d, actual=%d)\n", message, (int)(expected), (int)(actual)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected: %d\n", (int)(expected)); \
            fprintf(stderr, "   actual: %d\n", (int)(actual)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if (strcmp((expected), (actual)) == 0) { \
            g_test_stats.passed++; \
            printf("  PASS: %s (value=\"%s\")\n", message, (expected)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected: \"%s\"\n", (expected)); \
            fprintf(stderr, "   actual: \"%s\"\n", (actual)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_SUCCESS(err_code, message) \
    TEST_ASSERT_EQUAL_INT(0, (err_code), message)

#define TEST_ASSERT_FAILED(err_code, message) \
    TEST_ASSERT_TRUE((err_code) != 0, message)

#define TEST_CASE_START(test_name) \
    printf("\n"); \
    printf("============================================================\n"); \
    printf("Test case: %s\n", #test_name); \
    printf("============================================================\n")

#define TEST_CASE_END() \
    printf("\n")

#define PRINT_TEST_STATS() \
    do { \
        printf("\n"); \
        printf("============================================================\n"); \
        printf("Test Statistics\n"); \
        printf("============================================================\n"); \
        printf("Total:  %d\n", g_test_stats.total); \
        printf("Passed: %d\n", g_test_stats.passed); \
        printf("Failed: %d\n", g_test_stats.failed); \
        printf("Rate:   %.2f%%\n", \
               g_test_stats.total > 0 ? \
               (float)g_test_stats.passed / g_test_stats.total * 100.0f : 0.0f); \
        printf("============================================================\n"); \
        \
        if (g_test_stats.failed > 0) { \
            printf("FAILED!\n"); \
        } else if (g_test_stats.total > 0) { \
            printf("ALL PASSED!\n"); \
        } else { \
            printf("No tests executed\n"); \
        } \
        printf("\n"); \
    } while(0)

#define TESTS_PASSED() (g_test_stats.failed == 0 && g_test_stats.total > 0)

#define RESET_TEST_STATS() \
    do { \
        g_test_stats.passed = 0; \
        g_test_stats.failed = 0; \
        g_test_stats.total = 0; \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("\n>>> Running: %s\n", #test_func); \
        test_func(); \
    } while(0)

#define TEST_ASSERT_EQUAL_LONG(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((long)(expected) == (long)(actual)) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected: %ld\n", (long)(expected)); \
            fprintf(stderr, "   actual: %ld\n", (long)(actual)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((unsigned int)(expected) == (unsigned int)(actual)) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected: %u\n", (unsigned int)(expected)); \
            fprintf(stderr, "   actual: %u\n", (unsigned int)(actual)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual, tolerance, message) \
    do { \
        g_test_stats.total++; \
        double exp_val = (double)(expected); \
        double act_val = (double)(actual); \
        double tol_val = (double)(tolerance); \
        if (fabs(exp_val - act_val) <= tol_val) { \
            g_test_stats.passed++; \
            printf("  PASS: %s (expected=%.6f, actual=%.6f, diff=%.6f)\n", \
                   message, exp_val, act_val, fabs(exp_val - act_val)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected: %.6f\n", exp_val); \
            fprintf(stderr, "   actual: %.6f\n", act_val); \
            fprintf(stderr, "   diff: %.6f (tolerance: %.6f)\n", fabs(exp_val - act_val), tol_val); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_POINTER(expected, actual, message) \
    do { \
        g_test_stats.total++; \
        if ((expected) == (actual)) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   expected ptr: %p\n", (void*)(expected)); \
            fprintf(stderr, "   actual ptr: %p\n", (void*)(actual)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_STRING_CONTAINS(haystack, needle, message) \
    do { \
        g_test_stats.total++; \
        if (strstr((haystack), (needle)) != NULL) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   substring not found: \"%s\"\n", (needle)); \
            fprintf(stderr, "   in: \"%s\"\n", (haystack)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_STRING_STARTS_WITH(str, prefix, message) \
    do { \
        g_test_stats.total++; \
        size_t prefix_len = strlen(prefix); \
        if (strncmp((str), (prefix), prefix_len) == 0) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   string does not start with \"%s\"\n", (prefix)); \
            fprintf(stderr, "   actual: \"%s\"\n", (str)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_STRING_ENDS_WITH(str, suffix, message) \
    do { \
        g_test_stats.total++; \
        size_t str_len = strlen(str); \
        size_t suffix_len = strlen(suffix); \
        if (str_len >= suffix_len && \
            strcmp((str) + str_len - suffix_len, (suffix)) == 0) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   string does not end with \"%s\"\n", (suffix)); \
            fprintf(stderr, "   actual: \"%s\"\n", (str)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_IN_RANGE(value, min_val, max_val, message) \
    do { \
        g_test_stats.total++; \
        if ((value) >= (min_val) && (value) <= (max_val)) { \
            g_test_stats.passed++; \
            printf("  PASS: %s (value=%d in [%d, %d])\n", \
                   message, (int)(value), (int)(min_val), (int)(max_val)); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   value %d not in [%d, %d]\n", \
                    (int)(value), (int)(min_val), (int)(max_val)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_ASSERT_ARRAY_NOT_EMPTY(arr, message) \
    TEST_ASSERT_NOT_NULL((arr), message)

#define TEST_ASSERT_MEMORY_EQUAL(expected, actual, size, message) \
    do { \
        g_test_stats.total++; \
        if (memcmp((expected), (actual), (size)) == 0) { \
            g_test_stats.passed++; \
            printf("  PASS: %s\n", message); \
        } else { \
            g_test_stats.failed++; \
            fprintf(stderr, "  FAIL: %s\n", message); \
            fprintf(stderr, "   memory mismatch (%zu bytes)\n", (size_t)(size)); \
            fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define TEST_SKIP(message) \
    do { \
        printf("  SKIP: %s (at %s:%d)\n", message, __FILE__, __LINE__); \
        return; \
    } while(0)

#define TEST_FAIL(message) \
    do { \
        g_test_stats.total++; \
        g_test_stats.failed++; \
        fprintf(stderr, "  FAIL: %s (at %s:%d)\n", message, __FILE__, __LINE__); \
    } while(0)

#define TEST_GROUP_START(group_name) \
    printf("\n"); \
    printf("####################################################\n"); \
    printf("Test group: %s\n", #group_name); \
    printf("####################################################\n")

#define TEST_GROUP_END() \
    printf("\n")

#endif /* __AGENTOS_TEST_MACROS_H__ */
