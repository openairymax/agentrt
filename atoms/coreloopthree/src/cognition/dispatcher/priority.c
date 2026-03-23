/**
 * @file priority.c
 * @brief 优先级调度策略（选择优先级最高的Agent）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include <stdlib.h>
#include <string.h>

typedef struct priority_data {
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    agentos_mutex_t* lock;
} priority_data_t;

static void priority_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    priority_data_t* data = (priority_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t priority_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {

    priority_data_t* data = (priority_data_t*)context;
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

    int highest_priority = -1;
    int best_index = -1;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t* agent = agents[i];
        if (agent->priority > highest_priority) {
            highest_priority = agent->priority;
            best_index = i;
        }
    }

    if (best_index >= 0) {
        agent_info_t* best = agents[best_index];
        *out_agent_id = strdup(best->agent_id);
        if (!*out_agent_id) return AGENTOS_ENOMEM;
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_dispatching_strategy_t* agentos_dispatching_priority_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!get_agents_func) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)malloc(sizeof(agentos_dispatching_strategy_t));
    if (!strat) return NULL;

    priority_data_t* data = (priority_data_t*)malloc(sizeof(priority_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data);
        free(strat);
        return NULL;
    }

    strat->dispatch = priority_dispatch;
    strat->destroy = priority_destroy;
    strat->data = data;

    return strat;
}