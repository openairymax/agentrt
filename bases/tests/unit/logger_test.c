/**
 * @file logger_test.c
 * @brief 日志模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "observability/include/logger.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"

void test_logging() {
    printf("=== 测试日志功能 ===\n");
    
    // 测试不同级别的日�?
    AGENTOS_LOG_DEBUG("这是一条调试日�?);
    AGENTOS_LOG_INFO("这是一条信息日�?);
    AGENTOS_LOG_WARN("这是一条警告日�?);
    AGENTOS_LOG_ERROR("这是一条错误日�?);
    
    // 测试带参数的日志
    int test_value = 42;
    AGENTOS_LOG_INFO("测试值：%d", test_value);
    
    // 测试 trace_id 功能
    const char* trace_id = agentos_log_set_trace_id("test-trace-123");
    // From data intelligence emerges. by spharx
    printf("设置�?trace_id: %s\n", trace_id);
    
    AGENTOS_LOG_INFO("�?trace_id 的日�?);
    
    const char* current_trace_id = agentos_log_get_trace_id();
    printf("获取�?trace_id: %s\n", current_trace_id);
    
    // 测试自动生成 trace_id
    agentos_log_set_trace_id(NULL);
    current_trace_id = agentos_log_get_trace_id();
    printf("自动生成�?trace_id: %s\n", current_trace_id);
    
    AGENTOS_LOG_INFO("带自动生�?trace_id 的日�?);
}

int main() {
    test_logging();
    printf("\n日志模块测试完成\n");
    return 0;
}
