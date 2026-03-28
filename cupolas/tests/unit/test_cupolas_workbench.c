/**
 * @file test_domes_workbench.c
 * @brief cupolas 工作台模块单元测试
 * @author Spharx
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/workbench/workbench.h"
#include "../../src/workbench/workbench_limits.h"
#include "../../src/workbench/workbench_container.h"
#include "../../src/platform/platform.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("[FAIL] %s: %s\n", name, msg); return 1; } while(0)

/* ============================================================================
 * 工作台基础功能测试
 * ============================================================================ */

static int test_workbench_default_config(void) {
    workbench_config_t manager;
    workbench_default_config(&manager);

    assert(manager.timeout_ms > 0);
    assert(manager.max_output_size > 0);
    assert(manager.redirect_stdout == true);
    assert(manager.redirect_stderr == true);

    TEST_PASS("workbench_default_config");
    return 0;
}

static int test_workbench_create_destroy(void) {
    workbench_t* wb = workbench_create(NULL);
    if (!wb) TEST_FAIL("workbench_create_destroy", "Failed to create workbench");

    workbench_state_t state = workbench_get_state(wb);
    if (state != WORKBENCH_STATE_IDLE) {
        workbench_destroy(wb);
        TEST_FAIL("workbench_create_destroy", "Initial state should be IDLE");
    }

    workbench_destroy(wb);
    TEST_PASS("workbench_create_destroy");
    return 0;
}

static int test_workbench_config_with_limits(void) {
    workbench_config_t manager;
    workbench_default_config(&manager);

    manager.enable_limits = true;
    manager.limits.max_memory_bytes = 256 * 1024 * 1024;
    manager.limits.max_cpu_time_ms = 60000;
    manager.limits.max_processes = 10;
    manager.limits.max_threads = 20;

    workbench_t* wb = workbench_create(&manager);
    if (!wb) TEST_FAIL("workbench_config_with_limits", "Failed to create workbench with limits");

    workbench_limits_t limits;
    if (workbench_get_limits(wb, &limits) != 0) {
        workbench_destroy(wb);
        TEST_FAIL("workbench_config_with_limits", "Failed to get limits");
    }

    assert(limits.max_memory_bytes == 256 * 1024 * 1024);
    assert(limits.max_cpu_time_ms == 60000);

    workbench_destroy(wb);
    TEST_PASS("workbench_config_with_limits");
    return 0;
}

/* ============================================================================
 * 资源限制测试
 * ============================================================================ */

static int test_limits_create_destroy(void) {
    limit_context_t* ctx = limits_create(512 * 1024 * 1024, 60000, 10);
    if (!ctx) TEST_FAIL("limits_create_destroy", "Failed to create limits context");

    limits_destroy(ctx);
    TEST_PASS("limits_create_destroy");
    return 0;
}

static int test_limits_memory(void) {
    limit_context_t* ctx = limits_create(256 * 1024 * 1024, 0, 0);
    if (!ctx) TEST_FAIL("limits_memory", "Failed to create limits context");

    if (limits_set_memory(ctx, 128 * 1024 * 1024, LIMIT_MODE_ENFORCED) != 0) {
        limits_destroy(ctx);
        TEST_FAIL("limits_memory", "Failed to set memory limit");
    }

    limits_destroy(ctx);
    TEST_PASS("limits_memory");
    return 0;
}

static int test_limits_cpu_time(void) {
    limit_context_t* ctx = limits_create(0, 30000, 0);
    if (!ctx) TEST_FAIL("limits_cpu_time", "Failed to create limits context");

    if (limits_set_cpu_time(ctx, 60000, LIMIT_MODE_ENFORCED) != 0) {
        limits_destroy(ctx);
        TEST_FAIL("limits_cpu_time", "Failed to set CPU time limit");
    }

    limits_destroy(ctx);
    TEST_PASS("limits_cpu_time");
    return 0;
}

