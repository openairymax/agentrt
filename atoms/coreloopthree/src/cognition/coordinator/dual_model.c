/**
 * @file dual_model.c
 * @brief 双模型协调器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "agentos.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <stdlib.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include "../../../commons/utils/string/include/string_compat.h
#include "../../../commons/utils/cognition/include/cognition_common.h""
#include <string.h
#include "../../../commons/utils/cognition/include/cognition_common.h">

/**
 * @brief 双模型协调器上下�?
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

    snprintf(result, total_len,
        "Dual model coordination: primary=%s(%.2f), secondary=%s(%.2f)",
        coordinator->primary_model ? coordinator->primary_model : "none",
        coordinator->primary_weight,
        coordinator->secondary_model ? coordinator->secondary_model : "none",
        coordinator->secondary_weight);

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
    AGENTOS_FREE(coordinator);
}

/**
 * @brief 创建双模型协调器
 */
agentos_error_t agentos_coordinator_dual_create(
    const char* primary_model,
    const char* secondary_model,
    float primary_weight,
    agentos_coordinator_base_t** out_coordinator) {
    if (!out_coordinator) return AGENTOS_EINVAL;

    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)
        AGENTOS_CALLOC(1, sizeof(dual_model_coordinator_t));
    if (!coordinator) return AGENTOS_ENOMEM;

    coordinator->base.coordinate = dual_coordinate;
    coordinator->base.destroy = dual_destroy;

    coordinator->primary_model = primary_model ? AGENTOS_STRDUP(primary_model) : NULL;
    coordinator->secondary_model = secondary_model ? AGENTOS_STRDUP(secondary_model) : NULL;
    coordinator->primary_weight = primary_weight > 0 ? primary_weight : 0.7f;
    coordinator->secondary_weight = 1.0f - coordinator->primary_weight;

    *out_coordinator = &coordinator->base;
    return AGENTOS_SUCCESS;
}
