/**
 * @file test_error.c
 * @brief 错误处理模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

static void test_error_strerror(void)
{
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

static void test_error_codes(void)
{
    printf("  test_error_codes...\n");

    assert(AGENTOS_SUCCESS == 0);
    assert(AGENTOS_EUNKNOWN != 0);
    assert(AGENTOS_EINVAL != 0);
    assert(AGENTOS_ENOMEM != 0);

    printf("    PASSED\n");
}

static void test_error_new_codes(void)
{
    printf("  test_error_new_codes...\n");

    assert(AGENTOS_ERR_INVALID_PARAM == -2);
    assert(AGENTOS_ERR_NULL_POINTER == -3);
    assert(AGENTOS_ERR_OUT_OF_MEMORY == -4);
    assert(AGENTOS_ERR_NOT_FOUND == -6);
    assert(AGENTOS_ERR_ALREADY_EXISTS == -7);
    assert(AGENTOS_ERR_TIMEOUT == -8);
    assert(AGENTOS_ERR_NOT_SUPPORTED == -9);
    assert(AGENTOS_ERR_PERMISSION_DENIED == -10);
    assert(AGENTOS_ERR_IO == -11);
    assert(AGENTOS_ERR_OVERFLOW == -14);
    assert(AGENTOS_ERR_CANCELED == -16);
    assert(AGENTOS_ERR_BUSY == -17);
    assert(AGENTOS_ERR_INTERRUPTED == -19);

    assert(AGENTOS_ERR_SYS_NOT_INIT == -101);
    assert(AGENTOS_ERR_SYS_RESOURCE == -102);

    assert(agentos_strerror(AGENTOS_ERR_INVALID_PARAM) != NULL);
    assert(agentos_strerror(AGENTOS_ERR_OUT_OF_MEMORY) != NULL);
    assert(agentos_strerror(AGENTOS_ERR_NOT_FOUND) != NULL);
    assert(agentos_strerror(AGENTOS_ERR_OVERFLOW) != NULL);
    assert(agentos_strerror(AGENTOS_ERR_SYS_NOT_INIT) != NULL);

    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  Error Module Unit Tests\n");
    printf("=========================================\n");

    test_error_strerror();
    test_error_codes();
    test_error_new_codes();

    printf("\nAll error module tests PASSED\n");
    return 0;
}
