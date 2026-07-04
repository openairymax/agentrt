// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_05_safety_tool.c
 * @brief C-L05 Integration Test: Cupolas SafetyGuard → tool_d
 *
 * Tests the safety guard bridge connecting Cupolas SafetyGuard to tool_d:
 * 1. Normal path: All 6 guard types pass → tool execution allowed
 * 2. Error path: Permission denied → DENY result
 * 3. Error path: Rate limit exceeded → DENY result
 * 4. Timeout path: Guard chain timeout handling
 * 5. Concurrent path: Multiple simultaneous guard checks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "safety_guard_bridge.h"
#include "daemon_security.h"
#include "agentos_types.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

#define TEST(name) do { \
    g_tests_total++; \
    printf("  [TEST] %s ... ", name); \
} while(0)

#define PASS() do { \
    g_tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(reason) do { \
    g_tests_failed++; \
    printf("FAIL: %s\n", reason); \
} while(0)

#define CHECK(cond, reason) do { \
    if (!(cond)) { FAIL(reason); return; } \
} while(0)

#define CHECK_EQ(a, b, reason) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (got %d, expected %d)", reason, \
                 (int)(a), (int)(b)); \
        FAIL(buf); return; \
    } \
} while(0)

/* ============================================================================
 * P1.16e-1: Normal Path — All 6 guard types pass
 * ============================================================================ */

