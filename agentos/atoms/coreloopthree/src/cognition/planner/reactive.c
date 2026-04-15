/**
 * @file reactive.c
 * @brief 反应式规划策略：快速生成单步或简单计�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include <agentos/cognition_common.h>
#include "llm_client.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>

typedef struct reactive_data {
    agentos_llm_service_t* llm;
    char* model_name;
    agentos_mutex_t* lock;
} reactive_data_t;

/**
 * @brief 销毁反应式规划策略
 */
static void reactive_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    reactive_data_t* data = (reactive_data_t*)strategy->data;
    if (data) {
        if (data->model_name) AGENTOS_FREE(data->model_name);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief 反应式规划：使用轻量模型快速生成单任务计划
 */
static agentos_error_t reactive_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reactive_data_t* data = (reactive_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    char prompt[2048];
    snprintf(prompt, sizeof(prompt),
        "Given the user goal, determine the single most appropriate task. "
        "Output a JSON object with 'task_name' and 'agent_role'. Goal: %s",
        intent->goal);

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = data->model_name ? data->model_name : "default";
    req.prompt = prompt;
    req.temperature = 0.3f;
    req.max_tokens = 256;

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(data->llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) {
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    plan->plan_id = AGENTOS_STRDUP("reactive_plan");
    plan->nodes = (agentos_task_node_t**)AGENTOS_MALLOC(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        if (plan->plan_id) AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
    if (!node) {
        if (plan->plan_id) AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan->nodes);
        AGENTOS_FREE(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    node->task_id = AGENTOS_STRDUP("task_1");
    node->agent_role = AGENTOS_STRDUP("default");
    node->depends_on = NULL;
    node->depends_count = 0;
    node->timeout_ms = 30000;
    node->priority = 128;
    node->input = NULL;

    if (!node->task_id || !node->agent_role) {
        if (node->task_id) AGENTOS_FREE(node->task_id);
        if (node->agent_role) AGENTOS_FREE(node->agent_role);
        AGENTOS_FREE(node);
        if (plan->plan_id) AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan->nodes);
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
static void reactive_set_config(agentos_plan_strategy_t* strategy,
                                const char* primary, const char* secondary) {
    (void)secondary;
    if (!strategy || !primary) return;
    reactive_data_t* data = (reactive_data_t*)strategy->data;
    if (!data) return;
    agentos_mutex_lock(data->lock);
    if (data->model_name) AGENTOS_FREE(data->model_name);
    data->model_name = AGENTOS_STRDUP(primary);
    agentos_mutex_unlock(data->lock);
}

/**
 * @brief 创建反应式规划策�?
 * @param llm LLM 服务
 * @return 策略指针，失败返�?NULL
 */
agentos_plan_strategy_t* agentos_plan_reactive_create(agentos_llm_service_t* llm) {
    if (!llm) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    reactive_data_t* data = (reactive_data_t*)AGENTOS_CALLOC(1, sizeof(reactive_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->plan = reactive_plan;
    strat->destroy = reactive_destroy;
    strat->set_config = reactive_set_config;
    strat->data = data;

    return strat;
}
