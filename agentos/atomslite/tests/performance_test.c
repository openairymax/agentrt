/**
 * @file performance_test.c
 * @brief atomsmini模块性能测试
 * @date 2026-03-26
 * @copyright (c) 2026, AgentOS Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "agentos_coreloopthreelite.h"
#include "agentos_memoryrovollite.h"

/* 性能测试配置 */
#define NUM_TASKS_PER_TEST 1000
#define NUM_MEMORY_ITEMS_PER_TEST 500
#define NUM_VECTOR_SEARCHES_PER_TEST 100

/* 内存使用测量辅助函数 */
static size_t get_current_memory_usage(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    /* Linux/Unix 实现 */
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) {
        return 0;
    }
    long size, resident, share, text, lib, data, dt;
    if (fscanf(f, "%ld %ld %ld %ld %ld %ld %ld", &size, &resident, &share, &text, &lib, &data, &dt) != 7) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (size_t)resident * sysconf(_SC_PAGESIZE);
#endif
}

/**
 * @brief 测试coreloopthreelite性能
 */
static void test_coreloopthreelite_performance(void) {
    printf("\n=== CoreLoopThreeLite Performance Test ===\n");
    
    agentos_clt_error_t err;
    agentos_clt_engine_handle_t* engine = NULL;
    
    /* 测量初始化时间 */
    clock_t start = clock();
    err = agentos_clt_engine_init(&engine, 4); /* 4个线程 */
    clock_t end = clock();
    double init_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != AGENTOS_CLT_SUCCESS) {
        printf("Failed to initialize engine: %s\n", agentos_clt_error_str(err));
        return;
    }
    
    printf("Engine initialization time: %.3f seconds\n", init_time);
    
    /* 测量初始内存使用 */
    size_t initial_memory = get_current_memory_usage();
    
    /* 批量提交任务 */
    start = clock();
    
    for (int i = 0; i < NUM_TASKS_PER_TEST; i++) {
        char task_data[64];
        snprintf(task_data, sizeof(task_data), "Performance test task %d", i);
        
        agentos_clt_task_id_t task_id;
        err = agentos_clt_task_submit(engine, task_data, strlen(task_data), &task_id);
        
        if (err != AGENTOS_CLT_SUCCESS) {
            printf("Failed to submit task %d: %s\n", i, agentos_clt_error_str(err));
            break;
        }
    }
    
    end = clock();
    double submit_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double tasks_per_second = NUM_TASKS_PER_TEST / submit_time;
    
    printf("Task submission performance: %.0f tasks/sec (%.3f seconds for %d tasks)\n",
           tasks_per_second, submit_time, NUM_TASKS_PER_TEST);
    
    /* 等待所有任务完成 */
    start = clock();
    err = agentos_clt_engine_cleanup(&engine);
    end = clock();
    double cleanup_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Engine cleanup time: %.3f seconds\n", cleanup_time);
    
    /* 测量峰值内存使用 */
    size_t peak_memory = get_current_memory_usage();
    size_t memory_increase = peak_memory - initial_memory;
    
    printf("Memory usage: initial=%.2f MB, peak=%.2f MB, increase=%.2f MB\n",
           initial_memory / (1024.0 * 1024.0),
           peak_memory / (1024.0 * 1024.0),
           memory_increase / (1024.0 * 1024.0));
    
    /* 性能目标：每秒至少处理5000个任务 */
    if (tasks_per_second < 5000.0) {
        printf("WARNING: Task submission performance (%.0f tasks/sec) below target (5000 tasks/sec)\n",
               tasks_per_second);
    } else {
        printf("SUCCESS: Task submission performance meets target (%.0f tasks/sec >= 5000 tasks/sec)\n",
               tasks_per_second);
    }
}

/**
 * @brief 测试memoryrovollite性能
 */
