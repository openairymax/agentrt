/**
 * @file weighted.c
 * @brief 加权调度策略 - 可配置权重
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

typedef struct weighted_data {
    float cost_weight;
    float perf_weight;
    float trust_weight;
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    agentos_mutex_t* lock;
} weighted_data_t;

static void weighted_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static float compute_score(const agent_info_t* agent, const weighted_data_t* data) {
    float cost_score = 1.0f / (agent->cost_estimate + 1.0f);
    float perf_score = agent->success_rate;
    float trust_score = agent->trust_score;
    return data->cost_weight * cost_score +
           data->perf_weight * perf_score +
           data->trust_weight * trust_score;
}

static agentos_error_t weighted_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {

    weighted_data_t* data = (weighted_data_t*)context;
    if (!data || !task || !out_agent_id) return AGENTOS_EINVAL;

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

    float best_score = -FLT_MAX;
    int best_index = -1;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t* agent = agents[i];
        float score = compute_score(agent, data);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    if (best_index >= 0) {
        agent_info_t* best_agent = agents[best_index];
        *out_agent_id = strdup(best_agent->agent_id);
        if (!*out_agent_id) {
            AGENTOS_LOG_ERROR("Failed to duplicate agent_id");
            return AGENTOS_ENOMEM;
        }
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_dispatching_strategy_t* agentos_dispatching_weighted_create(
    const weighted_config_t* config,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!get_agents_func) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)malloc(sizeof(agentos_dispatching_strategy_t));
    if (!strat) {
        AGENTOS_LOG_ERROR("Failed to allocate weighted strategy");
        return NULL;
    }

    weighted_data_t* data = (weighted_data_t*)malloc(sizeof(weighted_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    if (config) {
        data->cost_weight = config->cost_weight;
        data->perf_weight = config->perf_weight;
        data->trust_weight = config->trust_weight;
    } else {
        data->cost_weight = 0.3f;
        data->perf_weight = 0.4f;
        data->trust_weight = 0.3f;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data);
        free(strat);
        return NULL;
    }

    strat->dispatch = weighted_dispatch;
    strat->destroy = weighted_destroy;
    strat->data = data;

    return strat;
}