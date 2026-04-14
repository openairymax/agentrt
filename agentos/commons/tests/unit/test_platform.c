/**
 * @file test_platform.c
 * @brief platform.h 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>
#include <assert.h>

#include "platform.h"

/* ==================== 测试辅助�?==================== */

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

/* ==================== 测试用例 ==================== */

/**
 * @brief 测试平台检测宏
 */
static int test_platform_detection(void) {
#ifdef AGENTOS_PLATFORM_WINDOWS
    printf("  Platform: Windows (%d-bit)\n", AGENTOS_PLATFORM_BITS);
    TEST_ASSERT(AGENTOS_PLATFORM_WINDOWS == 1, "Platform should be Windows");
    TEST_ASSERT(strcmp(AGENTOS_PLATFORM_NAME, "Windows") == 0, "Platform name should be Windows");
#else
    printf("  Platform: POSIX\n");
#endif
    return 0;
}

/**
 * @brief 测试时间函数
 */
static int test_time_functions(void) {
    uint64_t time1 = agentos_time_ns();
    
    /* 等待一小段时间 */
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec ts = {0, 10000000}; /* 10ms */
    nanosleep(&ts, NULL);
#endif
    
    uint64_t time2 = agentos_time_ns();
    
    TEST_ASSERT(time2 > time1, "Time should increase");
    TEST_ASSERT((time2 - time1) >= 5000000, "Time difference should be at least 5ms");
    
    printf("  Time elapsed: %llu ns\n", (unsigned long long)(time2 - time1));
    return 0;
}

/**
 * @brief 测试内存分配函数
 */
static int test_memory_allocation(void) {
    void* ptr1 = agentos_mem_alloc(1024);
    TEST_ASSERT(ptr1 != NULL, "Memory allocation should succeed");
    
    /* 测试重复分配 */
    void* ptr2 = agentos_mem_alloc(512);
    TEST_ASSERT(ptr2 != NULL, "Second allocation should succeed");
    
    /* 测试零大小分�?*/
    void* ptr3 = agentos_mem_alloc(0);
    TEST_ASSERT(ptr3 == NULL, "Zero-size allocation should return NULL");
    
    /* 清理 */
    agentos_mem_free(ptr1);
    agentos_mem_free(ptr2);
    
    printf("  Memory allocation: OK\n");
    return 0;
}

/**
 * @brief 测试字符串函�?
 */
static int test_string_functions(void) {
    char buffer[64];
    
    /* 测试 agentos_strlcpy */
    const char* src = "Hello, World!";
    char* result = agentos_strlcpy(buffer, sizeof(buffer), src);
    TEST_ASSERT(result == buffer, "strlcpy should return buffer");
    TEST_ASSERT(strcmp(buffer, src) == 0, "String should be copied correctly");
    
    /* 测试截断 */
    char small_buffer[8];
    agentos_strlcpy(small_buffer, sizeof(small_buffer), "This is a long string");
    TEST_ASSERT(strlen(small_buffer) < sizeof(small_buffer), "String should be truncated");
    TEST_ASSERT(small_buffer[sizeof(small_buffer) - 1] == '\0', "String should be null-terminated");
    
    /* 测试 agentos_strlcat */
    char concat_buffer[32] = "Hello";
    agentos_strlcat(concat_buffer, sizeof(concat_buffer), ", World!");
    TEST_ASSERT(strcmp(concat_buffer, "Hello, World!") == 0, "String should be concatenated");
    
    printf("  String functions: OK\n");
    return 0;
}

/**
 * @brief 测试文件操作函数
 */
static int test_file_operations(void) {
    /* 测试文件大小获取 */
    const char* test_file = "test_temp_file.txt";
    FILE* f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "Test content");
        fclose(f);
        
        int64_t size = agentos_file_size(test_file);
        TEST_ASSERT(size > 0, "File size should be positive");
        
        /* 清理 */
        remove(test_file);
        
        printf("  File operations: OK (size=%lld bytes)\n", (long long)size);
    } else {
        printf("  File operations: Skipped (cannot create test file)\n");
    }
    
    return 0;
}

/**
 * @brief 测试线程原语
 */
static int test_thread_primitives(void) {
    agentos_mutex_t mutex = AGENTOS_INVALID_MUTEX;
    
    /* 测试互斥锁初始化 */
    int ret = agentos_mutex_init(&mutex);
    TEST_ASSERT(ret == 0, "Mutex initialization should succeed");
    TEST_ASSERT(mutex != AGENTOS_INVALID_MUTEX, "Mutex should be valid after init");
    
    /* 测试加锁解锁 */
    ret = agentos_mutex_lock(&mutex);
    TEST_ASSERT(ret == 0, "Mutex lock should succeed");
    
    ret = agentos_mutex_unlock(&mutex);
    TEST_ASSERT(ret == 0, "Mutex unlock should succeed");
    
    /* 清理 */
    ret = agentos_mutex_destroy(&mutex);
    TEST_ASSERT(ret == 0, "Mutex destroy should succeed");
    
    printf("  Thread primitives: OK\n");
    return 0;
}

/**
 * @brief 测试网络函数
 */
static int test_network_functions(void) {
    /* 测试网络初始�?*/
    int ret = agentos_network_init();
    TEST_ASSERT(ret == 0 || ret == AGENTOS_ERR_ALREADY_EXISTS, 
                "Network initialization should succeed or already exist");
    
    /* 测试忽略 SIGPIPE */
    ret = agentos_ignore_sigpipe();
    TEST_ASSERT(ret == 0, "Ignore SIGPIPE should succeed");
    
    /* 清理 */
    agentos_network_cleanup();
    
    printf("  Network functions: OK\n");
    return 0;
}

/* ==================== 主函�?==================== */

int main(void) {
    printf("===========================================\n");
    printf("  agentos/commons/platform 单元测试\n");
    printf("===========================================\n\n");
    
    TEST_RUN(test_platform_detection);
    TEST_RUN(test_time_functions);
    TEST_RUN(test_memory_allocation);
    TEST_RUN(test_string_functions);
    TEST_RUN(test_file_operations);
    TEST_RUN(test_thread_primitives);
    TEST_RUN(test_network_functions);
    
    printf("\n===========================================\n");
    printf("  测试结果�?d 通过�?d 失败\n", passed_tests, failed_tests);
    printf("===========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}
