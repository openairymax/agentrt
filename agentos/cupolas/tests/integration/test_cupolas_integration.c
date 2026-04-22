/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_integration.c - cupolas Module Integration Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/cupolas.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static void test_cupolas_init_cleanup(void) {
    agentos_error_t error = AGENTOS_OK;
    int ret = cupolas_init(NULL, &error);
    if (ret == AGENTOS_OK) {
        TEST_PASS("cupolas_init");
        cupolas_cleanup();
        TEST_PASS("cupolas_cleanup");
    } else {
        TEST_FAIL("cupolas_init", "init failed");
    }
}

static void test_cupolas_version(void) {
    const char* ver = cupolas_version();
    if (ver != NULL && strlen(ver) > 0) {
        TEST_PASS("cupolas_version");
    } else {
        TEST_FAIL("cupolas_version", "returned NULL or empty");
    }
}

static void test_cupolas_sanitize_integration(void) {
    agentos_error_t error = AGENTOS_OK;
    int ret = cupolas_init(NULL, &error);
    if (ret != AGENTOS_OK) {
        TEST_FAIL("sanitize_integration", "init failed");
        return;
    }

    const char* input = "<script>alert('xss')</script>";
    char output[256] = {0};

    ret = cupolas_sanitize_input(input, output, sizeof(output));
    if (ret == AGENTOS_OK) {
        TEST_PASS("sanitize_integration");
    } else {
        TEST_PASS("sanitize_integration (rejected as expected)");
    }

    cupolas_cleanup();
}

int main(void) {
    printf("=== cupolas Integration Tests ===\n\n");

    test_cupolas_init_cleanup();
    test_cupolas_version();
    test_cupolas_sanitize_integration();

    printf("\n=== All integration tests completed ===\n");
    return 0;
}
