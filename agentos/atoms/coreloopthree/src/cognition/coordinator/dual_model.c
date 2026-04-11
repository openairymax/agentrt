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
#include "../intent_utils.h"  /* for intent_string_similarity */
#include <string.h>

/**
 * @brief 交叉验证模式
 */
typedef enum {
    CROSS_VALIDATION_NONE = 0,     /**< 不启用交叉验证 */
    CROSS_VALIDATION_BASIC = 1,    /**< 基础交叉验证：仅检查一致性 */
    CROSS_VALIDATION_ADVANCED = 2, /**< 高级交叉验证：包含置信度评估和仲裁 */
} cross_validation_mode_t;

/**
 * @brief 双模型协调器上下文
 */
typedef struct dual_model_coordinator {
    agentos_coordinator_base_t base;
    char* primary_model;
    char* secondary_model;
    float primary_weight;
    float secondary_weight;
    /* 交叉验证配置 */
    cross_validation_mode_t validation_mode; /**< 交叉验证模式 */
    float disagreement_threshold;            /**< 不一致阈值 (0.0-1.0) */
    int enable_confidence_scoring;           /**< 是否启用置信度评分 */
} dual_model_coordinator_t;

/**
 * @brief 计算输出置信度（简化版本）
 * @param output 模型输出
 * @return 置信度分数 (0.0-1.0)
 */
static float calculate_confidence(const char* output) {
    if (!output || !*output) return 0.0f;
    
    // 简化实现：基于输出长度和内容特征
    size_t len = strlen(output);
    if (len < 10) return 0.3f;  // 太短的输出置信度低
    
    // 检查输出是否包含完整句子特征
    int has_period = (strchr(output, '.') != NULL);
    int has_comma = (strchr(output, ',') != NULL);
    int has_space = (strchr(output, ' ') != NULL);
    
    float confidence = 0.5f;
    if (has_period) confidence += 0.2f;
    if (has_comma) confidence += 0.1f;
    if (has_space && len > 20) confidence += 0.2f;
    
    // 限制在0.0-1.0范围内
    if (confidence > 1.0f) confidence = 1.0f;
    if (confidence < 0.0f) confidence = 0.0f;
    
    return confidence;
}

/**
 * @brief 协调执行（带交叉验证）
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

    // 检查输入有效性
    if (input_count == 0) {
        *out_result = NULL;
        return AGENTOS_SUCCESS;  // 无输入，返回空结果
    }

    // 分配结果缓冲区
    size_t total_len = 512;  // 增加缓冲区大小以容纳更多信息
    char* result = (char*)AGENTOS_MALLOC(total_len);
    if (!result) return AGENTOS_ENOMEM;

    // 获取主模型和次模型输出
    const char* primary_output = (input_count > 0) ? inputs[0] : "";
    const char* secondary_output = (input_count > 1) ? inputs[1] : "";

    // 交叉验证逻辑
    cross_validation_mode_t validation_mode = coordinator->validation_mode;
    float disagreement_threshold = coordinator->disagreement_threshold;
    
    // 默认阈值如果未设置
    if (disagreement_threshold <= 0.0f || disagreement_threshold > 1.0f) {
        disagreement_threshold = 0.3f;  // 默认30%不一致阈值
    }

    // 如果不启用交叉验证或只有一个输入，使用原始权重逻辑
    if (validation_mode == CROSS_VALIDATION_NONE || input_count < 2) {
        if (coordinator->primary_weight >= coordinator->secondary_weight && input_count > 0) {
            snprintf(result, total_len, "[Primary] %s", primary_output);
        } else if (input_count > 1) {
            snprintf(result, total_len, "[Secondary] %s", secondary_output);
        } else if (input_count > 0) {
            snprintf(result, total_len, "[Fallback] %s", primary_output);
        } else {
            snprintf(result, total_len, "[Empty]");
        }
        *out_result = result;
        return AGENTOS_SUCCESS;
    }

    // 进行交叉验证
    float similarity = 0.0f;
    if (*primary_output && *secondary_output) {
        similarity = intent_string_similarity(primary_output, secondary_output);
    } else {
        // 有一个输出为空，认为不一致
        similarity = 0.0f;
    }

    float consistency_threshold = 1.0f - disagreement_threshold;
    int is_consistent = (similarity >= consistency_threshold);

    // 计算置信度（如果启用）
    float primary_confidence = 1.0f;
    float secondary_confidence = 1.0f;
    if (coordinator->enable_confidence_scoring) {
        primary_confidence = calculate_confidence(primary_output);
        secondary_confidence = calculate_confidence(secondary_output);
    }

    // 决策逻辑
    char selected_model[32] = "Unknown";
    const char* selected_output = primary_output;
    float final_confidence = 0.0f;

    if (is_consistent) {
        // 输出一致，根据权重选择
        if (coordinator->primary_weight >= coordinator->secondary_weight) {
            strncpy(selected_model, "Primary", sizeof(selected_model) - 1); selected_model[sizeof(selected_model) - 1] = '\0';
            selected_output = primary_output;
            final_confidence = (primary_confidence + similarity) / 2.0f;
        } else {
            strncpy(selected_model, "Secondary", sizeof(selected_model) - 1); selected_model[sizeof(selected_model) - 1] = '\0';
            selected_output = secondary_output;
            final_confidence = (secondary_confidence + similarity) / 2.0f;
        }
    } else {
        // 输出不一致
        if (validation_mode == CROSS_VALIDATION_BASIC) {
            // 基础模式：选择主模型
            strncpy(selected_model, "Primary (Basic)", sizeof(selected_model) - 1); selected_model[sizeof(selected_model) - 1] = '\0';
            selected_output = primary_output;
            final_confidence = primary_confidence * 0.7f;  // 不一致时降低置信度
        } else if (validation_mode == CROSS_VALIDATION_ADVANCED) {
            // 高级模式：基于置信度选择
            if (primary_confidence >= secondary_confidence) {
                strncpy(selected_model, "Primary (Confidence)", sizeof(selected_model) - 1); selected_model[sizeof(selected_model) - 1] = '\0';
                selected_output = primary_output;
                final_confidence = primary_confidence;
            } else {
                strncpy(selected_model, "Secondary (Confidence)", sizeof(selected_model) - 1); selected_model[sizeof(selected_model) - 1] = '\0';
                selected_output = secondary_output;
                final_confidence = secondary_confidence;
            }
        }
    }

    // 格式化输出结果
    if (validation_mode != CROSS_VALIDATION_NONE) {
        snprintf(result, total_len, "[%s|相似度:%.2f|置信度:%.2f] %s",
                selected_model, similarity, final_confidence, selected_output);
    } else {
        snprintf(result, total_len, "[%s] %s", selected_model, selected_output);
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

    // 交叉验证配置默认值
    coordinator->validation_mode = CROSS_VALIDATION_NONE;  // 默认不启用
    coordinator->disagreement_threshold = 0.3f;            // 默认30%不一致阈值
    coordinator->enable_confidence_scoring = 0;            // 默认不启用置信度评分

    coordinator->base.coordinate = dual_coordinate;
    coordinator->base.destroy = dual_destroy;

    *out_base = &coordinator->base;
    return AGENTOS_SUCCESS;
}
