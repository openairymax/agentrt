/**
 * @file reactive.c
 * @brief 反应式规划策略：快速生成单步或简单计划
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "cognition.h"
#include "llm_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct reactive_data {
    agentos_llm_service_t* llm;
    agentos_mutex_t* lock;
} reactive_data_t;

static void reactive_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    reactive_data_t* data = (reactive_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t reactive_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reactive_data_t* data = (reactive_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    // 使用LLM生成一个简单的单任务计划
    char prompt[2048];
    snprintf(prompt, sizeof(prompt),
        "Given the user goal, determine the single most appropriate task. "
        "Output a JSON object with 'task_name' and 'agent_role'. Goal: %s",
        intent->goal);

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = "gpt-3.5-turbo"; // 使用轻量模型
    req.prompt = prompt;
    req.temperature = 0.3;
    req.max_tokens = 256;

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(data->llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    // 解析响应（简化，直接使用LLM输出的文本作为任务名）
    char* task_name = resp->text;

    // 构建计划
    agentos_task_plan_t* plan = (agentos_task_plan_t*)calloc(1, sizeof(agentos_task_plan_t));
    if (!plan) {
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    plan->plan_id = strdup("reactive_plan");
    plan->nodes = (agentos_task_node_t**)malloc(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        free(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    agentos_task_node_t* node = (agentos_task_node_t*)calloc(1, sizeof(agentos_task_node_t));
    if (!node) {
        free(plan->nodes);
        free(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    node->task_id = strdup("task_1");
    node->agent_role = strdup("default"); // 实际应从LLM输出解析
    node->depends_on = NULL;
    node->depends_count = 0;
    node->timeout_ms = 30000;
    node->priority = 128;
    node->input = NULL;

    plan->nodes[0] = node;
    plan->node_count = 1;

    plan->entry_points = (char**)malloc(sizeof(char*));
    if (plan->entry_points) {
        plan->entry_count = 1;
        plan->entry_points[0] = node->task_id;
    }

    agentos_llm_response_free(resp);
    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

agentos_plan_strategy_t* agentos_plan_reactive_create(agentos_llm_service_t* llm) {
    if (!llm) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)malloc(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    reactive_data_t* data = (reactive_data_t*)malloc(sizeof(reactive_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data);
        free(strat);
        return NULL;
    }

    strat->plan = reactive_plan;
    strat->destroy = reactive_destroy;
    strat->data = data;

    return strat;
}