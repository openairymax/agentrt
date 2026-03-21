/**
 * @file ml_based.c
 * @brief 基于机器学习的调度策略
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "cognition.h"
#include "agent_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 简单的模型推理接口（假设）
 */
typedef struct ml_model {
    void* handle;
    int (*predict)(void* handle, const float* features, int n_features, float* scores, int n_outputs);
} ml_model_t;

typedef struct ml_data {
    ml_model_t* model;
    char* model_path;
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    agentos_mutex_t* lock;
} ml_data_t;

static void ml_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    ml_data_t* data = (ml_data_t*)strategy->data;
    if (data) {
        if (data->model) {
            // 释放模型资源（假设有 model_free 函数）
        }
        if (data->model_path) free(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t ml_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {

    ml_data_t* data = (ml_data_t*)context;
    if (!data || !data->model || !task || !out_agent_id) return AGENTOS_EINVAL;

    agent_info_t** agents = NULL;
    size_t agent_count = 0;
    agentos_error_t err;

    if (candidates && count > 0) {
        agents = (agent_info_t**)candidates;
        agent_count = count;
    } else {
        err = data->get_agents(data->registry_ctx, task->agent_role, &agents, &agent_count);
        if (err != AGENTOS_SUCCESS) return err;
        if (agent_count == 0) return AGENTOS_ENOENT;
    }

    // 将 Agent 信息转换为特征向量
    int n_features = 4; // cost, success_rate, trust_score, priority
    float* features = (float*)malloc(agent_count * n_features * sizeof(float));
    if (!features) return AGENTOS_ENOMEM;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t* agent = agents[i];
        features[i * n_features + 0] = agent->cost_estimate;
        features[i * n_features + 1] = agent->success_rate;
        features[i * n_features + 2] = agent->trust_score;
        features[i * n_features + 3] = (float)agent->priority;
    }

    float* scores = (float*)malloc(agent_count * sizeof(float));
    if (!scores) {
        free(features);
        return AGENTOS_ENOMEM;
    }

    int ret = data->model->predict(data->model->handle, features, agent_count * n_features, scores, agent_count);
    free(features);

    if (ret != 0) {
        free(scores);
        return AGENTOS_EIO;
    }

    int best_idx = 0;
    float best_score = scores[0];
    for (size_t i = 1; i < agent_count; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_idx = i;
        }
    }
    free(scores);

    agent_info_t* best_agent = agents[best_idx];
    *out_agent_id = strdup(best_agent->agent_id);
    if (!*out_agent_id) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

agentos_dispatching_strategy_t* agentos_dispatching_ml_create(
    const char* model_path,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!model_path || !get_agents_func) return NULL;

    // 实际应加载模型，这里仅占位
    // ml_model_t* model = model_load(model_path);
    // if (!model) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)malloc(sizeof(agentos_dispatching_strategy_t));
    if (!strat) return NULL;

    ml_data_t* data = (ml_data_t*)malloc(sizeof(ml_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->model = NULL; // 实际应赋值
    data->model_path = strdup(model_path);
    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentos_mutex_create();
    if (!data->lock || !data->model_path) {
        if (data->model_path) free(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
        free(strat);
        return NULL;
    }

    strat->dispatch = ml_dispatch;
    strat->destroy = ml_destroy;
    strat->data = data;

    return strat;
}