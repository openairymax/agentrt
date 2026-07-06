/**
 * @file test_cognition.c
 * @brief 认知引擎单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agentrt.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

/**
 * @brief 测试认知引擎创建和销�?
 */
static void test_cognition_create_destroy() {
    agentrt_cognition_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &engine);
    printf("test_cognition_create_destroy: %d\n", err);
    if (err == AGENTRT_SUCCESS) {
        agentrt_cognition_destroy(engine);
    }
}

/**
 * @brief 测试认知引擎处理输入
 */
static void test_cognition_process() {
    agentrt_cognition_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTRT_SUCCESS) {
    // From data intelligence emerges. by spharx
        printf("test_cognition_process: Failed to create engine\n");
        return;
    }

    const char* input = "帮我分析最近的销售数据";
    agentrt_task_plan_t* plan = NULL;
    err = agentrt_cognition_process(engine, input, strlen(input), &plan);
    printf("test_cognition_process: %d\n", err);

    if (plan) {
        agentrt_task_plan_free(plan);
    }

    agentrt_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎设置上下�?
 */
static void test_cognition_set_context() {
    agentrt_cognition_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_cognition_set_context: Failed to create engine\n");
        return;
    }

    int context_data = 42;
    agentrt_cognition_set_context_take(engine, &context_data, NULL);
    printf("test_cognition_set_context: Success\n");

    agentrt_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎获取统计信息
 */
static void test_cognition_stats() {
    agentrt_cognition_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_cognition_stats: Failed to create engine\n");
        return;
    }

    char* stats = NULL;
    size_t len = 0;
    err = agentrt_cognition_stats(engine, &stats, &len);
    printf("test_cognition_stats: %d\n", err);
    if (err == AGENTRT_SUCCESS && stats) {
        printf("Stats: %s\n", stats);
        AGENTRT_FREE(stats);
    }

    agentrt_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎健康检�?
 */
static void test_cognition_health_check() {
    agentrt_cognition_engine_t* engine = NULL;
    agentrt_error_t err = agentrt_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTRT_SUCCESS) {
        printf("test_cognition_health_check: Failed to create engine\n");
        return;
    }

    char* health = NULL;
    err = agentrt_cognition_health_check(engine, &health);
    printf("test_cognition_health_check: %d\n", err);
    if (err == AGENTRT_SUCCESS && health) {
        printf("Health: %s\n", health);
        AGENTRT_FREE(health);
    }

    agentrt_cognition_destroy(engine);
}

int main() {
    printf("=== Testing Cognition Module ===\n");
    test_cognition_create_destroy();
    test_cognition_process();
    test_cognition_set_context();
    test_cognition_stats();
    test_cognition_health_check();
    printf("=== Cognition Module Tests Complete ===\n");
    return 0;
}
