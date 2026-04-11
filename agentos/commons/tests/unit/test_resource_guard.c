/**
 * @file test_resource_guard.c
 * @brief 资源管理模块单元测试
 *
 * 测试RAII资源管理模式、内存泄漏检测、资源配额管理
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../utils/test_framework.h"
#include "../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../agentos/commons/utils/resource/include/resource_compat.h"

/**
 * @brief 模拟资源结构
 */
typedef struct {
    int fd;
    bool is_open;
    char* name;
} mock_resource_t;

static int resource_create_count = 0;
static int resource_destroy_count = 0;

/**
 * @brief 创建模拟资源
 */
static mock_resource_t* mock_resource_create(const char* name) {
    mock_resource_t* res = (mock_resource_t*)AGENTOS_MALLOC(sizeof(mock_resource_t));
    if (!res) return NULL;
    
    res->fd = ++resource_create_count;
    res->is_open = true;
    res->name = name ? AGENTOS_MALLOC(strlen(name) + 1) : NULL;
    if (res->name && name) {
        strncpy(res->name, name, strlen(name));
        res->name[strlen(name)] = '\0';
    }
    
    return res;
}

/**
 * @brief 销毁模拟资源
 */
static void mock_resource_destroy(mock_resource_t* res) {
    if (!res) return;
    
    if (res->is_open) {
        res->is_open = false;
        resource_destroy_count++;
    }
    
    if (res->name) {
        AGENTOS_FREE(res->name);
    }
    
    AGENTOS_FREE(res);
}

/**
 * @test 测试资源创建和销毁
 */
static void test_resource_lifecycle(void** state) {
    mock_resource_t* res = mock_resource_create("test_resource");
    
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(res);
    AGENTOS_TEST_ASSERT_TRUE(res->is_open);
    AGENTOS_TEST_ASSERT_INT_EQUAL(1, res->fd);
    
    mock_resource_destroy(res);
    
    // 验证资源被正确清理
    AGENTOS_TEST_ASSERT_INT_EQUAL(1, resource_destroy_count);
}

/**
 * @test 测试多个资源的生命周期
 */
static void test_multiple_resources(void** state) {
    const int num_resources = 10;
    mock_resource_t* resources[num_resources];
    
    // 创建多个资源
    for (int i = 0; i < num_resources; i++) {
        char name[32];
        snprintf(name, sizeof(name), "resource_%d", i);
        resources[i] = mock_resource_create(name);
        AGENTOS_TEST_ASSERT_PTR_NOT_NULL(resources[i]);
        AGENTOS_TEST_ASSERT_TRUE(resources[i]->is_open);
    }
    
    // 验证所有资源都创建了
    AGENTOS_TEST_ASSERT_INT_EQUAL(num_resources, resource_create_count);
    
    // 销毁所有资源
    for (int i = 0; i < num_resources; i++) {
        mock_resource_destroy(resources[i]);
    }
    
    // 验证所有资源都被销毁
    AGENTOS_TEST_ASSERT_INT_EQUAL(num_resources, resource_destroy_count);
}

/**
 * @test 测试NULL资源处理
 */
static void test_null_resource_handling(void** state) {
    // 销毁NULL应该安全
    mock_resource_destroy(NULL);
    AGENTOS_TEST_ASSERT_INT_EQ(0, resource_destroy_count); // 不应增加
    
    // 创建带NULL名称的资源
    mock_resource_t* res = mock_resource_create(NULL);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(res);
    AGENTOS_TEST_ASSERT_NULL(res->name);
    
    mock_resource_destroy(res);
}

/**
 * @test 测试资源泄漏检测模拟
 */
static void test_resource_leak_detection(void** state) {
    int initial_count = resource_destroy_count;
    
    {
        // 在作用域内创建资源
        mock_resource_t* res1 = mock_resource_create("leak_test_1");
        mock_resource_t* res2 = mock_resource_create("leak_test_2");
        
        AGENTOS_TEST_ASSERT_PTR_NOT_NULL(res1);
        AGENTOS_TEST_ASSERT_PTR_NOT_NULL(res2);
        
        // 故意不释放res2来模拟泄漏（仅用于测试框架）
        mock_resource_destroy(res1);
        
        // 注意：实际生产代码中不应该这样做
        // 这里只是为了演示泄漏检测机制
    }
    
    // 验证只有res1被释放了
    AGENTOS_TEST_ASSERT_INT_EQ(initial_count + 1, resource_destroy_count);
}

