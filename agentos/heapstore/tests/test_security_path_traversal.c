/**
 * @file test_security_path_traversal.c
 * @brief heapstore 模块安全测试 - 路径遍历防护
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
#include "../include/heapstore_log.h"
#include "../include/utils.h"

#define TEST_PASS(name) printf("✅ PASS: %s\n", name)
#define TEST_FAIL(name, reason) printf("❌ FAIL: %s - %s\n", name, reason)

static int test_sanitize_path_component_valid(void) {
    char output[256];

    if (heapstore_sanitize_path_component(output, "valid_service", sizeof(output)) != 0) {
        TEST_FAIL("sanitize_valid", "Should accept valid service name");
        return -1;
    }
    if (strcmp(output, "valid_service") != 0) {
        TEST_FAIL("sanitize_valid", "Output should match input");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service-123", sizeof(output)) != 0) {
        TEST_FAIL("sanitize_valid_hyphen", "Should accept hyphen");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service.name", sizeof(output)) != 0) {
        TEST_FAIL("sanitize_valid_dot", "Should accept dot");
        return -1;
    }

    TEST_PASS("sanitize_path_component_valid");
    return 0;
}

static int test_sanitize_path_component_traversal(void) {
    char output[256];

    if (heapstore_sanitize_path_component(output, "../etc/passwd", sizeof(output)) != -1) {
        TEST_FAIL("block_parent_dir", "Should block parent directory traversal");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "..\\windows\\system32", sizeof(output)) != -1) {
        TEST_FAIL("block_windows_traversal", "Should block Windows-style traversal");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "....//....//etc/passwd", sizeof(output)) != -1) {
        TEST_FAIL("block_double_traversal", "Should block double traversal");
        return -1;
    }

    TEST_PASS("sanitize_path_component_traversal");
    return 0;
}

static int test_sanitize_path_component_slashes(void) {
    char output[256];

    if (heapstore_sanitize_path_component(output, "service/name", sizeof(output)) != -1) {
        TEST_FAIL("block_forward_slash", "Should block forward slash");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service\\name", sizeof(output)) != -1) {
        TEST_FAIL("block_backslash", "Should block backslash");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "/absolute/path", sizeof(output)) != -1) {
        TEST_FAIL("block_absolute_path", "Should block absolute path");
        return -1;
    }

    TEST_PASS("sanitize_path_component_slashes");
    return 0;
}

static int test_sanitize_path_component_special_chars(void) {
    char output[256];

    if (heapstore_sanitize_path_component(output, "service; rm -rf /", sizeof(output)) != 0) {
        TEST_FAIL("replace_semicolon", "Should replace semicolon");
        return -1;
    }
    if (strchr(output, ';') != NULL) {
        TEST_FAIL("replace_semicolon", "Semicolon should be replaced");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service$(whoami)", sizeof(output)) != 0) {
        TEST_FAIL("replace_command_subst", "Should replace command substitution");
        return -1;
    }
    if (strchr(output, '$') != NULL || strchr(output, '(') != NULL) {
        TEST_FAIL("replace_command_subst", "Special chars should be replaced");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "service`id`", sizeof(output)) != 0) {
        TEST_FAIL("replace_backticks", "Should replace backticks");
        return -1;
    }
    if (strchr(output, '`') != NULL) {
        TEST_FAIL("replace_backticks", "Backticks should be replaced");
        return -1;
    }

    TEST_PASS("sanitize_path_component_special_chars");
    return 0;
}

static int test_sanitize_path_component_null_checks(void) {
    char output[256];

    if (heapstore_sanitize_path_component(NULL, "test", sizeof(output)) != -1) {
        TEST_FAIL("null_output", "Should reject NULL output");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, NULL, sizeof(output)) != -1) {
        TEST_FAIL("null_input", "Should reject NULL input");
        return -1;
    }

    if (heapstore_sanitize_path_component(output, "test", 0) != -1) {
        TEST_FAIL("zero_size", "Should reject zero size");
        return -1;
    }

    TEST_PASS("sanitize_path_component_null_checks");
    return 0;
}

static int test_is_safe_identifier(void) {
    if (!heapstore_is_safe_identifier("valid_service")) {
        TEST_FAIL("safe_valid", "Should accept valid identifier");
        return -1;
    }

    if (heapstore_is_safe_identifier("../etc/passwd")) {
        TEST_FAIL("unsafe_traversal", "Should reject traversal");
        return -1;
    }

    if (heapstore_is_safe_identifier("service/name")) {
        TEST_FAIL("unsafe_slash", "Should reject slash");
        return -1;
    }

    if (heapstore_is_safe_identifier("service;cmd")) {
        TEST_FAIL("unsafe_semicolon", "Should reject semicolon");
        return -1;
    }

    if (heapstore_is_safe_identifier(NULL)) {
        TEST_FAIL("unsafe_null", "Should reject NULL");
        return -1;
    }

    if (heapstore_is_safe_identifier("")) {
        TEST_FAIL("unsafe_empty", "Should reject empty string");
        return -1;
    }

    TEST_PASS("is_safe_identifier");
    return 0;
}

static int test_log_path_traversal_blocked(void) {
    heapstore_error_t err = heapstore_log_init();
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("log_init", "Failed to initialize log system");
        return -1;
    }

    err = heapstore_log_write(heapstore_LOG_INFO, "../etc/passwd", NULL, NULL, 0, "test message");
    if (err == heapstore_SUCCESS) {
        TEST_FAIL("log_traversal_blocked", "Should reject path traversal in service name");
        heapstore_log_shutdown();
        return -1;
    }

    err = heapstore_log_write(heapstore_LOG_INFO, "service/../../etc/passwd", NULL, NULL, 0, "test message");
    if (err == heapstore_SUCCESS) {
        TEST_FAIL("log_nested_traversal_blocked", "Should reject nested path traversal");
        heapstore_log_shutdown();
        return -1;
    }

    heapstore_log_shutdown();
    TEST_PASS("log_path_traversal_blocked");
    return 0;
}

static int test_log_valid_service_allowed(void) {
    heapstore_error_t err = heapstore_log_init();
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("log_init", "Failed to initialize log system");
        return -1;
    }

    err = heapstore_log_write(heapstore_LOG_INFO, "valid_service", NULL, NULL, 0, "test message");
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("log_valid_service", "Should accept valid service name");
        heapstore_log_shutdown();
        return -1;
    }

    err = heapstore_log_write(heapstore_LOG_INFO, "service-123_test", NULL, NULL, 0, "test message");
    if (err != heapstore_SUCCESS) {
        TEST_FAIL("log_valid_service_complex", "Should accept complex valid name");
        heapstore_log_shutdown();
        return -1;
    }

    heapstore_log_shutdown();
    TEST_PASS("log_valid_service_allowed");
    return 0;
}

int main(void) {
    int failed = 0;

    printf("\n");
    printf("========================================\n");
    printf(" heapstore Security Tests\n");
    printf(" Path Traversal Prevention\n");
    printf("========================================\n\n");

    if (test_sanitize_path_component_valid() != 0) failed++;
    if (test_sanitize_path_component_traversal() != 0) failed++;
    if (test_sanitize_path_component_slashes() != 0) failed++;
    if (test_sanitize_path_component_special_chars() != 0) failed++;
    if (test_sanitize_path_component_null_checks() != 0) failed++;
    if (test_is_safe_identifier() != 0) failed++;
    if (test_log_path_traversal_blocked() != 0) failed++;
    if (test_log_valid_service_allowed() != 0) failed++;

    printf("\n========================================\n");
    printf(" Test Results: %d passed, %d failed\n", 8 - failed, failed);
    printf("========================================\n\n");

    return (failed == 0) ? 0 : 1;
}
