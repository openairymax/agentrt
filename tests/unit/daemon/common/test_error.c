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

    assert(strcmp(agentrt_strerror(AGENTRT_OK), "Success") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_UNKNOWN), "Unknown error") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_NOT_FOUND), "Not found") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_TIMEOUT), "Operation timed out") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_IO), "I/O error") == 0);
    assert(strcmp(agentrt_strerror(AGENTRT_ERR_PARSE_ERROR), "Parse error") == 0);

    printf("    PASSED\n");
}

static void test_error_chain(void) {
    printf("  test_error_chain...\n");

    agentrt_error_clear();

    agentrt_error_push_ex(AGENTRT_ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "Invalid param test");

    agentrt_error_chain_t* chain = agentrt_error_get_chain();
    assert(chain != NULL);
    assert(chain->code == AGENTRT_ERR_INVALID_PARAM);
    assert(chain->depth == 1);

    agentrt_error_clear();
    assert(agentrt_error_get_chain()->code == AGENTRT_OK);

    printf("    PASSED\n");
}

static void test_error_macros(void) {
    printf("  test_error_macros...\n");

    assert(AGENTRT_OK == 0);
    assert(AGENTRT_ERR_UNKNOWN < 0);
    assert(AGENTRT_ERR_INVALID_PARAM < 0);
    assert(AGENTRT_ERR_OUT_OF_MEMORY < 0);
    assert(AGENTRT_ERR_SERVICE_BASE < 0);
    assert(AGENTRT_ERR_LLM_BASE < 0);
    assert(AGENTRT_ERR_TOOL_BASE < 0);

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