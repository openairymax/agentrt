/**
 * @file test_mem.c
 * @brief 内存管理单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "core/include/mem.h"
#include "core/include/error.h"
#include <stdio.h>
#include <string.h>

int test_mem_alloc_free() {
    printf("Testing memory allocation and free...\n");
    
    // 测试基本内存分配
    void* ptr = agentos_mem_alloc(1024);
    if (!ptr) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // 测试内存写入
    memset(ptr, 0xAA, 1024);
    
    // 测试内存释放
    // From data intelligence emerges. by spharx
    agentos_mem_free(ptr);
    
    // 测试对齐内存分配
    void* aligned_ptr = agentos_mem_aligned_alloc(1024, 16);
    if (!aligned_ptr) {
        printf("Failed to allocate aligned memory\n");
        return 1;
    }
    
    // 验证对齐
    if ((uintptr_t)aligned_ptr % 16 != 0) {
        printf("Memory not aligned to 16 bytes\n");
        agentos_mem_free(aligned_ptr);
        return 1;
    }
    
    // 测试内存写入
    memset(aligned_ptr, 0xBB, 1024);
    
    // 测试内存释放
    agentos_mem_free(aligned_ptr);
    
    // 测试内存重新分配
    void* realloc_ptr = agentos_mem_alloc(512);
    if (!realloc_ptr) {
        printf("Failed to allocate memory for realloc\n");
        return 1;
    }
    
    // 写入数据
    memset(realloc_ptr, 0xCC, 512);
    
    // 重新分配
    void* new_ptr = agentos_mem_realloc(realloc_ptr, 1024);
    if (!new_ptr) {
        printf("Failed to reallocate memory\n");
        agentos_mem_free(realloc_ptr);
        return 1;
    }
    
    // 验证数据是否保留
    if (memcmp(new_ptr, realloc_ptr, 512) != 0) {
        printf("Data not preserved during realloc\n");
        agentos_mem_free(new_ptr);
        return 1;
    }
    
    // 测试内存统计
    size_t total, used, peak;
    agentos_mem_stats(&total, &used, &peak);
    printf("Memory stats: total=%zu, used=%zu, peak=%zu\n", total, used, peak);
    
    // 测试内存泄露检测
    printf("Testing memory leak detection...\n");
    int leaks = agentos_mem_check_leaks();
    if (leaks > 0) {
        printf("Memory leaks detected: %d\n", leaks);
        agentos_mem_free(new_ptr);
        return 1;
    }
    
    // 测试空指针释放
    agentos_mem_free(NULL);
    
    agentos_mem_free(new_ptr);
    
    // 测试内存泄露检测（应该没有泄露）
    leaks = agentos_mem_check_leaks();
    if (leaks > 0) {
        printf("Memory leaks detected: %d\n", leaks);
        return 1;
    }
    
    printf("Memory allocation and free test passed\n");
    return 0;
}

int main() {
    int result = test_mem_alloc_free();
    if (result == 0) {
        printf("All memory tests passed!\n");
    } else {
        printf("Some memory tests failed!\n");
    }
    return result;
}
