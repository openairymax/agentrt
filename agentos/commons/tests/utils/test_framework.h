/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file test_framework.h
 * @brief AgentOS 测试框架 - CMocka 集成层
 * 
 * @details
 * 本文件提供 CMocka 测试框架的集成封装，包括：
 * - 统一的测试宏定义
 * - 测试夹具（Fixture）支持
 * - 内存泄漏检测
 * - 测试覆盖率报告
 * - 参数化测试支持
 * 
 * 使用方法：
 * @code
 * #include "test_framework.h"
 * 
 * static void test_example(void **state) {
 *     AGENTOS_TEST_ASSERT(1 == 1);
 *     AGENTOS_TEST_ASSERT_PTR_NOT_NULL(malloc(10));
 * }
 * 
 * int main(void) {
 *     const struct CMUnitTest tests[] = {
 *         cmocka_unit_test(test_example),
 *     };
 *     return cmocka_run_group_tests(tests, NULL, NULL);
 * }
 * @endcode
 * 
 * @author Spharx AgentOS Team
 * @date 2026-04-01
 * @version 1.0
 * 
 * @see ARCHITECTURAL_PRINCIPLES.md E-8 可测试性原则
 */

#ifndef AGENTOS_TEST_FRAMEWORK_H
#define AGENTOS_TEST_FRAMEWORK_H

/* CMocka 标准头文件 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#ifdef AGENTOS_HAS_CMOCKA
#include <cmocka.h>
#else
/* 使用 CMocka stub 实现（用于无 CMocka 环境的编译） */
#include "cmocka_stub.h"
#endif /* AGENTOS_HAS_CMOCKA */

#include "../../../utils/types/include/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 测试宏定义
 * ============================================================================ */

/**
 * @defgroup TestMacros 测试断言宏
 * @brief 扩展 CMocka 的断言宏
 * @{
 */

/* 基础断言宏（继承自 CMocka） */
#define AGENTOS_TEST_ASSERT(condition)             assert_true(condition)
#define AGENTOS_TEST_ASSERT_NOT(condition)         assert_false(condition)
#define AGENTOS_TEST_ASSERT_EQUAL(a, b)            assert_int_equal(a, b)
#define AGENTOS_TEST_ASSERT_NOT_EQUAL(a, b)        assert_int_not_equal(a, b)
#define AGENTOS_TEST_ASSERT_PTR_EQUAL(a, b)        assert_ptr_equal(a, b)
#define AGENTOS_TEST_ASSERT_PTR_NOT_EQUAL(a, b)    assert_ptr_not_equal(a, b)
#define AGENTOS_TEST_ASSERT_PTR_NULL(ptr)          assert_null(ptr)
#define AGENTOS_TEST_ASSERT_PTR_NOT_NULL(ptr)      assert_non_null(ptr)
#define AGENTOS_TEST_ASSERT_STRING_EQUAL(a, b)     assert_string_equal(a, b)
#define AGENTOS_TEST_ASSERT_STRING_NOT_EQUAL(a, b) assert_string_not_equal(a, b)
#define AGENTOS_TEST_ASSERT_MEMORY_EQUAL(a, b, n)  assert_memory_equal(a, b, n)
#define AGENTOS_TEST_ASSERT_MEMORY_NOT_EQUAL(a, b, n) assert_memory_not_equal(a, b, n)
#define AGENTOS_TEST_ASSERT_FLOAT_EQUAL(a, b, eps) assert_float_equal(a, b, eps)
#define AGENTOS_TEST_ASSERT_DOUBLE_EQUAL(a, b, eps) assert_double_equal(a, b, eps)
#define AGENTOS_TEST_ASSERT_IN_RANGE(value, min, max) assert_in_range(value, min, max)
#define AGENTOS_TEST_ASSERT_NOT_IN_RANGE(value, min, max) assert_not_in_range(value, min, max)

/**
 * @brief 错误码断言
 * @param result 实际结果
 * @param expected 期望的错误码
 */
#define AGENTOS_TEST_ASSERT_ERROR(result, expected) \
    assert_int_equal((int)(result), (int)(expected))

/**
 * @brief 成功断言
 * @param result 实际结果
 */
#define AGENTOS_TEST_ASSERT_SUCCESS(result) \
    assert_int_equal((int)(result), AGENTOS_SUCCESS)

/**
 * @brief 失败断言
 * @param result 实际结果
 */
#define AGENTOS_TEST_ASSERT_FAILURE(result) \
    assert_true((int)(result) < 0)

/**
 * @brief 范围断言
 * @param value 待检查值
 * @param min 最小值
 * @param max 最大值
 */
#define AGENTOS_TEST_ASSERT_RANGE(value, min, max) \
    assert_true((value) >= (min) && (value) <= (max))

/**
 * @brief 位掩码断言
 * @param value 待检查值
 * @param mask 期望的掩码位
 */
#define AGENTOS_TEST_ASSERT_BITS_SET(value, mask) \
    assert_true(((value) & (mask)) == (mask))

/**
 * @brief 位掩码清除断言
 * @param value 待检查值
 * @param mask 不期望的掩码位
 */
