/**
 * @file test_heapstore_batch.c
 * @brief heapstore 批量写入模块单元测试
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 测试覆盖目标: 90%+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heapstore.h"
#include "private.h"

/* ==================== 测试框架宏定义 ==================== */

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            test_failures++; \
        } else { \
            printf("PASS: %s\n", msg); \
            test_passes++; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: %s (expected=%d, actual=%d, line %d)\n", \
                   msg, (int)(expected), (int)(actual), __LINE__); \
            test_failures++; \
        } else { \
            printf("PASS: %s\n", msg); \
            test_passes++; \
        } \
    } while(0)

static int test_passes = 0;
static int test_failures = 0;

/* ==================== 测试用例 ==================== */

/**
 * @brief 测试批量上下文初始化和销毁
 */
static void test_batch_init_destroy(void) {
    printf("\n=== Test: Batch Init/Destroy ===\n");

    heapstore_batch_context_t* ctx = NULL;
    size_t capacity = 100;

    heapstore_error_t err = heapstore_batch_begin(capacity, &ctx);
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "batch_begin should succeed");
    TEST_ASSERT(ctx != NULL, "context should not be NULL");
    TEST_ASSERT_EQ(0, ctx->count, "initial count should be 0");

    err = heapstore_batch_destroy(ctx);
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "batch_destroy should succeed");
}

/**
 * @brief 测试批量添加日志
 */
static void test_batch_add_log(void) {
    printf("\n=== Test: Batch Add Log ===\n");

    heapstore_batch_context_t* ctx = NULL;
    heapstore_error_t err = heapstore_batch_begin(100, &ctx);
    if (err != heapstore_SUCCESS || !ctx) {
        TEST_ASSERT(false, "Failed to init batch context");
        return;
    }

    err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_INFO, "test_service", "trace_001", "Test message");
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "add_log should succeed");
    TEST_ASSERT_EQ(1, ctx->count, "count should be 1 after add");

    err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_ERROR, "test_service2", NULL, "Error message");
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "add_log without trace_id should succeed");
    TEST_ASSERT_EQ(2, ctx->count, "count should be 2 after second add");

    err = heapstore_batch_destroy(ctx);
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "cleanup should succeed");
}

/**
 * @brief 测试参数验证（边界条件）
 */
static void test_batch_parameter_validation(void) {
    printf("\n=== Test: Parameter Validation ===\n");

    heapstore_error_t err;

    err = heapstore_batch_begin(0, NULL);
    TEST_ASSERT(err != heapstore_SUCCESS, "batch_begin with NULL should fail");

    heapstore_batch_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.capacity = 10;
    ctx.count = 5;

    err = heapstore_batch_add_log(NULL, HEAPSTORE_LOG_INFO, "svc", NULL, "msg");
    TEST_ASSERT(err != heapstore_SUCCESS, "add_log with NULL ctx should fail");

    err = heapstore_batch_add_log(&ctx, HEAPSTORE_LOG_INFO, NULL, NULL, "msg");
    TEST_ASSERT(err != heapstore_SUCCESS, "add_log with NULL service should fail");

    err = heapstore_batch_add_log(&ctx, HEAPSTORE_LOG_INFO, "svc", NULL, NULL);
    TEST_ASSERT(err != heapstore_SUCCESS, "add_log with NULL message should fail");

    ctx.count = ctx.capacity;
    err = heapstore_batch_add_log(&ctx, HEAPSTORE_LOG_INFO, "svc", NULL, "msg");
    TEST_ASSERT(err == heapstore_ERR_OUT_OF_MEMORY, "add_log when full should return OUT_OF_MEMORY");
}

/**
 * @brief 测试批量提交和回滚
 */
static void test_batch_commit_rollback(void) {
    printf("\n=== Test: Batch Commit/Rollback ===\n");

    heapstore_batch_context_t* ctx = NULL;
    heapstore_error_t err = heapstore_batch_begin(100, &ctx);
    if (err != heapstore_SUCCESS || !ctx) {
        TEST_ASSERT(false, "Failed to init batch context");
        return;
    }

    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Log message %d", i);
        err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_INFO, "test_svc", NULL, msg);
        if (err != heapstore_SUCCESS) {
            break;
        }
    }

    TEST_ASSERT_EQ(5, ctx->count, "should have 5 items before commit");

    err = heapstore_batch_commit(ctx);
    TEST_ASSERT(err == heapstore_SUCCESS || err == heapstore_ERR_NOT_INITIALIZED,
                 "commit should succeed or indicate not initialized");
    TEST_ASSERT_EQ(0, ctx->count, "count should be 0 after commit");

    heapstore_batch_destroy(ctx);

    ctx = NULL;
    err = heapstore_batch_begin(50, &ctx);
    if (err != heapstore_SUCCESS || !ctx) {
        return;
    }

    err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_INFO, "svc", NULL, "rollback test");
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "add before rollback should succeed");

    err = heapstore_batch_rollback(ctx);
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "rollback should succeed");
    TEST_ASSERT_EQ(0, ctx->count, "count should be 0 after rollback");

    heapstore_batch_destroy(ctx);
}

/**
 * @brief 测试容量限制
 */
static void test_batch_capacity_limit(void) {
    printf("\n=== Test: Capacity Limit ===\n");

    const size_t small_capacity = 3;
    heapstore_batch_context_t* ctx = NULL;
    heapstore_error_t err = heapstore_batch_begin(small_capacity, &ctx);
    if (err != heapstore_SUCCESS || !ctx) {
        TEST_ASSERT(false, "Failed to init batch context");
        return;
    }

    TEST_ASSERT_EQ(small_capacity, ctx->capacity, "capacity should match");

    for (size_t i = 0; i < small_capacity; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Item %zu", i);
        err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_INFO, "svc", NULL, msg);
        TEST_ASSERT_EQ(heapstore_SUCCESS, err, "add within capacity should succeed");
    }

    TEST_ASSERT_EQ(small_capacity, ctx->count, "should reach capacity limit");

    err = heapstore_batch_add_log(ctx, HEAPSTORE_LOG_INFO, "svc", NULL, "overflow");
    TEST_ASSERT_EQ(heapstore_ERR_OUT_OF_MEMORY, err, "overflow should return error");

    heapstore_batch_destroy(ctx);
}

/**
 * @brief 测试重复销毁安全性
 */
static void test_batch_double_destroy(void) {
    printf("\n=== Test: Double Destroy Safety ===\n");

    heapstore_batch_context_t* ctx = NULL;
    heapstore_error_t err = heapstore_batch_begin(10, &ctx);
    if (err != heapstore_SUCCESS || !ctx) {
        return;
    }

    err = heapstore_batch_destroy(ctx);
    TEST_ASSERT_EQ(heapstore_SUCCESS, err, "first destroy should succeed");

    err = heapstore_batch_destroy(ctx);
    TEST_ASSERT(err != heapstore_SUCCESS, "second destroy of same pointer should fail");
}

/* ==================== 主测试入口 ==================== */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("HeapStore Batch Module Unit Tests\n");
    printf("========================================\n");

    test_passes = 0;
    test_failures = 0;

    test_batch_init_destroy();
    test_batch_add_log();
    test_batch_parameter_validation();
    test_batch_commit_rollback();
    test_batch_capacity_limit();
    test_batch_double_destroy();

    printf("\n========================================\n");
    printf("Test Results: %d passed, %d failed\n", test_passes, test_failures);
    printf("Total tests: %d\n", test_passes + test_failures);
    printf("Coverage target: 90%%+\n");
    printf("========================================\n");

    return (test_failures > 0) ? 1 : 0;
}
