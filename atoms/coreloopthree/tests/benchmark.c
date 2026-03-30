﻿/**
 * @file benchmark.c
 * @brief 核心循环性能基准测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <time.h>

/**
 * @brief 基准测试：任务提交性能
 */
static void benchmark_task_submit() {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS) {
        printf("benchmark_task_submit: Failed to create loop\n");
        return;
    }

    const char* input = "帮我分析最近的销售数�?;
    size_t input_len = strlen(input);
    int num_tasks = 1000;
    char** task_ids = (char**)AGENTOS_MALLOC(num_tasks * sizeof(char*));
    if (!task_ids) {
        agentos_loop_destroy(loop);
        return;
        // From data intelligence emerges. by spharx
    }

    clock_t start = clock();
    for (int i = 0; i < num_tasks; i++) {
        err = agentos_loop_submit(loop, input, input_len, &task_ids[i]);
        if (err != AGENTOS_SUCCESS) {
            printf("benchmark_task_submit: Failed to submit task %d\n", i);
            break;
        }
    }
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("benchmark_task_submit: %d tasks in %.3f seconds (%.3f tasks/sec)\n", 
           num_tasks, elapsed, num_tasks / elapsed);

    // 释放任务 ID
    for (int i = 0; i < num_tasks; i++) {
        if (task_ids[i]) {
            AGENTOS_FREE(task_ids[i]);
        }
    }
    AGENTOS_FREE(task_ids);

    agentos_loop_destroy(loop);
}

/**
 * @brief 基准测试：任务查询性能
 */
static void benchmark_task_query() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("benchmark_task_query: Failed to create engine\n");
        return;
    }

    // 提交多个任务
    int num_tasks = 1000;
    char** task_ids = (char**)AGENTOS_MALLOC(num_tasks * sizeof(char*));
    if (!task_ids) {
        agentos_execution_destroy(engine);
        return;
    }

    for (int i = 0; i < num_tasks; i++) {
        agentos_task_t task = {
            .task_id = NULL,
            .id_len = 0,
            .agent_id = "test_agent",
            .agent_id_len = strlen("test_agent"),
            .status = TASK_STATUS_PENDING,
            .input = NULL,
            .output = NULL,
            .created_ns = 0,
            .started_ns = 0,
            .completed_ns = 0,
            .timeout_ms = 1000,
            .retry_count = 0,
            .max_retries = 3,
            .error_msg = NULL
        };

        err = agentos_execution_submit(engine, &task, &task_ids[i]);
        if (err != AGENTOS_SUCCESS) {
            printf("benchmark_task_query: Failed to submit task %d\n", i);
            break;
        }
    }

    // 测试查询性能
    clock_t start = clock();
    for (int i = 0; i < num_tasks; i++) {
        if (task_ids[i]) {
            agentos_task_status_t status;
            err = agentos_execution_query(engine, task_ids[i], &status);
            if (err != AGENTOS_SUCCESS) {
                printf("benchmark_task_query: Failed to query task %d\n", i);
                break;
            }
        }
    }
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("benchmark_task_query: %d queries in %.3f seconds (%.3f queries/sec)\n", 
           num_tasks, elapsed, num_tasks / elapsed);

    // 释放任务 ID
    for (int i = 0; i < num_tasks; i++) {
        if (task_ids[i]) {
            AGENTOS_FREE(task_ids[i]);
        }
    }
    AGENTOS_FREE(task_ids);

    agentos_execution_destroy(engine);
}

/**
 * @brief 基准测试：记忆写入性能
 */
static void benchmark_memory_write() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("benchmark_memory_write: Failed to create engine\n");
        return;
    }

    int num_records = 1000;
    char** record_ids = (char**)AGENTOS_MALLOC(num_records * sizeof(char*));
    if (!record_ids) {
        agentos_memory_destroy(engine);
        return;
    }

    agentos_memory_record_t record = {
        .record_id = NULL,
        .id_len = 0,
        .type = MEMORY_TYPE_RAW,
        .timestamp_ns = 0,
        .source_agent = "test_agent",
        .source_len = strlen("test_agent"),
        .trace_id = "test_trace",
        .trace_len = strlen("test_trace"),
        .data = (void*)"test data",
        .data_len = strlen("test data"),
        .importance = 0.5,
        .access_count = 0
    };

    clock_t start = clock();
    for (int i = 0; i < num_records; i++) {
        err = agentos_memory_write(engine, &record, &record_ids[i]);
        if (err != AGENTOS_SUCCESS) {
            printf("benchmark_memory_write: Failed to write record %d\n", i);
            break;
        }
    }
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("benchmark_memory_write: %d records in %.3f seconds (%.3f records/sec)\n", 
           num_records, elapsed, num_records / elapsed);

    // 释放记录 ID
    for (int i = 0; i < num_records; i++) {
        if (record_ids[i]) {
            AGENTOS_FREE(record_ids[i]);
        }
    }
    AGENTOS_FREE(record_ids);

    agentos_memory_destroy(engine);
}

/**
 * @brief 基准测试：记忆查询性能
 */
static void benchmark_memory_query() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("benchmark_memory_query: Failed to create engine\n");
        return;
    }

    // 写入一些记�?
    int num_records = 1000;
    for (int i = 0; i < num_records; i++) {
        agentos_memory_record_t record = {
            .record_id = NULL,
            .id_len = 0,
            .type = MEMORY_TYPE_RAW,
            .timestamp_ns = 0,
            .source_agent = "test_agent",
            .source_len = strlen("test_agent"),
            .trace_id = "test_trace",
            .trace_len = strlen("test_trace"),
            .data = (void*)"test data",
            .data_len = strlen("test data"),
            .importance = 0.5,
            .access_count = 0
        };

        char* record_id = NULL;
        err = agentos_memory_write(engine, &record, &record_id);
        if (err == AGENTOS_SUCCESS && record_id) {
            AGENTOS_FREE(record_id);
        }
    }

    // 测试查询性能
    agentos_memory_query_t query = {
        .text = "test",
        .text_len = strlen("test"),
        .start_time = 0,
        .end_time = 0,
        .source_agent = NULL,
        .trace_id = NULL,
        .limit = 10,
        .offset = 0,
        .include_raw = 1
    };

    int num_queries = 100;
    clock_t start = clock();
    for (int i = 0; i < num_queries; i++) {
        agentos_memory_result_t* result = NULL;
        err = agentos_memory_query(engine, &query, &result);
        if (err == AGENTOS_SUCCESS && result) {
            agentos_memory_result_free(result);
        }
    }
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("benchmark_memory_query: %d queries in %.3f seconds (%.3f queries/sec)\n", 
           num_queries, elapsed, num_queries / elapsed);

    agentos_memory_destroy(engine);
}

int main() {
    printf("=== Running Benchmark Tests ===\n");
    benchmark_task_submit();
    benchmark_task_query();
    benchmark_memory_write();
    benchmark_memory_query();
    printf("=== Benchmark Tests Complete ===\n");
    return 0;
}
