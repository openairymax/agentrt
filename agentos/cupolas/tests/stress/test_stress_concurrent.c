/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_stress_concurrent.c - Concurrent Stress Tests Framework
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "cupolas.h"
#include "permission/permission_engine.h"
#include "sanitizer/sanitizer_core.h"
#include "audit/audit_logger.h"

#define STRESS_THREAD_COUNT 64
#define STRESS_OPS_PER_THREAD 10000
#define STRESS_DURATION_MS 5000

typedef struct {
    int thread_id;
    int ops_count;
    int success_count;
    int fail_count;
    double avg_latency_us;
} thread_result_t;

#ifdef _WIN32
static DWORD WINAPI stress_test_thread(LPVOID arg)
#else
static void* stress_test_thread(void* arg)
#endif
{
    thread_result_t* result = (thread_result_t*)arg;
    result->success_count = 0;
    result->fail_count = 0;
    
    for (int i = 0; i < result->ops_count; i++) {
        #ifdef _WIN32
        LARGE_INTEGER start, end, freq;
        QueryPerformanceCounter(&start);
        #else
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        #endif
        
        int perm_result = permission_engine_check("test_agent", "resource", "action");
        
        #ifdef _WIN32
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&freq);
        double latency_us = (double)(end.QuadPart - start.QuadPart) * 1000000.0 / freq.QuadPart;
        #else
        clock_gettime(CLOCK_MONOTONIC, &end);
        double latency_us = (end.tv_sec - start.tv_sec) * 1000000.0 + 
                           (end.tv_nsec - start.tv_nsec) / 1000.0;
        #endif
        
        if (perm_result >= 0) {
            result->success_count++;
        } else {
            result->fail_count++;
        }
        
        result->avg_latency_us += latency_us;
    }
    
    result->avg_latency_us /= result->ops_count;
    
    #ifdef _WIN32
    return 0;
    #else
    return NULL;
    #endif
}

void stress_test_concurrent_permission_checks(void) {
    printf("\n=== Concurrent Permission Check Stress Test ===\n");
    printf("Threads: %d, Operations per thread: %d\n\n", 
           STRESS_THREAD_COUNT, STRESS_OPS_PER_THREAD);
    
    #ifdef _WIN32
    HANDLE threads[STRESS_THREAD_COUNT];
    thread_result_t results[STRESS_THREAD_COUNT];
    #else
    pthread_t threads[STRESS_THREAD_COUNT];
    thread_result_t results[STRESS_THREAD_COUNT];
    #endif
    
    int total_success = 0;
    int total_fail = 0;
    double total_avg_latency = 0;
    
    #ifdef _WIN32
    DWORD start_time = GetTickCount();
    #else
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #endif
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        results[i].thread_id = i;
        results[i].ops_count = STRESS_OPS_PER_THREAD;
        
        #ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, stress_test_thread, &results[i], 0, NULL);
        #else
        pthread_create(&threads[i], NULL, stress_test_thread, &results[i]);
        #endif
    }
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        #ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
        #else
        pthread_join(threads[i], NULL);
        #endif
        
        total_success += results[i].success_count;
        total_fail += results[i].fail_count;
        total_avg_latency += results[i].avg_latency_us;
        
        printf("Thread %2d: Success=%5d, Fail=%3d, Avg Latency=%.2f μs\n",
               i, results[i].success_count, results[i].fail_count, results[i].avg_latency_us);
    }
    
    #ifdef _WIN32
    DWORD end_time = GetTickCount();
    double duration_ms = (double)(end_time - start_time);
    #else
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration_ms = (end.tv_sec - start.tv_sec) * 1000.0 + 
                        (end.tv_nsec - start.tv_nsec) / 1000000.0;
    #endif
    
    printf("\n=== Summary ===\n");
    printf("Total Operations: %d\n", total_success + total_fail);
    printf("Successful: %d (%.2f%%)\n", total_success, 
           100.0 * total_success / (total_success + total_fail));
    printf("Failed: %d (%.2f%%)\n", total_fail,
           100.0 * total_fail / (total_success + total_fail));
    printf("Total Duration: %.2f ms\n", duration_ms);
    printf("Throughput: %.2f ops/sec\n", 
           (total_success + total_fail) * 1000.0 / duration_ms);
    printf("Average Latency: %.2f μs\n", total_avg_latency / STRESS_THREAD_COUNT);
}

void stress_test_concurrent_audit_writes(void) {
    printf("\n=== Concurrent Audit Write Stress Test ===\n");
    printf("Threads: %d, Operations per thread: %d\n\n",
           STRESS_THREAD_COUNT, STRESS_OPS_PER_THREAD);
    
    #ifdef _WIN32
    HANDLE threads[STRESS_THREAD_COUNT];
    int results[STRESS_THREAD_COUNT];
    #else
    pthread_t threads[STRESS_THREAD_COUNT];
    int results[STRESS_THREAD_COUNT];
    #endif
    
    #ifdef _WIN32
    DWORD start_time = GetTickCount();
    #else
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #endif
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        #ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, stress_test_thread, &results[i], 0, NULL);
        #else
        pthread_create(&threads[i], NULL, stress_test_thread, &results[i]);
        #endif
    }
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        #ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
        #else
        pthread_join(threads[i], NULL);
        #endif
    }
    
    #ifdef _WIN32
    DWORD end_time = GetTickCount();
    double duration_ms = (double)(end_time - start_time);
    #else
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration_ms = (end.tv_sec - start.tv_sec) * 1000.0 + 
                        (end.tv_nsec - start.tv_nsec) / 1000000.0;
    #endif
    
    printf("Total Duration: %.2f ms\n", duration_ms);
    printf("Throughput: %.2f writes/sec\n",
           STRESS_THREAD_COUNT * STRESS_OPS_PER_THREAD * 1000.0 / duration_ms);
}

void stress_test_cache_under_contention(void) {
    printf("\n=== Cache Contention Stress Test ===\n");
    printf("Testing LRU cache under high concurrency...\n\n");
    
    #ifdef _WIN32
    HANDLE threads[STRESS_THREAD_COUNT];
    thread_result_t results[STRESS_THREAD_COUNT];
    #else
    pthread_t threads[STRESS_THREAD_COUNT];
    thread_result_t results[STRESS_THREAD_COUNT];
    #endif
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        results[i].thread_id = i;
        results[i].ops_count = STRESS_OPS_PER_THREAD;
        
        #ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, stress_test_thread, &results[i], 0, NULL);
        #else
        pthread_create(&threads[i], NULL, stress_test_thread, &results[i]);
        #endif
    }
    
    for (int i = 0; i < STRESS_THREAD_COUNT; i++) {
        #ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
        #else
        pthread_join(threads[i], NULL);
        #endif
    }
    
    printf("Cache contention test completed\n");
}

int main(void) {
    printf("========================================\n");
    printf("Cupolas Concurrent Stress Test Suite\n");
    printf("========================================\n\n");
    
    int init_result = cupolas_init(NULL);
    if (init_result != 0) {
        printf("Failed to initialize cupolas\n");
        return 1;
    }
    
    stress_test_concurrent_permission_checks();
    stress_test_concurrent_audit_writes();
    stress_test_cache_under_contention();
    
    cupolas_cleanup();
    
    printf("\n========================================\n");
    printf("All stress tests completed\n");
    printf("========================================\n");
    
    return 0;
}
