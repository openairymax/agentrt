/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_hook_builtin_handlers.c - P0.20.1 内置 Hook 处理器测试
 *
 * 验证审计/metrics/trace 三个生产 Hook 处理器的注册、触发、导出和注销。
 * 测试使用自定义测试框架（TEST_PASS/TEST_FAIL/RUN_TEST 宏）。
 *
 * 测试用例：
 *   1. test_builtin_handlers_register    — 12 个 handler 正确注册
 *   2. test_builtin_handlers_trigger     — 触发后 metrics 计数递增
 *   3. test_metrics_dump                 — JSON 导出格式正确
 *   4. test_metrics_count                — 计数器精确递增
 *   5. test_builtin_handlers_unregister  — 注销后不再执行
 */

#include "hook_builtin_handlers.h"
#include "agentos_hook.h"
#include "hook_registry.h"
#include "memory_compat.h"

#include <stdio.h>
#include <string.h>

/* ==================== 测试框架 ==================== */

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) \
    do { \
        printf("[FAIL] %s: %s\n", name, msg); \
        g_failed++; \
        return; \
    } while (0)

static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(func) \
    do { \
        func(); \
        g_passed++; \
    } while (0)

/* ==================== 辅助函数 ==================== */

/* 计算当前注册的 hook 总数（通过遍历 8 种类型） */
static size_t count_all_hooks(void)
{
    size_t total = 0;
    for (int t = 0; t < HOOK_TYPE_COUNT; t++) {
        total += agentos_hook_count_by_type((hook_type_t)t);
    }
    return total;
}

/* ==================== 测试用例 ==================== */

/**
 * 测试 1：验证 12 个内置 handler 正确注册
 *
 * 注册后应能通过 agentos_hook_get 查找到：
 *   - audit_handler_error / audit_handler_tool（2 个）
 *   - metrics_handler_pre_exec .. metrics_handler_on_memory_evolve（8 个）
 *   - trace_handler_pre_exec / trace_handler_post_exec（2 个）
 */
static void test_builtin_handlers_register(void)
{
    /* 初始化 hook 系统（幂等，test_hook_integration 可能已初始化） */
    if (agentos_hook_init() != 0) {
        TEST_FAIL("builtin_handlers_register", "agentos_hook_init failed");
    }

    /* 注册内置 handler */
    if (agentos_hook_register_builtin_handlers() != 0) {
        TEST_FAIL("builtin_handlers_register", "register_builtin_handlers failed");
    }

    /* 验证 hook 总数 >= 12（可能包含其他测试注册的 hook） */
    size_t total = count_all_hooks();
    if (total < 12) {
        TEST_FAIL("builtin_handlers_register", "expected >= 12 hooks, got < 12");
    }

    /* 验证审计 handler 已注册 */
    if (agentos_hook_get("audit_handler_error") == NULL) {
        TEST_FAIL("builtin_handlers_register", "audit_handler_error not found");
    }
    if (agentos_hook_get("audit_handler_tool") == NULL) {
        TEST_FAIL("builtin_handlers_register", "audit_handler_tool not found");
    }

    /* 验证 metrics handler 已注册（抽样检查 3 个） */
    if (agentos_hook_get("metrics_handler_pre_exec") == NULL) {
        TEST_FAIL("builtin_handlers_register", "metrics_handler_pre_exec not found");
    }
    if (agentos_hook_get("metrics_handler_on_error") == NULL) {
        TEST_FAIL("builtin_handlers_register", "metrics_handler_on_error not found");
    }
    if (agentos_hook_get("metrics_handler_on_memory_evolve") == NULL) {
        TEST_FAIL("builtin_handlers_register", "metrics_handler_on_memory_evolve not found");
    }

    /* 验证 trace handler 已注册 */
    if (agentos_hook_get("trace_handler_pre_exec") == NULL) {
        TEST_FAIL("builtin_handlers_register", "trace_handler_pre_exec not found");
    }
    if (agentos_hook_get("trace_handler_post_exec") == NULL) {
        TEST_FAIL("builtin_handlers_register", "trace_handler_post_exec not found");
    }

    TEST_PASS("builtin_handlers_register");
}

