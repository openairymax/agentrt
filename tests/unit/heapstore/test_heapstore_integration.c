/**
 * @file test_heapstore_integration.c
 * @brief AgentOS 数据分区集成测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "heapstore.h"
#include "heapstore_log.h"
#include "heapstore_registry.h"
#include "heapstore_trace.h"
#include "heapstore_ipc.h"
#include "heapstore_memory.h"

static void test_full_lifecycle(void) {
    printf("Test: full_lifecycle...");

    heapstore_config_t manager = {
        .root_path = "test_heapstore_integration",
        .max_log_size_mb = 50,
        .log_retention_days = 7,
        .trace_retention_days = 3,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true,
        .enable_trace_export = true,
        .db_vacuum_interval_days = 7
    };

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS);

    assert(heapstore_is_initialized() == true);
    assert(strcmp(heapstore_get_root(), "test_heapstore_integration") == 0);

    char path[512];
    err = heapstore_get_full_path(heapstore_PATH_LOGS, path, sizeof(path));
    assert(err == heapstore_SUCCESS);
    printf("  Logs path: %s\n", path);

    heapstore_stats_t stats;
    err = heapstore_get_stats(&stats);
    assert(err == heapstore_SUCCESS);

    heapstore_shutdown();
    assert(heapstore_is_initialized() == false);

    printf("PASS\n");
}

static void test_all_subsystems_init(void) {
    printf("Test: all_subsystems_init...");

    heapstore_config_t manager = {
        .root_path = "test_heapstore_subsystems"
    };

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS);

    err = heapstore_registry_init();
    if (err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED) {
        printf("  Registry initialized\n");
    }

    err = heapstore_trace_init();
    if (err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED) {
        printf("  Trace initialized\n");
    }

    err = heapstore_ipc_init();
    if (err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED) {
        printf("  IPC initialized\n");
    }

    err = heapstore_memory_init();
    if (err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED) {
        printf("  Memory initialized\n");
    }

    err = heapstore_log_init();
    if (err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED) {
        printf("  Log initialized\n");
    }

    heapstore_shutdown();
    printf("PASS\n");
}

static void test_logging_across_services(void) {
    printf("Test: logging_across_services...");

    heapstore_config_t manager = {
        .root_path = "test_heapstore_logging"
    };

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS);

    heapstore_log_set_level(heapstore_LOG_DEBUG);

    heapstore_LOG_INFO("service_a", "trace_001", "Service A started");
    heapstore_LOG_INFO("service_b", "trace_002", "Service B started");
    heapstore_LOG_ERROR("service_c", "trace_003", "Service C encountered error");

    char path[512];
    err = heapstore_log_get_service_path("service_a", path, sizeof(path));
    assert(err == heapstore_SUCCESS);
    printf("  Service A log: %s\n", path);

    heapstore_log_shutdown();
    heapstore_shutdown();

    printf("PASS\n");
}

static void test_registry_workflow(void) {
    printf("Test: registry_workflow...");

    heapstore_error_t err = heapstore_init(&(heapstore_config_t){.root_path = "test_heapstore_reg"});
    assert(err == heapstore_SUCCESS);

    heapstore_agent_record_t agent;
    memset(&agent, 0, sizeof(agent));
    snprintf(agent.id, sizeof(agent.id), "agent_integration_%ld", (long)time(NULL));
    snprintf(agent.name, sizeof(agent.name), "Integration Test Agent");
    snprintf(agent.type, sizeof(agent.type), "planning");
    snprintf(agent.version, sizeof(agent.version), "1.0.0");
    snprintf(agent.status, sizeof(agent.status), "active");
    agent.created_at = (uint64_t)time(NULL);
    agent.updated_at = agent.created_at;

    err = heapstore_registry_add_agent(&agent);
    if (err == heapstore_SUCCESS) {
        heapstore_agent_record_t get_agent;
        err = heapstore_registry_get_agent(agent.id, &get_agent);
        assert(err == heapstore_SUCCESS);
        assert(strcmp(get_agent.name, agent.name) == 0);

        snprintf(agent.status, sizeof(agent.status), "inactive");
        agent.updated_at = (uint64_t)time(NULL);
        err = heapstore_registry_update_agent(&agent);
        assert(err == heapstore_SUCCESS);

        err = heapstore_registry_delete_agent(agent.id);
        assert(err == heapstore_SUCCESS);
    }

    heapstore_shutdown();
    printf("PASS\n");
}

static void test_trace_workflow(void) {
    printf("Test: trace_workflow...");

    heapstore_error_t err = heapstore_init(&(heapstore_config_t){.root_path = "test_heapstore_trace"});
    assert(err == heapstore_SUCCESS);

    heapstore_span_t span;
    memset(&span, 0, sizeof(span));
    snprintf(span.trace_id, sizeof(span.trace_id), "trace_integration_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_integration_001");
    snprintf(span.parent_span_id, sizeof(span.parent_span_id), "");
    snprintf(span.name, sizeof(span.name), "integration_test_operation");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 50000000;
    snprintf(span.service_name, sizeof(span.service_name), "integration_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = heapstore_trace_write_span(&span);
    assert(err == heapstore_SUCCESS);

    uint64_t total = 0, pending = 0, size = 0;
    err = heapstore_trace_get_stats(&total, &pending, &size);
    assert(err == heapstore_SUCCESS);
    printf("  Total spans: %lu, Pending: %lu\n", (unsigned long)total, (unsigned long)pending);

    heapstore_trace_shutdown();
    heapstore_shutdown();

    printf("PASS\n");
}

static void test_concurrent_access_simulation(void) {
    printf("Test: concurrent_access_simulation...");

    heapstore_error_t err = heapstore_init(&(heapstore_config_t){.root_path = "test_heapstore_concurrent"});
    assert(err == heapstore_SUCCESS);

    for (int i = 0; i < 10; i++) {
        heapstore_log_set_level(heapstore_LOG_INFO);
        heapstore_LOG_INFO("concurrent_service", "trace_concurrent", "Log iteration %d", i);

        uint64_t total = 0, pending = 0, size = 0;
        heapstore_trace_get_stats(&total, &pending, &size);
    }

    uint32_t ch_count = 0, buf_count = 0;
    uint64_t total_size = 0;
    heapstore_ipc_get_stats(&ch_count, &buf_count, &total_size);
    printf("  IPC - Channels: %u, Buffers: %u\n", ch_count, buf_count);

    uint32_t pool_count = 0, alloc_count = 0;
    heapstore_memory_get_stats(&pool_count, &alloc_count, &total_size);
    printf("  Memory - Pools: %u, Allocations: %u\n", pool_count, alloc_count);

    heapstore_shutdown();
    printf("PASS\n");
}

static void test_cleanup_dry_run(void) {
    printf("Test: cleanup_dry_run...");

    heapstore_config_t manager = {
        .root_path = "test_heapstore_cleanup",
        .enable_auto_cleanup = true
    };

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS);

    uint64_t freed_dry = 0;
    err = heapstore_cleanup(true, &freed_dry);
    assert(err == heapstore_SUCCESS);
    printf("  Dry run - would free: %lu bytes\n", (unsigned long)freed_dry);

    uint64_t freed_actual = 0;
    err = heapstore_cleanup(false, &freed_actual);
    assert(err == heapstore_SUCCESS);
    printf("  Actual - freed: %lu bytes\n", (unsigned long)freed_actual);

    heapstore_shutdown();
    printf("PASS\n");
}

static void test_config_reload(void) {
    printf("Test: config_reload...");

    heapstore_config_t manager = {
        .root_path = "test_heapstore_reload",
        .max_log_size_mb = 100,
        .log_retention_days = 7
    };

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS);

    heapstore_config_t new_config = {
        .max_log_size_mb = 200,
        .log_retention_days = 14,
        .trace_retention_days = 5,
        .enable_auto_cleanup = false
    };

    err = heapstore_reload_config(&new_config);
    assert(err == heapstore_SUCCESS);

    err = heapstore_reload_config(NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    heapstore_shutdown();
    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS heapstore Integration Tests ===\n\n");

    test_full_lifecycle();
    test_all_subsystems_init();
    test_logging_across_services();
    test_registry_workflow();
    test_trace_workflow();
    test_concurrent_access_simulation();
    test_cleanup_dry_run();
    test_config_reload();

    printf("\n=== All Integration Tests Passed ===\n");
    return 0;
}

