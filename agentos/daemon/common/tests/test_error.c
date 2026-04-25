/**
 * @file test_error.c
 * @brief 错误处理模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agentos_types.h"

static void test_error_strerror(void) {
    printf("  test_error_strerror...\n");

    assert(agentos_strerror(AGENTOS_SUCCESS) != NULL);
    assert(agentos_strerror(AGENTOS_EUNKNOWN) != NULL);
    assert(agentos_strerror(AGENTOS_EINVAL) != NULL);
    assert(agentos_strerror(AGENTOS_ENOMEM) != NULL);
    assert(agentos_strerror(AGENTOS_ENOENT) != NULL);
    assert(agentos_strerror(AGENTOS_ETIMEDOUT) != NULL);
    assert(agentos_strerror(AGENTOS_EIO) != NULL);
    assert(agentos_strerror(-999) != NULL);

    printf("    PASSED\n");
}

static void test_error_codes(void) {
    printf("  test_error_codes...\n");

    assert(AGENTOS_SUCCESS == 0);
    assert(AGENTOS_EUNKNOWN != 0);
    assert(AGENTOS_EINVAL != 0);
    assert(AGENTOS_ENOMEM != 0);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Error Module Unit Tests\n");
    printf("=========================================\n");

    test_error_strerror();
    test_error_codes();

    printf("\nAll error module tests PASSED\n");
    return 0;
}
