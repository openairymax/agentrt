/**
 * @file test_execution.c
 * @brief 执行引擎单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

/**
 * @brief 测试执行引擎创建和销�?
 */
static void test_execution_create_destroy() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    printf("test_execution_create_destroy: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_execution_destroy(engine);
    }
}

/**
 * @brief 测试执行单元注册和注销
 */
static void test_execution_register_unregister() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    if (err != AGENTOS_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_execution_register_unregister: Failed to create engine\n");
        return;
    }

    // 创建一个简单的执行单元
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (unit) {
        unit->data = NULL;
        unit->execute = NULL;
        unit->destroy = NULL;
        unit->get_metadata = NULL;

        err = agentos_execution_register_unit(engine, "test_unit", unit);
        printf("test_execution_register: %d\n", err);

        agentos_execution_unregister_unit(engine, "test_unit");
        AGENTOS_FREE(unit);
    }

    agentos_execution_destroy(engine);
}

/**
 * @brief 测试任务提交和查�?
 */
static void test_execution_submit_query() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_execution_submit_query: Failed to create engine\n");
        return;
    }

    // 创建一个任�?
    agentos_task_t task = {
        .task_id = "test_task",
        .id_len = strlen("test_task"),
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

    char* task_id = NULL;
    err = agentos_execution_submit(engine, &task, &task_id);
    printf("test_execution_submit: %d\n", err);
    if (err == AGENTOS_SUCCESS && task_id) {
        printf("Task ID: %s\n", task_id);

        // 查询任务状�?
        agentos_task_status_t status;
        err = agentos_execution_query(engine, task_id, &status);
        printf("test_execution_query: %d, status: %d\n", err, status);

        AGENTOS_FREE(task_id);
    }

    agentos_execution_destroy(engine);
}

/**
 * @brief 测试任务取消
 */
static void test_execution_cancel() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_execution_cancel: Failed to create engine\n");
        return;
    }

    // 创建一个任�?
    agentos_task_t task = {
        .task_id = "test_task_cancel",
        .id_len = strlen("test_task_cancel"),
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

    char* task_id = NULL;
    err = agentos_execution_submit(engine, &task, &task_id);
    if (err == AGENTOS_SUCCESS && task_id) {
        // 取消任务
        err = agentos_execution_cancel(engine, task_id);
        printf("test_execution_cancel: %d\n", err);

        AGENTOS_FREE(task_id);
    }

    agentos_execution_destroy(engine);
}

/**
 * @brief 测试执行引擎健康检�?
 */
static void test_execution_health_check() {
    agentos_execution_engine_t* engine = NULL;
    agentos_error_t err = agentos_execution_create(4, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_execution_health_check: Failed to create engine\n");
        return;
    }

    char* health = NULL;
    err = agentos_execution_health_check(engine, &health);
    printf("test_execution_health_check: %d\n", err);
    if (err == AGENTOS_SUCCESS && health) {
        printf("Health: %s\n", health);
        AGENTOS_FREE(health);
    }

    agentos_execution_destroy(engine);
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
