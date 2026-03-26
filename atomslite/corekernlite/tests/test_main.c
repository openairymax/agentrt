/**
 * @file test_main.c
 * @brief AgentOS Lite 单元测试主程序
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/agentos_lite.h"
#include <stdio.h>
#include <string.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("[FAIL] %s:%d - %s\n", __FILE__, __LINE__, message); \
            g_tests_failed++; \
            return; \
        } \
        printf("[PASS] %s\n", message); \
        g_tests_passed++; \
    } while(0)

static void test_error_handling(void) {
    printf("\n=== Testing Error Handling ===\n");
    
    const char* msg = agentos_lite_strerror(AGENTOS_LITE_SUCCESS);
    TEST_ASSERT(msg != NULL, "Error message for SUCCESS");
    
    msg = agentos_lite_strerror(AGENTOS_LITE_ENOMEM);
    TEST_ASSERT(msg != NULL, "Error message for ENOMEM");
    
    msg = agentos_lite_strerror(AGENTOS_LITE_ETIMEDOUT);
    TEST_ASSERT(msg != NULL, "Error message for ETIMEDOUT");
    
    msg = agentos_lite_strerror(-999);
    TEST_ASSERT(msg != NULL, "Error message for unknown error");
    
    TEST_ASSERT(agentos_lite_is_success(AGENTOS_LITE_SUCCESS) == 1, "is_success for SUCCESS");
    TEST_ASSERT(agentos_lite_is_success(AGENTOS_LITE_EINVAL) == 0, "is_success for EINVAL");
    TEST_ASSERT(agentos_lite_is_error(AGENTOS_LITE_EINVAL) == 1, "is_error for EINVAL");
    TEST_ASSERT(agentos_lite_is_error(AGENTOS_LITE_SUCCESS) == 0, "is_error for SUCCESS");
}

static void test_memory_management(void) {
    printf("\n=== Testing Memory Management ===\n");
    
    agentos_lite_error_t err = agentos_lite_mem_init(0);
    TEST_ASSERT(err == AGENTOS_LITE_SUCCESS, "Memory subsystem initialization");
    
    void* ptr = agentos_lite_mem_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Allocate 1024 bytes");
    if (ptr) {
        memset(ptr, 0xAA, 1024);
        agentos_lite_mem_free(ptr);
        TEST_ASSERT(1, "Free allocated memory");
    }
    
    ptr = agentos_lite_mem_aligned_alloc(4096, 64);
    TEST_ASSERT(ptr != NULL, "Allocate aligned memory");
    if (ptr) {
        TEST_ASSERT(((size_t)ptr & 63) == 0, "Memory is 64-byte aligned");
        agentos_lite_mem_aligned_free(ptr);
        TEST_ASSERT(1, "Free aligned memory");
    }
    
    size_t total, used, peak;
    agentos_lite_mem_stats(&total, &used, &peak);
    printf("  Memory stats: total=%zu, used=%zu, peak=%zu\n", total, used, peak);
    TEST_ASSERT(1, "Get memory statistics");
    
    size_t leaks = agentos_lite_mem_check_leaks();
    printf("  Memory leaks: %zu blocks\n", leaks);
    TEST_ASSERT(1, "Check memory leaks");
    
    agentos_lite_mem_cleanup();
    TEST_ASSERT(1, "Memory subsystem cleanup");
}

static void test_mutex_operations(void) {
    printf("\n=== Testing Mutex Operations ===\n");
    
    agentos_lite_mutex_t* mutex = agentos_lite_mutex_create();
    TEST_ASSERT(mutex != NULL, "Create mutex");
    
    if (mutex) {
        agentos_lite_mutex_lock(mutex);
        TEST_ASSERT(1, "Lock mutex");
        
        int try_result = agentos_lite_mutex_trylock(mutex);
        TEST_ASSERT(try_result != 0, "Trylock on locked mutex fails");
        
        agentos_lite_mutex_unlock(mutex);
        TEST_ASSERT(1, "Unlock mutex");
        
        try_result = agentos_lite_mutex_trylock(mutex);
        TEST_ASSERT(try_result == 0, "Trylock on unlocked mutex succeeds");
        agentos_lite_mutex_unlock(mutex);
        
        agentos_lite_mutex_destroy(mutex);
        TEST_ASSERT(1, "Destroy mutex");
    }
}

static void test_cond_operations(void) {
    printf("\n=== Testing Condition Variable Operations ===\n");
    
    agentos_lite_cond_t* cond = agentos_lite_cond_create();
    TEST_ASSERT(cond != NULL, "Create condition variable");
    
    agentos_lite_mutex_t* mutex = agentos_lite_mutex_create();
    TEST_ASSERT(mutex != NULL, "Create mutex for cond");
    
    if (cond && mutex) {
        agentos_lite_mutex_lock(mutex);
        TEST_ASSERT(1, "Lock mutex for timed wait");
        
        uint64_t start = agentos_lite_time_get_ms();
        agentos_lite_error_t err = agentos_lite_cond_wait(cond, mutex, 100);
        uint64_t elapsed = agentos_lite_time_get_ms() - start;
        
        TEST_ASSERT(err == AGENTOS_LITE_ETIMEDOUT, "Timed wait times out");
        TEST_ASSERT(elapsed >= 90 && elapsed <= 150, "Timeout approximately correct");
        
        agentos_lite_cond_signal(cond);
        TEST_ASSERT(1, "Signal condition variable");
        
        agentos_lite_cond_broadcast(cond);
        TEST_ASSERT(1, "Broadcast condition variable");
        
        agentos_lite_mutex_unlock(mutex);
        
        agentos_lite_cond_destroy(cond);
        agentos_lite_mutex_destroy(mutex);
        TEST_ASSERT(1, "Destroy condition variable and mutex");
    }
}

static int g_thread_test_counter = 0;
static agentos_lite_mutex_t* g_thread_test_mutex = NULL;

static void thread_func(void* arg) {
    int* value = (int*)arg;
    
    if (g_thread_test_mutex) {
        agentos_lite_mutex_lock(g_thread_test_mutex);
        g_thread_test_counter++;
        if (value) *value = 42;
        agentos_lite_mutex_unlock(g_thread_test_mutex);
    }
    
    agentos_lite_task_sleep(10);
}

static void test_thread_operations(void) {
    printf("\n=== Testing Thread Operations ===\n");
    
    g_thread_test_mutex = agentos_lite_mutex_create();
    TEST_ASSERT(g_thread_test_mutex != NULL, "Create mutex for thread test");
    
    int test_value = 0;
    g_thread_test_counter = 0;
    
    agentos_lite_thread_t thread;
    agentos_lite_thread_attr_t attr = {
        .name = "test_thread",
        .priority = AGENTOS_LITE_TASK_PRIORITY_NORMAL,
        .stack_size = 0
    };
    
    agentos_lite_error_t err = agentos_lite_thread_create(&thread, &attr, thread_func, &test_value);
    TEST_ASSERT(err == AGENTOS_LITE_SUCCESS, "Create thread");
    
    if (err == AGENTOS_LITE_SUCCESS) {
        err = agentos_lite_thread_join(thread);
        TEST_ASSERT(err == AGENTOS_LITE_SUCCESS, "Join thread");
        
        TEST_ASSERT(test_value == 42, "Thread modified value correctly");
        TEST_ASSERT(g_thread_test_counter == 1, "Thread executed once");
    }
    

    
    if (g_thread_test_mutex) {
        agentos_lite_mutex_destroy(g_thread_test_mutex);
        g_thread_test_mutex = NULL;
    }
}

static void test_time_operations(void) {
    printf("\n=== Testing Time Operations ===\n");
    
    uint64_t ns = agentos_lite_time_get_ns();
    TEST_ASSERT(ns > 0, "Get nanoseconds");
    printf("  Nanoseconds: %llu\n", (unsigned long long)ns);
    
    uint64_t us = agentos_lite_time_get_us();
    TEST_ASSERT(us > 0, "Get microseconds");
    printf("  Microseconds: %llu\n", (unsigned long long)us);
    
    uint64_t ms = agentos_lite_time_get_ms();
    TEST_ASSERT(ms > 0, "Get milliseconds");
    printf("  Milliseconds: %llu\n", (unsigned long long)ms);
    
    uint64_t unix_ts = agentos_lite_time_get_unix();
    TEST_ASSERT(unix_ts > 1700000000ULL, "Get Unix timestamp");
    printf("  Unix timestamp: %llu\n", (unsigned long long)unix_ts);
    
    uint64_t start = agentos_lite_time_get_ns();
    agentos_lite_task_sleep(10);
    uint64_t end = agentos_lite_time_get_ns();
    
    uint64_t diff = agentos_lite_time_diff_ns(start, end);
    TEST_ASSERT(diff >= 10000000ULL, "Time difference calculation");
    printf("  Sleep duration: %llu ns\n", (unsigned long long)diff);
    
    TEST_ASSERT(1, "Time operations completed successfully");
}

static void test_kernel_lifecycle(void) {
    printf("\n=== Testing Kernel Lifecycle ===\n");
    
    agentos_lite_error_t err = agentos_lite_init();
    TEST_ASSERT(err == AGENTOS_LITE_SUCCESS, "Initialize kernel");
    
    const char* version = agentos_lite_version();
    TEST_ASSERT(version != NULL, "Get version string");
    printf("  Version: %s\n", version ? version : "NULL");
    
    agentos_lite_cleanup();
    TEST_ASSERT(1, "Cleanup kernel");
    
    err = agentos_lite_init();
    TEST_ASSERT(err == AGENTOS_LITE_SUCCESS, "Reinitialize kernel");
    agentos_lite_cleanup();
    TEST_ASSERT(1, "Cleanup after reinit");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("  AgentOS Lite Unit Tests\n");
    printf("========================================\n");
    
    test_kernel_lifecycle();
    test_error_handling();
    test_memory_management();
    test_mutex_operations();
    test_cond_operations();
    test_thread_operations();
    test_time_operations();
    
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("========================================\n");
    
    return g_tests_failed;
}
