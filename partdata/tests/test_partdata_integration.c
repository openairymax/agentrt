/**
 * @file test_partdata_integration.c
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

#include "partdata.h"
#include "partdata_log.h"
#include "partdata_registry.h"
#include "partdata_trace.h"
#include "partdata_ipc.h"
#include "partdata_memory.h"

static void test_full_lifecycle(void) {
    printf("Test: full_lifecycle...");

    partdata_config_t config = {
        .root_path = "test_partdata_integration",
        .max_log_size_mb = 50,
        .log_retention_days = 7,
        .trace_retention_days = 3,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true,
        .enable_trace_export = true,
        .db_vacuum_interval_days = 7
    };

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS);

    assert(partdata_is_initialized() == true);
    assert(strcmp(partdata_get_root(), "test_partdata_integration") == 0);

    char path[512];
    err = partdata_get_full_path(PARTDATA_PATH_LOGS, path, sizeof(path));
    assert(err == PARTDATA_SUCCESS);
    printf("  Logs path: %s\n", path);

    partdata_stats_t stats;
    err = partdata_get_stats(&stats);
    assert(err == PARTDATA_SUCCESS);

    partdata_shutdown();
    assert(partdata_is_initialized() == false);

    printf("PASS\n");
}

static void test_all_subsystems_init(void) {
    printf("Test: all_subsystems_init...");

    partdata_config_t config = {
        .root_path = "test_partdata_subsystems"
    };

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS);

    err = partdata_registry_init();
    if (err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED) {
        printf("  Registry initialized\n");
    }

    err = partdata_trace_init();
    if (err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED) {
        printf("  Trace initialized\n");
    }

    err = partdata_ipc_init();
    if (err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED) {
        printf("  IPC initialized\n");
    }

    err = partdata_memory_init();
    if (err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED) {
        printf("  Memory initialized\n");
    }

    err = partdata_log_init();
    if (err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED) {
        printf("  Log initialized\n");
    }

    partdata_shutdown();
    printf("PASS\n");
}

static void test_logging_across_services(void) {
    printf("Test: logging_across_services...");

    partdata_config_t config = {
        .root_path = "test_partdata_logging"
    };

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS);

    partdata_log_set_level(PARTDATA_LOG_DEBUG);

    PARTDATA_LOG_INFO("service_a", "trace_001", "Service A started");
    PARTDATA_LOG_INFO("service_b", "trace_002", "Service B started");
    PARTDATA_LOG_ERROR("service_c", "trace_003", "Service C encountered error");

    char path[512];
    err = partdata_log_get_service_path("service_a", path, sizeof(path));
    assert(err == PARTDATA_SUCCESS);
    printf("  Service A log: %s\n", path);

    partdata_log_shutdown();
    partdata_shutdown();

    printf("PASS\n");
}

static void test_registry_workflow(void) {
    printf("Test: registry_workflow...");

    partdata_error_t err = partdata_init(&(partdata_config_t){.root_path = "test_partdata_reg"});
    assert(err == PARTDATA_SUCCESS);

    partdata_agent_record_t agent;
    memset(&agent, 0, sizeof(agent));
    snprintf(agent.id, sizeof(agent.id), "agent_integration_%ld", (long)time(NULL));
    snprintf(agent.name, sizeof(agent.name), "Integration Test Agent");
    snprintf(agent.type, sizeof(agent.type), "planning");
    snprintf(agent.version, sizeof(agent.version), "1.0.0");
    snprintf(agent.status, sizeof(agent.status), "active");
    agent.created_at = (uint64_t)time(NULL);
    agent.updated_at = agent.created_at;

    err = partdata_registry_add_agent(&agent);
    if (err == PARTDATA_SUCCESS) {
        partdata_agent_record_t get_agent;
        err = partdata_registry_get_agent(agent.id, &get_agent);
        assert(err == PARTDATA_SUCCESS);
        assert(strcmp(get_agent.name, agent.name) == 0);

        snprintf(agent.status, sizeof(agent.status), "inactive");
        agent.updated_at = (uint64_t)time(NULL);
        err = partdata_registry_update_agent(&agent);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_registry_delete_agent(agent.id);
        assert(err == PARTDATA_SUCCESS);
    }

    partdata_shutdown();
    printf("PASS\n");
}

static void test_trace_workflow(void) {
    printf("Test: trace_workflow...");

    partdata_error_t err = partdata_init(&(partdata_config_t){.root_path = "test_partdata_trace"});
    assert(err == PARTDATA_SUCCESS);

    partdata_span_t span;
    memset(&span, 0, sizeof(span));
    snprintf(span.trace_id, sizeof(span.trace_id), "trace_integration_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_integration_001");
    snprintf(span.parent_span_id, sizeof(span.parent_span_id), "");
    snprintf(span.name, sizeof(span.name), "integration_test_operation");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 50000000;
    snprintf(span.service_name, sizeof(span.service_name), "integration_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = partdata_trace_write_span(&span);
    assert(err == PARTDATA_SUCCESS);

    uint64_t total = 0, pending = 0, size = 0;
    err = partdata_trace_get_stats(&total, &pending, &size);
    assert(err == PARTDATA_SUCCESS);
    printf("  Total spans: %lu, Pending: %lu\n", (unsigned long)total, (unsigned long)pending);

    partdata_trace_shutdown();
    partdata_shutdown();

    printf("PASS\n");
}

static void test_concurrent_access_simulation(void) {
    printf("Test: concurrent_access_simulation...");

    partdata_error_t err = partdata_init(&(partdata_config_t){.root_path = "test_partdata_concurrent"});
    assert(err == PARTDATA_SUCCESS);

    for (int i = 0; i < 10; i++) {
        partdata_log_set_level(PARTDATA_LOG_INFO);
        PARTDATA_LOG_INFO("concurrent_service", "trace_concurrent", "Log iteration %d", i);

        uint64_t total = 0, pending = 0, size = 0;
        partdata_trace_get_stats(&total, &pending, &size);
    }

    uint32_t ch_count = 0, buf_count = 0;
    uint64_t total_size = 0;
    partdata_ipc_get_stats(&ch_count, &buf_count, &total_size);
    printf("  IPC - Channels: %u, Buffers: %u\n", ch_count, buf_count);

    uint32_t pool_count = 0, alloc_count = 0;
    partdata_memory_get_stats(&pool_count, &alloc_count, &total_size);
    printf("  Memory - Pools: %u, Allocations: %u\n", pool_count, alloc_count);

    partdata_shutdown();
    printf("PASS\n");
}

static void test_cleanup_dry_run(void) {
    printf("Test: cleanup_dry_run...");

    partdata_config_t config = {
        .root_path = "test_partdata_cleanup",
        .enable_auto_cleanup = true
    };

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS);

    uint64_t freed_dry = 0;
    err = partdata_cleanup(true, &freed_dry);
    assert(err == PARTDATA_SUCCESS);
    printf("  Dry run - would free: %lu bytes\n", (unsigned long)freed_dry);

    uint64_t freed_actual = 0;
    err = partdata_cleanup(false, &freed_actual);
    assert(err == PARTDATA_SUCCESS);
    printf("  Actual - freed: %lu bytes\n", (unsigned long)freed_actual);

    partdata_shutdown();
    printf("PASS\n");
}

static void test_config_reload(void) {
    printf("Test: config_reload...");

    partdata_config_t config = {
        .root_path = "test_partdata_reload",
        .max_log_size_mb = 100,
        .log_retention_days = 7
    };

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS);

    partdata_config_t new_config = {
        .max_log_size_mb = 200,
        .log_retention_days = 14,
        .trace_retention_days = 5,
        .enable_auto_cleanup = false
    };

    err = partdata_reload_config(&new_config);
    assert(err == PARTDATA_SUCCESS);

    err = partdata_reload_config(NULL);
    assert(err == PARTDATA_ERR_INVALID_PARAM);

    partdata_shutdown();
    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS Partdata Integration Tests ===\n\n");

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
