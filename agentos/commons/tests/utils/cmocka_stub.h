/**
 * @file cmocka_stub.h
 * @brief CMocka 测试框架 stub 头文件（用于编译时无 CMocka 的情况）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 当系统未安装 CMocka 测试框架时，使用此 stub 文件进行编译。
 * 提供基本的测试宏定义和函数声明，支持单元测试的基本功能。
 */

#ifndef AGENTOS_CMOCKA_STUB_H
#define AGENTOS_CMOCKA_STUB_H

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== CMocka 类型定义 ==================== */

/** @brief 测试状态指针类型 */
typedef void** CMockaState;

/** @brief 测试夹具函数类型 */
typedef void (*CMFixtureFunction)(void** state);

/** @brief 单元测试函数类型 */
typedef void (*CMUnitTestFunction)(void** state);

/** @brief 单元测试结构 */
struct CMUnitTest {
    const char* name;
    void (*test_func)(void** state);
    void* (*setup)(void** state);
    void* (*teardown)(void** state);
    int state;
};

/* ==================== 基本断言宏 ==================== */

#define assert_true(a) _assert_true((a), #a, __FILE__, __LINE__)
#define assert_false(a) _assert_false((a), #a, __FILE__, __LINE__)
#define assert_null(a) _assert_null((const void*)(a), #a, __FILE__, __LINE__)
#define assert_non_null(a) _assert_non_null((const void*)(a), #a, __FILE__, __LINE__)
#define assert_int_equal(a, b) _assert_int_equal((a), (b), #a, #b, __FILE__, __LINE__)
#define assert_string_equal(a, b) _assert_string_equal((a), (b), #a, #b, __FILE__, __LINE__)
#define assert_memory_equal(a, b, size) _assert_memory_equal((a), (b), (size), #a, #b, __FILE__, __LINE__)
#define assert_in_set(value, set, size) _assert_in_set((value), (set), (size), #value, __FILE__, __LINE__)

/* ==================== 内存分配测试函数 ==================== */

/**
 * @brief 测试用内存分配失败注入
 * @param count 分配失败的次数
 */
void test_malloc_fail_inject(int count);

/**
 * @brief 测试用内存分配失败计数
 * @return 当前失败次数
 */
int test_malloc_fail_count(void);

/**
 * @brief 重置内存分配失败注入
 */
void test_malloc_fail_reset(void);

/* ==================== 内部实现函数 ==================== */

static inline void _assert_true(int condition, const char* expr,
                                 const char* file, int line) {
    if (!condition) {
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", expr, file, line);
        _exit(1);
    }
}

static inline void _assert_false(int condition, const char* expr,
                                  const char* file, int line) {
    if (condition) {
        fprintf(stderr, "ASSERTION FAILED: !%s at %s:%d\n", expr, file, line);
        _exit(1);
    }
}

static inline void _assert_null(const void* ptr, const char* expr,
                                 const char* file, int line) {
    if (ptr != NULL) {
        fprintf(stderr, "ASSERTION FAILED: %s is not NULL at %s:%d\n", expr, file, line);
        _exit(1);
    }
}

static inline void _assert_non_null(const void* ptr, const char* expr,
                                    const char* file, int line) {
    if (ptr == NULL) {
        fprintf(stderr, "ASSERTION FAILED: %s is NULL at %s:%d\n", expr, file, line);
        _exit(1);
    }
}

static inline void _assert_int_equal(int a, int b, const char* expr_a,
                                      const char* expr_b, const char* file, int line) {
    if (a != b) {
        fprintf(stderr, "ASSERTION FAILED: %s (%d) != %s (%d) at %s:%d\n",
                expr_a, a, expr_b, b, file, line);
        _exit(1);
    }
}

static inline void _assert_string_equal(const char* a, const char* b,
                                        const char* expr_a, const char* expr_b,
                                        const char* file, int line) {
    if ((a == NULL && b != NULL) || (a != NULL && b == NULL) ||
        (a != NULL && b != NULL && strcmp(a, b) != 0)) {
        fprintf(stderr, "ASSERTION FAILED: \"%s\" != \"%s\" at %s:%d\n",
                a ? a : "(null)", b ? b : "(null)", file, line);
        _exit(1);
    }
}

static inline void _assert_memory_equal(const void* a, const void* b, size_t size,
                                        const char* expr_a, const char* expr_b,
                                        const char* file, int line) {
    if (memcmp(a, b, size) != 0) {
        fprintf(stderr, "ASSERTION FAILED: memory at %s != memory at %s at %s:%d\n",
                expr_a, expr_b, file, line);
        _exit(1);
    }
}

static inline void _assert_in_set(int value, const int* set, size_t size,
                                  const char* expr, const char* file, int line) {
    int found = 0;
    for (size_t i = 0; i < size; i++) {
        if (set[i] == value) {
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "ASSERTION FAILED: %d not in set at %s:%d\n", value, file, line);
        _exit(1);
    }
}

/* ==================== CMocka 测试运行函数 ==================== */

/**
 * @brief 运行一组单元测试
 * @param tests 测试数组
 * @param num_tests 测试数量
 * @return 0 表示成功，非 0 表示失败
 */
static inline int cmocka_run_group_tests(const struct CMUnitTest* tests,
                                         size_t num_tests,
                                         CMFixtureFunction setup,
                                         CMFixtureFunction teardown) {
    (void)setup;
    (void)teardown;

    printf("Running %zu tests...\n", num_tests);

    int failed = 0;
    for (size_t i = 0; i < num_tests; i++) {
        printf("  [TEST] %s...", tests[i].name);
        tests[i].test_func(NULL);
        printf(" PASSED\n");
    }

    printf("All tests passed!\n");
    return 0;
}

/**
 * @brief 创建单元测试条目宏
 * @param test_func 测试函数名
 */
#define cmocka_unit_test(test_func) \
    { #test_func, (test_func), NULL, NULL, 0 }

#define cmocka_unit_test_setup_teardown(test_func, setup, teardown) \
    { #test_func, (test_func), (setup), (teardown), 0 }

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CMOCKA_STUB_H */