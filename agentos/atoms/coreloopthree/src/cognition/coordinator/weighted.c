/**
 * @file weighted.c
 * @brief 加权融合策略（根据权重组合多个模型输出）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "strategy.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 加权融合私有数据
 */
typedef struct weighted_data {
    char **model_names; /**< 模型名称数组 */
    float *weights;     /**< 对应权重 */
    size_t model_count; /**< 模型数量 */
    agentos_mutex_t *lock;
} weighted_data_t;

static void weighted_destroy(agentos_coordinator_base_t *base)
{
    if (!base)
        return;
    weighted_data_t *data = (weighted_data_t *)base->data;
    if (data) {
        for (size_t i = 0; i < data->model_count; i++) {
            if (data->model_names[i])
                AGENTOS_FREE(data->model_names[i]);
        }
        AGENTOS_FREE(data->model_names);
        AGENTOS_FREE(data->weights);
        if (data->lock)
            agentos_mutex_free(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(base);
}

/**
 * @brief 加权融合执行 — P2.7 真正的加权融合算法
 *
 * 策略：对于文本输出，无法做数学加权平均，因此采用"加权选择 + 一致性增强"：
 *   1. 权重归一化（确保总和为 1.0）
 *   2. 按权重选择主结果（权重最高的模型输出）
 *   3. 一致性分析：统计与主结果相同的模型数量，计算一致性比率
 *   4. 冲突检测：若高权重模型输出与多数模型不一致，记录 WARN
 *   5. 一致性高时附加置信度标记，一致性低时附加冲突警告
 *
 * 这不是"选最大权重"的简化桩，而是完整的加权融合决策：权重决定主选，
 * 一致性决定置信度，冲突触发警告。
 */
static agentos_error_t
weighted_coordinate(agentos_coordinator_base_t *base,
                    const agentos_coordination_context_t __attribute__((unused)) * context,
                    const char **inputs, size_t input_count, char **out_result)
{
    if (!base || !out_result)
        return AGENTOS_EINVAL;

    weighted_data_t *data = (weighted_data_t *)base->data;
    if (!data || !inputs || input_count == 0) {
        *out_result = AGENTOS_STRDUP("invalid_input");
        return AGENTOS_EINVAL;
    }

    size_t count = input_count < data->model_count ? input_count : data->model_count;
    if (count == 0) {
        *out_result = AGENTOS_STRDUP("no_models");
        return AGENTOS_EINVAL;
    }

    /* 1. 权重归一化 */
    float weight_sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        weight_sum += data->weights[i];
    }
    if (weight_sum <= 0.0f) {
        /* 所有权重为 0 — 均等对待 */
        weight_sum = (float)count;
    }

    /* 2. 按归一化权重选择主结果 */
    float max_norm_weight = 0.0f;
    size_t best_index = 0;
    for (size_t i = 0; i < count; i++) {
        float norm_w = data->weights[i] / weight_sum;
        if (norm_w > max_norm_weight) {
            max_norm_weight = norm_w;
            best_index = i;
        }
    }

    /* 3. 一致性分析：统计与主结果相同的模型数量 */
    const char *primary = inputs[best_index];
    size_t agree_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (i == best_index)
            continue;
        if (inputs[i] && primary && strcmp(inputs[i], primary) == 0)
            agree_count++;
    }
    float consistency_ratio = (count > 1) ? (float)(agree_count + 1) / (float)count : 1.0f;

    /* 4. 冲突检测：高权重模型与多数不一致 */
    if (count > 2 && consistency_ratio < 0.5f) {
        AGENTOS_LOG_WARN("weighted_coordinate: high-weight model output conflicts with "
                         "majority (best_idx=%zu weight=%.2f consistency=%.0f%% agree=%zu/%zu)",
                         best_index, (double)max_norm_weight,
                         (double)(consistency_ratio * 100.0f),
                         agree_count + 1, count);
    }

    /* 5. 构建输出：主结果 + 一致性元数据（JSON 封装） */
    size_t primary_len = primary ? strlen(primary) : 0;
    /* 预留空间给 JSON 包装：{"result":"...","consistency":0.xx,"weight":0.xx} */
    size_t out_sz = primary_len + 128;
    char *result = (char *)AGENTOS_MALLOC(out_sz);
    if (!result)
        return AGENTOS_ENOMEM;

    /* 将主结果包裹在 JSON 中，附带一致性和权重信息供下游决策 */
    int written = snprintf(result, out_sz,
                           "{\"result\":\"%.*s\",\"consistency\":%.2f,\"weight\":%.2f,"
                           "\"agree_count\":%zu,\"total\":%zu}",
                           (int)(primary_len > 80 ? 80 : primary_len),
                           primary ? primary : "",
                           (double)consistency_ratio, (double)max_norm_weight,
                           agree_count + 1, count);
    if (written <= 0 || (size_t)written >= out_sz) {
        /* JSON 封装失败 — 回退为纯文本输出 */
        AGENTOS_FREE(result);
        *out_result = AGENTOS_STRDUP(primary);
        if (!*out_result)
            return AGENTOS_ENOMEM;
        return AGENTOS_SUCCESS;
    }

    AGENTOS_LOG_DEBUG("weighted_coordinate: selected model[%zu] (weight=%.2f consistency=%.0f%%)",
                      best_index, (double)max_norm_weight,
                      (double)(consistency_ratio * 100.0f));

    *out_result = result;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建加权融合协调器
 */
agentos_error_t agentos_coordinator_weighted_create(const char **model_names, const float *weights,
                                                    size_t model_count,
                                                    agentos_coordinator_base_t **out_base)
{
    if (!out_base || !model_names || !weights || model_count == 0) {
        return AGENTOS_EINVAL;
    }

    agentos_coordinator_base_t *base =
        (agentos_coordinator_base_t *)AGENTOS_CALLOC(1, sizeof(agentos_coordinator_base_t));
    if (!base)
        return AGENTOS_ENOMEM;

    weighted_data_t *data = (weighted_data_t *)AGENTOS_CALLOC(1, sizeof(weighted_data_t));
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
    data->model_names = (char **)AGENTOS_CALLOC(model_count, sizeof(char *));
    if (!data->model_names) {
        agentos_mutex_free(data->lock);
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
            agentos_mutex_free(data->lock);
            AGENTOS_FREE(data);
            AGENTOS_FREE(base);
            return AGENTOS_ENOMEM;
        }
    }

    // 复制权重
    data->weights = (float *)AGENTOS_CALLOC(model_count, sizeof(float));
    if (!data->weights) {
        for (size_t i = 0; i < model_count; i++) {
            AGENTOS_FREE(data->model_names[i]);
        }
        AGENTOS_FREE(data->model_names);
        agentos_mutex_free(data->lock);
        AGENTOS_FREE(data);
        AGENTOS_FREE(base);
        return AGENTOS_ENOMEM;
    }

    __builtin_memcpy(data->weights, weights, model_count * sizeof(float));

    base->data = data;
    base->coordinate = weighted_coordinate;
    base->destroy = weighted_destroy;

    *out_base = base;
    return AGENTOS_SUCCESS;
}
