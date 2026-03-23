/**
 * @file round_robin.c
 * @brief 轮询调度策略（依次选择Agent）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include <stdlib.h>
#include <string.h>

typedef struct round_robin_data {
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    size_t current_index;                   /**< 当前索引（每个角色独立，简化用全局） */
    agentos_mutex_t* lock;
} round_robin_data_t;

static void round_robin_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    round_robin_data_t* data = (round_robin_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t round_robin_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {

    round_robin_data_t* data = (round_robin_data_t*)context;
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

    agentos_mutex_lock(data->lock);
    size_t idx = data->current_index % agent_count;
    data->current_index = (data->current_index + 1) % agent_count;
    agentos_mutex_unlock(data->lock);

    agent_info_t* selected = agents[idx];
    *out_agent_id = strdup(selected->agent_id);
    if (!*out_agent_id) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

agentos_dispatching_strategy_t* agentos_dispatching_round_robin_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!get_agents_func) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)malloc(sizeof(agentos_dispatching_strategy_t));
    if (!strat) return NULL;

    round_robin_data_t* data = (round_robin_data_t*)malloc(sizeof(round_robin_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->current_index = 0;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data);
        free(strat);
        return NULL;
    }

    strat->dispatch = round_robin_dispatch;
    strat->destroy = round_robin_destroy;
    strat->data = data;

    return strat;
}