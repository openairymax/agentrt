/**
 * @file reflective.c
 * @brief 反思式规划策略：结合历史经验调整计�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include <agentos/cognition_common.h>
#include "llm_client.h"
#include "memory.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>
#include <stdio.h>

typedef struct reflective_data {
    agentos_llm_service_t* llm;
    agentos_memory_engine_t* memory;
    char* model_name;
    agentos_mutex_t* lock;
} reflective_data_t;

/**
 * @brief 销毁反思式规划策略
 */
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

/**
 * @brief 反思式规划：检索历史记忆后生成优化计划
 */
static agentos_error_t reflective_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reflective_data_t* data = (reflective_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    if (data->memory) {
        agentos_memory_query_t query;
        memset(&query, 0, sizeof(query));
        query.text = intent->goal;
        query.text_len = strlen(intent->goal);
        query.limit = 5;
        query.include_raw = 0;

        agentos_memory_result_t* mem_result = NULL;
        agentos_error_t err = agentos_memory_query(data->memory, &query, &mem_result);
        if (err == AGENTOS_SUCCESS && mem_result) {
            agentos_memory_result_free(mem_result);
        }
    }

    char prompt[2048];
    snprintf(prompt, sizeof(prompt),
        "Based on the user goal, generate a task plan. Goal: %s",
        intent->goal);

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = data->model_name ? data->model_name : "default";
    req.prompt = prompt;
    req.temperature = 0.7f;
    req.max_tokens = 1024;

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(data->llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) {
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    plan->plan_id = AGENTOS_STRDUP("reflective_plan");
    if (!plan->plan_id) {
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
    if (!node) {
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    node->task_id = AGENTOS_STRDUP("reflective_task");
    node->agent_role = AGENTOS_STRDUP("default");
    node->timeout_ms = 30000;
    node->priority = 128;

    if (!node->task_id || !node->agent_role) {
        if (node->task_id) AGENTOS_FREE(node->task_id);
        if (node->agent_role) AGENTOS_FREE(node->agent_role);
        AGENTOS_FREE(node);
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    plan->nodes = (agentos_task_node_t**)AGENTOS_MALLOC(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        AGENTOS_FREE(node->task_id);
        AGENTOS_FREE(node->agent_role);
        AGENTOS_FREE(node);
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }
    plan->nodes[0] = node;
    plan->node_count = 1;

    plan->entry_points = (char**)AGENTOS_MALLOC(sizeof(char*));
    if (plan->entry_points) {
        plan->entry_count = 1;
        plan->entry_points[0] = node->task_id;
    }

    agentos_llm_response_free(resp);
    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置模型配置
 */
static void reflective_set_config(agentos_plan_strategy_t* strategy,
                                  const char* primary, const char* secondary) {
    (void)secondary;
    if (!strategy || !primary) return;
    reflective_data_t* data = (reflective_data_t*)strategy->data;
    if (!data) return;
    agentos_mutex_lock(data->lock);
    if (data->model_name) AGENTOS_FREE(data->model_name);
    data->model_name = AGENTOS_STRDUP(primary);
    agentos_mutex_unlock(data->lock);
}

/**
 * @brief 创建反思式规划策略
 * @param llm LLM 服务
 * @param memory_engine 记忆引擎
 * @return 策略指针，失败返�?NULL
 */
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
    strat->set_config = reflective_set_config;
    strat->data = data;

    return strat;
}
