/**
 * @file test_common_integration.c
 * @brief commons 模块集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>

#include "platform.h"
#include "error.h"
#include "logger.h"
#include "manager.h"
#include "token.h"
#include "cost.h"

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

/**
 * @brief 测试完整工作�?
 */
static int test_full_workflow(void) {
    printf("  Step 1: Initialize logging...\n");
    AGENTOS_LOG_INFO("Starting integration test");
    
    printf("  Step 2: Load configuration...\n");
    agentos_config_t* manager = agentos_config_load("test_config.json");
    /* 配置加载失败是正常的，因为测试文件不存在 */
    
    printf("  Step 3: Create token counter...\n");
    agentos_token_counter_t* counter = agentos_token_counter_create("gpt-4");
    if (counter) {
        size_t tokens = agentos_token_counter_count(counter, "Test message");
        printf("    Token count: %zu\n", tokens);
        agentos_token_counter_destroy(counter);
    }
    
    printf("  Step 4: Create cost estimator...\n");
    agentos_cost_estimator_t* estimator = agentos_cost_estimator_create(NULL);
    if (estimator) {
        double cost = agentos_cost_estimator_estimate(estimator, "gpt-4", 100, 50);
        printf("    Estimated cost: $%.6f\n", cost);
        agentos_cost_estimator_destroy(estimator);
    }
    
    printf("  Step 5: Error handling...\n");
    agentos_error_t err = AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "Test error");
    const char* err_str = agentos_strerror(err);
    printf("    Error: %s\n", err_str);
    
    if (manager) agentos_config_free(manager);
    
    AGENTOS_LOG_INFO("Integration test completed");
    
    return 0;
}

/**
 * @brief 测试跨平台兼容�?
 */
static int test_cross_platform(void) {
    /* 测试时间函数 */
    uint64_t start = agentos_time_ns();
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec ts = {0, 10000000};
    nanosleep(&ts, NULL);
#endif
    uint64_t end = agentos_time_ns();
    
    TEST_ASSERT(end > start, "Time should advance");
    printf("  Time elapsed: %llu ns\n", (unsigned long long)(end - start));
    
    /* 测试内存分配 */
    void* ptr = agentos_mem_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Memory allocation should succeed");
    agentos_mem_free(ptr);
    
    return 0;
}

/**
 * @brief 测试错误处理�?
 */
static int test_error_handling_chain(void) {
    agentos_error_chain_t* chain = agentos_error_chain_create();
    TEST_ASSERT(chain != NULL, "Error chain creation should succeed");
    
    /* 模拟错误�?*/
    agentos_error_chain_add(chain, AGENTOS_ERR_INVALID_PARAM, "file1.c", 10, "func1", "First error");
    agentos_error_chain_add(chain, AGENTOS_ERR_OUT_OF_MEMORY, "file2.c", 20, "func2", "Second error");
    
    int count = agentos_error_chain_count(chain);
    printf("  Error chain count: %d\n", count);
    TEST_ASSERT(count == 2, "Error chain should have 2 errors");
    
    agentos_error_chain_destroy(chain);
    
    return 0;
}

int main(void) {
    printf("===========================================\n");
    printf("  commons 模块集成测试\n");
    printf("===========================================\n\n");
    
    TEST_RUN(test_full_workflow);
    TEST_RUN(test_cross_platform);
    TEST_RUN(test_error_handling_chain);
    
    printf("\n===========================================\n");
    printf("  测试结果�?d 通过�?d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}
