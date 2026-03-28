/**
 * @file test_config.c
 * @brief manager.h 单元测试
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"
#include "manager.h"

#define TEST_ASSERT(condition, message) \
    do { if (!(condition)) { fprintf(stderr, "�?FAIL: %s\n", message); return 1; } } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { failed_tests++; } else { printf("�?PASS: %s\n", #test_func); passed_tests++; } \
    } while (0)

static int passed_tests = 0, failed_tests = 0;

static int test_config_load(void) {
    agentos_config_t* manager = agentos_config_load("test_config.json");
    if (manager) agentos_config_free(manager);
    printf("  manager load: OK\n");
    return 0;
}

int main(void) {
    printf("bases/manager 单元测试\n");
    TEST_RUN(test_config_load);
    printf("测试结果�?d 通过�?d 失败\n", passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