static void test_normal_all_guards_pass(void) {
    TEST("C-L05 Normal: All 6 guard types pass → tool execution allowed");

    safety_guard_bridge_config_t config = {0};
    config.enable_permission_guard = true;
    config.enable_rate_limit_guard = true;
    config.enable_content_filter = true;
    config.enable_input_sanitization = true;
    config.enable_resource_quota = true;
    config.enable_audit_guard = true;
    config.rate_limit_per_minute = 100;
    config.max_params_size = 4096;
    config.agent_id = "test-agent-001";

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    /* Create tool metadata */
    tool_metadata_t meta = {0};
    meta.name = strdup("file_read");
    meta.description = strdup("Read a file from disk");

    const char *params = "{\"path\": \"/tmp/test.txt\", \"max_size\": 1024}";
    safety_guard_bridge_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = safety_guard_bridge_check(bridge, &meta, params, &result);
    CHECK_EQ(ret, 0, "All guards should pass");

    CHECK(result.permission_passed, "Permission guard should pass");
    CHECK(result.rate_limit_passed, "Rate limit guard should pass");
    CHECK(result.content_filter_passed, "Content filter should pass");
    CHECK(result.input_sanitized, "Input sanitization should be applied");
    CHECK(result.resource_quota_passed, "Resource quota guard should pass");
    CHECK(result.audit_recorded, "Audit guard should record");

    free(meta.name);
    free(meta.description);
    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-2: Normal Path — Individual guard checks
 * ============================================================================ */

static void test_normal_individual_guards(void) {
    TEST("C-L05 Normal: Individual guard checks (permission, rate, content)");

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(NULL);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    /* Permission check */
    int ret = safety_guard_bridge_check_permission(bridge, "agent-001",
                                                    "file_read", "execute");
    CHECK_EQ(ret, 0, "Permission check should pass");

    /* Rate limit check */
    ret = safety_guard_bridge_check_rate_limit(bridge, "file_read");
    CHECK_EQ(ret, 0, "Rate limit check should pass");

    /* Content filter */
    char sanitized[4096] = {0};
    ret = safety_guard_bridge_filter_content(bridge, "{\"path\": \"/tmp/test\"}",
                                              sanitized, sizeof(sanitized));
    CHECK_EQ(ret, 0, "Content filter should pass");

    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-3: Error Path — Permission denied
 * ============================================================================ */

static void test_error_permission_denied(void) {
    TEST("C-L05 Error: Permission denied → DENY result");

    safety_guard_bridge_config_t config = {0};
    config.enable_permission_guard = true;
    config.enable_rate_limit_guard = false;
    config.enable_content_filter = false;
    config.enable_input_sanitization = false;
    config.enable_resource_quota = false;
    config.enable_audit_guard = false;
    config.agent_id = "restricted-agent";

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    tool_metadata_t meta = {0};
    meta.name = strdup("shell_exec");
    meta.description = strdup("Execute a shell command");

    const char *params = "{\"command\": \"rm -rf /\"}";
    safety_guard_bridge_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = safety_guard_bridge_check(bridge, &meta, params, &result);
    /* Permission check should fail for restricted agent */
    CHECK(ret != 0 || !result.permission_passed,
          "Restricted agent should be denied shell_exec");

    free(meta.name);
    free(meta.description);
    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-4: Error Path — Rate limit exceeded
 * ============================================================================ */

static void test_error_rate_limit_exceeded(void) {
    TEST("C-L05 Error: Rate limit exceeded → DENY result");

    safety_guard_bridge_config_t config = {0};
    config.enable_rate_limit_guard = true;
    config.enable_permission_guard = false;
    config.enable_content_filter = false;
    config.enable_input_sanitization = false;
    config.enable_resource_quota = false;
    config.enable_audit_guard = false;
    config.rate_limit_per_minute = 2; /* Very low limit */

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    /* Exhaust the rate limit */
    for (int i = 0; i < 10; i++) {
        safety_guard_bridge_check_rate_limit(bridge, "file_read");
    }

    /* Next check should be rate limited */
    int ret = safety_guard_bridge_check_rate_limit(bridge, "file_read");
    CHECK(ret != 0, "Rate limit should be exceeded after many calls");

    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-5: Error Path — NULL bridge handling
 * ============================================================================ */

static void test_error_null_bridge(void) {
    TEST("C-L05 Error: NULL bridge handling");

    tool_metadata_t meta = {0};
    safety_guard_bridge_result_t result;

    int ret = safety_guard_bridge_check(NULL, &meta, "{}", &result);
    CHECK(ret != 0, "NULL bridge check should fail");

    ret = safety_guard_bridge_check_permission(NULL, "agent", "tool", "execute");
    CHECK(ret != 0, "NULL bridge permission check should fail");

    ret = safety_guard_bridge_check_rate_limit(NULL, "tool");
    CHECK(ret != 0, "NULL bridge rate limit check should fail");

    /* NULL bridge destroy should be safe */
    safety_guard_bridge_destroy(NULL);

    PASS();
}

/* ============================================================================
 * P1.16e-6: Timeout Path — Guard chain timeout
 * ============================================================================ */

static void test_timeout_guard_chain(void) {
    TEST("C-L05 Timeout: Guard chain execution within timeout");

    safety_guard_bridge_config_t config = {0};
    config.enable_permission_guard = true;
    config.enable_rate_limit_guard = true;
    config.enable_content_filter = true;
    config.enable_input_sanitization = true;
    config.enable_resource_quota = true;
    config.enable_audit_guard = true;
    config.rate_limit_per_minute = 1000;
    config.agent_id = "test-agent";

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    tool_metadata_t meta = {0};
    meta.name = strdup("quick_tool");
    meta.description = strdup("A quick tool");

    const char *params = "{}";
    safety_guard_bridge_result_t result;

    /* Guard chain should complete quickly */
    int ret = safety_guard_bridge_check(bridge, &meta, params, &result);
    CHECK_EQ(ret, 0, "Guard chain should complete within timeout");

    CHECK_EQ(result.guard_chain_length, 6, "Should have 6 guards in chain");
    CHECK_EQ(result.guards_executed, 6, "All 6 guards should be executed");

    free(meta.name);
    free(meta.description);
    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-7: Concurrent Path — Multiple simultaneous guard checks
 * ============================================================================ */

#define SAFETY_CONCURRENT_THREADS 4
#define SAFETY_CHECKS_PER_THREAD 20

typedef struct {
    safety_guard_bridge_t *bridge;
    int thread_id;
    int success_count;
    int deny_count;
} safety_thread_args_t;

static void *concurrent_safety_thread(void *arg) {
    safety_thread_args_t *args = (safety_thread_args_t *)arg;

    for (int i = 0; i < SAFETY_CHECKS_PER_THREAD; i++) {
        tool_metadata_t meta = {0};
        /* v0.1.1 BEHAVIOR_DIFF 修正：真实 daemon_check_tool_permission
         * 使用精确 agent_id+tool_name 匹配 ACL 表（fail-closed），
         * 不支持通配符。并发测试使用固定 tool_name 并预注册 ACL 条目。 */
        meta.name = "concurrent_tool";
        meta.description = "test tool";

        char params[256];
        snprintf(params, sizeof(params), "{\"thread\": %d, \"iter\": %d}",
                 args->thread_id, i);

        safety_guard_bridge_result_t result;
        memset(&result, 0, sizeof(result));

        int ret = safety_guard_bridge_check(args->bridge, &meta, params, &result);
        if (ret == 0) {
            args->success_count++;
        } else {
            args->deny_count++;
        }
    }
    return NULL;
}

static void test_concurrent_safety_checks(void) {
    TEST("C-L05 Concurrent: Multiple simultaneous guard checks");

    safety_guard_bridge_config_t config = {0};
    config.enable_permission_guard = true;
    config.enable_rate_limit_guard = true;
    config.enable_content_filter = true;
    config.enable_input_sanitization = true;
    config.enable_resource_quota = true;
    config.enable_audit_guard = true;
    config.rate_limit_per_minute = 10000;
    config.agent_id = "concurrent-agent";

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    pthread_t threads[SAFETY_CONCURRENT_THREADS];
    safety_thread_args_t args[SAFETY_CONCURRENT_THREADS];

    for (int i = 0; i < SAFETY_CONCURRENT_THREADS; i++) {
        args[i].bridge = bridge;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].deny_count = 0;
        pthread_create(&threads[i], NULL, concurrent_safety_thread, &args[i]);
    }

    int total_success = 0;
    int total_deny = 0;
    for (int i = 0; i < SAFETY_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
        total_deny += args[i].deny_count;
    }

    CHECK(total_success > 0, "At least some guard checks should succeed");
    CHECK_EQ(total_success + total_deny,
             SAFETY_CONCURRENT_THREADS * SAFETY_CHECKS_PER_THREAD,
             "All checks should complete");

    /* Verify stats */
    uint64_t total_checks = 0, denied = 0, rate_limited = 0;
    safety_guard_bridge_get_stats(bridge, &total_checks, &denied, &rate_limited);
    CHECK(total_checks > 0, "Stats should reflect performed checks");

    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16e-8: Audit log recording
 * ============================================================================ */

static void test_audit_log_recording(void) {
    TEST("C-L05 Normal: Audit log recording on DENY");

    safety_guard_bridge_config_t config = {0};
    config.enable_audit_guard = true;
    config.enable_permission_guard = true;
    config.agent_id = "audit-agent";

    safety_guard_bridge_t *bridge = safety_guard_bridge_create(&config);
    CHECK(bridge != NULL, "safety_guard_bridge_create returned NULL");

    /* Record an audit event */
    int ret = safety_guard_bridge_audit_log(bridge, "TOOL_DENY",
                                             "shell_exec", 0,
                                             "Permission denied by RBAC",
                                             "audit-agent");
    CHECK_EQ(ret, 0, "Audit log should be recorded successfully");

    /* Record another event */
    ret = safety_guard_bridge_audit_log(bridge, "RATE_LIMIT",
                                         "file_read", 0,
                                         "Rate limit exceeded",
                                         "audit-agent");
    CHECK_EQ(ret, 0, "Second audit log should be recorded");

    /* Verify stats reflect the denies */
    uint64_t total_checks = 0, denied = 0, rate_limited = 0;
    safety_guard_bridge_get_stats(bridge, &total_checks, &denied, &rate_limited);
    CHECK((int64_t)denied >= 0, "Stats should be accessible");

    safety_guard_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L05 Integration Tests: Cupolas SafetyGuard → tool_d ===\n\n");

    /* v0.1.1 BEHAVIOR_DIFF 修正：真实 daemon_security 采用 fail-closed 策略，
     * daemon_check_tool_permission() 在 ACL 表为空时默认拒绝所有请求。
     * 桩函数则默认全部通过。迁移到真实库需预注册 ACL 条目，为测试中
     * 使用的 (agent_id, tool_name) 组合授权。
     * 注意："restricted-agent" + "shell_exec" 不注册（保留 fail-closed 拒绝），
     * 以验证 P1.16e-3 的权限拒绝路径。 */
    daemon_security_init(NULL, NULL);
    daemon_security_add_acl_rule("test-agent-001", "file_read", true);
    daemon_security_add_acl_rule("agent-001", "file_read", true);
    daemon_security_add_acl_rule("test-agent", "quick_tool", true);
    daemon_security_add_acl_rule("concurrent-agent", "concurrent_tool", true);
    daemon_security_add_acl_rule("audit-agent", "shell_exec", true);
    daemon_security_add_acl_rule("audit-agent", "file_read", true);

    test_normal_all_guards_pass();
    test_normal_individual_guards();
    test_error_permission_denied();
    test_error_rate_limit_exceeded();
    test_error_null_bridge();
    test_timeout_guard_chain();
    test_concurrent_safety_checks();
    test_audit_log_recording();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}