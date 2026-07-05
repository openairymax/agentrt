/**
 * @file test_common_integration.c
 * @brief commons 模块集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "cost.h"
#include "error.h"
#include "include/memory_compat.h"
#include "logger.h"
#include "manager.h"
#include "platform.h"
#include "string_compat.h"
#include "token.h"

#include <string.h>

#define TEST_ASSERT(condition, message)              \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "✗FAIL: %s\n", message); \
            return 1;                                \
        }                                            \
    } while (0)

#define TEST_RUN(test_func)                                    \
    do {                                                       \
        printf("🧪 Running %s...\n", #test_func);              \
        if (test_func() != 0) {                                \
            fprintf(stderr, "✗Test failed: %s\n", #test_func); \
            failed_tests++;                                    \
        } else {                                               \
            printf("✔PASS: %s\n", #test_func);                 \
            passed_tests++;                                    \
        }                                                      \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/**
 * @brief 测试完整工作?
 */
static int test_full_workflow(void)
{
    printf("  Step 1: Initialize logging...\n");
    AGENTRT_LOG_INFO("Starting integration test");

    printf("  Step 2: Load configuration...\n");
    agentrt_config_t *manager = agentrt_config_load("test_config.json");
    /* 配置加载失败是正常的，因为测试文件不存在 */

    printf("  Step 3: Create token counter...\n");
    agentrt_token_counter_t *counter = agentrt_token_counter_create("gpt-4");
    if (counter) {
        size_t tokens = agentrt_token_counter_count(counter, "Test message");
        printf("    Token count: %zu\n", tokens);
        agentrt_token_counter_destroy(counter);
    }

    printf("  Step 4: Create cost estimator...\n");
    agentrt_cost_estimator_t *estimator = agentrt_cost_estimator_create(NULL);
    if (estimator) {
        double cost = agentrt_cost_estimator_estimate(estimator, "gpt-4", 100, 50);
        printf("    Estimated cost: $%.6f\n", cost);
        agentrt_cost_estimator_destroy(estimator);
    }

    printf("  Step 5: Error handling...\n");
    agentrt_error_t err = AGENTRT_ERR_INVALID_PARAM;
    agentrt_error_push_ex(err, __FILE__, __LINE__, __func__, "%s", "Test error");
    const char *err_str = agentrt_strerror(err);
    printf("    Error: %s\n", err_str);

    if (manager)
        agentrt_config_free(manager);

    AGENTRT_LOG_INFO("Integration test completed");

    return 0;
}

/**
 * @brief 测试跨平台兼容?
 */
static int test_cross_platform(void)
{
    /* 测试时间函数 */
    uint64_t start = agentrt_time_ns();
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec ts = {0, 10000000};
    nanosleep(&ts, NULL);
#endif
    uint64_t end = agentrt_time_ns();

    TEST_ASSERT(end > start, "Time should advance");
    printf("  Time elapsed: %llu ns\n", (unsigned long long)(end - start));

    /* 测试内存分配 */
    void *ptr = agentrt_mem_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Memory allocation should succeed");
    agentrt_mem_free(ptr);

    return 0;
}

/**
 * @brief 测试错误处理?
 */
static int test_error_handling_chain(void)
{
    agentrt_error_chain_t *chain = agentrt_error_chain_create();
    TEST_ASSERT(chain != NULL, "Error chain creation should succeed");

    /* 模拟错误?*/
    agentrt_error_chain_add(chain, AGENTRT_ERR_INVALID_PARAM, "file1.c", 10, "func1",
                            "First error");
    agentrt_error_chain_add(chain, AGENTRT_ERR_OUT_OF_MEMORY, "file2.c", 20, "func2",
                            "Second error");

    int count = agentrt_error_chain_count(chain);
    printf("  Error chain count: %d\n", count);
    TEST_ASSERT(count == 2, "Error chain should have 2 errors");

    agentrt_error_chain_destroy(chain);

    return 0;
}

int main(void)
{
    printf("===========================================\n");
    printf("  commons 模块集成测试\n");
    printf("===========================================\n\n");

    TEST_RUN(test_full_workflow);
    TEST_RUN(test_cross_platform);
    TEST_RUN(test_error_handling_chain);

    printf("\n===========================================\n");
    printf("  测试结果：%d 通过：%d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");

    return failed_tests > 0 ? 1 : 0;
}