static void test_memoryrovollite_performance(void) {
    printf("\n=== MemoryRovolLite Performance Test ===\n");
    
    agentos_mrl_error_t err;
    agentos_mrl_storage_handle_t* storage = NULL;
    
    /* 测量初始化时间 */
    clock_t start = clock();
    err = agentos_mrl_storage_create(&storage, AGENTOS_MRL_MEMORY_TYPE, 
                                    NUM_MEMORY_ITEMS_PER_TEST * 2, "perf_test_db");
    clock_t end = clock();
    double init_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (err != AGENTOS_MRL_SUCCESS) {
        printf("Failed to create storage: %s\n", agentos_mrl_error_str(err));
        return;
    }
    
    printf("Storage initialization time: %.3f seconds\n", init_time);
    
    /* 测量初始内存使用 */
    size_t initial_memory = get_current_memory_usage();
    
    /* 批量插入测试 */
    start = clock();
    
    for (int i = 0; i < NUM_MEMORY_ITEMS_PER_TEST; i++) {
        agentos_mrl_item_metadata_t metadata;
        memset(&metadata, 0, sizeof(metadata));
        metadata.id = i + 1;
        metadata.importance = 0.5f + (i % 10) * 0.05f;
        
        char content[64];
        snprintf(content, sizeof(content), "Performance test memory item %d", i);
        
        /* 创建简单的向量数据 */
        float vector_data[16];
        for (int j = 0; j < 16; j++) {
            vector_data[j] = (float)(i + j) / 100.0f;
        }
        
        agentos_mrl_item_handle_t* item_handle = NULL;
        err = agentos_mrl_item_save(
            storage,
            "perf_type",
            content,
            &metadata,
            vector_data,
            16,
            NULL,
            0,
            &item_handle
        );
        
        if (err != AGENTOS_MRL_SUCCESS) {
            printf("Failed to save item %d: %s\n", i, agentos_mrl_error_str(err));
            break;
        }
    }
    
    end = clock();
    double insert_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double inserts_per_second = NUM_MEMORY_ITEMS_PER_TEST / insert_time;
    
    printf("Insert performance: %.0f inserts/sec (%.3f seconds for %d inserts)\n",
           inserts_per_second, insert_time, NUM_MEMORY_ITEMS_PER_TEST);
    
    /* 向量搜索性能测试 */
    start = clock();
    
    for (int i = 0; i < NUM_VECTOR_SEARCHES_PER_TEST; i++) {
        float query_vector[16];
        for (int j = 0; j < 16; j++) {
            query_vector[j] = (float)(i + j) / 100.0f;
        }
        
        agentos_mrl_search_result_t results[5];
        size_t num_results = 0;
        
        err = agentos_mrl_search_by_vector(
            storage,
            query_vector,
            16,
            0.7f,
            results,
            5,
            &num_results
        );
        
        if (err != AGENTOS_MRL_SUCCESS && err != AGENTOS_MRL_NO_MATCH) {
            printf("Failed to search vector %d: %s\n", i, agentos_mrl_error_str(err));
            break;
        }
    }
    
    end = clock();
    double search_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double searches_per_second = NUM_VECTOR_SEARCHES_PER_TEST / search_time;
    
    printf("Vector search performance: %.0f searches/sec (%.3f seconds for %d searches)\n",
           searches_per_second, search_time, NUM_VECTOR_SEARCHES_PER_TEST);
    
    /* 测量峰值内存使用 */
    size_t peak_memory = get_current_memory_usage();
    size_t memory_increase = peak_memory - initial_memory;
    double memory_per_item = memory_increase / (double)NUM_MEMORY_ITEMS_PER_TEST;
    
    printf("Memory usage: initial=%.2f MB, peak=%.2f MB, increase=%.2f MB (%.1f KB/item)\n",
           initial_memory / (1024.0 * 1024.0),
           peak_memory / (1024.0 * 1024.0),
           memory_increase / (1024.0 * 1024.0),
           memory_per_item / 1024.0);
    
    /* 清理存储 */
    start = clock();
    err = agentos_mrl_storage_destroy(&storage);
    end = clock();
    double destroy_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Storage destruction time: %.3f seconds\n", destroy_time);
    
    /* 性能目标 */
    printf("\nPerformance Targets:\n");
    printf("- Insert performance target: >1000 inserts/sec (actual: %.0f) %s\n",
           inserts_per_second, inserts_per_second > 1000.0 ? "[PASS]" : "[FAIL]");
    printf("- Search performance target: >500 searches/sec (actual: %.0f) %s\n",
           searches_per_second, searches_per_second > 500.0 ? "[PASS]" : "[FAIL]");
    printf("- Memory efficiency target: <2 KB/item (actual: %.1f KB) %s\n",
           memory_per_item / 1024.0, memory_per_item < 2.0 * 1024.0 ? "[PASS]" : "[FAIL]");
}

/**
 * @brief 主性能测试函数
 */
int main(void) {
    printf("=========================================\n");
    printf("    AgentOS atomsmini Performance Test    \n");
    printf("=========================================\n");
    
    /* 记录整体开始时间 */
    clock_t overall_start = clock();
    size_t overall_initial_memory = get_current_memory_usage();
    
    /* 运行子模块性能测试 */
    test_coreloopthreelite_performance();
    test_memoryrovollite_performance();
    
    /* 记录整体结束时间 */
    clock_t overall_end = clock();
    size_t overall_peak_memory = get_current_memory_usage();
    
    double overall_time = ((double)(overall_end - overall_start)) / CLOCKS_PER_SEC;
    size_t overall_memory_increase = overall_peak_memory - overall_initial_memory;
    
    printf("\n=========================================\n");
    printf("Overall Performance Summary:\n");
    printf("- Total test time: %.3f seconds\n", overall_time);
    printf("- Memory increase: %.2f MB\n", overall_memory_increase / (1024.0 * 1024.0));
    printf("- Memory efficiency: %.2f MB/second\n", 
           overall_memory_increase / (1024.0 * 1024.0 * overall_time));
    
    /* 轻量化版本验证 */
    printf("\nLightweight Version Verification:\n");
    printf("The atomsmini module should have lower resource consumption than the original atoms module.\n");
    printf("Expected improvements:\n");
    printf("  1. 30%% faster task processing\n");
    printf("  2. 50%% lower memory usage\n");
    printf("  3. 40%% faster vector searches\n");
    printf("\nNote: For accurate comparison, run the same tests against the original atoms module.\n");
    
    return 0;
}
