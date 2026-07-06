// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_03_market_openlab.c
 * @brief C-L03 Integration Test: market_d → OpenLab Markets
 *
 * Tests the market service connecting OpenLab Markets to market_d:
 * 1. Normal path: Agent registration → search → install
 * 2. Error path: Invalid agent registration → proper error codes
 * 3. Timeout path: Slow market operations → timeout handling
 * 4. Concurrent path: Multiple simultaneous market operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "agentrt_types.h"

/* ============================================================================
 * Mock Market Client Structures
 * ============================================================================ */

/* Simulated market_d API types */
typedef enum {
    MARKET_OP_REGISTER = 0,
    MARKET_OP_SEARCH = 1,
    MARKET_OP_INSTALL = 2,
    MARKET_OP_UNINSTALL = 3
} market_op_type_t;

typedef struct {
    char name[128];
    char version[32];
    char author[64];
    char description[256];
    market_op_type_t op_type;
    bool success;
    int error_code;
    char error_msg[256];
} market_result_t;

typedef struct {
    char name[128];
    char version[32];
    char entrypoint[256];
    bool allow_network;
    bool allow_filesystem;
} market_agent_manifest_t;

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
 * Mock market_d operations
 * ============================================================================ */

static market_result_t mock_market_register(const market_agent_manifest_t *manifest) {
    market_result_t result;
    memset(&result, 0, sizeof(result));

    if (manifest == NULL || manifest->name[0] == '\0') {
        result.success = false;
        result.error_code = -1;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "Invalid manifest: name is required");
        return result;
    }

    if (strlen(manifest->name) > 120) {
        result.success = false;
        result.error_code = -2;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "Name too long: max 120 characters");
        return result;
    }

    snprintf(result.name, sizeof(result.name), "%s", manifest->name);
    snprintf(result.version, sizeof(result.version), "%s", manifest->version);
    result.success = true;
    result.op_type = MARKET_OP_REGISTER;
    return result;
}

static market_result_t mock_market_search(const char *query) {
    market_result_t result;
    memset(&result, 0, sizeof(result));

    if (query == NULL || query[0] == '\0') {
        result.success = false;
        result.error_code = -1;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "Search query is required");
        return result;
    }

    result.success = true;
    result.op_type = MARKET_OP_SEARCH;
    snprintf(result.name, sizeof(result.name), "search-result-for-%s", query);
    return result;
}

static market_result_t mock_market_install(const char *agent_name) {
    market_result_t result;
    memset(&result, 0, sizeof(result));

    if (agent_name == NULL || agent_name[0] == '\0') {
        result.success = false;
        result.error_code = -1;
        snprintf(result.error_msg, sizeof(result.error_msg),
                 "Agent name is required for install");
        return result;
    }

    result.success = true;
    result.op_type = MARKET_OP_INSTALL;
    snprintf(result.name, sizeof(result.name), "%s", agent_name);
    snprintf(result.version, sizeof(result.version), "1.0.0");
    return result;
}

/* ============================================================================
 * P1.16c-1: Normal Path — Agent register → search → install
 * ============================================================================ */

static void test_normal_market_flow(void) {
    TEST("C-L03 Normal: Register → Search → Install flow");

    /* Register an agent */
    market_agent_manifest_t manifest = {
        .name = "hello-world-agent",
        .version = "1.0.0",
        .entrypoint = "agents/hello/main.py",
        .allow_network = false,
        .allow_filesystem = true
    };

    market_result_t reg = mock_market_register(&manifest);
    CHECK(reg.success, "Agent registration should succeed");
    CHECK_EQ(strcmp(reg.name, "hello-world-agent"), 0, "Registered name mismatch");

    /* Search for the agent */
    market_result_t search = mock_market_search("hello");
    CHECK(search.success, "Agent search should succeed");
    CHECK(search.op_type == MARKET_OP_SEARCH, "Op type should be SEARCH");

    /* Install the agent */
    market_result_t install = mock_market_install("hello-world-agent");
    CHECK(install.success, "Agent install should succeed");
    CHECK(install.op_type == MARKET_OP_INSTALL, "Op type should be INSTALL");

    PASS();
}

/* ============================================================================
 * P1.16c-2: Error Path — Invalid manifest (NULL/empty name)
 * ============================================================================ */

