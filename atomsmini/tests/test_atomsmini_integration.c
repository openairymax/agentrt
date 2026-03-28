/**
 * @file test_atomsmini_integration.c
 * @brief atomsmini 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "mem.h"
#include "task.h"
#include "time.h"

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { \
            fprintf(stderr, "❌ Test failed: %s\n", #test_func); \
            failed_tests++; \
        } else { \
            printf("✅ PASS: %s\n", #test_func); \
            passed_tests++; \
        } \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/**
 * @brief 测试错误处理模块
 */
static int test_error_handling(void) {
    agentos_error_t* err = agentos_error_create(AGENTOS_ERR_INVALID_ARG, "Test error");
    TEST_ASSERT(err != NULL, "Error should be created");
    TEST_ASSERT(agentos_error_code(err) == AGENTOS_ERR_INVALID_ARG, "Error code should match");
    
    agentos_error_destroy(err);
    return 0;
}

/**
 * @brief 测试内存管理模块
 */
static int test_memory_management(void) {
    void* ptr = agentos_malloc(1024);
    TEST_ASSERT(ptr != NULL, "Memory should be allocated");
    
    memset(ptr, 0, 1024);
    agentos_free(ptr);
    
    return 0;
}

/**
 * @brief 测试任务管理模块
 */
static int test_task_management(void) {
    /* 简单测试任务创建和销毁 */
    agentos_task_t* task = agentos_task_create("test_task", NULL, NULL);
    TEST_ASSERT(task != NULL, "Task should be created");
    
    agentos_task_destroy(task);
    return 0;
}

/**
 * @brief 测试时间模块
 */
static int test_time_functions(void) {
    uint64_t time1 = agentos_time_ns();
    
    /* 等待一小段时间 */
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif
    
    uint64_t time2 = agentos_time_ns();
    TEST_ASSERT(time2 > time1, "Time should increase");
    
    return 0;
}

/**
 * @brief 测试完整工作流
 */
static int test_full_workflow(void) {
    printf("  Step 1: Initialize error handling...\n");
    agentos_error_t* err = NULL;
    
    printf("  Step 2: Allocate memory...\n");
    void* buffer = agentos_malloc(256);
    TEST_ASSERT(buffer != NULL, "Buffer should be allocated");
    
    printf("  Step 3: Create task...\n");
    agentos_task_t* task = agentos_task_create("workflow_task", NULL, &err);
    if (task) {
        printf("  Step 4: Run task...\n");
        /* 模拟任务执行 */
        
        printf("  Step 5: Cleanup task...\n");
        agentos_task_destroy(task);
    }
    
    printf("  Step 6: Free memory...\n");
    agentos_free(buffer);
    
    return 0;
}

int main(void) {
    printf("============================================\n");
    printf("  AgentOS Lite Integration Tests\n");
    printf("============================================\n\n");
    
    TEST_RUN(test_error_handling);
    TEST_RUN(test_memory_management);
    TEST_RUN(test_task_management);
    TEST_RUN(test_time_functions);
    TEST_RUN(test_full_workflow);
    
    printf("\n============================================\n");
    printf("  Test Summary\n");
    printf("============================================\n");
    printf("  Passed: %d\n", passed_tests);
    printf("  Failed: %d\n", failed_tests);
    printf("  Total:  %d\n", passed_tests + failed_tests);
    printf("============================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}

