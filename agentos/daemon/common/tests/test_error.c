/**
 * @file test_error.c
 * @brief 错误处理模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "error.h"

static void test_error_strerror(void) {
    printf("  test_error_strerror...\n");

    assert(strcmp(agentos_strerror(AGENTOS_OK), "Success") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_UNKNOWN), "Unknown error") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_NOT_FOUND), "Not found") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_TIMEOUT), "Operation timed out") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_IO), "I/O error") == 0);
    assert(strcmp(agentos_strerror(AGENTOS_ERR_PARSE_ERROR), "Parse error") == 0);

    printf("    PASSED\n");
}

static void test_error_chain(void) {
    printf("  test_error_chain...\n");

    agentos_error_clear();

    agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "Invalid param test");

    agentos_error_chain_t* chain = agentos_error_get_chain();
    assert(chain != NULL);
    assert(chain->code == AGENTOS_ERR_INVALID_PARAM);
    assert(chain->depth == 1);

    agentos_error_clear();
    assert(agentos_error_get_chain()->code == AGENTOS_OK);

    printf("    PASSED\n");
}

static void test_error_macros(void) {
    printf("  test_error_macros...\n");

    assert(AGENTOS_OK == 0);
    assert(AGENTOS_ERR_UNKNOWN < 0);
    assert(AGENTOS_ERR_INVALID_PARAM < 0);
    assert(AGENTOS_ERR_OUT_OF_MEMORY < 0);
    assert(AGENTOS_ERR_SERVICE_BASE < 0);
    assert(AGENTOS_ERR_LLM_BASE < 0);
    assert(AGENTOS_ERR_TOOL_BASE < 0);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Error Module Unit Tests\n");
    printf("=========================================\n");

    test_error_strerror();
    test_error_chain();
    test_error_macros();

    printf("\n✅ All error module tests PASSED\n");
    return 0;
}