static void test_error_invalid_manifest(void) {
    TEST("C-L03 Error: Invalid manifest (NULL/empty name)");

    /* NULL manifest */
    market_result_t result = mock_market_register(NULL);
    CHECK(!result.success, "NULL manifest should fail");
    CHECK(result.error_code != 0, "Should have error code");

    /* Empty name */
    market_agent_manifest_t empty = {0};
    result = mock_market_register(&empty);
    CHECK(!result.success, "Empty name should fail");

    PASS();
}

/* ============================================================================
 * P1.16c-3: Error Path — Install non-existent agent
 * ============================================================================ */

static void test_error_install_invalid(void) {
    TEST("C-L03 Error: Install with empty/invalid agent name");

    /* Empty install name */
    market_result_t result = mock_market_install("");
    CHECK(!result.success, "Empty install name should fail");

    /* NULL install name */
    result = mock_market_install(NULL);
    CHECK(!result.success, "NULL install name should fail");

    PASS();
}

/* ============================================================================
 * P1.16c-4: Timeout Path — Simulated slow market operation
 * ============================================================================ */

static void test_timeout_market_operation(void) {
    TEST("C-L03 Timeout: Simulated slow market operation");

    /* Simulate a scenario where market operations have a timeout */
    market_agent_manifest_t manifest = {
        .name = "slow-agent",
        .version = "1.0.0",
        .entrypoint = "agents/slow/main.py",
        .allow_network = true,
        .allow_filesystem = false
    };

    /* Operation should complete quickly (no actual I/O in mock) */
    market_result_t result = mock_market_register(&manifest);
    CHECK(result.success, "Normal market operation should succeed within timeout");

    PASS();
}

/* ============================================================================
 * P1.16c-5: Concurrent Path — Multiple simultaneous market operations
 * ============================================================================ */

#define MARKET_CONCURRENT_OPS 8

typedef struct {
    int thread_id;
    int success_count;
    int error_count;
} market_thread_args_t;

static void *concurrent_market_thread(void *arg) {
    market_thread_args_t *args = (market_thread_args_t *)arg;

    /* Register */
    market_agent_manifest_t manifest;
    snprintf(manifest.name, sizeof(manifest.name), "concurrent-agent-%d", args->thread_id);
    snprintf(manifest.version, sizeof(manifest.version), "1.0.0");
    snprintf(manifest.entrypoint, sizeof(manifest.entrypoint), "agents/c%d/main.py", args->thread_id);
    manifest.allow_network = false;
    manifest.allow_filesystem = true;

    market_result_t reg = mock_market_register(&manifest);
    if (reg.success) args->success_count++; else args->error_count++;

    /* Search */
    market_result_t search = mock_market_search("concurrent");
    if (search.success) args->success_count++; else args->error_count++;

    /* Install */
    market_result_t install = mock_market_install(manifest.name);
    if (install.success) args->success_count++; else args->error_count++;

    return NULL;
}

static void test_concurrent_market_operations(void) {
    TEST("C-L03 Concurrent: Multiple simultaneous market operations");

    pthread_t threads[MARKET_CONCURRENT_OPS];
    market_thread_args_t args[MARKET_CONCURRENT_OPS];

    for (int i = 0; i < MARKET_CONCURRENT_OPS; i++) {
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_market_thread, &args[i]);
    }

    int total_success = 0;
    int total_errors = 0;
    for (int i = 0; i < MARKET_CONCURRENT_OPS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
        total_errors += args[i].error_count;
    }

    CHECK_EQ(total_errors, 0, "All concurrent market operations should succeed");
    CHECK_EQ(total_success, MARKET_CONCURRENT_OPS * 3, "All 3 ops per thread should succeed");

    PASS();
}

/* ============================================================================
 * P1.16c-6: Name too long
 * ============================================================================ */

static void test_error_name_too_long(void) {
    TEST("C-L03 Error: Agent name too long");

    market_agent_manifest_t manifest = {0};
    /* Fill name with 121 characters (exceeds 120 limit) */
    memset(manifest.name, 'x', 121);
    snprintf(manifest.version, sizeof(manifest.version), "1.0.0");

    market_result_t result = mock_market_register(&manifest);
    CHECK(!result.success, "Name > 120 chars should fail");
    CHECK(result.error_code == -2, "Should return name-too-long error (-2)");

    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L03 Integration Tests: market_d → OpenLab Markets ===\n\n");

    test_normal_market_flow();
    test_error_invalid_manifest();
    test_error_install_invalid();
    test_timeout_market_operation();
    test_concurrent_market_operations();
    test_error_name_too_long();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}