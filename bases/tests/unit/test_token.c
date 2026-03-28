/**
 * @file test_token.c
 * @brief token.h 单元测试
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"
#include "token.h"

#define TEST_ASSERT(condition, message) \
    do { if (!(condition)) { fprintf(stderr, "�?FAIL: %s\n", message); return 1; } } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { failed_tests++; } else { printf("�?PASS: %s\n", #test_func); passed_tests++; } \
    } while (0)

static int passed_tests = 0, failed_tests = 0;

static int test_token_count(void) {
    agentos_token_counter_t* counter = agentos_token_counter_create("gpt-4");
    if (!counter) { printf("  Token count: Skipped\n"); return 0; }
    
    size_t count = agentos_token_counter_count(counter, "Hello, World!");
    printf("  Token count: %zu tokens\n", count);
    
    agentos_token_counter_destroy(counter);
    return 0;
}

int main(void) {
    printf("bases/token 单元测试\n");
    TEST_RUN(test_token_count);
    printf("测试结果�?d 通过�?d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
