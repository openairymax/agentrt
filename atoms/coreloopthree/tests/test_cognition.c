/**
 * @file test_cognition.c
 * @brief 认知引擎测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cognition.h"
#include "agentos.h"
#include "logger.h"

// 简单的规划策略实现
static agentos_error_t test_plan_strategy(
    const agentos_intent_t* intent,
    void* data,
    agentos_task_plan_t** out_plan) {
    
    (void)data; // 未使用参数
    
    // 创建简单的任务计划
    agentos_task_plan_t* plan = calloc(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;
    
    // 创建单个任务节点
    plan->num_nodes = 1;
    plan->nodes = calloc(1, sizeof(agentos_task_node_t));
    if (!plan->nodes) {
        free(plan);
        return AGENTOS_ENOMEM;
    }
    
    agentos_task_node_t* node = &plan->nodes[0];
    node->task_id = strdup("test_task_1");
    node->role = strdup("test_agent");
    node->description = strdup("Test task description");
    
    if (!node->task_id || !node->role || !node->description) {
        if (node->task_id) free(node->task_id);
        if (node->role) free(node->role);
        if (node->description) free(node->description);
        free(plan->nodes);
        free(plan);
        return AGENTOS_ENOMEM;
    }
    
    // 设置入口点
    plan->num_entry_points = 1;
    plan->entry_points = calloc(1, sizeof(uint32_t));
    if (!plan->entry_points) {
        free(node->task_id);
        free(node->role);
        free(node->description);
        free(plan->nodes);
        free(plan);
        return AGENTOS_ENOMEM;
    }
    plan->entry_points[0] = 0;
    
    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

// 简单的协调策略实现
static agentos_error_t test_coordinator_strategy(
    const agentos_task_result_t* results,
    size_t num_results,
    void* data,
    char** out_final_result) {
    
    (void)data; // 未使用参数
    
    // 简单的结果合并：将所有结果连接起来
    size_t total_len = 0;
    for (size_t i = 0; i < num_results; i++) {
        if (results[i].result) {
            total_len += strlen(results[i].result);
        }
    }
    
    char* combined = malloc(total_len + 1);
    if (!combined) return AGENTOS_ENOMEM;
    
    combined[0] = '\0';
    for (size_t i = 0; i < num_results; i++) {
        if (results[i].result) {
            strcat(combined, results[i].result);
        }
    }
    
    *out_final_result = combined;
    return AGENTOS_SUCCESS;
}

// 简单的分发策略实现
static agentos_error_t test_dispatching_strategy(
    const agentos_task_node_t* node,
    void* data,
    char** out_agent_id) {
    
    (void)data; // 未使用参数
    
    // 根据角色返回代理ID
    if (node->role && strcmp(node->role, "test_agent") == 0) {
        *out_agent_id = strdup("test_agent_1");
        return *out_agent_id ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }
    
    return AGENTOS_ENOTSUP;
}

// 测试认知引擎创建
static void test_cognition_create() {
    printf("测试认知引擎创建...\n");
    
    agentos_cognition_engine_t* engine = NULL;
    agentos_plan_strategy_t plan_strat = {test_plan_strategy, NULL};
    agentos_coordinator_strategy_t coord_strat = {test_coordinator_strategy, NULL};
    agentos_dispatching_strategy_t disp_strat = {test_dispatching_strategy, NULL};
    
    agentos_error_t err = agentos_cognition_create(&plan_strat, &coord_strat, &disp_strat, &engine);
    assert(err == AGENTOS_SUCCESS);
    assert(engine != NULL);
    
    printf("✓ 认知引擎创建成功\n");
    
    // 清理
    agentos_cognition_destroy(engine);
}

// 测试认知引擎处理
static void test_cognition_process() {
    printf("测试认知引擎处理...\n");
    
    agentos_cognition_engine_t* engine = NULL;
    agentos_plan_strategy_t plan_strat = {test_plan_strategy, NULL};
    agentos_coordinator_strategy_t coord_strat = {test_coordinator_strategy, NULL};
    agentos_dispatching_strategy_t disp_strat = {test_dispatching_strategy, NULL};
    
    agentos_error_t err = agentos_cognition_create(&plan_strat, &coord_strat, &disp_strat, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    // 测试处理
    agentos_task_plan_t* plan = NULL;
    const char* input = "Test input";
    
    err = agentos_cognition_process(engine, input, strlen(input), &plan);
    assert(err == AGENTOS_SUCCESS);
    assert(plan != NULL);
    assert(plan->num_nodes == 1);
    assert(plan->nodes != NULL);
    assert(plan->nodes[0].task_id != NULL);
    assert(strcmp(plan->nodes[0].task_id, "test_task_1") == 0);
    
    printf("✓ 认知引擎处理成功\n");
    
    // 清理
    agentos_task_plan_free(plan);
    agentos_cognition_destroy(engine);
}

// 测试错误处理
static void test_cognition_errors() {
    printf("测试认知引擎错误处理...\n");
    
    // 测试空指针参数
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, NULL);
    assert(err == AGENTOS_EINVAL);
    
    // 测试无效输入
    agentos_cognition_engine_t* engine = NULL;
    agentos_plan_strategy_t plan_strat = {test_plan_strategy, NULL};
    
    err = agentos_cognition_create(&plan_strat, NULL, NULL, &engine);
    assert(err == AGENTOS_SUCCESS);
    
    agentos_task_plan_t* plan = NULL;
    err = agentos_cognition_process(NULL, "test", 4, &plan);
    assert(err == AGENTOS_EINVAL);
    
    err = agentos_cognition_process(engine, NULL, 0, &plan);
    assert(err == AGENTOS_EINVAL);
    
    err = agentos_cognition_process(engine, "test", 4, NULL);
    assert(err == AGENTOS_EINVAL);
    
    printf("✓ 错误处理正确\n");
    
    // 清理
    agentos_cognition_destroy(engine);
}

int main() {
    printf("开始认知引擎测试...\n\n");
    
    // 初始化日志系统
    agentos_logger_init();
    
    // 运行测试
    test_cognition_create();
    test_cognition_process();
    test_cognition_errors();
    
    printf("\n所有测试通过！\n");
    return 0;
}