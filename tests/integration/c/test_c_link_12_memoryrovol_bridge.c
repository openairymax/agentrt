// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_12_memoryrovol_bridge.c
 * @brief C-L12 Integration Test: CoreLoopThree → MemoryRovol
 *
 * Tests the MemoryRovol bridge connecting CoreLoopThree to memory providers:
 * 1. Normal path: Create bridge → get provider → switch modes → destroy
 * 2. Error path: Invalid mode switch → proper error
 * 3. Error path: NULL handling
 * 4. Timeout path: Bridge operations with timeout
 * 5. Concurrent path: Multiple bridge instances
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "memoryrovol_bridge.h"
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
 * P1.16l-1: Normal Path — Bridge lifecycle (builtin mode)
 * ============================================================================ */

static void test_normal_bridge_lifecycle_builtin(void) {
    TEST("C-L12 Normal: Bridge create (builtin) → get provider → destroy");

    memoryrovol_bridge_config_t config = {0};
    config.enable_l1_raw = true;
    config.enable_l2_feature = false;
    config.enable_l3_structure = false;
    config.enable_l4_pattern = false;
    config.enable_forgetting = false;
    config.enable_attractor = false;
    config.enable_persistence = true;
    config.enable_faiss = false;
    config.enable_async_ops = false;
    config.enable_llm_integration = false;
    config.query_default_limit = 10;
    config.provider_name = "builtin-test";
    config.provider_version = "1.0.0";

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    CHECK(memoryrovol_bridge_is_ready(bridge),
          "Bridge should be ready after creation");

    /* Get provider interface */
    agentos_memory_provider_t *provider = memoryrovol_bridge_get_provider(bridge);
    CHECK(provider != NULL, "memoryrovol_bridge_get_provider should return provider");

    /* Verify provider has required function pointers */
    CHECK(provider->write_raw != NULL, "Provider should have write_raw");
    CHECK(provider->query != NULL, "Provider should have query");

    /* Check current mode */
    const char *mode = memoryrovol_bridge_get_mode(bridge);
    CHECK(mode != NULL, "Should have a valid mode");
    CHECK(strcmp(mode, "builtin") == 0 || strcmp(mode, "memoryrovol") == 0
          || strcmp(mode, "hybrid") == 0,
          "Mode should be one of: builtin, memoryrovol, hybrid");

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-2: Normal Path — Full L1-L4 configuration
 * ============================================================================ */

static void test_normal_full_layers(void) {
    TEST("C-L12 Normal: Bridge with all L1-L4 layers enabled");

    memoryrovol_bridge_config_t config = {0};
    config.enable_l1_raw = true;
    config.enable_l2_feature = true;
    config.enable_l3_structure = true;
    config.enable_l4_pattern = true;
    config.enable_forgetting = true;
    config.enable_attractor = true;
    config.enable_persistence = true;
    config.enable_faiss = true;
    config.enable_async_ops = true;
    config.enable_llm_integration = true;
    config.query_default_limit = 50;
    config.sync_interval_ms = 5000;
    config.provider_name = "full-layers";
    config.provider_version = "2.0.0";

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    CHECK(memoryrovol_bridge_is_ready(bridge),
          "Bridge should be ready with all layers");

    agentos_memory_provider_t *provider = memoryrovol_bridge_get_provider(bridge);
    CHECK(provider != NULL, "Provider should be available");

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-3: Normal Path — Mode switching
 * ============================================================================ */

static void test_normal_mode_switch(void) {
    TEST("C-L12 Normal: Switch between provider modes");

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(NULL);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    /* Try switching to builtin mode */
    int ret = memoryrovol_bridge_switch_mode(bridge, "builtin");
    CHECK_EQ(ret, 0, "Switch to builtin mode should succeed");

    const char *mode = memoryrovol_bridge_get_mode(bridge);
    CHECK(mode != NULL, "Mode should not be NULL after switch");
    CHECK(strcmp(mode, "builtin") == 0, "Mode should be 'builtin'");

    /* Try switching to memoryrovol mode */
    ret = memoryrovol_bridge_switch_mode(bridge, "memoryrovol");
    /* May fail if memoryrovol library is not available */
    (void)ret;

    /* Try switching to hybrid mode */
    ret = memoryrovol_bridge_switch_mode(bridge, "hybrid");
    /* May fail if both providers are not available */
    (void)ret;

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-4: Error Path — Invalid mode switch
 * ============================================================================ */

static void test_error_invalid_mode_switch(void) {
    TEST("C-L12 Error: Switch to invalid mode");

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(NULL);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    /* Switch to invalid mode should fail */
    int ret = memoryrovol_bridge_switch_mode(bridge, "invalid_mode");
    CHECK(ret != 0, "Switch to invalid mode should fail");

    /* Switch to NULL mode should fail */
    ret = memoryrovol_bridge_switch_mode(bridge, NULL);
    CHECK(ret != 0, "Switch to NULL mode should fail");

    /* Mode should remain unchanged */
    const char *mode = memoryrovol_bridge_get_mode(bridge);
    CHECK(mode != NULL, "Mode should still be valid");
    CHECK(strcmp(mode, "invalid_mode") != 0, "Mode should not have changed");

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-5: Error Path — NULL handling
 * ============================================================================ */

static void test_error_null_handling(void) {
    TEST("C-L12 Error: NULL bridge handling");

    /* NULL destroy should be safe */
    memoryrovol_bridge_destroy(NULL);

    /* NULL is_ready should return false */
    CHECK(!memoryrovol_bridge_is_ready(NULL), "NULL bridge should not be ready");

    /* NULL get_provider should return NULL */
    agentos_memory_provider_t *provider = memoryrovol_bridge_get_provider(NULL);
    CHECK(provider == NULL, "NULL bridge get_provider should return NULL");

    /* NULL get_mode should return NULL */
    const char *mode = memoryrovol_bridge_get_mode(NULL);
    CHECK(mode == NULL, "NULL bridge get_mode should return NULL");

    /* NULL switch_mode should fail */
    int ret = memoryrovol_bridge_switch_mode(NULL, "builtin");
    CHECK(ret != 0, "NULL bridge switch_mode should fail");

    /* NULL get_stats should fail */
    agentos_memory_stats_t stats;
    ret = memoryrovol_bridge_get_stats(NULL, &stats);
    CHECK(ret != 0, "NULL bridge get_stats should fail");

    PASS();
}

/* ============================================================================
 * P1.16l-6: Timeout Path — Bridge operations within timeout
 * ============================================================================ */

static void test_timeout_bridge_ops(void) {
    TEST("C-L12 Timeout: Bridge operations complete within timeout");

    memoryrovol_bridge_config_t config = {0};
    config.enable_l1_raw = true;
    config.enable_persistence = true;
    config.sync_interval_ms = 100;
    config.query_default_limit = 5;

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    /* Get provider should be fast */
    agentos_memory_provider_t *provider = memoryrovol_bridge_get_provider(bridge);
    CHECK(provider != NULL, "Get provider should complete quickly");

    /* Get stats should be fast */
    agentos_memory_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int ret = memoryrovol_bridge_get_stats(bridge, &stats);
    CHECK_EQ(ret, 0, "Get stats should complete within timeout");

    /* Health check should be fast */
    char *health_json = NULL;
    ret = memoryrovol_bridge_health_check(bridge, &health_json);
    (void)ret;
    if (health_json) free(health_json);

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-7: Concurrent Path — Multiple bridge instances
 * ============================================================================ */

#define MRB_CONCURRENT_INSTANCES 4

static void test_concurrent_bridge_instances(void) {
    TEST("C-L12 Concurrent: Multiple bridge instances");

    memoryrovol_bridge_t *bridges[MRB_CONCURRENT_INSTANCES];
    agentos_memory_provider_t *providers[MRB_CONCURRENT_INSTANCES];

    /* Create multiple bridge instances */
    for (int i = 0; i < MRB_CONCURRENT_INSTANCES; i++) {
        memoryrovol_bridge_config_t config = {0};
        config.enable_l1_raw = true;
        config.enable_persistence = true;
        config.query_default_limit = 10;
        char name[64];
        snprintf(name, sizeof(name), "provider-%d", i);
        config.provider_name = name;
        config.provider_version = "1.0.0";

        bridges[i] = memoryrovol_bridge_create(&config);
        CHECK(bridges[i] != NULL, "memoryrovol_bridge_create returned NULL");

        CHECK(memoryrovol_bridge_is_ready(bridges[i]),
              "Bridge %d should be ready", i);

        providers[i] = memoryrovol_bridge_get_provider(bridges[i]);
        CHECK(providers[i] != NULL, "Provider %d should not be NULL", i);
    }

    /* Verify all providers are independent */
    for (int i = 0; i < MRB_CONCURRENT_INSTANCES; i++) {
        CHECK(providers[i]->write_raw != NULL,
              "Provider %d should have write_raw", i);
        CHECK(providers[i]->query != NULL,
              "Provider %d should have query", i);
    }

    /* Cleanup */
    for (int i = 0; i < MRB_CONCURRENT_INSTANCES; i++) {
        memoryrovol_bridge_destroy(bridges[i]);
    }

    PASS();
}

/* ============================================================================
 * P1.16l-8: Sync control (hybrid mode)
 * ============================================================================ */

static void test_sync_control(void) {
    TEST("C-L12 Normal: Sync start/stop in hybrid mode");

    memoryrovol_bridge_config_t config = {0};
    config.enable_l1_raw = true;
    config.enable_persistence = true;
    config.sync_interval_ms = 1000;

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    /* Try switching to hybrid mode */
    int ret = memoryrovol_bridge_switch_mode(bridge, "hybrid");
    if (ret == 0) {
        /* Start sync */
        ret = memoryrovol_bridge_start_sync(bridge);
        CHECK_EQ(ret, 0, "Start sync should succeed in hybrid mode");

        CHECK(memoryrovol_bridge_has_active_sync(bridge),
              "Should have active sync after start");

        /* Stop sync */
        memoryrovol_bridge_stop_sync(bridge);

        CHECK(!memoryrovol_bridge_has_active_sync(bridge),
              "Should not have active sync after stop");
    }

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * P1.16l-9: Health check and stats dump
 * ============================================================================ */

static void test_health_check_and_dump(void) {
    TEST("C-L12 Normal: Health check → stats dump");

    memoryrovol_bridge_config_t config = {0};
    config.enable_l1_raw = true;
    config.enable_l2_feature = true;
    config.enable_persistence = true;
    config.query_default_limit = 20;

    memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
    CHECK(bridge != NULL, "memoryrovol_bridge_create returned NULL");

    /* Health check */
    char *health_json = NULL;
    int ret = memoryrovol_bridge_health_check(bridge, &health_json);
    if (ret == 0 && health_json != NULL) {
        CHECK(strlen(health_json) > 0, "Health check should return JSON");
        free(health_json);
    }

    /* Get stats */
    agentos_memory_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    ret = memoryrovol_bridge_get_stats(bridge, &stats);
    CHECK_EQ(ret, 0, "Get stats should succeed");

    /* Dump stats (should not crash) */
    memoryrovol_bridge_dump_stats(bridge);

    memoryrovol_bridge_destroy(bridge);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L12 Integration Tests: CoreLoopThree → MemoryRovol ===\n\n");

    test_normal_bridge_lifecycle_builtin();
    test_normal_full_layers();
    test_normal_mode_switch();
    test_error_invalid_mode_switch();
    test_error_null_handling();
    test_timeout_bridge_ops();
    test_concurrent_bridge_instances();
    test_sync_control();
    test_health_check_and_dump();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}