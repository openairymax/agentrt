/**
 * @file test_execution.c
 * @brief 执行引擎单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentrt.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

/**
 * @brief 测试执行引擎创建和销�?
 */
static void test_execution_create_destroy() {
    agentrt_execution_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_execution_create(4, &engine);
    printf("test_execution_create_destroy: %d\n", err);
    if (err == AGENTRT_SUCCESS) {
        agentrt_execution_destroy(engine);
    }
}

/**
 * @brief 测试执行单元注册和注销
 */
static void test_execution_register_unregister() {
    agentrt_execution_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_execution_create(4, &engine);
    if (err != AGENTRT_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_execution_register_unregister: Failed to create engine\n");
        return;
    }

    // 创建一个简单的执行单元
    agentrt_execution_unit_t* unit = (agentrt_execution_unit_t*)AGENTRT_MALLOC(sizeof(agentrt_execution_unit_t));
    if (unit) {
        unit->execution_unit_data = NULL;
        unit->execution_unit_execute = NULL;
        unit->execution_unit_destroy = NULL;
        unit->execution_unit_get_metadata = NULL;

        err = agentrt_execution_register_unit(engine, "test_unit", *unit);
        printf("test_execution_register: %d\n", err);

        agentrt_execution_unregister_unit(engine, "test_unit");
        AGENTRT_FREE(unit);
    }

    agentrt_execution_destroy(engine);
}

/**
 * @brief 测试任务提交和查�?
 */
static void test_execution_submit_query() {
    agentrt_execution_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_execution_create(4, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_execution_submit_query: Failed to create engine\n");
        return;
    }

    // 创建一个任�?
    agentrt_task_t task = {
        .task_id = "test_task",
        .task_id_len = strlen("test_task"),
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

    char* task_id = NULL;
    err = agentrt_execution_submit(engine, &task, &task_id);
    printf("test_execution_submit: %d\n", err);
    if (err == AGENTRT_SUCCESS && task_id) {
        printf("Task ID: %s\n", task_id);

        // 查询任务状�?
        agentrt_task_status_t status;
        err = agentrt_execution_query(engine, task_id, &status);
        printf("test_execution_query: %d, status: %d\n", err, status);

        AGENTRT_FREE(task_id);
    }

    agentrt_execution_destroy(engine);
}

/**
 * @brief 测试任务取消
 */
static void test_execution_cancel() {
    agentrt_execution_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_execution_create(4, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_execution_cancel: Failed to create engine\n");
        return;
    }

    // 创建一个任�?
    agentrt_task_t task = {
        .task_id = "test_task_cancel",
        .task_id_len = strlen("test_task_cancel"),
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

    char* task_id = NULL;
    err = agentrt_execution_submit(engine, &task, &task_id);
    if (err == AGENTRT_SUCCESS && task_id) {
        // 取消任务
        err = agentrt_execution_cancel(engine, task_id);
        printf("test_execution_cancel: %d\n", err);

        AGENTRT_FREE(task_id);
    }

    agentrt_execution_destroy(engine);
}

/**
 * @brief 测试执行引擎健康检�?
 */
static void test_execution_health_check() {
    agentrt_execution_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_execution_create(4, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_execution_health_check: Failed to create engine\n");
        return;
    }

    char* health = NULL;
    err = agentrt_execution_health_check(engine, &health);
    printf("test_execution_health_check: %d\n", err);
    if (err == AGENTRT_SUCCESS && health) {
        printf("Health: %s\n", health);
        AGENTRT_FREE(health);
    }

    agentrt_execution_destroy(engine);
}

int main() {
    printf("=== Testing Execution Module ===\n");
    test_execution_create_destroy();
    test_execution_register_unregister();
    test_execution_submit_query();
    test_execution_cancel();
    test_execution_health_check();
    printf("=== Execution Module Tests Complete ===\n");
    return 0;
}
