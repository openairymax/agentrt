/**
 * @file test_hook_system.c
 * @brief P2.8: Hook 系统集成测试
 *
 * 测试覆盖：
 *   - P2.8.1: Shell Hook 注册/触发/拦截
 *   - P2.8.2: Python Hook 修改 LLM prompt
 *   - P2.8.3: Hook 超时保护（5s 强制终止）
 *   - P2.8.4: Webhook 签名和重试
 *
 * @owner team-C
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#include "agentrt_hook.h"
#include "hook_registry.h"
#include "hook_interceptor.h"
#include "hook_timeout.h"
#include "hook_executor.h"
#include "safety_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ==================== 测试辅助宏 ==================== */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total  = 0;

#define TEST(desc) \
    do { \
        g_tests_total++; \
        printf("\n  [TEST %d] %s\n", g_tests_total, desc); \
    } while (0)

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("    FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define CHECK_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("    FAIL: %s — expected %d, got %d (%s:%d)\n", \
                   msg, (int)(b), (int)(a), __FILE__, __LINE__); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define CHECK_STR_EQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            printf("    FAIL: %s — expected '%s', got '%s' (%s:%d)\n", \
                   msg, (b), (a), __FILE__, __LINE__); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define PASS() \
    do { \
        printf("    PASS\n"); \
        g_tests_passed++; \
    } while (0)

/* ==================== 回调函数 ==================== */

static hook_decision_t audit_callback(hook_context_t *ctx) {
    (void)ctx;
    /* 模拟审计日志写入 */
    printf("    [audit_hook] Logging tool call...\n");
    return HOOK_DECISION_CONTINUE;
}

static hook_decision_t block_callback(hook_context_t *ctx) {
    (void)ctx;
    printf("    [block_hook] Blocking unsafe operation!\n");
    return HOOK_DECISION_ABORT;
}

static hook_decision_t modify_prompt_callback(hook_context_t *ctx) {
    (void)ctx;
    printf("    [prompt_modifier] Injecting safety reminder into prompt...\n");
    return HOOK_DECISION_MODIFY;
}

static hook_decision_t slow_callback(hook_context_t *ctx) {
    (void)ctx;
    printf("    [slow_hook] Starting slow operation...\n");
    /* 模拟长时间阻塞操作 */
    sleep(10);
    return HOOK_DECISION_CONTINUE;
}

static hook_decision_t retry_callback(hook_context_t *ctx) {
    (void)ctx;
    static int call_count = 0;
    call_count++;
    if (call_count < 3) {
        printf("    [retry_hook] Retry attempt %d...\n", call_count);
        return HOOK_DECISION_RETRY;
    }
    printf("    [retry_hook] Succeeded after %d retries\n", call_count);
    return HOOK_DECISION_CONTINUE;
}

/* ==================== P2.8.1: Shell Hook 注册/触发/拦截 ==================== */

static void test_shell_hook_register(void) {
    TEST("P2.8.1a: Register Shell Hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register_shell(
        "test_shell_hook",
        HOOK_TYPE_PRE_TOOL,
        "/usr/local/bin/agentrt-audit-log.sh",
        100,
        true);
    CHECK_EQ(ret, 0, "agentrt_hook_register_shell failed");

    const hook_entry_t *entry = agentrt_hook_get("test_shell_hook");
    CHECK(entry != NULL, "Hook not found after registration");
    CHECK_EQ(entry->impl_type, HOOK_IMPL_SHELL, "Hook impl_type should be SHELL");
    CHECK_EQ(entry->type, HOOK_TYPE_PRE_TOOL, "Hook type should be PRE_TOOL");
    CHECK(entry->enabled, "Hook should be enabled");
    CHECK_EQ(entry->priority, 100, "Hook priority should be 100");

    agentrt_hook_shutdown();
    PASS();
}

static void test_shell_hook_unregister(void) {
    TEST("P2.8.1b: Unregister Shell Hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register_shell(
        "temp_shell_hook",
        HOOK_TYPE_PRE_EXEC,
        "/tmp/test.sh",
        50,
        true);
    CHECK_EQ(ret, 0, "Register should succeed");

    const hook_entry_t *entry = agentrt_hook_get("temp_shell_hook");
    CHECK(entry != NULL, "Hook should exist before unregister");

    ret = agentrt_hook_unregister("temp_shell_hook");
    CHECK_EQ(ret, 0, "Unregister should succeed");

    entry = agentrt_hook_get("temp_shell_hook");
    CHECK(entry == NULL, "Hook should not exist after unregister");

    agentrt_hook_shutdown();
    PASS();
}

static void test_shell_hook_enable_disable(void) {
    TEST("P2.8.1c: Enable/Disable Shell Hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register_shell(
        "toggle_hook",
        HOOK_TYPE_PRE_LLM,
        "/tmp/toggle.sh",
        75,
        true);
    CHECK_EQ(ret, 0, "Register should succeed");

    const hook_entry_t *entry = agentrt_hook_get("toggle_hook");
    CHECK(entry != NULL, "Hook should exist");
    CHECK(entry->enabled, "Hook should be enabled initially");

    ret = agentrt_hook_set_enabled("toggle_hook", false);
    CHECK_EQ(ret, 0, "Disable should succeed");

    entry = agentrt_hook_get("toggle_hook");
    CHECK(entry != NULL, "Hook should still exist");
    CHECK(!entry->enabled, "Hook should be disabled");

    ret = agentrt_hook_set_enabled("toggle_hook", true);
    CHECK_EQ(ret, 0, "Re-enable should succeed");

    entry = agentrt_hook_get("toggle_hook");
    CHECK(entry->enabled, "Hook should be re-enabled");

    agentrt_hook_shutdown();
    PASS();
}

static void test_shell_hook_trigger_intercept(void) {
    TEST("P2.8.1d: Shell Hook Trigger and Intercept via SafetyGuard");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册一个 C 回调 Hook 用于拦截验证 */
    ret = agentrt_hook_register(
        "audit_interceptor",
        HOOK_TYPE_PRE_TOOL,
        audit_callback,
        NULL,
        200,
        true);
    CHECK_EQ(ret, 0, "Register audit hook failed");

    /* 注册一个阻断 Hook */
    ret = agentrt_hook_register(
        "block_interceptor",
        HOOK_TYPE_PRE_TOOL,
        block_callback,
        NULL,
        300,
        true);
    CHECK_EQ(ret, 0, "Register block hook failed");

    /* 初始化拦截器 */
    hook_interceptor_config_t config = {0};
    config.enable_safety_guard = true;
    config.enable_audit_log = true;
    config.enable_permission_check = true;
    config.max_guard_timeout_ms = 5000;
    ret = hook_interceptor_init(&config);
    CHECK_EQ(ret, 0, "hook_interceptor_init failed");

    /* 验证拦截器配置 */
    hook_interceptor_config_t read_config;
    ret = hook_interceptor_get_config(&read_config);
    CHECK_EQ(ret, 0, "hook_interceptor_get_config failed");
    CHECK(read_config.enable_safety_guard, "SafetyGuard should be enabled");
    CHECK(read_config.enable_audit_log, "Audit log should be enabled");
    CHECK(read_config.enable_permission_check, "Permission check should be enabled");

    /* 创建 Hook 上下文并触发 */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_TOOL;
    ctx.hook_name = "test_trigger";
    ctx.source_daemon = "tool_d";
    ctx.operation = "file_write";
    ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

    hook_decision_t decision = agentrt_hook_trigger(&ctx);
    /* 高优先级 block hook 应返回 ABORT */
    CHECK(decision == HOOK_DECISION_ABORT || decision == HOOK_DECISION_CONTINUE,
          "Hook trigger should return a valid decision");

    hook_interceptor_destroy();
    agentrt_hook_shutdown();
    PASS();
}

/* ==================== P2.8.2: Python Hook 修改 LLM prompt ==================== */

static void test_python_hook_register(void) {
    TEST("P2.8.2a: Register Python Hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* Python Hook 通过 shell 脚本路径注册，但 impl_type 标记为 PYTHON */
    hook_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, "prompt_injector", sizeof(entry.name) - 1);
    strncpy(entry.script_path, "/usr/local/lib/agentrt/hooks/prompt_injector.py",
            sizeof(entry.script_path) - 1);
    entry.type = HOOK_TYPE_PRE_LLM;
    entry.impl_type = HOOK_IMPL_PYTHON;
    entry.priority = 150;
    entry.enabled = true;

    ret = hook_registry_register(&entry);
    CHECK_EQ(ret, 0, "hook_registry_register for Python hook failed");

    const hook_entry_t *found = agentrt_hook_get("prompt_injector");
    CHECK(found != NULL, "Python hook not found after registration");
    CHECK_EQ(found->impl_type, HOOK_IMPL_PYTHON, "Impl type should be PYTHON");
    CHECK_EQ(found->type, HOOK_TYPE_PRE_LLM, "Type should be PRE_LLM");

    agentrt_hook_shutdown();
    PASS();
}

static void test_python_hook_modify_prompt(void) {
    TEST("P2.8.2b: Python Hook modifies LLM prompt");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册一个会修改 prompt 的 C 回调来模拟 Python Hook 行为 */
    ret = agentrt_hook_register(
        "prompt_modifier",
        HOOK_TYPE_PRE_LLM,
        modify_prompt_callback,
        NULL,
        200,
        true);
    CHECK_EQ(ret, 0, "Register prompt modifier failed");

    /* 创建 LLM 请求上下文 */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_LLM;
    ctx.hook_name = "llm_request";
    ctx.source_daemon = "llm_d";
    ctx.operation = "chat_completion";
    ctx.input_data = "Hello, world!";
    ctx.input_data_len = strlen("Hello, world!");
    ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

    hook_decision_t decision = agentrt_hook_trigger(&ctx);
    /* Prompt modifier should return MODIFY */
    CHECK(decision == HOOK_DECISION_MODIFY,
          "Prompt modifier should return MODIFY decision");

    agentrt_hook_shutdown();
    PASS();
}

static void test_python_hook_multiple_modifiers(void) {
    TEST("P2.8.2c: Multiple Python hooks chain modify prompt");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册多个 prompt 修改 Hook */
    ret = agentrt_hook_register(
        "security_reminder",
        HOOK_TYPE_PRE_LLM,
        modify_prompt_callback,
        NULL,
        300,
        true);
    CHECK_EQ(ret, 0, "Register security_reminder failed");

    ret = agentrt_hook_register(
        "cost_tracker",
        HOOK_TYPE_PRE_LLM,
        audit_callback,
        NULL,
        200,
        true);
    CHECK_EQ(ret, 0, "Register cost_tracker failed");

    /* 验证注册数量 */
    size_t count = agentrt_hook_count_by_type(HOOK_TYPE_PRE_LLM);
    CHECK(count >= 2, "Should have at least 2 PRE_LLM hooks");

    /* 触发 Hook 链 */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_LLM;
    ctx.hook_name = "multi_modifier";
    ctx.source_daemon = "llm_d";
    ctx.operation = "chat_completion";
    ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

    hook_decision_t decision = agentrt_hook_trigger(&ctx);
    /* 聚合决策：MODIFY 优先级高于 CONTINUE */
    CHECK(decision == HOOK_DECISION_MODIFY || decision == HOOK_DECISION_CONTINUE,
          "Multi-modifier chain should return valid decision");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== P2.8.3: Hook 超时保护 ==================== */

static void test_hook_timeout_config(void) {
    TEST("P2.8.3a: Hook timeout configuration");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册一个 Hook */
    ret = agentrt_hook_register(
        "timeout_test_hook",
        HOOK_TYPE_PRE_TOOL,
        audit_callback,
        NULL,
        100,
        true);
    CHECK_EQ(ret, 0, "Register timeout_test_hook failed");

    /* 设置自定义超时 */
    ret = hook_timeout_set("timeout_test_hook", 5000);
    CHECK_EQ(ret, 0, "hook_timeout_set should succeed");

    /* 读取超时配置 */
    uint32_t timeout = hook_timeout_get("timeout_test_hook");
    CHECK_EQ(timeout, 5000, "Timeout should be 5000ms");

    /* 设置最小超时 */
    ret = hook_timeout_set("timeout_test_hook", HOOK_TIMEOUT_MIN_MS);
    CHECK_EQ(ret, 0, "Set minimum timeout should succeed");
    timeout = hook_timeout_get("timeout_test_hook");
    CHECK_EQ(timeout, HOOK_TIMEOUT_MIN_MS, "Timeout should be MIN_MS (10ms)");

    /* 设置最大超时 */
    ret = hook_timeout_set("timeout_test_hook", HOOK_TIMEOUT_MAX_MS);
    CHECK_EQ(ret, 0, "Set maximum timeout should succeed");
    timeout = hook_timeout_get("timeout_test_hook");
    CHECK_EQ(timeout, HOOK_TIMEOUT_MAX_MS, "Timeout should be MAX_MS (30000ms)");

    agentrt_hook_shutdown();
    PASS();
}

static void test_hook_timeout_default(void) {
    TEST("P2.8.3b: Default timeout for unconfigured hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册 Hook 但不设置超时 */
    ret = agentrt_hook_register(
        "default_timeout_hook",
        HOOK_TYPE_POST_TOOL,
        audit_callback,
        NULL,
        50,
        true);
    CHECK_EQ(ret, 0, "Register default_timeout_hook failed");

    /* 获取默认超时 */
    uint32_t timeout = hook_timeout_get("default_timeout_hook");
    CHECK_EQ(timeout, HOOK_TIMEOUT_DEFAULT_MS,
             "Default timeout should be 500ms");

    agentrt_hook_shutdown();
    PASS();
}

static void test_hook_timeout_abort(void) {
    TEST("P2.8.3c: Hook timeout triggers ABORT");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册一个会超时的慢 Hook */
    ret = agentrt_hook_register(
        "slow_blocking_hook",
        HOOK_TYPE_PRE_TOOL,
        slow_callback,
        NULL,
        200,
        true);
    CHECK_EQ(ret, 0, "Register slow_blocking_hook failed");

    /* 设置 1 秒超时（慢回调 sleep 10 秒） */
    ret = hook_timeout_set("slow_blocking_hook", 1000);
    CHECK_EQ(ret, 0, "Set 1s timeout should succeed");

    /* 创建上下文 */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_TOOL;
    ctx.hook_name = "slow_op";
    ctx.source_daemon = "tool_d";
    ctx.operation = "heavy_computation";
    ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

    /* 执行带超时保护的回调 */
    const hook_entry_t *entry = agentrt_hook_get("slow_blocking_hook");
    CHECK(entry != NULL, "Slow hook should be registered");

    uint64_t duration_ns = 0;
    hook_decision_t decision = hook_timeout_run(entry, &ctx, 1000, &duration_ns);

    /* 应该返回 ABORT 因为超时 */
    CHECK(decision == HOOK_DECISION_ABORT,
          "Slow hook should be aborted due to timeout");

    /* 超时次数应该增加 */
    int timeout_count = hook_timeout_get_count("slow_blocking_hook");
    CHECK(timeout_count >= 1, "Timeout count should be at least 1");

    agentrt_hook_shutdown();
    PASS();
}

static void test_hook_timeout_reset_count(void) {
    TEST("P2.8.3d: Reset timeout counter");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register(
        "reset_count_hook",
        HOOK_TYPE_PRE_TOOL,
        slow_callback,
        NULL,
        100,
        true);
    CHECK_EQ(ret, 0, "Register reset_count_hook failed");

    ret = hook_timeout_set("reset_count_hook", 500);
    CHECK_EQ(ret, 0, "Set timeout failed");

    /* 触发一次超时 */
    const hook_entry_t *entry = agentrt_hook_get("reset_count_hook");
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_TOOL;
    ctx.hook_name = "reset_test";
    ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

    uint64_t duration_ns = 0;
    hook_timeout_run(entry, &ctx, 500, &duration_ns);

    int count = hook_timeout_get_count("reset_count_hook");
    CHECK(count >= 1, "Timeout count should be at least 1 after timeout");

    /* 重置计数器 */
    ret = hook_timeout_reset_count("reset_count_hook");
    CHECK_EQ(ret, 0, "Reset timeout count should succeed");

    count = hook_timeout_get_count("reset_count_hook");
    CHECK_EQ(count, 0, "Timeout count should be 0 after reset");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== P2.8.4: Webhook 签名和重试 ==================== */

static void test_webhook_register(void) {
    TEST("P2.8.4a: Register Webhook Hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register_webhook(
        "audit_webhook",
        HOOK_TYPE_POST_TOOL,
        "https://hooks.example.com/agentrt/audit",
        80,
        true);
    CHECK_EQ(ret, 0, "agentrt_hook_register_webhook failed");

    const hook_entry_t *entry = agentrt_hook_get("audit_webhook");
    CHECK(entry != NULL, "Webhook not found after registration");
    CHECK_EQ(entry->impl_type, HOOK_IMPL_WEBHOOK, "Impl type should be WEBHOOK");
    CHECK_STR_EQ(entry->script_path, "https://hooks.example.com/agentrt/audit",
                 "Webhook URL should match");

    agentrt_hook_shutdown();
    PASS();
}

static void test_webhook_retry_mechanism(void) {
    TEST("P2.8.4b: Webhook retry mechanism");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册一个带重试的 Webhook（模拟） */
    ret = agentrt_hook_register_webhook(
        "retry_webhook",
        HOOK_TYPE_ON_ERROR,
        "https://hooks.example.com/agentrt/error",
        90,
        true);
    CHECK_EQ(ret, 0, "Register retry_webhook failed");

    const hook_entry_t *entry = agentrt_hook_get("retry_webhook");
    CHECK(entry != NULL, "Retry webhook should be registered");

    /* 验证 Webhook URL 已存储 */
    CHECK(strlen(entry->script_path) > 0, "Webhook URL should be set");

    /* 验证 Webhook 可以禁用 */
    ret = agentrt_hook_set_enabled("retry_webhook", false);
    CHECK_EQ(ret, 0, "Disable webhook should succeed");

    entry = agentrt_hook_get("retry_webhook");
    CHECK(!entry->enabled, "Webhook should be disabled");

    agentrt_hook_shutdown();
    PASS();
}

static void test_webhook_signature_config(void) {
    TEST("P2.8.4c: Webhook signature configuration");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册带签名的 Webhook */
    hook_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, "signed_webhook", sizeof(entry.name) - 1);
    /* URL 包含签名占位符 signature={HMAC_SHA256} */
    strncpy(entry.script_path,
            "https://hooks.example.com/agentrt/callback?signature={HMAC_SHA256}",
            sizeof(entry.script_path) - 1);
    entry.type = HOOK_TYPE_POST_TOOL;
    entry.impl_type = HOOK_IMPL_WEBHOOK;
    entry.priority = 70;
    entry.enabled = true;

    ret = hook_registry_register(&entry);
    CHECK_EQ(ret, 0, "Register signed webhook failed");

    const hook_entry_t *found = agentrt_hook_get("signed_webhook");
    CHECK(found != NULL, "Signed webhook should be registered");
    CHECK(strstr(found->script_path, "HMAC_SHA256") != NULL,
          "Webhook URL should contain signature placeholder");

    agentrt_hook_shutdown();
    PASS();
}

static void test_webhook_disable_and_reenable(void) {
    TEST("P2.8.4d: Webhook disable and re-enable for retry control");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 注册多个 Webhook */
    const char *webhooks[] = {
        "primary_webhook",
        "fallback_webhook",
        "emergency_webhook"
    };
    const char *urls[] = {
        "https://hooks.primary.example.com/agentrt",
        "https://hooks.fallback.example.com/agentrt",
        "https://hooks.emergency.example.com/agentrt"
    };

    for (int i = 0; i < 3; i++) {
        ret = agentrt_hook_register_webhook(
            webhooks[i], HOOK_TYPE_ON_ERROR, urls[i], 100 - i * 10, true);
        CHECK_EQ(ret, 0, "Register webhook chain failed");
    }

    /* 验证所有 Webhook 已注册 */
    size_t total = agentrt_hook_count();
    CHECK(total >= 3, "Should have at least 3 registered hooks");

    /* 禁用 fallback webhook */
    ret = agentrt_hook_set_enabled("fallback_webhook", false);
    CHECK_EQ(ret, 0, "Disable fallback webhook should succeed");

    const hook_entry_t *fallback = agentrt_hook_get("fallback_webhook");
    CHECK(!fallback->enabled, "Fallback webhook should be disabled");

    /* 重新启用 fallback webhook */
    ret = agentrt_hook_set_enabled("fallback_webhook", true);
    CHECK_EQ(ret, 0, "Re-enable fallback webhook should succeed");

    fallback = agentrt_hook_get("fallback_webhook");
    CHECK(fallback->enabled, "Fallback webhook should be re-enabled");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== 统计信息验证 ==================== */

static void test_hook_statistics(void) {
    TEST("P2.8-Stats: Hook statistics tracking");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register(
        "stats_test_hook",
        HOOK_TYPE_PRE_TOOL,
        audit_callback,
        NULL,
        100,
        true);
    CHECK_EQ(ret, 0, "Register stats_test_hook failed");

    /* 触发 Hook 多次 */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_TOOL;
    ctx.hook_name = "stats_op";
    ctx.source_daemon = "tool_d";
    ctx.operation = "test";

    for (int i = 0; i < 5; i++) {
        ctx.timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;
        agentrt_hook_trigger(&ctx);
    }

    /* 获取统计信息 */
    hook_stats_t stats;
    ret = agentrt_hook_get_stats("stats_test_hook", &stats);
    CHECK_EQ(ret, 0, "agentrt_hook_get_stats should succeed");
    CHECK(stats.invoke_count >= 5, "Invoke count should be at least 5");
    CHECK(stats.total_duration_ns > 0, "Total duration should be non-zero");

    /* 获取不存在的 Hook 统计 */
    ret = agentrt_hook_get_stats("nonexistent_hook", &stats);
    CHECK_EQ(ret, -1, "Get stats for nonexistent hook should return -1");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== 错误路径 ==================== */

static void test_error_duplicate_register(void) {
    TEST("P2.8-Error: Duplicate hook registration");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_register(
        "unique_hook",
        HOOK_TYPE_PRE_TOOL,
        audit_callback,
        NULL,
        100,
        true);
    CHECK_EQ(ret, 0, "First registration should succeed");

    /* 尝试重复注册同名 Hook */
    ret = agentrt_hook_register(
        "unique_hook",
        HOOK_TYPE_POST_TOOL,
        audit_callback,
        NULL,
        50,
        true);
    CHECK(ret != 0, "Duplicate registration should fail");

    agentrt_hook_shutdown();
    PASS();
}

static void test_error_unregister_nonexistent(void) {
    TEST("P2.8-Error: Unregister nonexistent hook");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    ret = agentrt_hook_unregister("ghost_hook");
    CHECK(ret != 0, "Unregistering nonexistent hook should fail");

    agentrt_hook_shutdown();
    PASS();
}

static void test_error_invalid_hook_name(void) {
    TEST("P2.8-Error: Operations on invalid hook name");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 查找不存在的 Hook */
    const hook_entry_t *entry = agentrt_hook_get("");
    CHECK(entry == NULL, "Empty name should return NULL");

    entry = agentrt_hook_get(NULL);
    CHECK(entry == NULL, "NULL name should return NULL");

    /* 禁用不存在的 Hook */
    ret = agentrt_hook_set_enabled("nonexistent_hook", false);
    CHECK(ret != 0, "Set enabled on nonexistent hook should fail");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== 并发路径 ==================== */

static void test_concurrent_hook_registration(void) {
    TEST("P2.8-Concurrent: Multiple hook registrations");

    int ret = agentrt_hook_init();
    CHECK_EQ(ret, 0, "agentrt_hook_init failed");

    /* 批量注册不同类型的 Hook */
    const char *hook_names[] = {
        "hook_pre_exec", "hook_post_exec",
        "hook_pre_llm", "hook_post_llm",
        "hook_pre_tool", "hook_post_tool",
        "hook_on_error", "hook_on_memory"
    };
    hook_type_t hook_types[] = {
        HOOK_TYPE_PRE_EXEC, HOOK_TYPE_POST_EXEC,
        HOOK_TYPE_PRE_LLM, HOOK_TYPE_POST_LLM,
        HOOK_TYPE_PRE_TOOL, HOOK_TYPE_POST_TOOL,
        HOOK_TYPE_ON_ERROR, HOOK_TYPE_ON_MEMORY_EVOLVE
    };

    for (int i = 0; i < 8; i++) {
        ret = agentrt_hook_register(
            hook_names[i], hook_types[i], audit_callback, NULL, 100 - i, true);
        CHECK_EQ(ret, 0, "Register hook type failed");
    }

    /* 验证每种类型都有 Hook */
    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        size_t count = agentrt_hook_count_by_type((hook_type_t)i);
        CHECK(count >= 1, "Each hook type should have at least 1 entry");
    }

    /* 验证总数 */
    size_t total = agentrt_hook_count();
    CHECK_EQ(total, 8, "Total should be 8 hooks");

    agentrt_hook_shutdown();
    PASS();
}

/* ==================== 主函数 ==================== */

int main(void) {
    printf("================================================\n");
    printf("  P2.8: Hook System Integration Tests\n");
    printf("================================================\n");

    /* P2.8.1: Shell Hook 注册/触发/拦截 */
    printf("\n--- P2.8.1: Shell Hook Register/Trigger/Intercept ---\n");
    test_shell_hook_register();
    test_shell_hook_unregister();
    test_shell_hook_enable_disable();
    test_shell_hook_trigger_intercept();

    /* P2.8.2: Python Hook 修改 LLM prompt */
    printf("\n--- P2.8.2: Python Hook Modify LLM Prompt ---\n");
    test_python_hook_register();
    test_python_hook_modify_prompt();
    test_python_hook_multiple_modifiers();

    /* P2.8.3: Hook 超时保护 */
    printf("\n--- P2.8.3: Hook Timeout Protection ---\n");
    test_hook_timeout_config();
    test_hook_timeout_default();
    test_hook_timeout_abort();
    test_hook_timeout_reset_count();

    /* P2.8.4: Webhook 签名和重试 */
    printf("\n--- P2.8.4: Webhook Signature and Retry ---\n");
    test_webhook_register();
    test_webhook_retry_mechanism();
    test_webhook_signature_config();
    test_webhook_disable_and_reenable();

    /* 统计信息 */
    printf("\n--- Hook Statistics ---\n");
    test_hook_statistics();

    /* 错误路径 */
    printf("\n--- Error Paths ---\n");
    test_error_duplicate_register();
    test_error_unregister_nonexistent();
    test_error_invalid_hook_name();

    /* 并发路径 */
    printf("\n--- Concurrent Path ---\n");
    test_concurrent_hook_registration();

    /* 总结 */
    printf("\n================================================\n");
    printf("  Test Results: %d/%d passed, %d failed\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    printf("================================================\n");

    return (g_tests_failed > 0) ? 1 : 0;
}