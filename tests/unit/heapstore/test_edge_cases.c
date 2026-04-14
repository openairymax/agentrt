/**
 * @file test_edge_cases.c
 * @brief heapstore 模块边界条件测试
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/heapstore.h"
#include "../include/utils.h"

#define TEST_PASS(name) printf("✅ PASS: %s\n", name)
#define TEST_FAIL(name, reason) printf("❌ FAIL: %s - %s\n", name, reason)

static int test_error_message_format(void) {
    const char* msg = heapstore_strerror(heapstore_ERR_INVALID_PARAM);

    if (msg == NULL) {
        TEST_FAIL("error_msg_not_null", "Error message should not be NULL");
        return -1;
    }

    if (strncmp(msg, "[ERROR]", 7) != 0) {
        TEST_FAIL("error_msg_format", "Error message should start with [ERROR]");
        return -1;
    }

    if (strstr(msg, "(context:") == NULL) {
        TEST_FAIL("error_msg_context", "Error message should contain (context:)");
        return -1;
    }

    if (strstr(msg, "Suggestion:") == NULL) {
        TEST_FAIL("error_msg_suggestion", "Error message should contain Suggestion:");
        return -1;
    }

    const char* success_msg = heapstore_strerror(heapstore_SUCCESS);
    if (strncmp(success_msg, "[OK]", 4) != 0) {
        TEST_FAIL("success_msg_format", "Success message should start with [OK]");
        return -1;
    }

    TEST_PASS("error_message_format");
    return 0;
}

static int test_null_pointer_handling(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_null_test", "Failed to initialize for null test");
        return -1;
    }

    err = heapstore_log_write_fast(NULL, 0, "test");
    if (err != heapstore_ERR_INVALID_PARAM) {
        TEST_FAIL("null_service_name", "Should reject NULL service name");
        heapstore_shutdown();
        return -1;
    }

    err = heapstore_log_write_fast("test_service", 0, NULL);
    if (err != heapstore_ERR_INVALID_PARAM) {
        TEST_FAIL("null_message", "Should reject NULL message");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();
    TEST_PASS("null_pointer_handling");
    return 0;
}

static int test_empty_string_handling(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test2";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_empty_test", "Failed to initialize for empty test");
        return -1;
    }

    err = heapstore_log_write_fast("", 0, "test");
    if (err != heapstore_ERR_INVALID_PARAM) {
        TEST_FAIL("empty_service_name", "Should reject empty service name");
        heapstore_shutdown();
        return -1;
    }

    err = heapstore_log_write_fast("test_service", 0, "");
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("empty_message", "Should accept empty message (valid edge case)");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();
    TEST_PASS("empty_string_handling");
    return 0;
}

static int test_very_long_input(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test3";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_long_test", "Failed to initialize for long test");
        return -1;
    }

    char long_service[10000];
    memset(long_service, 'A', sizeof(long_service));
    long_service[sizeof(long_service) - 1] = '\0';

    err = heapstore_log_write_fast(long_service, 0, "test");
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("long_service_name", "Should handle very long service name gracefully");
        heapstore_shutdown();
        return -1;
    }

    char long_message[100000];
    memset(long_message, 'B', sizeof(long_message));
    long_message[sizeof(long_message) - 1] = '\0';

    err = heapstore_log_write_fast("test_service", 0, long_message);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("long_message", "Should handle very long message gracefully");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();
    TEST_PASS("very_long_input");
    return 0;
}

static int test_special_characters_in_service_name(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test4";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_special_test", "Failed to initialize for special test");
        return -1;
    }

    const char* dangerous_names[] = {
        "service; rm -rf /",
        "service$(whoami)",
        "service`id`",
        "service|cat /etc/passwd",
        "service& echo pwned",
        "service\ninjection",
        "service\ttab",
        NULL
    };

    for (int i = 0; dangerous_names[i] != NULL; i++) {
        err = heapstore_log_write_fast(dangerous_names[i], 0, "test");
        if (err != heapstore_SUCCESS) {
            TEST_FAIL("dangerous_name", "Should handle dangerous characters safely");
            heapstore_shutdown();
            return -1;
        }
    }

    heapstore_shutdown();
    TEST_PASS("special_characters_in_service_name");
    return 0;
}

static int test_sanitize_function_edge_cases(void) {
    char output[256];

    if (heapstore_sanitize_path_component(output, "valid_service", sizeof(output)) != 0) {
        TEST_FAIL("valid_service", "Should accept valid service name");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service-with-dash", sizeof(output)) != 0) {
        TEST_FAIL("service_with_dash", "Should accept dash");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service.with.dot", sizeof(output)) != 0) {
        TEST_FAIL("service_with_dot", "Should accept dot");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service_123", sizeof(output)) != 0) {
        TEST_FAIL("service_with_numbers", "Should accept numbers");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "", sizeof(output)) != -1) {
        TEST_FAIL("empty_input", "Should reject empty input");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "a", sizeof(output)) != 0) {
        TEST_FAIL("single_char", "Should accept single character");
        return -1;
    }

    char very_long[10000];
    memset(very_long, 'A', sizeof(very_long) - 1);
    very_long[sizeof(very_long) - 1] = '\0';

    if (heapstore_sanitize_path_component(output, very_long, sizeof(output)) != -1) {
        TEST_FAIL("buffer_overflow", "Should reject input larger than buffer");
        return -1;
    }

    TEST_PASS("sanitize_function_edge_cases");
    return 0;
}

static int test_initialization_edge_cases(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test5";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("first_init", "First init should succeed");
        return -1;
    }

    err = heapstore_init(&config);
    if (err != heapstore_ERR_ALREADY_INITIALIZED) {
        TEST_FAIL("double_init", "Double init should fail with ERR_ALREADY_INITIALIZED");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();

    bool initialized = heapstore_is_initialized();
    if (initialized) {
        TEST_FAIL("after_shutdown", "Should not be initialized after shutdown");
        return -1;
    }

    TEST_PASS("initialization_edge_cases");
    return 0;
}

static int test_stats_and_metrics(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test6";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_stats", "Failed to initialize for stats test");
        return -1;
    }

    heapstore_stats_t stats = {0};
    err = heapstore_get_stats(&stats);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("get_stats", "get_stats should succeed");
        heapstore_shutdown();
        return -1;
    }

    if (stats.total_disk_usage_bytes == 0) {
        TEST_FAIL("stats_disk_usage", "Disk usage should be > 0 after init");
        heapstore_shutdown();
        return -1;
    }

    heapstore_metrics_t metrics = {0};
    err = heapstore_get_metrics(&metrics);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("get_metrics", "get_metrics should succeed");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();
    TEST_PASS("stats_and_metrics");
    return 0;
}

static int test_health_check(void) {
    heapstore_config_t config = {0};
    config.root_path = "/tmp/heapstore_edge_test7";

    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("init_for_health", "Failed to initialize for health check");
        return -1;
    }

    bool registry_ok, trace_ok, log_ok, ipc_ok, memory_ok;
    err = heapstore_health_check(&registry_ok, &trace_ok, &log_ok, &ipc_ok, &memory_ok);
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("health_check", "health_check should succeed");
        heapstore_shutdown();
        return -1;
    }

    if (!registry_ok || !log_ok) {
        TEST_FAIL("health_status", "Core subsystems should be healthy");
        heapstore_shutdown();
        return -1;
    }

    heapstore_shutdown();
    TEST_PASS("health_check");
    return 0;
}

int main(void) {
    int failed = 0;

    printf("\n");
    printf("========================================\n");
    printf(" heapstore Edge Cases Test Suite\n");
    printf("========================================\n\n");

    if (test_error_message_format() != 0) failed++;
    if (test_null_pointer_handling() != 0) failed++;
    if (test_empty_string_handling() != 0) failed++;
    if (test_very_long_input() != 0) failed++;
    if (test_special_characters_in_service_name() != 0) failed++;
    if (test_sanitize_function_edge_cases() != 0) failed++;
    if (test_initialization_edge_cases() != 0) failed++;
    if (test_stats_and_metrics() != 0) failed++;
    if (test_health_check() != 0) failed++;

    printf("\n========================================\n");
    printf(" Test Results: %d passed, %d failed\n", 9 - failed, failed);
    printf("========================================\n\n");

    return (failed == 0) ? 0 : 1;
}
