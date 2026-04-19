/**
 * @file weighted.c
 * @brief 加权融合策略（根据权重组合多个模型输出）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 加权融合私有数据
 */
typedef struct weighted_data {
    char** model_names;           /**< 模型名称数组 */
    float* weights;                /**< 对应权重 */
    size_t model_count;            /**< 模型数量 */
    agentos_mutex_t* lock;
} weighted_data_t;

static void weighted_destroy(agentos_coordinator_base_t* base) {
    if (!base) return;
    weighted_data_t* data = (weighted_data_t*)base->data;
    if (data) {
        for (size_t i = 0; i < data->model_count; i++) {
            if (data->model_names[i]) AGENTOS_FREE(data->model_names[i]);
        }
        AGENTOS_FREE(data->model_names);
        AGENTOS_FREE(data->weights);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(base);
}

/**
 * @brief 加权融合执行
 */
static agentos_error_t weighted_coordinate(
    agentos_coordinator_base_t* base,
    const agentos_coordination_context_t* context,
    const char** inputs,
    size_t input_count,
    char** out_result) {
    (void)context;
    if (!base || !out_result) return AGENTOS_EINVAL;

    weighted_data_t* data = (weighted_data_t*)base->data;
    if (!data || !inputs || input_count == 0) {
        *out_result = AGENTOS_STRDUP("invalid_input");
        return AGENTOS_EINVAL;
    }

    // 简单实现：选择权重最高的输入
    // 生产环境应实现真正的加权融合算法
    float max_weight = 0.0f;
    size_t best_index = 0;

    for (size_t i = 0; i < input_count && i < data->model_count; i++) {
        if (data->weights[i] > max_weight) {
            max_weight = data->weights[i];
            best_index = i;
        }
    }

    *out_result = AGENTOS_STRDUP(inputs[best_index]);
    if (!*out_result) return AGENTOS_ENOMEM;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建加权融合协调器
 */
agentos_error_t agentos_coordinator_weighted_create(
    const char** model_names,
    const float* weights,
    size_t model_count,
    agentos_coordinator_base_t** out_base) {
    if (!out_base || !model_names || !weights || model_count == 0) {
        return AGENTOS_EINVAL;
    }

    agentos_coordinator_base_t* base = (agentos_coordinator_base_t*)AGENTOS_CALLOC(1, sizeof(agentos_coordinator_base_t));
    if (!base) return AGENTOS_ENOMEM;

    weighted_data_t* data = (weighted_data_t*)AGENTOS_CALLOC(1, sizeof(weighted_data_t));
    if (!data) {
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    data->model_count = model_count;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        AGENTOS_FREE(data);
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    // 复制模型名称
    data->model_names = (char**)AGENTOS_CALLOC(model_count, sizeof(char*));
    if (!data->model_names) {
        agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < model_count; i++) {
        data->model_names[i] = AGENTOS_STRDUP(model_names[i]);
        if (!data->model_names[i]) {
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE(data->model_names[j]);
            }
            AGENTOS_FREE(data->model_names);
            agentos_mutex_destroy(data->lock);
            AGENTOS_FREE(data);
            AGENTOS_FREE(base);
            return AGENTOS_ENOMEM;
        }
    }

    // 复制权重
    data->weights = (float*)AGENTOS_CALLOC(model_count, sizeof(float));
    if (!data->weights) {
        for (size_t i = 0; i < model_count; i++) {
            AGENTOS_FREE(data->model_names[i]);
        }
        AGENTOS_FREE(data->model_names);
        agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    memcpy(data->weights, weights, model_count * sizeof(float));

    base->data = data;
    base->coordinate = weighted_coordinate;
    base->destroy = weighted_destroy;

    *out_base = base;
    return AGENTOS_SUCCESS;
}
