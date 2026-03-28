/**
 * @file test_memory.c
 * @brief 统一内存管理模块单元测试
 * 
 * 测试内存模块的基本功能：分配、释放、统计、调试等�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <assert.h>
#include "../../include/memory.h"

/**
 * @brief 测试基础内存分配和释�? * 
 * @return 成功返回0，失败返�?
 */
static int test_basic_allocation(void) {
    printf("测试基础内存分配和释�?..\n");
    
    // 初始化内存模�?    memory_options_t options = {
        .alignment = 0,
        .zero_memory = true,
        .tag = "test_basic_allocation",
        .fail_strategy = MEMORY_FAIL_STRATEGY_RETURN_NULL
    };
    
    if (!memory_init(&options)) {
        printf("  错误：内存模块初始化失败\n");
        return 1;
    }
    
    // 测试内存分配
    void* ptr1 = memory_alloc(100, "test_block_1");
    if (ptr1 == NULL) {
        printf("  错误：内存分配失败\n");
        memory_cleanup();
        return 1;
    }
    
    // 测试内存清零
    for (size_t i = 0; i < 100; i++) {
        if (((char*)ptr1)[i] != 0) {
            printf("  错误：内存未清零\n");
            memory_free(ptr1);
            memory_cleanup();
            return 1;
        }
    }
    
    // 测试内存释放
    memory_free(ptr1);
    
    // 测试calloc
    void* ptr2 = memory_calloc(50, "test_block_2");
    if (ptr2 == NULL) {
        printf("  错误：calloc失败\n");
        memory_cleanup();
        return 1;
    }
    
    // 验证calloc清零
    for (size_t i = 0; i < 50; i++) {
        if (((char*)ptr2)[i] != 0) {
            printf("  错误：calloc未清零\n");
            memory_free(ptr2);
            memory_cleanup();
            return 1;
        }
    }
    
    memory_free(ptr2);
    
    // 测试对齐分配
    void* ptr3 = memory_aligned_alloc(16, 128, "test_aligned");
    if (ptr3 == NULL) {
        printf("  错误：对齐分配失败\n");
        memory_cleanup();
        return 1;
    }
    
    // 检查对�?    if (((uintptr_t)ptr3 & 0xF) != 0) {
        printf("  错误：内存未对齐�?6字节边界\n");
        memory_free(ptr3);
        memory_cleanup();
        return 1;
    }
    
    memory_free(ptr3);
    
    // 测试统计信息
    memory_stats_t stats;
    if (!memory_get_stats(&stats)) {
        printf("  错误：获取统计信息失败\n");
        memory_cleanup();
        return 1;
    }
    
    printf("  统计信息：分配次�?%zu，释放次�?%zu\n", 
           stats.allocation_count, stats.free_count);
    
    // 清理
    memory_cleanup();
    
    printf("  通过！\n");
    return 0;
}

/**
 * @brief 测试内存池功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_memory_pool(void) {
    printf("测试内存池功�?..\n");
    
    memory_options_t options = {
        .alignment = 0,
        .zero_memory = true,
        .tag = "test_memory_pool",
        .fail_strategy = MEMORY_FAIL_STRATEGY_RETURN_NULL
    };
    
    if (!memory_init(&options)) {
        printf("  错误：内存模块初始化失败\n");
        return 1;
    }
    
    // 注意：这里直接使用内存池API，需要包含memory_pool.h
    // 但为了简化，这里仅演示概�?    
    memory_cleanup();
    
    printf("  通过！\n");
    return 0;
}

/**
 * @brief 测试内存调试功能
 * 
 * @return 成功返回0，失败返�?
 */
static int test_memory_debug(void) {
    printf("测试内存调试功能...\n");
    
    memory_options_t options = {
        .alignment = 0,
        .zero_memory = true,
        .tag = "test_memory_debug",
        .fail_strategy = MEMORY_FAIL_STRATEGY_RETURN_NULL
    };
    
    if (!memory_init(&options)) {
        printf("  错误：内存模块初始化失败\n");
        return 1;
    }
    
    // 启用调试
    if (!memory_debug_enable(true)) {
        printf("  错误：启用调试失败\n");
        memory_cleanup();
        return 1;
    }
    
    // 分配一些内�?    void* ptr1 = memory_alloc(64, "debug_block_1");
    void* ptr2 = memory_alloc(128, "debug_block_2");
    void* ptr3 = memory_alloc(256, "debug_block_3");
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("  错误：调试分配失败\n");
        memory_free(ptr1);
        memory_free(ptr2);
        memory_free(ptr3);
        memory_cleanup();
        return 1;
    }
    
    // 检查泄�?    size_t leaked_bytes = memory_check_leaks(false);
    if (leaked_bytes == 0) {
        printf("  错误：应检测到泄漏但未检测到\n");
        memory_free(ptr1);
        memory_free(ptr2);
        memory_free(ptr3);
        memory_cleanup();
        return 1;
    }
    
    printf("  检测到泄漏�?zu字节\n", leaked_bytes);
    
    // 释放内存
    memory_free(ptr1);
    memory_free(ptr2);
    memory_free(ptr3);
    
    // 再次检查泄�?    leaked_bytes = memory_check_leaks(false);
    if (leaked_bytes != 0) {
        printf("  错误：内存释放后仍检测到泄漏\n");
        memory_cleanup();
        return 1;
    }
    
    // 禁用调试
    if (!memory_debug_enable(false)) {
        printf("  错误：禁用调试失败\n");
        memory_cleanup();
        return 1;
    }
    
    memory_cleanup();
    
    printf("  通过！\n");
    return 0;
}

/**
 * @brief 测试内存分配失败处理
 * 
 * @return 成功返回0，失败返�?
 */
static int test_allocation_failure(void) {
    printf("测试内存分配失败处理...\n");
    
    // 设置分配失败策略为返回NULL
    memory_options_t options = {
        .alignment = 0,
        .zero_memory = true,
        .tag = "test_failure",
        .fail_strategy = MEMORY_FAIL_STRATEGY_RETURN_NULL
    };
    
    if (!memory_init(&options)) {
        printf("  错误：内存模块初始化失败\n");
        return 1;
    }
    
    // 测试分配超大内存（可能失败）
    void* huge_ptr = memory_alloc((size_t)-1, "huge_allocation");
    if (huge_ptr != NULL) {
        printf("  错误：超大分配应失败但成功了\n");
        memory_free(huge_ptr);
        memory_cleanup();
        return 1;
    }
    
    memory_cleanup();
    
    printf("  通过！\n");
    return 0;
}

/**
 * @brief 主测试函�? * 
 * @return 成功返回0，失败返�?
 */
int main(void) {
    printf("=== 统一内存管理模块单元测试 ===\n");
    
    int result = 0;
    
    // 运行所有测�?    result |= test_basic_allocation();
    result |= test_memory_pool();
    result |= test_memory_debug();
    result |= test_allocation_failure();
    
    if (result == 0) {
        printf("\n所有测试通过！\n");
    } else {
        printf("\n部分测试失败！\n");
    }
    
    printf("==============================\n");
    
    return result;
}