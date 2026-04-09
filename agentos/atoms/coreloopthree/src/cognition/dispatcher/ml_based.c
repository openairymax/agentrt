/**
 * @file ml_based.c
 * @brief 基于机器学习的调度器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <math.h>

/**
 * @brief ML调度器上下文
 */
typedef struct ml_dispatcher {
    agentos_dispatcher_base_t base;
    float* model_weights;
    size_t weight_count;
    char** model_names;
    size_t model_count;
    float learning_rate;
} ml_dispatcher_t;

/**
 * @brief 计算模型得分
 */
static float compute_model_score(
    ml_dispatcher_t* dispatcher,
    const char* model_name,
    const char* task_type) {
    if (!dispatcher || !model_name) return 0.0f;

    float base_score = 0.5f;

    for (size_t i = 0; i < dispatcher->model_count; i++) {
        if (strcmp(dispatcher->model_names[i], model_name) == 0) {
            if (i < dispatcher->weight_count) {
                base_score = dispatcher->model_weights[i];
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

/**
 * @brief 选择最佳模�?
 */
static agentos_error_t ml_select(
    agentos_dispatcher_base_t* base,
    const agentos_dispatch_context_t* context,
    char** out_model_id) {
    if (!base || !context || !out_model_id) {
        return AGENTOS_EINVAL;
    }

    ml_dispatcher_t* dispatcher = (ml_dispatcher_t*)base;

    if (dispatcher->model_count == 0) {
        *out_model_id = AGENTOS_STRDUP("default");
        return AGENTOS_SUCCESS;
    }

    float best_score = -1.0f;
    const char* best_model = dispatcher->model_names[0];

    for (size_t i = 0; i < dispatcher->model_count; i++) {
        float score = compute_model_score(
            dispatcher,
            dispatcher->model_names[i],
            context->task_type);
        if (score > best_score) {
            best_score = score;
            best_model = dispatcher->model_names[i];
        }
    }

    *out_model_id = AGENTOS_STRDUP(best_model);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 更新模型权重（在线学习）
 */
static agentos_error_t ml_update_weights(
    ml_dispatcher_t* dispatcher,
    const char* model_name,
    float reward) {
    if (!dispatcher || !model_name) return AGENTOS_EINVAL;

    for (size_t i = 0; i < dispatcher->model_count; i++) {
        if (strcmp(dispatcher->model_names[i], model_name) == 0) {
            if (i < dispatcher->weight_count) {
                float old_weight = dispatcher->model_weights[i];
                dispatcher->model_weights[i] += dispatcher->learning_rate * (reward - old_weight);
                dispatcher->model_weights[i] = fmaxf(0.0f, fminf(1.0f, dispatcher->model_weights[i]));
            }
            break;
        }
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁ML调度�?
 */
static void ml_destroy(agentos_dispatcher_base_t* base) {
    if (!base) return;

    ml_dispatcher_t* dispatcher = (ml_dispatcher_t*)base;
    if (dispatcher->model_weights) AGENTOS_FREE(dispatcher->model_weights);
    if (dispatcher->model_names) {
        for (size_t i = 0; i < dispatcher->model_count; i++) {
            if (dispatcher->model_names[i]) AGENTOS_FREE(dispatcher->model_names[i]);
        }
        AGENTOS_FREE(dispatcher->model_names);
    }
    AGENTOS_FREE(dispatcher);
}

/**
 * @brief 创建ML调度�?
 */
agentos_error_t agentos_dispatcher_ml_create(
    const char** model_names,
    size_t model_count,
    agentos_dispatcher_base_t** out_dispatcher) {
    if (!out_dispatcher) return AGENTOS_EINVAL;

    ml_dispatcher_t* dispatcher = (ml_dispatcher_t*)
        AGENTOS_CALLOC(1, sizeof(ml_dispatcher_t));
    if (!dispatcher) return AGENTOS_ENOMEM;

    dispatcher->base.select = ml_select;
    dispatcher->base.destroy = ml_destroy;
    dispatcher->learning_rate = 0.1f;

    if (model_names && model_count > 0) {
        dispatcher->model_count = model_count;
        dispatcher->weight_count = model_count;
        dispatcher->model_weights = (float*)AGENTOS_CALLOC(model_count, sizeof(float));
        dispatcher->model_names = (char**)AGENTOS_CALLOC(model_count, sizeof(char*));

        if (!dispatcher->model_weights || !dispatcher->model_names) {
            if (dispatcher->model_weights) AGENTOS_FREE(dispatcher->model_weights);
            if (dispatcher->model_names) AGENTOS_FREE(dispatcher->model_names);
            AGENTOS_FREE(dispatcher);
            return AGENTOS_ENOMEM;
        }

        for (size_t i = 0; i < model_count; i++) {
            dispatcher->model_weights[i] = 1.0f / model_count;
            dispatcher->model_names[i] = AGENTOS_STRDUP(model_names[i]);
        }
    }

    *out_dispatcher = &dispatcher->base;
    return AGENTOS_SUCCESS;
}
