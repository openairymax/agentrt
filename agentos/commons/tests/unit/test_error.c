/**
 * @file test_error.c
 * @brief error.h 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <assert.h>

#include "error.h"

/* ==================== 测试辅助�?==================== */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "�?FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { \
            fprintf(stderr, "�?Test failed: %s\n", #test_func); \
            failed_tests++; \
        } else { \
            printf("�?PASS: %s\n", #test_func); \
            passed_tests++; \
        } \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* ==================== 测试用例 ==================== */

/**
 * @brief 测试错误码定�?
 */
static int test_error_codes(void) {
    TEST_ASSERT(AGENTOS_OK == 0, "AGENTOS_OK should be 0");
    TEST_ASSERT(AGENTOS_SUCCESS == 0, "AGENTOS_SUCCESS should be 0");
    TEST_ASSERT(AGENTOS_ERR_UNKNOWN == -1, "AGENTOS_ERR_UNKNOWN should be -1");
    TEST_ASSERT(AGENTOS_ERR_INVALID_PARAM == -2, "AGENTOS_ERR_INVALID_PARAM should be -2");
    TEST_ASSERT(AGENTOS_ERR_NULL_POINTER == -3, "AGENTOS_ERR_NULL_POINTER should be -3");
    TEST_ASSERT(AGENTOS_ERR_OUT_OF_MEMORY == -4, "AGENTOS_ERR_OUT_OF_MEMORY should be -4");
    
    printf("  Error codes: OK\n");
    return 0;
}

/**
 * @brief 测试错误字符串转�?
 */
static int test_error_strings(void) {
    const char* str;
    
    str = agentos_strerror(AGENTOS_OK);
    TEST_ASSERT(str != NULL, "Error string for OK should not be NULL");
    TEST_ASSERT(strcmp(str, "Success") == 0 || strcmp(str, "OK") == 0, 
                "Error string should indicate success");
    
    str = agentos_strerror(AGENTOS_ERR_UNKNOWN);
    TEST_ASSERT(str != NULL, "Error string for UNKNOWN should not be NULL");
    
    str = agentos_strerror(AGENTOS_ERR_INVALID_PARAM);
    TEST_ASSERT(str != NULL, "Error string for INVALID_PARAM should not be NULL");
    
    str = agentos_strerror(AGENTOS_ERR_OUT_OF_MEMORY);
    TEST_ASSERT(str != NULL, "Error string for OUT_OF_MEMORY should not be NULL");
    
    str = agentos_strerror(-999); /* 未知错误�?*/
    TEST_ASSERT(str != NULL, "Error string for unknown code should not be NULL");
    
    printf("  Error strings: OK\n");
    return 0;
}

/**
 * @brief 测试错误处理宏
 */
static int test_error_macros(void) {
    agentos_error_clear();
    
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "Test error");
    
    agentos_error_chain_t* chain = agentos_error_get_chain();
    TEST_ASSERT(chain != NULL, "Error chain should exist after AGENTOS_ERROR_HANDLE");
    
    int depth = agentos_error_chain_get_depth(chain);
    TEST_ASSERT(depth >= 1, "Error chain should have at least 1 error after AGENTOS_ERROR_HANDLE");
    
    agentos_error_clear();
    
    printf("  Error macros: OK\n");
    return 0;
}

/**
 * @brief 测试错误链
 */
static int test_error_chain(void) {
    agentos_error_clear();
    
    agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, "test.c", 10, "test_func", "%s", "First error");
    agentos_error_push_ex(AGENTOS_ERR_OUT_OF_MEMORY, "test.c", 20, "test_func", "%s", "Second error");
    
    agentos_error_chain_t* chain = agentos_error_get_chain();
    TEST_ASSERT(chain != NULL, "Error chain should exist");
    
    int depth = agentos_error_chain_get_depth(chain);
    TEST_ASSERT(depth >= 2, "Error chain should have at least 2 errors");
    
    agentos_error_clear();
    
    printf("  Error chain: OK\n");
    return 0;
}

/**
 * @brief 测试错误上下文
 */
static int test_error_context(void) {
    agentos_error_clear();
    
    agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, "test.c", 10, "test_func", "%s", "Context error");
    
    agentos_error_chain_t* chain = agentos_error_get_chain();
    TEST_ASSERT(chain != NULL, "Error chain should exist for context test");
    
    int depth = agentos_error_chain_get_depth(chain);
    TEST_ASSERT(depth >= 1, "Context should have at least 1 entry");
    
    agentos_error_clear();
    
    printf("  Error context: OK\n");
    return 0;
}

/* ==================== 主函�?==================== */

int main(void) {
    printf("===========================================\n");
    printf("  agentos/commons/error 单元测试\n");
    printf("===========================================\n\n");
    
    TEST_RUN(test_error_codes);
    TEST_RUN(test_error_strings);
    TEST_RUN(test_error_macros);
    TEST_RUN(test_error_chain);
    TEST_RUN(test_error_context);
    
    printf("\n===========================================\n");
    printf("  测试结果�?d 通过�?d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}