#define AGENTOS_TEST_ASSERT_BITS_CLEAR(value, mask) \
    assert_true(((value) & (mask)) == 0)

/** @} */ /* end of TestMacros */

/* ============================================================================
 * 测试夹具（Fixture）宏
 * ============================================================================ */

/**
 * @defgroup TestFixtures 测试夹具宏
 * @brief 简化测试夹具的定义
 * @{
 */

/**
 * @brief 定义测试组
 * @param name 测试组名称
 */
#define AGENTOS_TEST_GROUP(name) \
    static int setup_##name(void **state); \
    static int teardown_##name(void **state)

/**
 * @brief 定义单元测试
 * @param group 测试组名称
 * @param test 测试函数名称
 */
#define AGENTOS_UNIT_TEST(group, test) \
    cmocka_unit_test_setup_teardown(test, setup_##group, teardown_##group)

/**
 * @brief 定义简单单元测试（无夹具）
 * @param test 测试函数名称
 */
#define AGENTOS_SIMPLE_TEST(test) \
    cmocka_unit_test(test)

/**
 * @brief 定义参数化测试
 * @param test 测试函数名称
 * @param setup 设置函数
 * @param teardown 清理函数
 */
#define AGENTOS_PARAM_TEST(test, setup, teardown) \
    cmocka_unit_test_setup_teardown(test, setup, teardown)

/** @} */ /* end of TestFixtures */

/* ============================================================================
 * 测试辅助宏
 * ============================================================================ */

/**
 * @defgroup TestHelpers 测试辅助宏
 * @brief 测试常用的辅助功能
 * @{
 */

/**
 * @brief 跳过测试
 * @param reason 跳过原因
 */
#define AGENTOS_TEST_SKIP(reason) \
    skip(); print_message("Skipped: %s\n", reason)

/**
 * @brief 测试消息输出
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_TEST_MESSAGE(fmt, ...) \
    print_message("[TEST] " fmt "\n", ##__VA_ARGS__)

/**
 * @brief 测试调试输出
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_TEST_DEBUG(fmt, ...) \
    print_message("[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * @brief 测试警告输出
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
#define AGENTOS_TEST_WARN(fmt, ...) \
    print_message("[WARN] " fmt "\n", ##__VA_ARGS__)

/**
 * @brief 测试开始标记
 * @param name 测试名称
 */
#define AGENTOS_TEST_BEGIN(name) \
    print_message("\n========== TEST: %s ==========\n", name)

/**
 * @brief 测试结束标记
 * @param name 测试名称
 */
#define AGENTOS_TEST_END(name) \
    print_message("========== END: %s ==========\n\n", name)

/**
 * @brief 性能测试计时开始
 */
#define AGENTOS_PERF_BEGIN() \
    uint64_t _perf_start = agentos_get_timestamp_ns()

/**
 * @brief 性能测试计时结束
 * @param threshold_us 阈值（微秒）
 */
#define AGENTOS_PERF_END(threshold_us) \
    do { \
        uint64_t _perf_end = agentos_get_timestamp_ns(); \
        uint64_t _perf_us = (_perf_end - _perf_start) / 1000; \
        print_message("[PERF] Elapsed: %lu us (threshold: %lu us)\n", \
                      (unsigned long)_perf_us, (unsigned long)(threshold_us)); \
        assert_true(_perf_us <= (threshold_us)); \
    } while(0)

/** @} */ /* end of TestHelpers */

/* ============================================================================
 * 内存测试宏
 * ============================================================================ */

/**
 * @defgroup MemoryTestMacros 内存测试宏
 * @brief 内存相关的测试辅助
 * @{
 */

/**
 * @brief 测试内存分配
 * @param ptr 指针
 * @param size 期望大小
 */
#define AGENTOS_TEST_ASSERT_ALLOCATED(ptr, size) \
    do { \
        assert_non_null(ptr); \
        if (ptr) { \
            size_t _actual_size = agentos_test_get_alloc_size(ptr); \
            assert_true(_actual_size >= (size)); \
        } \
    } while(0)

/**
 * @brief 测试内存对齐
 * @param ptr 指针
 * @param alignment 对齐值
 */
#define AGENTOS_TEST_ASSERT_ALIGNED(ptr, alignment) \
    assert_true(((uintptr_t)(ptr) % (alignment)) == 0)

/**
 * @brief 测试缓冲区边界
 * @param buf 缓冲区
 * @param len 长度
 * @param offset 偏移
 * @param access_size 访问大小
 */
#define AGENTOS_TEST_ASSERT_BUFFER_SAFE(buf, len, offset, access_size) \
    assert_true((offset) + (access_size) <= (len))

/** @} */ /* end of MemoryTestMacros */

/* ============================================================================
 * 测试运行器
 * ============================================================================ */

/**
 * @brief 测试运行器结构
 */
typedef struct {
    const char* name;                   /**< 测试组名称 */
    const struct CMUnitTest* tests;     /**< 测试数组 */
    size_t test_count;                  /**< 测试数量 */
    CMFixtureFunction setup;            /**< 全局设置函数 */
    CMFixtureFunction teardown;         /**< 全局清理函数 */
} agentos_test_runner_t;

/**
 * @brief 运行测试组
 * @param runner 测试运行器
 * @return 测试结果（0 成功，非 0 失败）
 */
int agentos_run_tests(const agentos_test_runner_t* runner);

/**
 * @brief 运行单个测试
 * @param test 测试函数
 * @return 测试结果
 */
int agentos_run_single_test(CMUnitTestFunction test);

/* ============================================================================
 * 测试工具函数
 * ============================================================================ */

/**
 * @brief 获取分配内存的大小
 * @param ptr 内存指针
 * @return 分配大小，失败返回 0
 */
size_t agentos_test_get_alloc_size(void* ptr);

/**
 * @brief 检查内存泄漏
 * @return 泄漏字节数，0 表示无泄漏
 */
size_t agentos_test_check_leaks(void);

/**
 * @brief 生成随机字符串
 * @param buffer 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_test_random_string(char* buffer, size_t len);

/**
 * @brief 生成随机字节
 * @param buffer 输出缓冲区
 * @param len 缓冲区长度
 */
void agentos_test_random_bytes(void* buffer, size_t len);

/**
 * @brief 获取当前时间戳（纳秒）
 * @return 时间戳
 */
uint64_t agentos_get_timestamp_ns(void);

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳
 */
uint64_t agentos_get_timestamp_ms(void);

/**
 * @brief 创建临时文件
 * @param prefix 文件名前缀
 * @param buffer 输出缓冲区
 * @param len 缓冲区长度
 * @return 文件句柄，失败返回 NULL
 */
FILE* agentos_test_create_temp_file(const char* prefix, char* buffer, size_t len);

/**
 * @brief 删除临时文件
 * @param path 文件路径
 */
void agentos_test_delete_temp_file(const char* path);

/**
 * @brief 比较两个文件内容
 * @param path1 文件1路径
 * @param path2 文件2路径
 * @return true 相同，false 不同
 */
bool agentos_test_compare_files(const char* path1, const char* path2);

/* ============================================================================
 * 参数化测试支持
 * ============================================================================ */

/**
 * @brief 参数化测试数据结构
 */
typedef struct {
    const char* name;               /**< 参数名称 */
    void* value;                    /**< 参数值 */
    size_t value_size;              /**< 值大小 */
} agentos_test_param_t;

/**
 * @brief 参数化测试数据数组
 */
typedef struct {
    agentos_test_param_t* params;   /**< 参数数组 */
    size_t count;                   /**< 参数数量 */
} agentos_test_params_t;

/**
 * @brief 创建参数化测试数据
 * @param count 参数数量
 * @return 参数数组
 */
agentos_test_params_t* agentos_test_params_create(size_t count);

/**
 * @brief 释放参数化测试数据
 * @param params 参数数组
 */
void agentos_test_params_free(agentos_test_params_t* params);

/**
 * @brief 设置参数值
 * @param params 参数数组
 * @param index 参数索引
 * @param name 参数名称
 * @param value 参数值
 * @param value_size 值大小
 */
void agentos_test_params_set(
    agentos_test_params_t* params,
    size_t index,
    const char* name,
    const void* value,
    size_t value_size
);

/**
 * @brief 获取参数值
 * @param params 参数数组
 * @param name 参数名称
 * @param value [out] 参数值
 * @param value_size 值大小
 * @return 错误码
 */
agentos_error_t agentos_test_params_get(
    const agentos_test_params_t* params,
    const char* name,
    void* value,
    size_t value_size
);

/* ============================================================================
 * 测试报告
 * ============================================================================ */

/**
 * @brief 测试结果结构
 */
typedef struct {
    const char* test_name;          /**< 测试名称 */
    bool passed;                    /**< 是否通过 */
    const char* message;            /**< 结果消息 */
    uint64_t duration_us;           /**< 执行时间（微秒） */
} agentos_test_result_t;

/**
 * @brief 测试报告结构
 */
typedef struct {
    const char* group_name;         /**< 测试组名称 */
    agentos_test_result_t* results; /**< 测试结果数组 */
    size_t result_count;            /**< 结果数量 */
    size_t passed_count;            /**< 通过数量 */
    size_t failed_count;            /**< 失败数量 */
    size_t skipped_count;           /**< 跳过数量 */
    uint64_t total_duration_us;     /**< 总执行时间 */
} agentos_test_report_t;

/**
 * @brief 生成测试报告
 * @param report 测试报告
 * @param format 格式（"text", "json", "xml", "junit"）
 * @param buffer 输出缓冲区
 * @param len 缓冲区长度
 * @return 错误码
 */
agentos_error_t agentos_test_generate_report(
    const agentos_test_report_t* report,
    const char* format,
    char* buffer,
    size_t len
);

/**
 * @brief 打印测试报告
 * @param report 测试报告
 */
void agentos_test_print_report(const agentos_test_report_t* report);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TEST_FRAMEWORK_H */
