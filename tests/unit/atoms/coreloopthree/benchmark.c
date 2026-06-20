/**
 * @file benchmark.c
 * @brief 核心循环性能基准测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
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

    const char* input = "帮我分析最近的销售数据";
    size_t input_len = strlen(input);
    int num_tasks = 1000;
    char** task_ids;
    SAFE_MALLOC_ARRAY(task_ids, num_tasks, sizeof(char*));
    if (!task_ids) {
        agentos_loop_destroy(loop);
        return;
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

    int num_tasks = 1000;
    char** task_ids;
    SAFE_MALLOC_ARRAY(task_ids, num_tasks, sizeof(char*));
    if (!task_ids) {
        agentos_execution_destroy(engine);
        return;
    }

    for (int i = 0; i < num_tasks; i++) {
        agentos_task_t task = {
            .task_id = NULL,
            .task_id_len = 0,
            .task_agent_id = "test_agent",
            .task_agent_id_len = strlen("test_agent"),
            .task_status = TASK_STATUS_PENDING,
            .task_input = NULL,
            .task_output = NULL,
            .task_created_ns = 0,
            .task_started_ns = 0,
            .task_completed_ns = 0,
            .task_timeout_ms = 1000,
            .task_retry_count = 0,
            .task_max_retries = 3,
            .task_error_msg = NULL
        };

        err = agentos_execution_submit(engine, &task, &task_ids[i]);
        if (err != AGENTOS_SUCCESS) {
            printf("benchmark_task_query: Failed to submit task %d\n", i);
            break;
        }
    }

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
    char** record_ids;
    SAFE_MALLOC_ARRAY(record_ids, num_records, sizeof(char*));
    if (!record_ids) {
        agentos_memory_destroy(engine);
        return;
    }

    agentos_memory_record_t record = {
        .memory_record_id = NULL,
        .memory_record_id_len = 0,
        .memory_record_type = AGENTOS_MEMTYPE_TEXT,
        .memory_record_timestamp_ns = 0,
        .memory_record_source_agent = "test_agent",
        .memory_record_source_len = strlen("test_agent"),
        .memory_record_trace_id = "test_trace",
        .memory_record_trace_len = strlen("test_trace"),
        .memory_record_data = (void*)"test data",
        .memory_record_data_len = strlen("test data"),
        .memory_record_importance = 0.5f,
        .memory_record_access_count = 0
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

    int num_records = 1000;
    for (int i = 0; i < num_records; i++) {
        agentos_memory_record_t record = {
            .memory_record_id = NULL,
            .memory_record_id_len = 0,
            .memory_record_type = AGENTOS_MEMTYPE_TEXT,
            .memory_record_timestamp_ns = 0,
            .memory_record_source_agent = "test_agent",
            .memory_record_source_len = strlen("test_agent"),
            .memory_record_trace_id = "test_trace",
            .memory_record_trace_len = strlen("test_trace"),
            .memory_record_data = (void*)"test data",
            .memory_record_data_len = strlen("test data"),
            .memory_record_importance = 0.5f,
            .memory_record_access_count = 0
        };

        char* record_id = NULL;
        err = agentos_memory_write(engine, &record, &record_id);
        if (err == AGENTOS_SUCCESS && record_id) {
            AGENTOS_FREE(record_id);
        }
    }

    agentos_memory_query_t query = {
        .memory_query_text = "test",
        .memory_query_text_len = strlen("test"),
        .memory_query_start_time = 0,
        .memory_query_end_time = 0,
        .memory_query_source_agent = NULL,
        .memory_query_trace_id = NULL,
        .memory_query_limit = 10,
        .memory_query_offset = 0,
        .memory_query_include_raw = 1
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