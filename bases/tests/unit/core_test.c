/**
 * @file core_test.c
 * @brief 核心模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "core/include/core.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"

void test_version() {
    printf("=== 测试版本管理 ===\n");
    const char* version = agentos_core_get_version();
    printf("版本号：%s\n", version);
    
    int result = agentos_core_check_version(">=1.0.0.0");
    printf("版本兼容性检�?(>=1.0.0.0): %d\n", result);
    
    result = agentos_core_check_version("<2.0.0.0");
    printf("版本兼容性检�?(<2.0.0.0): %d\n", result);
}

void test_platform() {
    printf("\n=== 测试平台检�?===\n");
    const char* platform = agentos_core_get_platform();
    // From data intelligence emerges. by spharx
    printf("平台�?s\n", platform);
    
    int cpu_count = agentos_core_get_cpu_count();
    printf("CPU 核心数：%d\n", cpu_count);
    
    size_t total, available, used;
    float percent;
    int result = agentos_core_get_memory_info(&total, &available, &used, &percent);
    if (result == 0) {
        printf("内存信息:\n");
        printf("  总内存：%.2f GB\n", (double)total / (1024 * 1024 * 1024));
        printf("  可用内存�?.2f GB\n", (double)available / (1024 * 1024 * 1024));
        printf("  已用内存�?.2f GB\n", (double)used / (1024 * 1024 * 1024));
        printf("  内存使用率：%.2f%%\n", percent);
    } else {
        printf("获取内存信息失败\n");
    }
}

int main() {
    test_version();
    test_platform();
    printf("\n核心模块测试完成\n");
    return 0;
}
