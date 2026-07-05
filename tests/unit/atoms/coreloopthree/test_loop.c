/**
 * @file test_loop.c
 * @brief 核心循环单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"
#include "agentrt.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

/**
 * @brief 测试核心循环创建和销�?
 */
static void test_loop_create_destroy() {
    agentrt_core_loop_t* loop = NULL;
    agentrt_error_t err = agentrt_loop_create(NULL, &loop);
    printf("test_loop_create_destroy: %d\n", err);
    if (err == AGENTRT_SUCCESS) {
        agentrt_loop_destroy(loop);
    }
}

/**
 * @brief 测试核心循环提交任务
 */
static void test_loop_submit() {
    agentrt_core_loop_t* loop = NULL;
    agentrt_error_t err = agentrt_loop_create(NULL, &loop);
    if (err != AGENTRT_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_loop_submit: Failed to create loop\n");
        return;
    }

    // 提交一个任务
    char* task_id = NULL;
    const char* input = "帮我分析最近的销售数据";
    err = agentrt_loop_submit(loop, input, strlen(input), &task_id);
    printf("test_loop_submit: %d\n", err);
    if (err == AGENTRT_SUCCESS && task_id) {
        printf("Task ID: %s\n", task_id);
        AGENTRT_FREE(task_id);
    }

    agentrt_loop_destroy(loop);
}

/**
 * @brief 测试核心循环获取引擎
 */
static void test_loop_get_engines() {
    agentrt_core_loop_t* loop = NULL;
    agentrt_error_t err = agentrt_loop_create(NULL, &loop);
    if (err != AGENTRT_SUCCESS) {
        printf("test_loop_get_engines: Failed to create loop\n");
        return;
    }

    // 获取引擎
    agentrt_cognition_engine_t* cognition = NULL;
    agentrt_execution_engine_t* execution = NULL;
    agentrt_memory_engine_t* memory = NULL;
    agentrt_loop_get_engines(loop, &cognition, &execution, &memory);
    printf("test_loop_get_engines: cognition=%p, execution=%p, memory=%p\n",
           (void *)cognition, (void *)execution, (void *)memory);

    agentrt_loop_destroy(loop);
}

/**
 * @brief 测试核心循环配置
 */
static void test_loop_config() {
    // 创建配置
    agentrt_loop_config_t manager = {
        .loop_config_cognition_threads = 2,
        .loop_config_execution_threads = 4,
        .loop_config_memory_threads = 2,
        .loop_config_max_queued_tasks = 100,
        .loop_config_stats_interval_ms = 10000,
        .loop_config_plan_strategy = NULL,
        .loop_config_coord_strategy = NULL,
        .loop_config_disp_strategy = NULL
    };

    agentrt_core_loop_t* loop = NULL;
    agentrt_error_t err = agentrt_loop_create(&manager, &loop);
    printf("test_loop_config: %d\n", err);
    if (err == AGENTRT_SUCCESS) {
        agentrt_loop_destroy(loop);
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
