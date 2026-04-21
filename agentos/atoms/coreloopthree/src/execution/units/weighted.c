/**
 * @file weighted.c
 * @brief 加权调度策略（基于成本、性能、信任度加权评分），可配置权重
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include "include/memory_compat.h"
#include "strategy_common.h"

typedef struct weighted_data {
    weighted_config_t manager;
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    agentos_mutex_t* lock;
} weighted_data_t;

static void weighted_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

static float compute_score(const agent_info_t* agent, const weighted_data_t* data) {
    strategy_agent_info_t strategy_agent;
    memset(&strategy_agent, 0, sizeof(strategy_agent));
    strategy_agent.cost_estimate = agent->cost_estimate;
    strategy_agent.success_rate = agent->success_rate;
    strategy_agent.trust_score = agent->trust_score;
    strategy_agent.name = agent->role;
    strategy_agent.user_data = NULL;
    return strategy_compute_weighted_score(&strategy_agent, &data->manager);
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
        err = data->get_agents(data->registry_ctx, task->task_node_agent_role, &agents, &agent_count);
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
            best_index = (int)i;
        }
    }

    if (best_index >= 0) {
        agent_info_t* best_agent = agents[best_index];
        *out_agent_id = AGENTOS_STRDUP(best_agent->agent_id);
        if (!*out_agent_id) return AGENTOS_ENOMEM;
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_dispatching_strategy_t* agentos_dispatching_weighted_create(
    const weighted_config_t* manager,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!get_agents_func) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_dispatching_strategy_t));
    if (!strat) return NULL;
    memset(strat, 0, sizeof(*strat));

    weighted_data_t* data = (weighted_data_t*)AGENTOS_MALLOC(sizeof(weighted_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }
    memset(data, 0, sizeof(*data));

    if (manager) {
        data->manager = *manager;
    } else {
        data->manager.cost_weight = 0.3f;
        data->manager.perf_weight = 0.4f;
        data->manager.trust_weight = 0.3f;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->dispatch = weighted_dispatch;
    strat->destroy = weighted_destroy;
    strat->data = data;

    return strat;
}
