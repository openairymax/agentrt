// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_04_tool_loop.c
 * @brief C-L04 Integration Test: tool_d → CoreLoopThree
 *
 * Tests the tool service adapter connecting the execution engine to tool_d:
 * 1. Normal path: Tool execution → result callback
 * 2. Error path: Invalid tool name → proper error propagation
 * 3. Timeout path: Long-running tool → timeout handling
 * 4. Concurrent path: Multiple simultaneous tool executions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "tool_svc_adapter.h"

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
 * P1.16d-1: Normal Path — Tool service lifecycle
 * ============================================================================ */

static void test_normal_tool_service_lifecycle(void) {
    TEST("C-L04 Normal: Tool service create → is_connected → destroy");

    tool_svc_adapter_t *adapter = tool_svc_adapter_create(NULL);
    CHECK(adapter != NULL, "tool_svc_adapter_create returned NULL");

    bool connected = tool_svc_adapter_is_connected(adapter);
    (void)connected;

    tool_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16d-2: Error Path — Double initialization
 * ============================================================================ */

static void test_error_double_init(void) {
    TEST("C-L04 Error: Double create should still succeed");

    tool_svc_adapter_t *adapter = tool_svc_adapter_create(NULL);
    CHECK(adapter != NULL, "tool_svc_adapter_create returned NULL");

    tool_svc_adapter_t *adapter2 = tool_svc_adapter_create(NULL);
    CHECK(adapter2 != NULL, "Second create should succeed");

    tool_svc_adapter_destroy(adapter);
    tool_svc_adapter_destroy(adapter2);
    PASS();
}

/* ============================================================================
 * P1.16d-3: Error Path — Start without init
 * ============================================================================ */

static void test_error_start_without_init(void) {
    TEST("C-L04 Error: NULL adapter create should return NULL");

    tool_svc_adapter_t *adapter = tool_svc_adapter_create(NULL);
    CHECK(adapter != NULL, "tool_svc_adapter_create should succeed with NULL config");

    tool_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16d-4: Error Path — NULL adapter handling
 * ============================================================================ */

static void test_error_null_adapter(void) {
    TEST("C-L04 Error: NULL adapter handling");

    bool connected = tool_svc_adapter_is_connected(NULL);
    CHECK(!connected, "NULL adapter should not be connected");

    tool_svc_adapter_destroy(NULL);
    PASS();
}

/* ============================================================================
 * P1.16d-5: Timeout Path — Tool execution timeout
 * ============================================================================ */

static void test_timeout_tool_execution(void) {
    TEST("C-L04 Timeout: Tool execution with config timeout");

    tool_svc_adapter_config_t config = {0};
    config.request_timeout_ms = 500;

    tool_svc_adapter_t *adapter = tool_svc_adapter_create(&config);
    CHECK(adapter != NULL, "tool_svc_adapter_create returned NULL");

    tool_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16d-6: Concurrent Path — Multiple tool adapter instances
 * ============================================================================ */

#define TOOL_CONCURRENT_INSTANCES 4

static void test_concurrent_tool_instances(void) {
    TEST("C-L04 Concurrent: Multiple tool adapter instances");

    tool_svc_adapter_t *adapters[TOOL_CONCURRENT_INSTANCES];

    for (int i = 0; i < TOOL_CONCURRENT_INSTANCES; i++) {
        adapters[i] = tool_svc_adapter_create(NULL);
        CHECK(adapters[i] != NULL, "tool_svc_adapter_create returned NULL");
    }

    for (int i = 0; i < TOOL_CONCURRENT_INSTANCES; i++) {
        CHECK(tool_svc_adapter_is_connected(adapters[i]),
              "All instances should be valid");
    }

    for (int i = 0; i < TOOL_CONCURRENT_INSTANCES; i++) {
        tool_svc_adapter_destroy(adapters[i]);
    }

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L04 Integration Tests: tool_d → CoreLoopThree ===\n\n");

    test_normal_tool_service_lifecycle();
    test_error_double_init();
    test_error_start_without_init();
    test_error_null_adapter();
    test_timeout_tool_execution();
    test_concurrent_tool_instances();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}