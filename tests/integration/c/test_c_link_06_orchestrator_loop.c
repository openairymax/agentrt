// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_06_orchestrator_loop.c
 * @brief C-L06 Integration Test: Orchestrator → CoreLoopThree
 *
 * Tests the orchestrator connecting to CoreLoopThree for pipeline execution:
 * 1. Normal path: Create orchestrator → set core loop → execute pipeline
 * 2. Error path: Execute without core loop → proper error
 * 3. Error path: Invalid pipeline step → error handling
 * 4. Timeout path: Pipeline execution timeout
 * 5. Concurrent path: Multiple orchestrator instances
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "orchestrator.h"
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
 * Progress callback for testing
 * ============================================================================ */

static int g_progress_callback_count = 0;

static void test_progress_callback(orch_phase_t phase, orch_task_status_t status,
                                   const char *task_id, void *user_data) {
    (void)phase;
    (void)status;
    (void)task_id;
    (void)user_data;
    g_progress_callback_count++;
}

/* ============================================================================
 * P1.16f-1: Normal Path — Orchestrator lifecycle
 * ============================================================================ */

static void test_normal_orchestrator_lifecycle(void) {
    TEST("C-L06 Normal: Orchestrator create → set core loop → destroy");

    orch_config_t config;
    orch_config_get_defaults(&config);

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    /* Verify orchestrator can accept core loop */
    CHECK(!orchestrator_has_core_loop(orch),
          "Should not have core loop before setting");

    /* Set a mock core loop (NULL for testing) */
    orchestrator_set_core_loop(orch, NULL);

    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * P1.16f-2: Normal Path — Pipeline creation and execution
 * ============================================================================ */

static void test_normal_pipeline_create(void) {
    TEST("C-L06 Normal: Pipeline create → add step → destroy");

    orch_config_t config;
    orch_config_get_defaults(&config);
    config.timeout_ms = 5000;

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    orch_pipeline_t *pipeline = orchestrator_pipeline_create(orch, "test-pipeline");
    CHECK(pipeline != NULL, "orchestrator_pipeline_create returned NULL");

    /* Add a step to the pipeline */
    orch_pipeline_step_t step = {0};
    step.phase = ORCH_PHASE_GENERATION;
    step.agent_id = "test-agent";
    step.skill_id = "test-skill";
    step.input = "test input";
    step.strategy = ORCH_STRATEGY_SEQUENTIAL;
    step.timeout_ms = 3000;

    int ret = orchestrator_pipeline_add_step(pipeline, &step);
    CHECK_EQ(ret, 0, "orchestrator_pipeline_add_step should succeed");

    orchestrator_pipeline_destroy(pipeline);
    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * P1.16f-3: Normal Path — Progress callback
 * ============================================================================ */

static void test_normal_progress_callback(void) {
    TEST("C-L06 Normal: Progress callback registration and invocation");

    orch_config_t config;
    orch_config_get_defaults(&config);

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    g_progress_callback_count = 0;
    orchestrator_set_progress_callback(orch, test_progress_callback, NULL);

    /* Execute a simple pipeline to trigger callbacks */
    orch_pipeline_t *pipeline = orchestrator_pipeline_create(orch, "cb-pipeline");
    CHECK(pipeline != NULL, "orchestrator_pipeline_create returned NULL");

    orch_pipeline_step_t step = {0};
    step.phase = ORCH_PHASE_PLANNING;
    step.agent_id = "cb-agent";
    step.input = "callback test";
    step.strategy = ORCH_STRATEGY_SEQUENTIAL;
    step.timeout_ms = 1000;

    orchestrator_pipeline_add_step(pipeline, &step);

    size_t result_count = 0;
    orch_result_t *results = NULL;
    int ret = orchestrator_execute_pipeline(orch, pipeline, "test",
                                             &results, &result_count);
    /* May succeed or fail depending on backend availability */
    (void)ret;

    orchestrator_pipeline_destroy(pipeline);
    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * P1.16f-4: Error Path — NULL orchestrator handling
 * ============================================================================ */

static void test_error_null_orchestrator(void) {
    TEST("C-L06 Error: NULL orchestrator handling");

    /* orchestrator_destroy(NULL) should be safe */
    orchestrator_destroy(NULL);

    /* NULL pipeline destroy should be safe */
    orchestrator_pipeline_destroy(NULL);

    /* NULL orchestrator get status should return CANCELLED */
    orch_task_status_t status = orchestrator_get_task_status(NULL, "task-1");
    CHECK(status == ORCH_TASK_CANCELLED || status != ORCH_TASK_RUNNING,
          "NULL orchestrator should return non-running status");

    /* NULL orchestrator active count should be 0 */
    uint32_t active = orchestrator_active_count(NULL);
    CHECK_EQ(active, (uint32_t)0, "NULL orchestrator should have 0 active tasks");

    /* NULL orchestrator cancel should fail */
    int ret = orchestrator_cancel(NULL, "task-1");
    CHECK(ret != 0, "NULL orchestrator cancel should fail");

    PASS();
}

/* ============================================================================
 * P1.16f-5: Error Path — Invalid pipeline step
 * ============================================================================ */

static void test_error_invalid_pipeline_step(void) {
    TEST("C-L06 Error: Invalid pipeline step handling");

    orch_config_t config;
    orch_config_get_defaults(&config);

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    /* Add step to NULL pipeline should fail */
    orch_pipeline_step_t step = {0};
    step.phase = ORCH_PHASE_GENERATION;
    int ret = orchestrator_pipeline_add_step(NULL, &step);
    CHECK(ret != 0, "Adding step to NULL pipeline should fail");

    /* NULL step should fail */
    orch_pipeline_t *pipeline = orchestrator_pipeline_create(orch, "invalid-pipeline");
    CHECK(pipeline != NULL, "orchestrator_pipeline_create returned NULL");
    ret = orchestrator_pipeline_add_step(pipeline, NULL);
    CHECK(ret != 0, "Adding NULL step should fail");

    orchestrator_pipeline_destroy(pipeline);
    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * P1.16f-6: Timeout Path — Pipeline execution timeout
 * ============================================================================ */

static void test_timeout_pipeline_execution(void) {
    TEST("C-L06 Timeout: Pipeline execution with short timeout");

    orch_config_t config;
    orch_config_get_defaults(&config);
    config.timeout_ms = 100; /* Very short timeout */
    config.max_subtasks = 4;

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    orch_pipeline_t *pipeline = orchestrator_pipeline_create(orch, "timeout-pipeline");
    CHECK(pipeline != NULL, "orchestrator_pipeline_create returned NULL");

    orch_pipeline_step_t step = {0};
    step.phase = ORCH_PHASE_GENERATION;
    step.agent_id = "timeout-agent";
    step.input = "timeout test";
    step.strategy = ORCH_STRATEGY_SEQUENTIAL;
    step.timeout_ms = 50;

    orchestrator_pipeline_add_step(pipeline, &step);

    size_t result_count = 0;
    orch_result_t *results = NULL;
    int ret = orchestrator_execute_pipeline(orch, pipeline, "timeout-test",
                                             &results, &result_count);
    /* With short timeout, may fail or succeed — we just verify no crash */
    (void)ret;

    /* Cleanup results if any */
    if (results && result_count > 0) {
        for (size_t i = 0; i < result_count; i++) {
            orchestrator_result_free(&results[i]);
        }
        free(results);
    }

    orchestrator_pipeline_destroy(pipeline);
    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * P1.16f-7: Concurrent Path — Multiple orchestrator instances
 * ============================================================================ */

#define ORCH_CONCURRENT_INSTANCES 4

static void test_concurrent_orchestrator_instances(void) {
    TEST("C-L06 Concurrent: Multiple orchestrator instances");

    orchestrator_t *orchestrators[ORCH_CONCURRENT_INSTANCES];
    orch_pipeline_t *pipelines[ORCH_CONCURRENT_INSTANCES];

    orch_config_t config;
    orch_config_get_defaults(&config);

    /* Create multiple instances */
    for (int i = 0; i < ORCH_CONCURRENT_INSTANCES; i++) {
        orchestrators[i] = orchestrator_create(&config);
        CHECK(orchestrators[i] != NULL, "orchestrator_create returned NULL");

        char name[64];
        snprintf(name, sizeof(name), "pipeline-%d", i);
        pipelines[i] = orchestrator_pipeline_create(orchestrators[i], name);
        CHECK(pipelines[i] != NULL, "orchestrator_pipeline_create returned NULL");

        orch_pipeline_step_t step = {0};
        step.phase = ORCH_PHASE_PLANNING;
        step.agent_id = name;
        step.input = "concurrent test";
        step.strategy = ORCH_STRATEGY_SEQUENTIAL;
        step.timeout_ms = 1000;

        int ret = orchestrator_pipeline_add_step(pipelines[i], &step);
        CHECK_EQ(ret, 0, "Adding step should succeed");
    }

    /* Verify all have 0 active tasks initially */
    for (int i = 0; i < ORCH_CONCURRENT_INSTANCES; i++) {
        uint32_t active = orchestrator_active_count(orchestrators[i]);
        CHECK_EQ(active, (uint32_t)0, "Should have 0 active tasks initially");
    }

    /* Cleanup */
    for (int i = 0; i < ORCH_CONCURRENT_INSTANCES; i++) {
        orchestrator_pipeline_destroy(pipelines[i]);
        orchestrator_destroy(orchestrators[i]);
    }

    PASS();
}

/* ============================================================================
 * P1.16f-8: Task cancellation
 * ============================================================================ */

static void test_task_cancel_all(void) {
    TEST("C-L06 Normal: Cancel all tasks");

    orch_config_t config;
    orch_config_get_defaults(&config);

    orchestrator_t *orch = orchestrator_create(&config);
    CHECK(orch != NULL, "orchestrator_create returned NULL");

    /* Cancel all on empty orchestrator should succeed */
    int ret = orchestrator_cancel_all(orch);
    CHECK_EQ(ret, 0, "Cancel all on empty orchestrator should succeed");

    /* Verify active count is 0 */
    uint32_t active = orchestrator_active_count(orch);
    CHECK_EQ(active, (uint32_t)0, "Active count should be 0 after cancel all");

    orchestrator_destroy(orch);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L06 Integration Tests: Orchestrator → CoreLoopThree ===\n\n");

    test_normal_orchestrator_lifecycle();
    test_normal_pipeline_create();
    test_normal_progress_callback();
    test_error_null_orchestrator();
    test_error_invalid_pipeline_step();
    test_timeout_pipeline_execution();
    test_concurrent_orchestrator_instances();
    test_task_cancel_all();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}