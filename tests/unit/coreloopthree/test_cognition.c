#include "cognition.h"
#include "agentos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试认知引擎创建和销毁
 */
static void test_cognition_create_destroy() {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    printf("test_cognition_create_destroy: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_cognition_destroy(engine);
    }
}

/**
 * @brief 测试认知引擎处理输入
 */
static void test_cognition_process() {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_cognition_process: Failed to create engine\n");
        return;
    }

    const char* input = "帮我分析最近的销售数据";
    agentos_task_plan_t* plan = NULL;
    err = agentos_cognition_process(engine, input, strlen(input), &plan);
    printf("test_cognition_process: %d\n", err);

    if (plan) {
        agentos_task_plan_free(plan);
    }

    agentos_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎设置上下文
 */
static void test_cognition_set_context() {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_cognition_set_context: Failed to create engine\n");
        return;
    }

    int context_data = 42;
    agentos_cognition_set_context(engine, &context_data, NULL);
    printf("test_cognition_set_context: Success\n");

    agentos_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎获取统计信息
 */
static void test_cognition_stats() {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_cognition_stats: Failed to create engine\n");
        return;
    }

    char* stats = NULL;
    size_t len = 0;
    err = agentos_cognition_stats(engine, &stats, &len);
    printf("test_cognition_stats: %d\n", err);
    if (err == AGENTOS_SUCCESS && stats) {
        printf("Stats: %s\n", stats);
        free(stats);
    }

    agentos_cognition_destroy(engine);
}

/**
 * @brief 测试认知引擎健康检查
 */
static void test_cognition_health_check() {
    agentos_cognition_engine_t* engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS) {
        printf("test_cognition_health_check: Failed to create engine\n");
        return;
    }

    char* health = NULL;
    err = agentos_cognition_health_check(engine, &health);
    printf("test_cognition_health_check: %d\n", err);
    if (err == AGENTOS_SUCCESS && health) {
        printf("Health: %s\n", health);
        free(health);
    }

    agentos_cognition_destroy(engine);
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
