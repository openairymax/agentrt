/**
 * @file reflective.c
 * @brief 反思式规划策略：结合历史经验调整计划
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "cognition.h"
#include "llm_client.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct reflective_data {
    agentos_llm_service_t* llm;
    agentos_memory_engine_t* memory;   /**< 记忆引擎 */
    agentos_mutex_t* lock;
} reflective_data_t;

static void reflective_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    reflective_data_t* data = (reflective_data_t*)strategy->data;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

static agentos_error_t reflective_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reflective_data_t* data = (reflective_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    // 从记忆引擎检索类似意图的历史计划
    agentos_memory_query_t query;
    memset(&query, 0, sizeof(query));
    query.text = intent->goal;
    query.text_len = strlen(intent->goal);
    query.limit = 5;
    query.include_raw = 0; // 不需要原始数据

    agentos_memory_result_t* mem_result = NULL;
    agentos_error_t err = agentos_memory_query(data->memory, &query, &mem_result);
    if (err == AGENTOS_SUCCESS && mem_result && mem_result->count > 0) {
        // 有历史计划，可以复用或调整
        // 简化：直接使用第一个历史计划
        // 实际需要反序列化等
        // 这里仅示意
        agentos_memory_result_free(mem_result);
        // 暂时用回退方案
    } else {
        if (mem_result) agentos_memory_result_free(mem_result);
    }

    // 回退：使用反应式规划生成一个简单计划
    char prompt[2048];
    snprintf(prompt, sizeof(prompt),
        "Based on the user goal, generate a task plan. Goal: %s",
        intent->goal);

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = "gpt-4";
    req.prompt = prompt;
    req.temperature = 0.7;
    req.max_tokens = 1024;

    agentos_llm_response_t* resp = NULL;
    err = agentos_llm_complete(data->llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    // 简单创建一个计划（实际应解析LLM输出）
    agentos_task_plan_t* plan = (agentos_task_plan_t*)calloc(1, sizeof(agentos_task_plan_t));
    if (!plan) {
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    plan->plan_id = strdup("reflective_plan");
    plan->nodes = NULL;
    plan->node_count = 0;
    plan->entry_points = NULL;
    plan->entry_count = 0;

    // 创建一个任务节点
    agentos_task_node_t* node = (agentos_task_node_t*)calloc(1, sizeof(agentos_task_node_t));
    if (!node) {
        free(plan->plan_id);
        free(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }

    node->task_id = strdup("reflective_task");
    node->agent_role = strdup("default");
    node->depends_on = NULL;
    node->depends_count = 0;
    node->timeout_ms = 30000;
    node->priority = 128;
    node->input = NULL;

    plan->nodes = (agentos_task_node_t**)malloc(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        free(node->task_id);
        free(node->agent_role);
        free(node);
        free(plan->plan_id);
        free(plan);
        agentos_llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }
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

agentos_plan_strategy_t* agentos_plan_reflective_create(
    agentos_llm_service_t* llm,
    agentos_memory_engine_t* memory_engine) {

    if (!llm || !memory_engine) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)malloc(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    reflective_data_t* data = (reflective_data_t*)malloc(sizeof(reflective_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->llm = llm;
    data->memory = memory_engine;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data);
        free(strat);
        return NULL;
    }

    strat->plan = reflective_plan;
    strat->destroy = reflective_destroy;
    strat->data = data;

    return strat;
}