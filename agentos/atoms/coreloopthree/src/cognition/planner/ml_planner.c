/**
 * @file ml_planner.c
 * @brief ML-based planning strategy with graceful degradation
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#include "../../include/cognition.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"

/**
 * @brief Simple model inference interface (placeholder for actual ML runtime)
 */
typedef struct ml_model {
    void* handle;
    int (*predict)(void* handle, const float* input, int input_len, float* output, int output_len);
} ml_model_t;

typedef struct ml_planner_data {
    ml_model_t* model;
    char* model_path;
    void* llm;
    agentos_mutex_t* lock;
    bool fallback_mode;
} ml_planner_data_t;

static void ml_planner_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    ml_planner_data_t* data = (ml_planner_data_t*)strategy->data;
    if (data) {
        if (data->model) {
            if (data->model->handle) {
                /* TODO: Unload actual ML model via runtime API */
            }
            AGENTOS_FREE(data->model);
        }
        if (data->model_path) AGENTOS_FREE(data->model_path);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief Attempt to load ML model from path
 *
 * When no model runtime is available, sets fallback_mode=true
 * and generates simple heuristic plans instead.
 */
static bool ml_planner_try_load_model(ml_planner_data_t* data) {
    if (!data || !data->model_path) return false;

    /* Check if model file exists */
    FILE* f = fopen(data->model_path, "rb");
    if (!f) {
        AGENTOS_LOG_INFO("ML planner: model file not found, using fallback mode");
        data->fallback_mode = true;
        return false;
    }
    fclose(f);

    /* TODO-PHASE2: Load actual ML model when runtime is integrated (延期到第二阶段)
     * Expected flow:
     *   1. Initialize ML runtime (ONNX/TensorFlow Lite/custom)
     *   2. Load model from data->model_path
     *   3. Set data->model with handle and predict function
     *   4. Set data->fallback_mode = false
     */
    data->model = (ml_model_t*)AGENTOS_CALLOC(1, sizeof(ml_model_t));
    if (!data->model) {
        data->fallback_mode = true;
        return false;
    }

    data->model->handle = NULL;
    data->model->predict = NULL;
    data->fallback_mode = true;

    AGENTOS_LOG_INFO("ML planner: model placeholder initialized, fallback mode active");
    return true;
}

/**
 * @brief Generate a fallback plan using heuristic rules
 *
 * When no ML model is available, generates a simple single-task plan
 * based on intent type analysis.
 */
static agentos_error_t ml_planner_fallback_plan(
    const agentos_intent_t* intent,
    agentos_task_plan_t** out_plan) {

    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->plan_id = AGENTOS_STRDUP("ml_fallback_plan");
    plan->nodes = NULL;
    plan->node_count = 0;
    plan->entry_points = NULL;
    plan->entry_count = 0;

    agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
    if (!node) {
        AGENTOS_FREE(plan->plan_id);
        AGENTOS_FREE(plan);
        return AGENTOS_ENOMEM;
    }

    /* Assign task based on intent priority heuristic */
    node->task_id = AGENTOS_STRDUP("ml_fallback_task");
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

static agentos_error_t ml_planner_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    ml_planner_data_t* data = (ml_planner_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    if (!data->model || data->fallback_mode) {
        /* No ML model available: use heuristic fallback planning
         * This provides a functional baseline while ML integration is pending */
        AGENTOS_LOG_INFO("ML planner: using fallback heuristic plan");
        return ml_planner_fallback_plan(intent, out_plan);
    }

    /* ML model available: full inference pipeline
     * Feature extraction -> Model forward pass -> Plan decoding */
    if (data->model->predict && data->model->handle) {
        /* TODO-PHASE2: Implement full ML inference pipeline (延期到第二阶段)
         * 1. Extract features from intent (type, complexity, context)
         * 2. Run model predict() to get task decomposition
         * 3. Decode model output into agentos_task_plan_t structure
         * 4. Validate plan feasibility
         */
    }

    /* Fallback if model predict is not yet implemented */
    return ml_planner_fallback_plan(intent, out_plan);
}

agentos_plan_strategy_t* agentos_plan_ml_create(
    const char* model_path,
    void* llm) {

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_MALLOC(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    ml_planner_data_t* data = (ml_planner_data_t*)AGENTOS_CALLOC(1, sizeof(ml_planner_data_t));
    if (!data) {
        AGENTOS_FREE(strat);
        return NULL;
    }

    data->model = NULL;
    data->model_path = model_path ? AGENTOS_STRDUP(model_path) : NULL;
    data->llm = llm;
    data->fallback_mode = true;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        if (data->model_path) AGENTOS_FREE(data->model_path);
        AGENTOS_FREE(data);
        AGENTOS_FREE(strat);
        return NULL;
    }

    /* Attempt to load model if path provided */
    if (model_path) {
        ml_planner_try_load_model(data);
    }

    strat->plan = ml_planner_plan;
    strat->destroy = ml_planner_destroy;
    strat->data = data;

    return strat;
}