/**
 * 测试 2：验证触发 hook 后 metrics 计数器递增
 *
 * 触发 PRE_EXEC 和 ON_ERROR hook，验证对应类型的计数器 > 0。
 */
static void test_builtin_handlers_trigger(void)
{
    /* 记录触发前的计数 */
    uint64_t pre_exec_before = agentos_hook_metrics_get_count(HOOK_TYPE_PRE_EXEC);
    uint64_t on_error_before = agentos_hook_metrics_get_count(HOOK_TYPE_ON_ERROR);

    /* 构造 PRE_EXEC hook 上下文并触发 */
    hook_context_t ctx1;
    AGENTOS_MEMSET(&ctx1, 0, sizeof(ctx1));
    ctx1.type = HOOK_TYPE_PRE_EXEC;
    ctx1.source_daemon = "test";
    ctx1.operation = "test_pre_exec";
    ctx1.timestamp_ns = 1000000000ULL;
    hook_decision_t dec1 = agentos_hook_trigger(&ctx1);
    if (dec1 != HOOK_DECISION_CONTINUE) {
        TEST_FAIL("builtin_handlers_trigger", "PRE_EXEC should return CONTINUE");
    }

    /* 构造 ON_ERROR hook 上下文并触发 */
    hook_context_t ctx2;
    AGENTOS_MEMSET(&ctx2, 0, sizeof(ctx2));
    ctx2.type = HOOK_TYPE_ON_ERROR;
    ctx2.source_daemon = "test";
    ctx2.operation = "test_on_error";
    ctx2.timestamp_ns = 2000000000ULL;
    hook_decision_t dec2 = agentos_hook_trigger(&ctx2);
    if (dec2 != HOOK_DECISION_CONTINUE) {
        TEST_FAIL("builtin_handlers_trigger", "ON_ERROR should return CONTINUE");
    }

    /* 验证计数器递增 */
    uint64_t pre_exec_after = agentos_hook_metrics_get_count(HOOK_TYPE_PRE_EXEC);
    uint64_t on_error_after = agentos_hook_metrics_get_count(HOOK_TYPE_ON_ERROR);

    if (pre_exec_after <= pre_exec_before) {
        TEST_FAIL("builtin_handlers_trigger", "PRE_EXEC count did not increase");
    }
    if (on_error_after <= on_error_before) {
        TEST_FAIL("builtin_handlers_trigger", "ON_ERROR count did not increase");
    }

    TEST_PASS("builtin_handlers_trigger");
}

/**
 * 测试 3：验证 metrics JSON 导出格式正确
 *
 * 导出 JSON 后验证包含关键字段："hook_metrics"、"PRE_EXEC"、"ON_ERROR"。
 */
static void test_metrics_dump(void)
{
    char buf[512];
    int n = agentos_hook_metrics_dump(buf, sizeof(buf));
    if (n <= 0) {
        TEST_FAIL("metrics_dump", "dump returned non-positive");
    }

    /* 验证 JSON 包含 hook_metrics 顶层键 */
    if (strstr(buf, "\"hook_metrics\"") == NULL) {
        TEST_FAIL("metrics_dump", "missing 'hook_metrics' key");
    }

    /* 验证包含所有 8 种事件类型名称 */
    const char *type_names[] = {
        "PRE_EXEC", "POST_EXEC", "PRE_LLM", "POST_LLM",
        "PRE_TOOL", "POST_TOOL", "ON_ERROR", "ON_MEMORY_EVOLVE"
    };
    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        if (strstr(buf, type_names[i]) == NULL) {
            TEST_FAIL("metrics_dump", "missing type name in JSON");
        }
    }

    /* 验证 JSON 以 }} 结尾 */
    size_t len = (size_t)n;
    if (len < 2 || buf[len - 1] != '}' || buf[len - 2] != '}') {
        TEST_FAIL("metrics_dump", "JSON does not end with '}}'");
    }

    /* 验证缓冲区不足时返回 -1 */
    char tiny[8];
    if (agentos_hook_metrics_dump(tiny, sizeof(tiny)) != -1) {
        TEST_FAIL("metrics_dump", "tiny buffer should return -1");
    }

    TEST_PASS("metrics_dump");
}

