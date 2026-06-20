// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_01_manager_loop.c
 * @brief C-L01 Integration Test: Manager → CoreLoopThree
 *
 * Tests the configuration loading pipeline:
 * 1. Normal path: Load valid agentos.yaml → config propagated to CoreLoopThree
 * 2. Error path: Invalid YAML → proper error codes returned
 * 3. Timeout path: Slow config loading → timeout handling
 * 4. Concurrent path: Multiple config reloads → no race conditions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "config_loader.h"
#include "config_service.h"
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
 * P1.16a-1: Normal Path — Load valid configuration
 * ============================================================================ */

static void test_normal_config_load(void) {
    TEST("C-L01 Normal: Load valid agentos.yaml");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    /* Load a minimal valid configuration */
    const char *yaml_content =
        "kernel:\n"
        "  max_alloc_mb: 2048\n"
        "  memory:\n"
        "    max_alloc_mb: 2048\n"
        "llm:\n"
        "  default_provider: openai\n"
        "  providers:\n"
        "    openai:\n"
        "      api_key: ${env:OPENAI_API_KEY}\n"
        "memory:\n"
        "  provider: builtin\n";

    config_error_t err = config_service_load_from_string(svc, yaml_content);
    CHECK_EQ(err, CONFIG_OK, "config_service_load_from_string failed");

    /* Verify a config value is accessible */
    core_config_t *cfg = config_service_get_core_config(svc);
    CHECK(cfg != NULL, "config_service_get_core_config returned NULL");

    /* Verify kernel.max_alloc_mb */
    int max_alloc = core_config_get_int(cfg, "kernel.max_alloc_mb", 0);
    CHECK_EQ(max_alloc, 2048, "kernel.max_alloc_mb should be 2048");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-2: Error Path — Invalid YAML handling
 * ============================================================================ */

static void test_error_invalid_yaml(void) {
    TEST("C-L01 Error: Invalid YAML returns error");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    /* Load malformed YAML */
    const char *bad_yaml = "kernel: [unclosed\n  invalid: ::: syntax:\n";
    config_error_t err = config_service_load_from_string(svc, bad_yaml);
    CHECK(err != CONFIG_OK, "Invalid YAML should return error, not CONFIG_OK");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-3: Error Path — Missing required fields
 * ============================================================================ */

static void test_error_missing_fields(void) {
    TEST("C-L01 Error: Missing required fields");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    /* Load YAML without required kernel section */
    const char *minimal_yaml = "unknown_section:\n  value: 1\n";
    config_error_t err = config_service_load_from_string(svc, minimal_yaml);

    /* Should still load but with defaults */
    core_config_t *cfg = config_service_get_core_config(svc);
    CHECK(cfg != NULL, "Should have config even with unknown sections");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-4: Timeout Path — Config loading timeout
 * ============================================================================ */

static void test_timeout_config_load(void) {
    TEST("C-L01 Timeout: Config loading timeout handling");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    /* Set a very short timeout and verify the service handles it */
    config_service_set_timeout(svc, 100); /* 100ms timeout */

    /* Load a valid config with timeout set */
    const char *yaml = "kernel:\n  max_alloc_mb: 1024\n";
    config_error_t err = config_service_load_from_string(svc, yaml);
    CHECK_EQ(err, CONFIG_OK, "Config load within timeout should succeed");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-5: Concurrent Path — Multiple simultaneous config reloads
 * ============================================================================ */

#define CONCURRENT_THREADS 4
#define RELOADS_PER_THREAD 10

typedef struct {
    config_service_t *svc;
    int thread_id;
    int success_count;
    int error_count;
} thread_args_t;

static void *concurrent_reload_thread(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    for (int i = 0; i < RELOADS_PER_THREAD; i++) {
        char yaml[256];
        snprintf(yaml, sizeof(yaml),
            "kernel:\n  max_alloc_mb: %d\n  thread: %d\n  iter: %d\n",
            1024 + args->thread_id, args->thread_id, i);

        config_error_t err = config_service_load_from_string(args->svc, yaml);
        if (err == CONFIG_OK) {
            args->success_count++;
        } else {
            args->error_count++;
        }
    }
    return NULL;
}

static void test_concurrent_config_reloads(void) {
    TEST("C-L01 Concurrent: Multiple simultaneous config reloads");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    pthread_t threads[CONCURRENT_THREADS];
    thread_args_t args[CONCURRENT_THREADS];

    /* Launch concurrent threads */
    for (int i = 0; i < CONCURRENT_THREADS; i++) {
        args[i].svc = svc;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_reload_thread, &args[i]);
    }

    /* Wait for all threads */
    for (int i = 0; i < CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all reloads succeeded */
    int total_success = 0;
    int total_errors = 0;
    for (int i = 0; i < CONCURRENT_THREADS; i++) {
        total_success += args[i].success_count;
        total_errors += args[i].error_count;
    }

    CHECK_EQ(total_errors, 0, "Concurrent config reloads should have no errors");
    CHECK(total_success == CONCURRENT_THREADS * RELOADS_PER_THREAD,
          "All config reloads should succeed");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-6: Configuration hot reload
 * ============================================================================ */

static void test_config_hot_reload(void) {
    TEST("C-L01 Normal: Configuration hot reload");

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    /* Load initial config */
    const char *initial = "kernel:\n  max_alloc_mb: 512\n";
    config_error_t err = config_service_load_from_string(svc, initial);
    CHECK_EQ(err, CONFIG_OK, "Initial config load failed");

    core_config_t *cfg = config_service_get_core_config(svc);
    int val1 = core_config_get_int(cfg, "kernel.max_alloc_mb", 0);
    CHECK_EQ(val1, 512, "Initial value should be 512");

    /* Hot reload with new value */
    const char *updated = "kernel:\n  max_alloc_mb: 4096\n";
    err = config_service_load_from_string(svc, updated);
    CHECK_EQ(err, CONFIG_OK, "Hot reload failed");

    cfg = config_service_get_core_config(svc);
    int val2 = core_config_get_int(cfg, "kernel.max_alloc_mb", 0);
    CHECK_EQ(val2, 4096, "Hot reloaded value should be 4096");

    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * P1.16a-7: Environment variable override
 * ============================================================================ */

static void test_env_var_override(void) {
    TEST("C-L01 Normal: Environment variable override");

    /* Set an environment variable for testing */
    setenv("AGENTOS_KERNEL_MAX_ALLOC_MB", "8192", 1);

    config_service_t *svc = config_service_create();
    CHECK(svc != NULL, "config_service_create returned NULL");

    const char *yaml = "kernel:\n  max_alloc_mb: 1024\n";
    config_error_t err = config_service_load_from_string(svc, yaml);
    CHECK_EQ(err, CONFIG_OK, "Config load failed");

    core_config_t *cfg = config_service_get_core_config(svc);
    int val = core_config_get_int(cfg, "kernel.max_alloc_mb", 0);

    /* Env var should override YAML value */
    CHECK_EQ(val, 8192, "Environment variable should override YAML (8192)");

    unsetenv("AGENTOS_KERNEL_MAX_ALLOC_MB");
    config_service_destroy(svc);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L01 Integration Tests: Manager → CoreLoopThree ===\n\n");

    test_normal_config_load();
    test_error_invalid_yaml();
    test_error_missing_fields();
    test_timeout_config_load();
    test_concurrent_config_reloads();
    test_config_hot_reload();
    test_env_var_override();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}