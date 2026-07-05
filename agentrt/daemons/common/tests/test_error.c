/**
 * @file test_error.c
 * @brief 错误处理模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt_types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

static void test_error_strerror(void)
{
    printf("  test_error_strerror...\n");

    assert(agentrt_strerror(AGENTRT_SUCCESS) != NULL);
    assert(agentrt_strerror(AGENTRT_EUNKNOWN) != NULL);
    assert(agentrt_strerror(AGENTRT_EINVAL) != NULL);
    assert(agentrt_strerror(AGENTRT_ENOMEM) != NULL);
    assert(agentrt_strerror(AGENTRT_ENOENT) != NULL);
    assert(agentrt_strerror(AGENTRT_ETIMEDOUT) != NULL);
    assert(agentrt_strerror(AGENTRT_EIO) != NULL);
    assert(agentrt_strerror(-999) != NULL);

    printf("    PASSED\n");
}

static void test_error_codes(void)
{
    printf("  test_error_codes...\n");

    assert(AGENTRT_SUCCESS == 0);
    assert(AGENTRT_EUNKNOWN != 0);
    assert(AGENTRT_EINVAL != 0);
    assert(AGENTRT_ENOMEM != 0);

    printf("    PASSED\n");
}

static void test_error_new_codes(void)
{
    printf("  test_error_new_codes...\n");

    assert(AGENTRT_ERR_INVALID_PARAM == -2);
    assert(AGENTRT_ERR_NULL_POINTER == -3);
    assert(AGENTRT_ERR_OUT_OF_MEMORY == -4);
    assert(AGENTRT_ERR_NOT_FOUND == -6);
    assert(AGENTRT_ERR_ALREADY_EXISTS == -7);
    assert(AGENTRT_ERR_TIMEOUT == -8);
    assert(AGENTRT_ERR_NOT_SUPPORTED == -9);
    assert(AGENTRT_ERR_PERMISSION_DENIED == -10);
    assert(AGENTRT_ERR_IO == -11);
    assert(AGENTRT_ERR_OVERFLOW == -14);
    assert(AGENTRT_ERR_CANCELED == -16);
    assert(AGENTRT_ERR_BUSY == -17);
    assert(AGENTRT_ERR_INTERRUPTED == -19);

    assert(AGENTRT_ERR_SYS_NOT_INIT == -101);
    assert(AGENTRT_ERR_SYS_RESOURCE == -102);

    assert(agentrt_strerror(AGENTRT_ERR_INVALID_PARAM) != NULL);
    assert(agentrt_strerror(AGENTRT_ERR_OUT_OF_MEMORY) != NULL);
    assert(agentrt_strerror(AGENTRT_ERR_NOT_FOUND) != NULL);
    assert(agentrt_strerror(AGENTRT_ERR_OVERFLOW) != NULL);
    assert(agentrt_strerror(AGENTRT_ERR_SYS_NOT_INIT) != NULL);

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
