/**
 * @file test_resource_quota.c
 * @brief 资源配额管理单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "resource_quota.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("[PASS] %s\n", message); \
    } else { \
        tests_failed++; \
        printf("[FAIL] %s\n", message); \
    } \
} while(0)

void test_resource_manager_create(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 1024 * 1024,
        .max_cpu_time_ms = 5000,
        .max_io_ops = 1000,
        .max_network_bytes = 10 * 1024 * 1024
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_error_t result = agentos_resource_manager_create(&quota, "test_resource", &manager);

    TEST_ASSERT(result == 0 && manager != NULL, "Resource manager creation should succeed");
    TEST_ASSERT(strcmp(manager->resource_id, "test_resource") == 0, "Resource ID should match");
    TEST_ASSERT(manager->enabled == 1, "Manager should be enabled by default");
    TEST_ASSERT(manager->exceeded_flags == 0, "No flags exceeded initially");

    if (manager) {
        agentos_resource_manager_destroy(manager);
    }
}

void test_resource_manager_null_params(void) {
    agentos_resource_quota_t quota = {0};
    agentos_resource_manager_t* manager = NULL;

    agentos_error_t result1 = agentos_resource_manager_create(NULL, "test", &manager);
    TEST_ASSERT(result1 != 0, "Should fail with NULL quota");

    agentos_error_t result2 = agentos_resource_manager_create(&quota, NULL, &manager);
    TEST_ASSERT(result2 != 0, "Should fail with NULL resource_id");

    agentos_error_t result3 = agentos_resource_manager_create(&quota, "test", NULL);
    TEST_ASSERT(result3 != 0, "Should fail with NULL out_manager");

    agentos_resource_manager_destroy(NULL);
    TEST_ASSERT(1 == 1, "Destroy with NULL should not crash");
}

void test_memory_quota_check(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 1000,
        .max_cpu_time_ms = 5000,
        .max_io_ops = 1000,
        .max_network_bytes = 10000
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_resource_manager_create(&quota, "test_mem", &manager);

    agentos_error_t result1 = agentos_resource_check_memory(manager, 500);
    TEST_ASSERT(result1 == 0, "Should allow allocation under limit");

    agentos_error_t result2 = agentos_resource_record_allocation(manager, 500);
    TEST_ASSERT(result2 == 0, "Record allocation should succeed");

    agentos_error_t result3 = agentos_resource_check_memory(manager, 600);
    TEST_ASSERT(result3 != 0, "Should reject allocation over limit (500+600 > 1000)");

    agentos_error_t result4 = agentos_resource_check_memory(manager, 500);
    TEST_ASSERT(result4 == 0, "Should allow exact limit allocation (500+500=1000)");

    agentos_resource_manager_destroy(manager);
}

void test_memory_allocation_tracking(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 10000,
        .max_cpu_time_ms = 5000,
        .max_io_ops = 1000,
        .max_network_bytes = 10000
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_resource_manager_create(&quota, "test_track", &manager);

    agentos_resource_record_allocation(manager, 1000);
    agentos_resource_record_allocation(manager, 2000);

    agentos_resource_usage_t usage;
    agentos_resource_get_usage(manager, &usage);

    TEST_ASSERT(usage.current_memory_bytes == 3000, "Memory usage should be 3000 bytes");
    TEST_ASSERT(usage.operation_count == 2, "Operation count should be 2");

    agentos_resource_record_free(manager, 1500);
    agentos_resource_get_usage(manager, &usage);

    TEST_ASSERT(usage.current_memory_bytes == 1500, "Memory usage should be 1500 after free");

    agentos_resource_manager_destroy(manager);
}

void test_io_quota_tracking(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 10000,
        .max_cpu_time_ms = 5000,
        .max_io_ops = 5,
        .max_network_bytes = 10000
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_resource_manager_create(&quota, "test_io", &manager);

    for (int i = 0; i < 4; i++) {
        agentos_error_t result = agentos_resource_record_io(manager);
        TEST_ASSERT(result == 0, "I/O operation should succeed within limit");
    }

    agentos_error_t result = agentos_resource_record_io(manager);
    TEST_ASSERT(result == 0, "5th I/O at limit should succeed");

    agentos_error_t result_over = agentos_resource_record_io(manager);
    TEST_ASSERT(result_over != 0, "6th I/O over limit should fail");

    int exceeded = agentos_resource_is_exceeded(manager);
    TEST_ASSERT(exceeded == 1, "Resource should be marked as exceeded");

    const char* info = agentos_resource_get_exceeded_info(manager);
    TEST_ASSERT(info != NULL && strstr(info, "[I/O]") != NULL, "Exceeded info should mention I/O");

    agentos_resource_manager_destroy(manager);
}

void test_disabled_manager(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 100,
        .max_cpu_time_ms = 100,
        .max_io_ops = 1,
        .max_network_bytes = 100
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_resource_manager_create(&quota, "test_disabled", &manager);
    manager->enabled = 0;

    agentos_error_t check_result = agentos_resource_check_memory(manager, 999999);
    TEST_ASSERT(check_result == 0, "Disabled manager should always pass checks");

    agentos_error_t alloc_result = agentos_resource_record_allocation(manager, 999999);
    TEST_ASSERT(alloc_result == 0, "Disabled manager should accept any allocation");

    int exceeded = agentos_resource_is_exceeded(NULL);
    TEST_ASSERT(exceeded == 0, "NULL manager should not be exceeded");

    agentos_resource_manager_destroy(manager);
}

void test_edge_cases(void) {
    agentos_resource_quota_t quota = {
        .max_memory_bytes = 0,
        .max_cpu_time_ms = 0,
        .max_io_ops = 0,
        .max_network_bytes = 0
    };

    agentos_resource_manager_t* manager = NULL;
    agentos_resource_manager_create(&quota, "test_edge", &manager);

    agentos_error_t zero_check = agentos_resource_check_memory(manager, 0);
    TEST_ASSERT(zero_check != 0, "Zero byte request should fail validation");

    agentos_error_t zero_alloc = agentos_resource_record_allocation(manager, 0);
    TEST_ASSERT(zero_alloc != 0, "Zero byte allocation should fail");

    agentos_error_t zero_free = agentos_resource_record_free(manager, 0);
    TEST_ASSERT(zero_free != 0, "Zero byte free should fail");

    agentos_resource_usage_t usage;
    agentos_resource_get_usage(NULL, &usage);
    TEST_ASSERT(1 == 1, "Get usage with NULL manager should not crash");

    agentos_resource_get_usage(manager, NULL);
    TEST_ASSERT(1 == 1, "Get usage with NULL output should not crash");

    const char* null_info = agentos_resource_get_exceeded_info(NULL);
    TEST_ASSERT(null_info != NULL, "Get exceeded info with NULL should return default message");

    agentos_resource_manager_destroy(manager);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("Resource Quota Manager Unit Tests\n");
    printf("========================================\n\n");

    printf("--- Resource Manager Creation Tests ---\n");
    test_resource_manager_create();
    test_resource_manager_null_params();

    printf("\n--- Memory Quota Tests ---\n");
    test_memory_quota_check();
    test_memory_allocation_tracking();

    printf("\n--- I/O Quota Tests ---\n");
    test_io_quota_tracking();

    printf("\n--- Edge Case Tests ---\n");
    test_disabled_manager();
    test_edge_cases();

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
