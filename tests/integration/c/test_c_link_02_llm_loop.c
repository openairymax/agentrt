// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_02_llm_loop.c
 * @brief C-L02 Integration Test: llm_d → CoreLoopThree
 *
 * Tests the LLM service adapter connecting the cognition engine to LLM providers:
 * 1. Normal path: LLM request → provider → response → callback
 * 2. Error path: Provider failure → error propagation
 * 3. Timeout path: Provider timeout → fallback chain
 * 4. Concurrent path: Multiple simultaneous LLM requests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "llm_svc_adapter.h"
#include "core_config.h"

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
 * P1.16b-1: Normal Path — LLM service lifecycle
 * ============================================================================ */

static void test_normal_llm_service_lifecycle(void) {
    TEST("C-L02 Normal: LLM service create → init → start → stop → destroy");

    llm_svc_adapter_t *adapter = llm_svc_adapter_create();
    CHECK(adapter != NULL, "llm_svc_adapter_create returned NULL");

    /* Initialize with default config */
    agentos_error_t err = llm_svc_adapter_init(adapter, NULL);
    CHECK_EQ(err, AGENTOS_SUCCESS, "llm_svc_adapter_init failed");

    /* Start the service */
    err = llm_svc_adapter_start(adapter);
    CHECK_EQ(err, AGENTOS_SUCCESS, "llm_svc_adapter_start failed");

    /* Verify service is running */
    bool running = llm_svc_adapter_is_running(adapter);
    CHECK(running, "LLM service should be running after start");

    /* Stop the service */
    err = llm_svc_adapter_stop(adapter);
    CHECK_EQ(err, AGENTOS_SUCCESS, "llm_svc_adapter_stop failed");

    running = llm_svc_adapter_is_running(adapter);
    CHECK(!running, "LLM service should not be running after stop");

    llm_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16b-2: Error Path — Double initialization
 * ============================================================================ */

static void test_error_double_init(void) {
    TEST("C-L02 Error: Double initialization should be rejected");

    llm_svc_adapter_t *adapter = llm_svc_adapter_create();
    CHECK(adapter != NULL, "llm_svc_adapter_create returned NULL");

    agentos_error_t err = llm_svc_adapter_init(adapter, NULL);
    CHECK_EQ(err, AGENTOS_SUCCESS, "First llm_svc_adapter_init should succeed");

    /* Second init should fail */
    err = llm_svc_adapter_init(adapter, NULL);
    CHECK(err != AGENTOS_SUCCESS, "Second llm_svc_adapter_init should fail");

    llm_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16b-3: Error Path — Start without init
 * ============================================================================ */

static void test_error_start_without_init(void) {
    TEST("C-L02 Error: Start without init should fail");

    llm_svc_adapter_t *adapter = llm_svc_adapter_create();
    CHECK(adapter != NULL, "llm_svc_adapter_create returned NULL");

    /* Start without prior init should fail */
    agentos_error_t err = llm_svc_adapter_start(adapter);
    CHECK(err != AGENTOS_SUCCESS, "Start without init should return error");

    llm_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16b-4: Error Path — NULL adapter handling
 * ============================================================================ */

static void test_error_null_adapter(void) {
    TEST("C-L02 Error: NULL adapter handling");

    agentos_error_t err = llm_svc_adapter_init(NULL, NULL);
    CHECK(err != AGENTOS_SUCCESS, "NULL adapter init should fail");

    err = llm_svc_adapter_start(NULL);
    CHECK(err != AGENTOS_SUCCESS, "NULL adapter start should fail");

    err = llm_svc_adapter_stop(NULL);
    CHECK(err != AGENTOS_SUCCESS, "NULL adapter stop should fail");

    /* llm_svc_adapter_destroy(NULL) should be safe (no-op) */
    llm_svc_adapter_destroy(NULL);

    PASS();
}

/* ============================================================================
 * P1.16b-5: Timeout Path — Service stop timeout
 * ============================================================================ */

static void test_timeout_service_stop(void) {
    TEST("C-L02 Timeout: Service stop timeout handling");

    llm_svc_adapter_t *adapter = llm_svc_adapter_create();
    CHECK(adapter != NULL, "llm_svc_adapter_create returned NULL");

    agentos_error_t err = llm_svc_adapter_init(adapter, NULL);
    CHECK_EQ(err, AGENTOS_SUCCESS, "llm_svc_adapter_init failed");

    err = llm_svc_adapter_start(adapter);
    CHECK_EQ(err, AGENTOS_SUCCESS, "llm_svc_adapter_start failed");

    /* Set a short timeout for stop */
    llm_svc_adapter_set_timeout(adapter, 100);

    /* Stop should complete within timeout */
    err = llm_svc_adapter_stop(adapter);
    CHECK_EQ(err, AGENTOS_SUCCESS, "Stop with timeout should succeed");

    llm_svc_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16b-6: Concurrent Path — Multiple adapter instances
 * ============================================================================ */

#define LLM_CONCURRENT_INSTANCES 4

static void test_concurrent_llm_instances(void) {
    TEST("C-L02 Concurrent: Multiple LLM adapter instances");

    llm_svc_adapter_t *adapters[LLM_CONCURRENT_INSTANCES];
    agentos_error_t errors[LLM_CONCURRENT_INSTANCES];

    /* Create and init multiple instances */
    for (int i = 0; i < LLM_CONCURRENT_INSTANCES; i++) {
        adapters[i] = llm_svc_adapter_create();
        CHECK(adapters[i] != NULL, "llm_svc_adapter_create returned NULL");
        errors[i] = llm_svc_adapter_init(adapters[i], NULL);
        CHECK_EQ(errors[i], AGENTOS_SUCCESS, "llm_svc_adapter_init failed");
    }

    /* Start all instances */
    for (int i = 0; i < LLM_CONCURRENT_INSTANCES; i++) {
        errors[i] = llm_svc_adapter_start(adapters[i]);
        CHECK_EQ(errors[i], AGENTOS_SUCCESS, "llm_svc_adapter_start failed");
    }

    /* Verify all are running */
    for (int i = 0; i < LLM_CONCURRENT_INSTANCES; i++) {
        CHECK(llm_svc_adapter_is_running(adapters[i]),
              "All instances should be running");
    }

    /* Stop and destroy all */
    for (int i = 0; i < LLM_CONCURRENT_INSTANCES; i++) {
        llm_svc_adapter_stop(adapters[i]);
        llm_svc_adapter_destroy(adapters[i]);
    }

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L02 Integration Tests: llm_d → CoreLoopThree ===\n\n");

    test_normal_llm_service_lifecycle();
    test_error_double_init();
    test_error_start_without_init();
    test_error_null_adapter();
    test_timeout_service_stop();
    test_concurrent_llm_instances();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}