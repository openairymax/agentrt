/**
 * @file dual_model.c
 * @brief 双模型协调器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

/**
 * @brief 双模型协调器上下文
 */
typedef struct dual_model_coordinator {
    agentos_coordinator_base_t base;
    char* primary_model;
    char* secondary_model;
    float primary_weight;
    float secondary_weight;
} dual_model_coordinator_t;

/**
 * @brief 协调执行
 */
static agentos_error_t dual_coordinate(
    agentos_coordinator_base_t* base,
    const agentos_coordination_context_t* context,
    const char** inputs,
    size_t input_count,
    char** out_result) {
    if (!base || !context || !out_result) {
        return AGENTOS_EINVAL;
    }

    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;

    size_t total_len = 256;
    char* result = (char*)AGENTOS_MALLOC(total_len);
    if (!result) return AGENTOS_ENOMEM;

    // 简单实现：根据权重选择主模型或次模型的输出
    if (coordinator->primary_weight >= coordinator->secondary_weight && input_count > 0) {
        snprintf(result, total_len, "[Primary] %s", inputs[0]);
    } else if (input_count > 1) {
        snprintf(result, total_len, "[Secondary] %s", inputs[1]);
    } else if (input_count > 0) {
        snprintf(result, total_len, "[Fallback] %s", inputs[0]);
    } else {
        snprintf(result, total_len, "[Empty]");
    }

    *out_result = result;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁协调器
 */
static void dual_destroy(agentos_coordinator_base_t* base) {
    if (!base) return;
    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;
    if (coordinator->primary_model) AGENTOS_FREE(coordinator->primary_model);
    if (coordinator->secondary_model) AGENTOS_FREE(coordinator->secondary_model);
    AGENTOS_FREE(base);
}

/**
 * @brief 创建双模型协调器
 */
agentos_error_t agentos_coordinator_dual_model_create(
    const char* primary_model,
    const char* secondary_model,
    float primary_weight,
    float secondary_weight,
    agentos_coordinator_base_t** out_base) {
    if (!out_base) return AGENTOS_EINVAL;

    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)AGENTOS_CALLOC(1, sizeof(dual_model_coordinator_t));
    if (!coordinator) return AGENTOS_ENOMEM;

    if (primary_model) {
        coordinator->primary_model = AGENTOS_STRDUP(primary_model);
        if (!coordinator->primary_model) {
            AGENTOS_FREE(coordinator);
            return AGENTOS_ENOMEM;
        }
    }

    if (secondary_model) {
        coordinator->secondary_model = AGENTOS_STRDUP(secondary_model);
        if (!coordinator->secondary_model) {
            if (coordinator->primary_model) AGENTOS_FREE(coordinator->primary_model);
            AGENTOS_FREE(coordinator);
            return AGENTOS_ENOMEM;
        }
    }

    coordinator->primary_weight = primary_weight;
    coordinator->secondary_weight = secondary_weight;

    coordinator->base.coordinate = dual_coordinate;
    coordinator->base.destroy = dual_destroy;

    *out_base = &coordinator->base;
    return AGENTOS_SUCCESS;
}
