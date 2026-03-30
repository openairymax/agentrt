/**
 * @file round_robin.c
 * @brief ��ѯ���Ȳ��ԣ�����ѡ��Agent��
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "agent_registry.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <stdlib.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "../../../commons/utils/string/include/string_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <string.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

typedef struct round_robin_data {
    void* registry_ctx;
    agent_registry_get_agents_func get_agents;
    size_t current_index;                   /**< ��ǰ������ÿ����ɫ����������ȫ�֣� */
    agentos_mutex_t* lock;
} round_robin_data_t;

static void round_robin_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;
    round_robin_data_t* data = (round_robin_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
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
    *out_agent_id = AGENTOS_STRDUP(selected->agent_id);
    if (!*out_agent_id) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

agentos_dispatching_strategy_t* agentos_dispatching_round_robin_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {

    if (!get_agents_func) return NULL;

    agentos_dispatching_strategy_t* strat = (agentos_dispatching_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_dispatching_strategy_t));
    if (!strat) return NULL;

    round_robin_data_t* data = (round_robin_data_t*)AGENTOS_MALLOC(sizeof(round_robin_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->current_index = 0;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->dispatch = round_robin_dispatch;
    strat->destroy = round_robin_destroy;
    strat->data = data;

    return strat;
}
