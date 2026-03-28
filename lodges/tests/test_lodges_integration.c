/**
 * @file test_lodges_integration.c
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

#include "lodges.h"
#include "lodges_log.h"
#include "lodges_registry.h"
#include "lodges_trace.h"
#include "lodges_ipc.h"
#include "lodges_memory.h"

static void test_full_lifecycle(void) {
    printf("Test: full_lifecycle...");

    lodges_config_t manager = {
        .root_path = "test_lodges_integration",
        .max_log_size_mb = 50,
        .log_retention_days = 7,
        .trace_retention_days = 3,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true,
        .enable_trace_export = true,
        .db_vacuum_interval_days = 7
    };

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS);

    assert(lodges_is_initialized() == true);
    assert(strcmp(lodges_get_root(), "test_lodges_integration") == 0);

    char path[512];
    err = lodges_get_full_path(lodges_PATH_LOGS, path, sizeof(path));
    assert(err == lodges_SUCCESS);
    printf("  Logs path: %s\n", path);

    lodges_stats_t stats;
    err = lodges_get_stats(&stats);
    assert(err == lodges_SUCCESS);

    lodges_shutdown();
    assert(lodges_is_initialized() == false);

    printf("PASS\n");
}

static void test_all_subsystems_init(void) {
    printf("Test: all_subsystems_init...");

    lodges_config_t manager = {
        .root_path = "test_lodges_subsystems"
    };

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS);

    err = lodges_registry_init();
    if (err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED) {
        printf("  Registry initialized\n");
    }

    err = lodges_trace_init();
    if (err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED) {
        printf("  Trace initialized\n");
    }

    err = lodges_ipc_init();
    if (err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED) {
        printf("  IPC initialized\n");
    }

    err = lodges_memory_init();
    if (err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED) {
        printf("  Memory initialized\n");
    }

    err = lodges_log_init();
    if (err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED) {
        printf("  Log initialized\n");
    }

    lodges_shutdown();
    printf("PASS\n");
}

static void test_logging_across_services(void) {
    printf("Test: logging_across_services...");

    lodges_config_t manager = {
        .root_path = "test_lodges_logging"
    };

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS);

    lodges_log_set_level(lodges_LOG_DEBUG);

    lodges_LOG_INFO("service_a", "trace_001", "Service A started");
    lodges_LOG_INFO("service_b", "trace_002", "Service B started");
    lodges_LOG_ERROR("service_c", "trace_003", "Service C encountered error");

    char path[512];
    err = lodges_log_get_service_path("service_a", path, sizeof(path));
    assert(err == lodges_SUCCESS);
    printf("  Service A log: %s\n", path);

    lodges_log_shutdown();
    lodges_shutdown();

    printf("PASS\n");
}

static void test_registry_workflow(void) {
    printf("Test: registry_workflow...");

    lodges_error_t err = lodges_init(&(lodges_config_t){.root_path = "test_lodges_reg"});
    assert(err == lodges_SUCCESS);

    lodges_agent_record_t agent;
    memset(&agent, 0, sizeof(agent));
    snprintf(agent.id, sizeof(agent.id), "agent_integration_%ld", (long)time(NULL));
    snprintf(agent.name, sizeof(agent.name), "Integration Test Agent");
    snprintf(agent.type, sizeof(agent.type), "planning");
    snprintf(agent.version, sizeof(agent.version), "1.0.0");
    snprintf(agent.status, sizeof(agent.status), "active");
    agent.created_at = (uint64_t)time(NULL);
    agent.updated_at = agent.created_at;

    err = lodges_registry_add_agent(&agent);
    if (err == lodges_SUCCESS) {
        lodges_agent_record_t get_agent;
        err = lodges_registry_get_agent(agent.id, &get_agent);
        assert(err == lodges_SUCCESS);
        assert(strcmp(get_agent.name, agent.name) == 0);

        snprintf(agent.status, sizeof(agent.status), "inactive");
        agent.updated_at = (uint64_t)time(NULL);
        err = lodges_registry_update_agent(&agent);
        assert(err == lodges_SUCCESS);

        err = lodges_registry_delete_agent(agent.id);
        assert(err == lodges_SUCCESS);
    }

    lodges_shutdown();
    printf("PASS\n");
}

static void test_trace_workflow(void) {
    printf("Test: trace_workflow...");

    lodges_error_t err = lodges_init(&(lodges_config_t){.root_path = "test_lodges_trace"});
    assert(err == lodges_SUCCESS);

    lodges_span_t span;
    memset(&span, 0, sizeof(span));
    snprintf(span.trace_id, sizeof(span.trace_id), "trace_integration_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_integration_001");
    snprintf(span.parent_span_id, sizeof(span.parent_span_id), "");
    snprintf(span.name, sizeof(span.name), "integration_test_operation");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 50000000;
    snprintf(span.service_name, sizeof(span.service_name), "integration_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = lodges_trace_write_span(&span);
    assert(err == lodges_SUCCESS);

    uint64_t total = 0, pending = 0, size = 0;
    err = lodges_trace_get_stats(&total, &pending, &size);
    assert(err == lodges_SUCCESS);
    printf("  Total spans: %lu, Pending: %lu\n", (unsigned long)total, (unsigned long)pending);

    lodges_trace_shutdown();
    lodges_shutdown();

    printf("PASS\n");
}

static void test_concurrent_access_simulation(void) {
    printf("Test: concurrent_access_simulation...");

    lodges_error_t err = lodges_init(&(lodges_config_t){.root_path = "test_lodges_concurrent"});
    assert(err == lodges_SUCCESS);

    for (int i = 0; i < 10; i++) {
        lodges_log_set_level(lodges_LOG_INFO);
        lodges_LOG_INFO("concurrent_service", "trace_concurrent", "Log iteration %d", i);

        uint64_t total = 0, pending = 0, size = 0;
        lodges_trace_get_stats(&total, &pending, &size);
    }

    uint32_t ch_count = 0, buf_count = 0;
    uint64_t total_size = 0;
    lodges_ipc_get_stats(&ch_count, &buf_count, &total_size);
    printf("  IPC - Channels: %u, Buffers: %u\n", ch_count, buf_count);

    uint32_t pool_count = 0, alloc_count = 0;
    lodges_memory_get_stats(&pool_count, &alloc_count, &total_size);
    printf("  Memory - Pools: %u, Allocations: %u\n", pool_count, alloc_count);

    lodges_shutdown();
    printf("PASS\n");
}

static void test_cleanup_dry_run(void) {
    printf("Test: cleanup_dry_run...");

    lodges_config_t manager = {
        .root_path = "test_lodges_cleanup",
        .enable_auto_cleanup = true
    };

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS);

    uint64_t freed_dry = 0;
    err = lodges_cleanup(true, &freed_dry);
    assert(err == lodges_SUCCESS);
    printf("  Dry run - would free: %lu bytes\n", (unsigned long)freed_dry);

    uint64_t freed_actual = 0;
    err = lodges_cleanup(false, &freed_actual);
    assert(err == lodges_SUCCESS);
    printf("  Actual - freed: %lu bytes\n", (unsigned long)freed_actual);

    lodges_shutdown();
    printf("PASS\n");
}

static void test_config_reload(void) {
    printf("Test: config_reload...");

    lodges_config_t manager = {
        .root_path = "test_lodges_reload",
        .max_log_size_mb = 100,
        .log_retention_days = 7
    };

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS);

    lodges_config_t new_config = {
        .max_log_size_mb = 200,
        .log_retention_days = 14,
        .trace_retention_days = 5,
        .enable_auto_cleanup = false
    };

    err = lodges_reload_config(&new_config);
    assert(err == lodges_SUCCESS);

    err = lodges_reload_config(NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    lodges_shutdown();
    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS lodges Integration Tests ===\n\n");

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

