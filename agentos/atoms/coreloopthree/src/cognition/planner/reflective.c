/**
 * @file reflective.c
 * @brief 反思式规划策略：结合历史经验调整计划
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "llm_client.h"
#include "strategy.h"
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct reflective_data {
    agentos_llm_service_t* llm;
    void* memory;
    char* model_name;
    agentos_mutex_t* lock;
} reflective_data_t;

static void reflective_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    reflective_data_t* data = (reflective_data_t*)strategy->data;
    if (data) {
        if (data->model_name) AGENTOS_FREE(data->model_name);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

static agentos_error_t reflective_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reflective_data_t* data = (reflective_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->task_plan_id = AGENTOS_STRDUP("reflective_plan");
    if (!plan->task_plan_id) {
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }

    agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
    if (!node) {
        AGENTOS_FREE(plan->task_plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }

    node->task_node_id = AGENTOS_STRDUP("reflective_task");
    node->task_node_agent_role = AGENTOS_STRDUP("default");

    if (!node->task_node_id || !node->task_node_agent_role) {
        if (node->task_node_id) AGENTOS_FREE(node->task_node_id);
        if (node->task_node_agent_role) AGENTOS_FREE(node->task_node_agent_role);
        AGENTOS_FREE(node);
        AGENTOS_FREE(plan->task_plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }

    plan->task_plan_nodes = (agentos_task_node_t**)AGENTOS_MALLOC(sizeof(agentos_task_node_t*));
    if (!plan->task_plan_nodes) {
        AGENTOS_FREE(node->task_node_id);
        AGENTOS_FREE(node->task_node_agent_role);
        AGENTOS_FREE(node);
        AGENTOS_FREE(plan->task_plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }
    plan->task_plan_nodes[0] = node;
    plan->task_plan_node_count = 1;

    plan->task_plan_entry_points = (char**)AGENTOS_MALLOC(sizeof(char*));
    if (plan->task_plan_entry_points) {
        plan->task_plan_entry_count = 1;
        plan->task_plan_entry_points[0] = AGENTOS_STRDUP(node->task_node_id);
        if (!plan->task_plan_entry_points[0]) {
            AGENTOS_FREE(plan->task_plan_entry_points);
            plan->task_plan_entry_count = 0;
        }
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

agentos_plan_strategy_t* agentos_plan_reflective_create(
    agentos_llm_service_t* llm,
    agentos_memory_engine_t* memory_engine) {

    if (!llm) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    reflective_data_t* data = (reflective_data_t*)AGENTOS_CALLOC(1, sizeof(reflective_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->llm = llm;
    data->memory = memory_engine;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->plan = reflective_plan;
    strat->destroy = reflective_destroy;
    strat->data = data;

    return strat;
}
