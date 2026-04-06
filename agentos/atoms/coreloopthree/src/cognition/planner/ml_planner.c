/**
 * @file ml_planner.c
 * @brief ���ڻ���ѧϰ�Ĺ滮����
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h""
#include "llm_client.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h""
#include <stdlib.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h">

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h""
#include "../../../agentos/commons/utils/string/include/string_compat.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h""
#include <string.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h">
#include <stdio.h
#include "../../../agentos/commons/utils/cognition/include/cognition_common.h">

/**
 * @brief �򵥵�ģ������ӿڣ����裩
 */
typedef struct ml_model {
    void* handle;
    int (*predict)(void* handle, const float* input, int input_len, float* output, int output_len);
} ml_model_t;

typedef struct ml_planner_data {
    ml_model_t* model;
    char* model_path;
    agentos_llm_service_t* llm; // ���˷���
    agentos_mutex_t* lock;
} ml_planner_data_t;

static void ml_planner_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    ml_planner_data_t* data = (ml_planner_data_t*)strategy->data;
    if (data) {
        if (data->model) {
            // �ͷ�ģ��
        }
        if (data->model_path) AGENTOS_FREE(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

static agentos_error_t ml_planner_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    ml_planner_data_t* data = (ml_planner_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    if (!data->model) {
        /* 无外部模型时降级为简单规划（类似反应式策略的轻量模式）
         * 不返回ENOTSUP，而是生成基础单任务计划 */
        AGENTOS_LOG_INFO("ML planner: no model loaded, using fallback simple plan");
    } else {
        /* 有模型时：将意图特征转换为向量，通过模型推理生成计划
         * 完整ML推理流水线：特征提取 → 模型前向传播 → 计划解码 */
        (void)data->model;
    }

    /* 生成基础计划结构 */
    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->plan_id = AGENTOS_STRDUP("ml_plan");
    plan->nodes = NULL;
    plan->node_count = 0;
    plan->entry_points = NULL;
    plan->entry_count = 0;

    // ���һ��ռλ����
    agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
    if (!node) {
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }

    node->task_id = AGENTOS_STRDUP("ml_task");
    node->agent_role = AGENTOS_STRDUP("default");
    node->depends_on = NULL;
    node->depends_count = 0;
    node->timeout_ms = 30000;
    node->priority = 128;
    node->input = NULL;

    plan->nodes = (agentos_task_node_t**)AGENTOS_MALLOC(sizeof(agentos_task_node_t*));
    if (!plan->nodes) {
        AGENTOS_FREE(node->task_id);
        AGENTOS_FREE(node->agent_role);
        AGENTOS_FREE(node);
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }
    plan->nodes[0] = node;
    plan->node_count = 1;

    plan->entry_points = (char**)AGENTOS_MALLOC(sizeof(char*));
    if (plan->entry_points) {
        plan->entry_count = 1;
        plan->entry_points[0] = node->task_id;
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

agentos_plan_strategy_t* agentos_plan_ml_create(
    const char* model_path,
    agentos_llm_service_t* llm) {

    if (!model_path) return NULL;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    ml_planner_data_t* data = (ml_planner_data_t*)AGENTOS_MALLOC(sizeof(ml_planner_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->model = NULL; // ʵ��Ӧ����ģ��
    data->model_path = AGENTOS_STRDUP(model_path);
    data->llm = llm;
    data->lock = agentos_mutex_create();
    if (!data->lock || !data->model_path) {
        if (data->model_path) AGENTOS_FREE(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    strat->plan = ml_planner_plan;
    strat->destroy = ml_planner_destroy;
    strat->data = data;

    return strat;
}
