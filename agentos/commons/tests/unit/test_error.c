/**
 * @file test_error.c
 * @brief error.h 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "agentos_types.h"
#include "agentos_memory.h"

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
    TEST_ASSERT(AGENTOS_SUCCESS == 0, "AGENTOS_SUCCESS should be 0");
    TEST_ASSERT(AGENTOS_EUNKNOWN != 0, "AGENTOS_EUNKNOWN should be non-zero");
    TEST_ASSERT(AGENTOS_EINVAL != 0, "AGENTOS_EINVAL should be non-zero");
    TEST_ASSERT(AGENTOS_ENOMEM != 0, "AGENTOS_ENOMEM should be non-zero");
    
    printf("  Error codes: OK\n");
    return 0;
}

/**
 * @brief 测试错误字符串转�?
 */
static int test_error_strings(void) {
    const char* str;
    
    str = agentos_strerror(AGENTOS_SUCCESS);
    TEST_ASSERT(str != NULL, "Error string for SUCCESS should not be NULL");
    
    str = agentos_strerror(AGENTOS_EUNKNOWN);
    TEST_ASSERT(str != NULL, "Error string for EUNKNOWN should not be NULL");
    
    str = agentos_strerror(AGENTOS_EINVAL);
    TEST_ASSERT(str != NULL, "Error string for EINVAL should not be NULL");
    
    str = agentos_strerror(AGENTOS_ENOMEM);
    TEST_ASSERT(str != NULL, "Error string for ENOMEM should not be NULL");
    
    str = agentos_strerror(-999);
    TEST_ASSERT(str != NULL, "Error string for unknown code should not be NULL");
    
    printf("  Error strings: OK\n");
    return 0;
}

int main(void) {
    printf("===========================================\n");
    printf("  agentos/commons/error unit tests\n");
    printf("===========================================\n\n");
    
    TEST_RUN(test_error_codes);
    TEST_RUN(test_error_strings);
    
    printf("\n===========================================\n");
    printf("  Results: %d passed, %d failed\n", passed_tests, failed_tests);
    printf("===========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}
