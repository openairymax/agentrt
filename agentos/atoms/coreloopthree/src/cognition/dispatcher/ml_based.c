/**
 * @file ml_based.c
 * @brief ML-Based Dispatching Strategy Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <math.h>

typedef struct ml_dispatch_data {
    float* model_weights;
    size_t weight_count;
    char** model_names;
    size_t model_count;
    float learning_rate;
} ml_dispatch_data_t;

__attribute__((unused))
static float compute_model_score(
    ml_dispatch_data_t* data,
    const char* model_name,
    const char* task_type) {
    if (!data || !model_name) return 0.0f;

    float base_score = 0.5f;

    for (size_t i = 0; i < data->model_count; i++) {
        if (strcmp(data->model_names[i], model_name) == 0) {
            if (i < data->weight_count) {
                base_score = data->model_weights[i];
            }
            break;
        }
    }

    if (task_type) {
        if (strstr(task_type, "code")) base_score *= 1.2f;
        else if (strstr(task_type, "reasoning")) base_score *= 1.1f;
        else if (strstr(task_type, "creative")) base_score *= 0.9f;
    }

    return fminf(base_score, 1.0f);
}

static agentos_error_t ml_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {
    if (!context || !out_agent_id) return AGENTOS_EINVAL;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)context;

    if (data->model_count == 0) {
        *out_agent_id = AGENTOS_STRDUP("default");
        return AGENTOS_SUCCESS;
    }

    (void)task;
    (void)candidates;
    (void)count;

    float best_score = -1.0f;
    const char* best_model = data->model_names[0];

    for (size_t i = 0; i < data->model_count; i++) {
        float score = data->model_weights[i < data->weight_count ? i : 0];
        if (score > best_score) {
            best_score = score;
            best_model = data->model_names[i];
        }
    }

    *out_agent_id = AGENTOS_STRDUP(best_model);
    return AGENTOS_SUCCESS;
}

static void ml_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)strategy->data;
    if (data) {
        if (data->model_weights) AGENTOS_FREE(data->model_weights);
        if (data->model_names) {
            for (size_t i = 0; i < data->model_count; i++) {
                if (data->model_names[i]) AGENTOS_FREE(data->model_names[i]);
            }
            AGENTOS_FREE(data->model_names);
        }
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

agentos_dispatching_strategy_t* agentos_dispatching_ml_create(
    const char* model_path,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {
    (void)model_path;
    (void)registry_ctx;
    (void)get_agents_func;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)AGENTOS_CALLOC(1, sizeof(ml_dispatch_data_t));
    if (!data) return NULL;

    data->learning_rate = 0.1f;

    agentos_dispatching_strategy_t* strategy = (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_dispatching_strategy_t));
    if (!strategy) {
        AGENTOS_FREE(data);
        return NULL;
    }

    strategy->dispatch = ml_dispatch;
    strategy->destroy = ml_destroy;
    strategy->data = data;

    return strategy;
}