/**
 * 测试 4：验证计数器精确递增
 *
 * 触发 N 次 POST_TOOL hook，验证计数器精确增加 N。
 */
static void test_metrics_count(void)
{
    const int N = 5;
    uint64_t before = agentos_hook_metrics_get_count(HOOK_TYPE_POST_TOOL);

    for (int i = 0; i < N; i++) {
        hook_context_t ctx;
        AGENTOS_MEMSET(&ctx, 0, sizeof(ctx));
        ctx.type = HOOK_TYPE_POST_TOOL;
        ctx.source_daemon = "test";
        ctx.operation = "test_post_tool";
        ctx.timestamp_ns = 3000000000ULL + (uint64_t)i;
        agentos_hook_trigger(&ctx);
    }

    uint64_t after = agentos_hook_metrics_get_count(HOOK_TYPE_POST_TOOL);
    if (after - before != (uint64_t)N) {
        TEST_FAIL("metrics_count", "count increase != N");
    }

    /* 验证越界 type 返回 0 */
    if (agentos_hook_metrics_get_count((hook_type_t)99) != 0) {
        TEST_FAIL("metrics_count", "out-of-range type should return 0");
    }
    if (agentos_hook_metrics_get_count((hook_type_t)-1) != 0) {
        TEST_FAIL("metrics_count", "negative type should return 0");
    }

    TEST_PASS("metrics_count");
}

/**
 * 测试 5：验证注销后 handler 不再执行
 *
 * 注销内置 handler 后，触发 hook 不再递增 metrics 计数器。
 */
static void test_builtin_handlers_unregister(void)
{
    /* 注销 */
    agentos_hook_unregister_builtin_handlers();

    /* 验证审计 handler 已注销 */
    if (agentos_hook_get("audit_handler_error") != NULL) {
        TEST_FAIL("builtin_handlers_unregister", "audit_handler_error still exists");
    }
    if (agentos_hook_get("metrics_handler_pre_exec") != NULL) {
        TEST_FAIL("builtin_handlers_unregister", "metrics_handler_pre_exec still exists");
    }
    if (agentos_hook_get("trace_handler_pre_exec") != NULL) {
        TEST_FAIL("builtin_handlers_unregister", "trace_handler_pre_exec still exists");
    }

    /* 触发 hook，验证 metrics 计数器不再递增（因为 handler 已注销） */
    uint64_t before = agentos_hook_metrics_get_count(HOOK_TYPE_PRE_LLM);

    hook_context_t ctx;
    AGENTOS_MEMSET(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_LLM;
    ctx.source_daemon = "test";
    ctx.operation = "test_after_unregister";
    ctx.timestamp_ns = 4000000000ULL;
    agentos_hook_trigger(&ctx);

    uint64_t after = agentos_hook_metrics_get_count(HOOK_TYPE_PRE_LLM);
    if (after != before) {
        TEST_FAIL("builtin_handlers_unregister", "count changed after unregister");
    }

    /* 重新注册（为后续测试恢复状态） */
    agentos_hook_register_builtin_handlers();

    TEST_PASS("builtin_handlers_unregister");
}

/* ==================== 主入口 ==================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("P0.20.1: Hook Builtin Handlers Tests\n");
    printf("========================================\n\n");

    RUN_TEST(test_builtin_handlers_register);
    RUN_TEST(test_builtin_handlers_trigger);
    RUN_TEST(test_metrics_dump);
    RUN_TEST(test_metrics_count);
    RUN_TEST(test_builtin_handlers_unregister);

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    return g_failed > 0 ? 1 : 0;
}
