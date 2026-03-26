/**
 * @file test_logger.c
 * @brief logger.h 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

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

static int test_log_levels(void) {
    AGENTOS_LOG_ERROR("Test error message");
    AGENTOS_LOG_WARN("Test warning message");
    AGENTOS_LOG_INFO("Test info message");
    AGENTOS_LOG_DEBUG("Test debug message");
    
    printf("  Log levels: OK\n");
    return 0;
}

static int test_trace_id(void) {
    const char* trace_id = "test-trace-123";
    
    agentos_log_set_trace_id(trace_id);
    const char* retrieved = agentos_log_get_trace_id();
    
    TEST_ASSERT(retrieved != NULL, "Trace ID should not be NULL");
    TEST_ASSERT(strcmp(retrieved, trace_id) == 0, "Trace ID should match");
    
    printf("  Trace ID: OK\n");
    return 0;
}

int main(void) {
    printf("===========================================\n");
    printf("  common/logger 单元测试\n");
    printf("===========================================\n\n");
    
    TEST_RUN(test_log_levels);
    TEST_RUN(test_trace_id);
    
    printf("\n===========================================\n");
    printf("  测试结果：%d 通过，%d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}
