/**
 * @file weighted.c
 * @brief 加权调度策略（基于成本、性能、信任度加权评分），可配置权重
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agent_registry.h"
#include "cognition.h"
#include "memory_compat.h"
#include "strategy_common.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


typedef struct weighted_data {
    weighted_config_t manager;
    void *registry_ctx;
    agent_registry_get_agents_func get_agents;
    agentrt_mutex_t *lock;
} weighted_data_t;

static void weighted_destroy(agentrt_dispatching_strategy_t *strategy)
{
    if (!strategy)
        return;
    weighted_data_t *data = (weighted_data_t *)strategy->data;
    if (data) {
        if (data->lock)
            agentrt_mutex_free(data->lock);
        AGENTRT_FREE(data);
    }
    AGENTRT_FREE(strategy);
}

static float compute_score(const agent_info_t *agent, const weighted_data_t *data)
{
    strategy_agent_info_t strategy_agent;
    __builtin_memset(&strategy_agent, 0, sizeof(strategy_agent));
    strategy_agent.cost_estimate = agent->cost_estimate;
    strategy_agent.success_rate = agent->success_rate;
    strategy_agent.trust_score = agent->trust_score;
    strategy_agent.name = agent->role;
    strategy_agent.user_data = NULL;
    return strategy_compute_weighted_score(&strategy_agent, &data->manager);
}

static agentrt_error_t weighted_dispatch(const agentrt_task_node_t *task, const void **candidates,
                                         size_t count, void *context, char **out_agent_id)
{

    weighted_data_t *data = (weighted_data_t *)context;
    if (!data || !task || !out_agent_id)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agent_info_t **agents = NULL;
    size_t agent_count = 0;
    agentrt_error_t err;

    if (candidates && count > 0) {
        agents = (agent_info_t **)candidates;
        agent_count = count;
    } else {
        err =
            data->get_agents(data->registry_ctx, task->task_node_agent_role, &agents, &agent_count);
        if (err != AGENTRT_SUCCESS)
            return err;
        if (agent_count == 0)
            ATM_RET_ERR(AGENTRT_ENOENT);
    }

    float best_score = -FLT_MAX;
    int best_index = -1;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t *agent = agents[i];
        float score = compute_score(agent, data);
        if (score > best_score) {
            best_score = score;
            best_index = (int)i;
        }
    }

    if (best_index >= 0) {
        agent_info_t *best_agent = agents[best_index];
        *out_agent_id = AGENTRT_STRDUP(best_agent->agent_id);
        if (!*out_agent_id)
            ATM_RET_ERR(AGENTRT_ENOMEM);
        return AGENTRT_SUCCESS;
    }

    ATM_RET_ERR(AGENTRT_ENOENT);
}

agentrt_dispatching_strategy_t *
agentrt_dispatching_weighted_create(const weighted_config_t *manager, void *registry_ctx,
                                    agent_registry_get_agents_func get_agents_func)
{

    if (!get_agents_func) return NULL;

    agentrt_dispatching_strategy_t *strat =
        (agentrt_dispatching_strategy_t *)AGENTRT_MALLOC(sizeof(agentrt_dispatching_strategy_t));
    if (!strat) return NULL;
    __builtin_memset(strat, 0, sizeof(*strat));

    weighted_data_t *data = (weighted_data_t *)AGENTRT_MALLOC(sizeof(weighted_data_t));
    if (!data) {
        AGENTRT_FREE(strat);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    __builtin_memset(data, 0, sizeof(*data));

    if (manager) {
        data->manager = *manager;
    } else {
        data->manager.cost_weight = 0.3f;
        data->manager.perf_weight = 0.4f;
        data->manager.trust_weight = 0.3f;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentrt_mutex_create();
    if (!data->lock) {
        AGENTRT_FREE(data);
        AGENTRT_FREE(strat);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    strat->dispatch = weighted_dispatch;
    strat->destroy = weighted_destroy;
    strat->data = data;

    return strat;
}
