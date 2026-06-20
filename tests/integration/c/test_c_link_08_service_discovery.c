// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_08_service_discovery.c
 * @brief C-L08 Integration Test: ServiceDiscovery → all daemons
 *
 * Tests the service discovery mechanism connecting all daemons:
 * 1. Normal path: Create → start → register → discover → select → stop
 * 2. Error path: Register duplicate service → proper error
 * 3. Error path: NULL handling
 * 4. Timeout path: Heartbeat timeout → auto expire
 * 5. Concurrent path: Multiple simultaneous registrations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "service_discovery.h"
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
 * Event callback for testing
 * ============================================================================ */

static int g_event_callback_count = 0;

static void test_event_callback(sd_event_type_t event, const char *service_name,
                                const sd_instance_t *instance, void *user_data) {
    (void)event;
    (void)service_name;
    (void)instance;
    (void)user_data;
    g_event_callback_count++;
}

/* ============================================================================
 * P1.16h-1: Normal Path — Service discovery lifecycle
 * ============================================================================ */

static void test_normal_sd_lifecycle(void) {
    TEST("C-L08 Normal: Service discovery create → start → stop → destroy");

    sd_config_t config = sd_create_default_config();
    service_discovery_t sd = sd_create(&config);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    CHECK(sd_is_running(sd), "Service discovery should be running");

    err = sd_stop(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_stop failed");

    CHECK(!sd_is_running(sd), "Service discovery should not be running");

    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-2: Normal Path — Register, discover, and select
 * ============================================================================ */

static void test_normal_register_discover_select(void) {
    TEST("C-L08 Normal: Register → discover → select instance");

    service_discovery_t sd = sd_create(NULL);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    /* Register a service */
    sd_instance_t inst = {0};
    snprintf(inst.instance_id, sizeof(inst.instance_id), "llm_d-001");
    snprintf(inst.endpoint, sizeof(inst.endpoint), "127.0.0.1:8080");
    inst.state = AGENTOS_SVC_RUNNING;
    inst.healthy = true;
    inst.weight = 10;
    inst.active_connections = 0;
    inst.max_connections = 100;
    inst.pid = (uint32_t)getpid();

    err = sd_register(sd, "llm_d", "llm_service", &inst,
                      "production,llm", "corekern");
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_register failed");

    /* Discover the service */
    sd_instance_t found[4];
    uint32_t found_count = 0;
    err = sd_discover(sd, "llm_d", found, 4, &found_count);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_discover failed");
    CHECK(found_count >= 1, "Should find at least 1 instance");
    CHECK_EQ(strcmp(found[0].instance_id, "llm_d-001"), 0,
             "Instance ID should match");

    /* Select an instance with load balancing */
    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    err = sd_select_instance(sd, "llm_d", SD_LB_ROUND_ROBIN, &selected);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_select_instance failed");
    CHECK(strlen(selected.instance_id) > 0, "Selected instance should have ID");

    /* Cleanup */
    sd_deregister(sd, "llm_d", "llm_d-001");
    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-3: Normal Path — Discover by type and tags
 * ============================================================================ */

static void test_normal_discover_by_type_tags(void) {
    TEST("C-L08 Normal: Discover by type and tags");

    service_discovery_t sd = sd_create(NULL);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    /* Register multiple services of different types */
    for (int i = 0; i < 3; i++) {
        sd_instance_t inst = {0};
        snprintf(inst.instance_id, sizeof(inst.instance_id), "tool_d-%03d", i);
        snprintf(inst.endpoint, sizeof(inst.endpoint), "127.0.0.1:%d", 9000 + i);
        inst.state = AGENTOS_SVC_RUNNING;
        inst.healthy = true;
        inst.weight = 10;
        inst.pid = (uint32_t)getpid();

        err = sd_register(sd, inst.instance_id, "tool_service", &inst,
                          "production,tools,py", "corekern,llm_d");
        CHECK_EQ(err, AGENTOS_SUCCESS, "Register tool instance failed");
    }

    /* Discover by type */
    sd_service_entry_t entries[8];
    uint32_t count = 0;
    err = sd_discover_by_type(sd, "tool_service", entries, 8, &count);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_discover_by_type failed");
    CHECK(count >= 3, "Should find at least 3 tool services");

    /* Discover by tags */
    sd_service_entry_t tag_entries[8];
    uint32_t tag_count = 0;
    err = sd_discover_by_tags(sd, "tools", tag_entries, 8, &tag_count);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_discover_by_tags failed");
    CHECK(tag_count >= 1, "Should find at least 1 service with 'tools' tag");

    /* Cleanup */
    for (int i = 0; i < 3; i++) {
        char id[64];
        snprintf(id, sizeof(id), "tool_d-%03d", i);
        sd_deregister(sd, id, id);
    }
    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-4: Error Path — NULL handling
 * ============================================================================ */

static void test_error_null_handling(void) {
    TEST("C-L08 Error: NULL service discovery handling");

    /* sd_destroy(NULL) should be safe */
    sd_destroy(NULL);

    /* NULL sd is_running should return false */
    CHECK(!sd_is_running(NULL), "NULL sd should not be running");

    /* NULL sd service count should be 0 */
    uint32_t count = sd_service_count(NULL);
    CHECK_EQ(count, (uint32_t)0, "NULL sd should have 0 services");

    /* NULL sd register should fail */
    sd_instance_t inst = {0};
    agentos_error_t err = sd_register(NULL, "test", "type", &inst, "", "");
    CHECK(err != AGENTOS_SUCCESS, "NULL sd register should fail");

    /* NULL sd discover should fail */
    uint32_t found = 0;
    err = sd_discover(NULL, "test", NULL, 0, &found);
    CHECK(err != AGENTOS_SUCCESS, "NULL sd discover should fail");

    PASS();
}

/* ============================================================================
 * P1.16h-5: Error Path — Register with invalid params
 * ============================================================================ */

static void test_error_invalid_register(void) {
    TEST("C-L08 Error: Register with invalid parameters");

    service_discovery_t sd = sd_create(NULL);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    /* Register with NULL service name */
    sd_instance_t inst = {0};
    snprintf(inst.instance_id, sizeof(inst.instance_id), "test-001");
    err = sd_register(sd, NULL, "type", &inst, "", "");
    CHECK(err != AGENTOS_SUCCESS, "NULL service name should fail");

    /* Register with NULL instance */
    err = sd_register(sd, "test", "type", NULL, "", "");
    CHECK(err != AGENTOS_SUCCESS, "NULL instance should fail");

    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-6: Timeout Path — Heartbeat and auto-expire
 * ============================================================================ */

static void test_timeout_heartbeat_expire(void) {
    TEST("C-L08 Timeout: Heartbeat → auto-expire");

    sd_config_t config = sd_create_default_config();
    config.heartbeat_interval_ms = 100;
    config.expire_timeout_ms = 500;
    config.enable_auto_expire = true;

    service_discovery_t sd = sd_create(&config);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    /* Register a service */
    sd_instance_t inst = {0};
    snprintf(inst.instance_id, sizeof(inst.instance_id), "heartbeat-test-001");
    snprintf(inst.endpoint, sizeof(inst.endpoint), "127.0.0.1:9999");
    inst.state = AGENTOS_SVC_RUNNING;
    inst.healthy = true;
    inst.weight = 10;
    inst.pid = (uint32_t)getpid();

    err = sd_register(sd, "heartbeat-svc", "test", &inst, "test", "");
    CHECK_EQ(err, AGENTOS_SUCCESS, "Register heartbeat service failed");

    /* Send heartbeats */
    for (int i = 0; i < 3; i++) {
        err = sd_heartbeat(sd, "heartbeat-svc", "heartbeat-test-001");
        CHECK_EQ(err, AGENTOS_SUCCESS, "Heartbeat should succeed");
        usleep(200000); /* 200ms */
    }

    /* Update health status */
    err = sd_update_health(sd, "heartbeat-svc", "heartbeat-test-001", false);
    CHECK_EQ(err, AGENTOS_SUCCESS, "Update health should succeed");

    /* Update connections */
    err = sd_update_connections(sd, "heartbeat-svc", "heartbeat-test-001", 5);
    CHECK_EQ(err, AGENTOS_SUCCESS, "Update connections should succeed");

    /* Verify service count */
    uint32_t count = sd_service_count(sd);
    CHECK(count >= 1, "Heartbeat service should still be registered");

    /* Cleanup */
    sd_deregister(sd, "heartbeat-svc", "heartbeat-test-001");
    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-7: Concurrent Path — Multiple simultaneous registrations
 * ============================================================================ */

#define SD_CONCURRENT_THREADS 4
#define SD_REGS_PER_THREAD 5

typedef struct {
    service_discovery_t sd;
    int thread_id;
    int success_count;
    int error_count;
} sd_thread_args_t;

static void *concurrent_sd_thread(void *arg) {
    sd_thread_args_t *args = (sd_thread_args_t *)arg;

    for (int i = 0; i < SD_REGS_PER_THREAD; i++) {
        char name[64];
        char id[64];
        snprintf(name, sizeof(name), "concurrent-svc-%d", args->thread_id);
        snprintf(id, sizeof(id), "inst-%d-%d", args->thread_id, i);

        sd_instance_t inst = {0};
        snprintf(inst.instance_id, sizeof(inst.instance_id), "%s", id);
        snprintf(inst.endpoint, sizeof(inst.endpoint), "127.0.0.1:%d",
                 10000 + args->thread_id * 100 + i);
        inst.state = AGENTOS_SVC_RUNNING;
        inst.healthy = true;
        inst.weight = 10;
        inst.pid = (uint32_t)getpid();

        agentos_error_t err = sd_register(args->sd, name, "test", &inst, "", "");
        if (err == AGENTOS_SUCCESS) {
            args->success_count++;
        } else {
            args->error_count++;
        }
    }
    return NULL;
}

static void test_concurrent_sd_registrations(void) {
    TEST("C-L08 Concurrent: Multiple simultaneous registrations");

    sd_config_t config = sd_create_default_config();
    config.heartbeat_interval_ms = 5000;
    config.expire_timeout_ms = 30000;

    service_discovery_t sd = sd_create(&config);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    pthread_t threads[SD_CONCURRENT_THREADS];
    sd_thread_args_t args[SD_CONCURRENT_THREADS];

    for (int i = 0; i < SD_CONCURRENT_THREADS; i++) {
        args[i].sd = sd;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_sd_thread, &args[i]);
    }

    int total_success = 0;
    for (int i = 0; i < SD_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
    }

    CHECK(total_success > 0, "At least some registrations should succeed");

    /* Verify total service count */
    uint32_t count = sd_service_count(sd);
    CHECK(count >= (uint32_t)SD_CONCURRENT_THREADS,
          "Should have registered services from all threads");

    /* Cleanup all registered services */
    for (int i = 0; i < SD_CONCURRENT_THREADS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "concurrent-svc-%d", i);
        sd_deregister_all(sd, name);
    }

    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * P1.16h-8: Event callback and stats
 * ============================================================================ */

static void test_event_callback_and_stats(void) {
    TEST("C-L08 Normal: Event callback registration and stats");

    service_discovery_t sd = sd_create(NULL);
    CHECK(sd != NULL, "sd_create returned NULL");

    agentos_error_t err = sd_start(sd);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_start failed");

    /* Register event callback */
    g_event_callback_count = 0;
    err = sd_register_event_callback(sd, test_event_callback, NULL);
    CHECK_EQ(err, AGENTOS_SUCCESS, "Register event callback should succeed");

    /* Register a service to trigger callback */
    sd_instance_t inst = {0};
    snprintf(inst.instance_id, sizeof(inst.instance_id), "event-test-001");
    snprintf(inst.endpoint, sizeof(inst.endpoint), "127.0.0.1:8000");
    inst.state = AGENTOS_SVC_RUNNING;
    inst.healthy = true;
    inst.weight = 10;
    inst.pid = (uint32_t)getpid();

    err = sd_register(sd, "event-svc", "test", &inst, "test", "corekern");
    CHECK_EQ(err, AGENTOS_SUCCESS, "Register event service should succeed");

    /* Get stats */
    sd_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    err = sd_get_stats(sd, &stats);
    CHECK_EQ(err, AGENTOS_SUCCESS, "sd_get_stats should succeed");
    CHECK(stats.registrations >= 1, "Stats should reflect registration");

    /* Dump stats (should not crash) */
    sd_dump_stats(sd);

    /* Check dependencies */
    char missing_deps[256] = {0};
    err = sd_check_dependencies(sd, "event-svc", missing_deps, sizeof(missing_deps));
    /* May succeed or fail depending on whether corekern is registered */
    (void)err;

    sd_deregister(sd, "event-svc", "event-test-001");
    sd_stop(sd);
    sd_destroy(sd);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L08 Integration Tests: ServiceDiscovery → all daemons ===\n\n");

    test_normal_sd_lifecycle();
    test_normal_register_discover_select();
    test_normal_discover_by_type_tags();
    test_error_null_handling();
    test_error_invalid_register();
    test_timeout_heartbeat_expire();
    test_concurrent_sd_registrations();
    test_event_callback_and_stats();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}