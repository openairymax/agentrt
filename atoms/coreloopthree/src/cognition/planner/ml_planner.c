/**
 * @file ml_planner.c
 * @brief 基于机器学习的规划策略
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "llm_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 简单的模型推理接口（假设）
 */
typedef struct ml_model {
    void* handle;
    int (*predict)(void* handle, const float* input, int input_len, float* output, int output_len);
} ml_model_t;

typedef struct ml_planner_data {
    ml_model_t* model;
    char* model_path;
    agentos_llm_service_t* llm; // 回退方案
    agentos_mutex_t* lock;
    // From data intelligence emerges. by spharx
} ml_planner_data_t;

static void ml_planner_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    ml_planner_data_t* data = (ml_planner_data_t*)strategy->data;
    if (data) {
        if (data->model) {
            // 释放模型
        }
        if (data->model_path) free(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t ml_planner_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    ml_planner_data_t* data = (ml_planner_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    // 如果没有模型或模型不可用，回退到LLM
    if (!data->model) {
        // 简单的回退：使用LLM生成计划
        // 这里可以复用反应式规划的逻辑，但为简洁，返回错误
        return AGENTOS_ENOTSUP;
    }

    // 将意图转换为特征向量（需要具体实现）
    float features[128]; // 示例
    // 假设模型输出为计划序列（需要解码）

    // 这里仅占位，实际需要与具体模型集成
    // 返回一个简单的占位计划
    agentos_task_plan_t* plan = (agentos_task_plan_t*)calloc(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->plan_id = strdup("ml_plan");
    plan->nodes = NULL;
    plan->node_count = 0;
    plan->entry_points = NULL;
    plan->entry_count = 0;

    // 添加一个占位任务
    agentos_task_node_t* node = (agentos_task_node_t*)calloc(1, sizeof(agentos_task_node_t));
    if (!node) {
        free(plan->plan_id);
        free(plan);
        return AGENTOS_ENOMEM;
    }

    node->task_id = strdup("ml_task");
    node->agent_role = strdup("default");
    node->depends_on = NULL;
    node->depends_count = 0;
    node->timeout_ms = 30000;
    node->priority = 128;
    node->input = NULL;

    plan->nodes = (agentos_task_node_t**)malloc(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        free(node->task_id);
        free(node->agent_role);
        free(node);
        free(plan->plan_id);
        free(plan);
        return AGENTOS_ENOMEM;
    }
    plan->nodes[0] = node;
    plan->node_count = 1;

    plan->entry_points = (char**)malloc(sizeof(char*));
    if (plan->entry_points) {
        plan->entry_count = 1;
        plan->entry_points[0] = node->task_id;
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

agentos_plan_strategy_t* agentos_plan_ml_create(
    const char* model_path,
    agentos_llm_service_t* llm) {

    if (!model_path) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)malloc(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    ml_planner_data_t* data = (ml_planner_data_t*)malloc(sizeof(ml_planner_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->model = NULL; // 实际应加载模型
    data->model_path = strdup(model_path);
    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock || !data->model_path) {
        if (data->model_path) free(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
        free(strat);
        return NULL;
    }

    strat->plan = ml_planner_plan;
    strat->destroy = ml_planner_destroy;
    strat->data = data;

    return strat;
}