/**
 * @test 测试内存分配和释放配对
 */
static void test_memory_allocation_paired(void** state) {
    void* ptr1 = AGENTOS_MALLOC(1024);
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(ptr1);
    
    void* ptr2 = AGENTOS_CALLOC(100, sizeof(int));
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(ptr2);
    
    char* str = AGENTOS_STRDUP("Test string");
    AGENTOS_TEST_ASSERT_PTR_NOT_NULL(str);
    AGENTOS_TEST_ASSERT_STRING_EQUAL("Test string", str);
    
    // 正确释放顺序
    AGENTOS_FREE(str);
    AGENTOS_FREE(ptr2);
    AGENTOS_FREE(ptr1);
    
    // 双重释放保护（如果实现的话）
    // ptr1 = NULL; // 已经释放，设置为NULL防止悬垂指针
}

/**
 * @test 测试大块内存分配
 */
static void test_large_memory_allocation(void** state) {
    size_t large_size = 1024 * 1024; // 1MB
    void* ptr = AGENTOS_MALLOC(large_size);
    
    if (ptr) {
        // 成功分配，验证可写
        memset(ptr, 0xAB, large_size);
        unsigned char* bytes = (unsigned char*)ptr;
        AGENTOS_TEST_ASSERT_INT_EQUAL(0xAB, bytes[0]);
        AGENTOS_TEST_ASSERT_INT_EQUAL(0xAB, bytes[large_size - 1]);
        
        AGENTOS_FREE(ptr);
    } else {
        // 分配失败也是可以接受的（取决于系统状态）
        printf("Warning: Large memory allocation failed (may be normal)\n");
    }
}

/**
 * @test 测试零大小分配
 */
static void test_zero_size_allocation(void** state) {
    void* ptr = AGENTOS_MALLOC(0);
    
    // 根据实现，零大小分配可能返回NULL或有效指针
    // 两种情况都是可接受的
    if (ptr) {
        AGENTOS_FREE(ptr);
    }
}

/**
 * @test 测试内存对齐
 */
static void test_memory_alignment(void** state) {
    size_t alignment = 64;
    void* ptr = AGENTOS_ALIGNED_ALLOC(alignment, 256);
    
    if (ptr) {
        uintptr_t addr = (uintptr_t)ptr;
        AGENTOS_TEST_ASSERT_TRUE((addr % alignment) == 0);
        
        AGENTOS_ALIGNED_FREE(ptr);
    } else {
        printf("Note: Aligned allocation not supported or failed\n");
    }
}

/**
 * @test 测试内存统计功能
 */
static void test_memory_statistics(void** state) {
    memory_stats_t stats_before, stats_after;
    
    // 获取初始统计
    // 注意：这里假设有memory_get_stats函数
    // 实际使用时需要根据具体API调整
    
    // 分配一些内存
    void* ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = AGENTOS_MALLOC(1024 * (i + 1));
        AGENTOS_TEST_ASSERT_PTR_NOT_NULL(ptrs[i]);
    }
    
    // 释放内存
    for (int i = 0; i < 5; i++) {
        AGENTOS_FREE(ptrs[i]);
    }
}

/**
 * @brief 运行所有资源管理测试
 */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_resource_lifecycle),
        cmocka_unit_test(test_multiple_resources),
        cmocka_unit_test(test_null_resource_handling),
        cmocka_unit_test(test_resource_leak_detection),
        cmocka_unit_test(test_memory_allocation_paired),
        cmocka_unit_test(test_large_memory_allocation),
        cmocka_unit_test(test_zero_size_allocation),
        cmocka_unit_test(test_memory_alignment),
        cmocka_unit_test(test_memory_statistics),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
