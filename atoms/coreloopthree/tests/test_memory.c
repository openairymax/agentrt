﻿/**
 * @file test_memory.c
 * @brief 记忆引擎单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>

/**
 * @brief 测试记忆引擎创建和销�?
 */
static void test_memory_create_destroy() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    printf("test_memory_create_destroy: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_memory_destroy(engine);
    }
}

/**
 * @brief 测试记忆写入
 */
static void test_memory_write() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_memory_write: Failed to create engine\n");
        return;
    }

    // 创建一个记忆记�?
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
    printf("test_memory_write: %d\n", err);
    if (err == AGENTOS_SUCCESS && record_id) {
        printf("Record ID: %s\n", record_id);
        AGENTOS_FREE(record_id);
    }

    agentos_memory_destroy(engine);
}

/**
 * @brief 测试记忆查询
 */
static void test_memory_query() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_memory_query: Failed to create engine\n");
        return;
    }

    // 创建一个记忆记�?
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
        // 创建查询条件
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

        agentos_memory_result_t* result = NULL;
        err = agentos_memory_query(engine, &query, &result);
        printf("test_memory_query: %d\n", err);
        if (err == AGENTOS_SUCCESS && result) {
            printf("Query results: %zu\n", result->count);
            agentos_memory_result_free(result);
        }

        AGENTOS_FREE(record_id);
    }

    agentos_memory_destroy(engine);
}

/**
 * @brief 测试根据 ID 获取记忆记录
 */
static void test_memory_get() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_memory_get: Failed to create engine\n");
        return;
    }

    // 创建一个记忆记�?
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
        // 根据 ID 获取记录
        agentos_memory_record_t* retrieved_record = NULL;
        err = agentos_memory_get(engine, record_id, 1, &retrieved_record);
        printf("test_memory_get: %d\n", err);
        if (err == AGENTOS_SUCCESS && retrieved_record) {
            printf("Retrieved record ID: %s\n", retrieved_record->record_id);
            agentos_memory_record_free(retrieved_record);
        }

        AGENTOS_FREE(record_id);
    }

    agentos_memory_destroy(engine);
}

/**
 * @brief 测试记忆挂载
 */
static void test_memory_mount() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_memory_mount: Failed to create engine\n");
        return;
    }

    // 创建一个记忆记�?
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
        // 挂载记忆
        err = agentos_memory_mount(engine, record_id, "test_context");
        printf("test_memory_mount: %d\n", err);

        AGENTOS_FREE(record_id);
    }

    agentos_memory_destroy(engine);
}

/**
 * @brief 测试记忆进化
 */
static void test_memory_evolve() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_memory_evolve: Failed to create engine\n");
        return;
    }

    // 触发记忆进化
    err = agentos_memory_evolve(engine, 0);
    printf("test_memory_evolve: %d\n", err);

    agentos_memory_destroy(engine);
}

/**
 * @brief 测试记忆引擎健康检�?
 */
static void test_memory_health_check() {
    agentos_memory_engine_t* engine = NULL;
    agentos_error_t err = agentos_memory_create(NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_memory_health_check: Failed to create engine\n");
        return;
    }

    char* health = NULL;
    err = agentos_memory_health_check(engine, &health);
    printf("test_memory_health_check: %d\n", err);
    if (err == AGENTOS_SUCCESS && health) {
        printf("Health: %s\n", health);
        AGENTOS_FREE(health);
    }

    agentos_memory_destroy(engine);
}

int main() {
    printf("=== Testing Memory Module ===\n");
    test_memory_create_destroy();
    test_memory_write();
    test_memory_query();
    test_memory_get();
    test_memory_mount();
    test_memory_evolve();
    test_memory_health_check();
    printf("=== Memory Module Tests Complete ===\n");
    return 0;
}