static int test_limits_processes(void) {
    limit_context_t* ctx = limits_create(0, 0, 5);
    if (!ctx) TEST_FAIL("limits_processes", "Failed to create limits context");

    if (limits_set_processes(ctx, 10, LIMIT_MODE_ENFORCED) != 0) {
        limits_destroy(ctx);
        TEST_FAIL("limits_processes", "Failed to set processes limit");
    }

    limits_destroy(ctx);
    TEST_PASS("limits_processes");
    return 0;
}

static int test_limits_stats(void) {
    limit_context_t* ctx = limits_create(512 * 1024 * 1024, 60000, 10);
    if (!ctx) TEST_FAIL("limits_stats", "Failed to create limits context");

    resource_stats_t stats;
    if (limits_get_stats(ctx, &stats) != 0) {
        limits_destroy(ctx);
        TEST_FAIL("limits_stats", "Failed to get stats");
    }

    assert(stats.memory_limit == 512 * 1024 * 1024);
    assert(stats.processes_limit == 10);

    limits_destroy(ctx);
    TEST_PASS("limits_stats");
    return 0;
}

static int test_limits_check(void) {
    limit_context_t* ctx = limits_create(100 * 1024 * 1024, 10000, 5);
    if (!ctx) TEST_FAIL("limits_check", "Failed to create limits context");

    limit_status_t status;
    if (limits_check(ctx, LIMIT_TYPE_MEMORY, &status) != false) {
        limits_destroy(ctx);
        TEST_FAIL("limits_check", "Memory should not be exceeded");
    }

    if (limits_check(ctx, LIMIT_TYPE_CPU_TIME, &status) != false) {
        limits_destroy(ctx);
        TEST_FAIL("limits_check", "CPU time should not be exceeded");
    }

    limits_destroy(ctx);
    TEST_PASS("limits_check");
    return 0;
}

static int test_limits_status_string(void) {
    const char* str = limits_status_string(LIMIT_STATUS_OK);
    assert(strcmp(str, "OK") == 0);

    str = limits_status_string(LIMIT_STATUS_SOFT_EXCEEDED);
    assert(strcmp(str, "Soft limit exceeded") == 0);

    str = limits_status_string(LIMIT_STATUS_HARD_EXCEEDED);
    assert(strcmp(str, "Hard limit exceeded") == 0);

    str = limits_status_string(LIMIT_STATUS_KILLED);
    assert(strcmp(str, "Killed by limit") == 0);

    TEST_PASS("limits_status_string");
    return 0;
}

/* ============================================================================
 * 容器配置测试
 * ============================================================================ */

static int test_container_config_init(void) {
    container_config_t manager;
    container_config_init(&manager);

    assert(manager.runtime == CONTAINER_RUNTIME_AUTO);
    assert(strcmp(manager.resources.network_mode, "none") == 0);
    assert(manager.resources.readonly_rootfs == true);
    assert(manager.resources.memory_limit == 512 * 1024 * 1024);

    TEST_PASS("container_config_init");
    return 0;
}

static int test_container_runtime_is_available(void) {
    bool available = container_runtime_is_available(CONTAINER_RUNTIME_DOCKER);
    bool result = container_runtime_is_available(CONTAINER_RUNTIME_AUTO);

    (void)available;
    (void)result;

    TEST_PASS("container_runtime_is_available");
    return 0;
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("cupolas Workbench Unit Tests\n");
    printf("========================================\n\n");

    printf("--- Workbench Basic Tests ---\n");
    if (test_workbench_default_config() != 0) return 1;
    if (test_workbench_create_destroy() != 0) return 1;
    if (test_workbench_config_with_limits() != 0) return 1;

    printf("\n--- Limits Tests ---\n");
    if (test_limits_create_destroy() != 0) return 1;
    if (test_limits_memory() != 0) return 1;
    if (test_limits_cpu_time() != 0) return 1;
    if (test_limits_processes() != 0) return 1;
    if (test_limits_stats() != 0) return 1;
    if (test_limits_check() != 0) return 1;
    if (test_limits_status_string() != 0) return 1;

    printf("\n--- Container Tests ---\n");
    if (test_container_config_init() != 0) return 1;
    if (test_container_runtime_is_available() != 0) return 1;

    printf("\n========================================\n");
    printf("All workbench tests passed!\n");
    printf("========================================\n");

    return 0;
}