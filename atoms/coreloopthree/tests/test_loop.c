﻿/**
 * @file test_loop.c
 * @brief 核心循环单元测试
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

/**
 * @brief 测试核心循环创建和销�?
 */
static void test_loop_create_destroy() {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    printf("test_loop_create_destroy: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_loop_destroy(loop);
    }
}

/**
 * @brief 测试核心循环提交任务
 */
static void test_loop_submit() {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_loop_submit: Failed to create loop\n");
        return;
    }

    // 提交一个任�?
    char* task_id = NULL;
    const char* input = "帮我分析最近的销售数�?;
    err = agentos_loop_submit(loop, input, strlen(input), &task_id);
    printf("test_loop_submit: %d\n", err);
    if (err == AGENTOS_SUCCESS && task_id) {
        printf("Task ID: %s\n", task_id);
        AGENTOS_FREE(task_id);
    }

    agentos_loop_destroy(loop);
}

/**
 * @brief 测试核心循环获取引擎
 */
static void test_loop_get_engines() {
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS) {
        printf("test_loop_get_engines: Failed to create loop\n");
        return;
    }

    // 获取引擎
    agentos_cognition_engine_t* cognition = NULL;
    agentos_execution_engine_t* execution = NULL;
    agentos_memory_engine_t* memory = NULL;
    agentos_loop_get_engines(loop, &cognition, &execution, &memory);
    printf("test_loop_get_engines: cognition=%p, execution=%p, memory=%p\n", 
           cognition, execution, memory);

    agentos_loop_destroy(loop);
}

/**
 * @brief 测试核心循环配置
 */
static void test_loop_config() {
    // 创建配置
    agentos_loop_config_t manager = {
        .cognition_threads = 2,
        .execution_threads = 4,
        .memory_threads = 2,
        .max_queued_tasks = 100,
        .stats_interval_ms = 10000,
        .plan_strategy = NULL,
        .coord_strategy = NULL,
        .disp_strategy = NULL
    };

    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(&manager, &loop);
    printf("test_loop_config: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_loop_destroy(loop);
    }
}

int main() {
    printf("=== Testing Core Loop Module ===\n");
    test_loop_create_destroy();
    test_loop_config();
    test_loop_get_engines();
    test_loop_submit();
    printf("=== Core Loop Module Tests Complete ===\n");
    return 0;
}
