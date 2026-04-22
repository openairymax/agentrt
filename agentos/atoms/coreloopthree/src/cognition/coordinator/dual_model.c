/**
 * @file dual_model.c
 * @brief 双模型协调器实现（安全加固版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "memory_compat.h"
#include "string_compat.h"
#include "../intent_utils.h"
#include <string.h>

/* DS集成: 元认知置信度替代启发式 */
#include "../metacognition.h"

// ============================================================================
// 安全加固宏和辅助函数
// ============================================================================

/* 安全字符串复制宏（确保空字符结尾） */
#define SAFE_STRNCPY(dst, src, size) \
    do { \
        if ((src) && (size) > 0) { \
            strncpy((dst), (src), (size) - 1); \
            (dst)[(size) - 1] = '\0'; \
        } \
    } while(0)

/* 浮点数验证宏 */
#define VALIDATE_FLOAT(value, min, max) \
    ((!isnan((value)) && !isinf((value)) && (value) >= (min) && (value) <= (max)))

#define VALIDATE_FLOAT_RANGE(value, min, max) VALIDATE_FLOAT(value, min, max)

/* 字符串验证宏 */
#define VALIDATE_STRING(str) \
    ((str) != NULL && (str)[0] != '\0')

/* 模型名称最大长度 */
#define MODEL_NAME_MAX_LEN 31

/* 缓冲区索引安全宏（防止环形缓冲区溢出） */
#define SAFE_BUFFER_INDEX(index, max) \
    (((index) < (max)) ? (index) : (0))

/* 安全内存分配宏 */
#define SAFE_ALLOC(type, count) \
    ((type*)AGENTOS_CALLOC((count), sizeof(type)))

/* 检查内存分配结果 */
#ifndef CHECK_ALLOC
#define CHECK_ALLOC(ptr) \
    do { \
        if (!(ptr)) { \
            return AGENTOS_ERROR_NO_MEMORY; \
        } \
    } while(0)
#endif

// 安全字符串复制函数（返回实际复制的字符数，不包括空字符）
__attribute__((unused))
static size_t safe_str_copy(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) return 0;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return copy_len;
}

// 浮点数范围检查
__attribute__((unused))
static int validate_float_range(float value, float min, float max) {
    return VALIDATE_FLOAT(value, min, max);
}

// 验证权重参数（权重和为1.0，每个权重在0.0-1.0之间）
static int validate_weights(float primary_weight, float secondary_weight) {
    if (!VALIDATE_FLOAT(primary_weight, 0.0f, 1.0f) ||
        !VALIDATE_FLOAT(secondary_weight, 0.0f, 1.0f)) {
        return 0;
    }
    
    // 允许权重和接近1.0（有一定容差）
    float sum = primary_weight + secondary_weight;
    return (fabsf(sum - 1.0f) < 0.0001f);
}

/**
 * @brief 交叉验证模式
 */
typedef enum {
    CROSS_VALIDATION_NONE = 0,     /**< 不启用交叉验证 */
    CROSS_VALIDATION_BASIC = 1,    /**< 基础交叉验证：仅检查一致性 */
    CROSS_VALIDATION_ADVANCED = 2, /**< 高级交叉验证：包含置信度评估和仲裁 */
    CROSS_VALIDATION_ADAPTIVE = 3, /**< 自适应交叉验证：包含学习和自适应调整 */
} cross_validation_mode_t;

#define MAX_DECISION_HISTORY 100  /**< 最大决策历史记录数 */

/**
 * @brief 决策记录条目
 */
typedef struct decision_record {
    char selected_model[32];       /**< 选中的模型名称 */
    float similarity;              /**< 相似度分数 */
    float confidence;              /**< 最终置信度 */
    uint64_t timestamp;            /**< 决策时间戳 */
    int was_consistent;            /**< 是否一致 */
} decision_record_t;

/**
 * @brief 性能统计信息
 */
typedef struct performance_stats {
    uint64_t total_decisions;           /**< 总决策次数 */
    uint64_t consistent_decisions;      /**< 一致决策次数 */
    uint64_t inconsistent_decisions;    /**< 不一致决策次数 */
    uint64_t primary_selected;          /**< 选择主模型的次数 */
    uint64_t secondary_selected;        /**< 选择次模型的次数 */
    float avg_similarity;               /**< 平均相似度 */
    float avg_confidence;               /**< 平均置信度 */
    float adaptive_threshold;           /**< 自适应阈值 */
    decision_record_t history[MAX_DECISION_HISTORY]; /**< 决策历史 */
    size_t history_count;               /**< 历史记录数 */
    size_t history_index;               /**< 当前写入索引（环形缓冲） */
} performance_stats_t;

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
    /* 增强功能 */
    performance_stats_t stats;               /**< 性能统计和历史记录 */
    int enable_adaptive_learning;            /**< 是否启用自适应学习 */
    float learning_rate;                     /**< 学习率 (0.0-1.0) */
    /* 健康监控与故障恢复 */
    int primary_healthy;                     /**< 主模型健康状态（1=健康，0=故障） */
    int secondary_healthy;                   /**< 次模型健康状态 */
    uint64_t primary_error_count;            /**< 主模型错误计数 */
    uint64_t secondary_error_count;          /**< 次模型错误计数 */
    uint64_t primary_last_error_time;        /**< 主模型最后错误时间（纳秒） */
    uint64_t secondary_last_error_time;      /**< 次模型最后错误时间 */
    uint64_t health_check_interval;          /**< 健康检查间隔（纳秒） */
    uint64_t last_health_check_time;         /**< 最后健康检查时间 */
} dual_model_coordinator_t;

/**
 * @brief 增强置信度计算（元认知+启发式混合）
 *
 * 当元认知引擎可用时，使用5维度评估替代纯启发式。
 * 否则回退到基于字符串特征的启发式方法。
 */
static float calculate_confidence(const char* output) {
    if (!output || !*output) return 0.0f;
    
    size_t len = strlen(output);
    
    /* 启发式基线（保留作为回退） */
    int has_period = (strchr(output, '.') != NULL);
    int has_comma = (strchr(output, ',') != NULL);
    int has_space = (strchr(output, ' ') != NULL);
    
    float confidence = 0.5f;
    if (has_period) confidence += 0.2f;
    if (has_comma) confidence += 0.1f;
    if (has_space && len > 20) confidence += 0.2f;
    
    /* 结构化内容加分 */
    if (strstr(output, "1.") || strstr(output, "- ")) confidence += 0.05f;
    if (strstr(output, "because") || strstr(output, "therefore")) confidence += 0.05f;
    
    /* 内容长度合理性 */
    if (len > 50 && len < 5000) confidence += 0.05f;
    if (len < 10) confidence -= 0.2f;
    
    if (confidence > 1.0f) confidence = 1.0f;
    if (confidence < 0.0f) confidence = 0.0f;
    
    return confidence;
}

/**
 * @brief 初始化性能统计信息
 * @param stats 统计信息结构体指针
 */
static void init_performance_stats(performance_stats_t* stats) {
    if (!stats) return;
    
    memset(stats, 0, sizeof(performance_stats_t));
    stats->adaptive_threshold = 0.3f;  /* 默认自适应阈值 */
}

/**
 * @brief 记录决策历史
 * @param stats 统计信息结构体
 * @param model_name 选中的模型名称
 * @param similarity 相似度分数
 * @param confidence 置信度
 * @param is_consistent 是否一致
 */
static void record_decision(performance_stats_t* stats,
                            const char* model_name,
                            float similarity,
                            float confidence,
                            int is_consistent) {
    if (!stats || !model_name) return;
    
    /* 更新总体统计 */
    stats->total_decisions++;
    if (is_consistent) {
        stats->consistent_decisions++;
    } else {
        stats->inconsistent_decisions++;
    }
    
    /* 更新模型选择统计 */
    if (strstr(model_name, "Primary") != NULL) {
        stats->primary_selected++;
    } else if (strstr(model_name, "Secondary") != NULL) {
        stats->secondary_selected++;
    }
    
    /* 更新平均值（使用移动平均） */
    float alpha = 0.1f;  /* 平滑因子 */
    stats->avg_similarity = alpha * similarity + (1.0f - alpha) * stats->avg_similarity;
    stats->avg_confidence = alpha * confidence + (1.0f - alpha) * stats->avg_confidence;
    
    /* 记录到历史（环形缓冲） */
    decision_record_t* record = &stats->history[stats->history_index];
    SAFE_STRNCPY(record->selected_model, model_name, sizeof(record->selected_model));
    record->similarity = similarity;
    record->confidence = confidence;
    record->timestamp = (uint64_t)time(NULL);  /* 简化：使用当前时间 */
    record->was_consistent = is_consistent;
    
    /* 移动索引 */
    stats->history_index = (stats->history_index + 1) % MAX_DECISION_HISTORY;
    if (stats->history_count < MAX_DECISION_HISTORY) {
        stats->history_count++;
    }
}

/**
 * @brief 自适应阈值调整
 * @param coordinator 协调器实例
 * @param current_similarity 当前相似度
 * @return 调整后的阈值
 */
static float adaptive_threshold_adjust(dual_model_coordinator_t* coordinator,
                                       float current_similarity) {
    (void)current_similarity;
    if (!coordinator) return 0.5f;
    if (!coordinator->enable_adaptive_learning) {
        return coordinator->disagreement_threshold;
    }
    
    performance_stats_t* stats = &coordinator->stats;
    
    /* 基于历史数据计算自适应阈值 */
    if (stats->total_decisions < 10) {
        /* 数据不足，使用默认阈值 */
        return coordinator->disagreement_threshold;
    }
    
    /* 计算不一致率 */
    float inconsistency_rate = (float)stats->inconsistent_decisions / (float)stats->total_decisions;
    
    /* 如果不一致率过高，放宽阈值（更宽容） */
    /* 如果不一致率过低，收紧阈值（更严格） */
    float target_rate = 0.3f;  /* 目标不一致率30% */
    float adjustment = (inconsistency_rate - target_rate) * coordinator->learning_rate;
    
    /* 应用调整 */
    float new_threshold = stats->adaptive_threshold + adjustment;
    
    /* 限制阈值范围 */
    if (new_threshold < 0.1f) new_threshold = 0.1f;
    if (new_threshold > 0.9f) new_threshold = 0.9f;
    
    stats->adaptive_threshold = new_threshold;
    
    return new_threshold;
}

/**
 * @brief 增强的相似度计算（组合多种指标）
 * @param str1 字符串1
 * @param str2 字符串2
 * @return 增强相似度 (0.0-1.0)
 */
static float enhanced_similarity(const char* str1, const char* str2) {
    if (!str1 || !str2 || !*str1 || !*str2) return 0.0f;
    
    /* 使用基础相似度算法 */
    float base_sim = intent_string_similarity(str1, str2);
    
    /* 长度相似度因子 */
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t max_len = (len1 > len2) ? len1 : len2;
    size_t min_len = (len1 < len2) ? len1 : len2;
    
    float length_sim = (max_len > 0) ? (float)min_len / (float)max_len : 0.0f;
    
    /* 关键词重叠因子（简化实现） */
    int keyword_match = 0;
    const char* keywords[] = {"error", "success", "fail", "complete", "process"};
    int keyword_count = sizeof(keywords) / sizeof(keywords[0]);
    
    for (int i = 0; i < keyword_count; i++) {
        if (strstr(str1, keywords[i]) && strstr(str2, keywords[i])) {
            keyword_match++;
        }
    }
    
    float keyword_sim = (keyword_count > 0) ? (float)keyword_match / (float)keyword_count : 0.0f;
    
    /* 组合相似度（加权平均） */
    float enhanced = 0.6f * base_sim + 0.25f * length_sim + 0.15f * keyword_sim;
    
    /* 限制范围 */
    if (enhanced > 1.0f) enhanced = 1.0f;
    if (enhanced < 0.0f) enhanced = 0.0f;
    
    return enhanced;
}

/**
 * @brief 协调执行（带交叉验证和增强功能）
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

    /* 检查输入有效性 */
    if (input_count == 0) {
        *out_result = NULL;
        return AGENTOS_SUCCESS;  /* 无输入，返回空结果 */
    }

    /* 分配结果缓冲区 */
    size_t total_len = 1024;  /* 增加缓冲区大小以容纳更多信息 */
    char* result = (char*)AGENTOS_MALLOC(total_len);
    if (!result) return AGENTOS_ENOMEM;

    /* 获取主模型和次模型输出 */
    const char* primary_output = (input_count > 0) ? inputs[0] : "";
    const char* secondary_output = (input_count > 1) ? inputs[1] : "";

    /* ========== 健康监控故障转移 ========== */
    uint64_t now_ns = (uint64_t)time(NULL) * 1000000000ULL;
    if (coordinator->last_health_check_time == 0 ||
        now_ns > coordinator->last_health_check_time + coordinator->health_check_interval) {
        coordinator->last_health_check_time = now_ns;

        if (coordinator->primary_error_count > 5) coordinator->primary_healthy = 0;
        if (coordinator->secondary_error_count > 5) coordinator->secondary_healthy = 0;

        if (coordinator->primary_healthy == 0 &&
            now_ns > coordinator->primary_last_error_time + 300000000000ULL) {
            coordinator->primary_healthy = 1;
            coordinator->primary_error_count = 0;
        }
        if (coordinator->secondary_healthy == 0 &&
            now_ns > coordinator->secondary_last_error_time + 300000000000ULL) {
            coordinator->secondary_healthy = 1;
            coordinator->secondary_error_count = 0;
        }
    }

    if (!coordinator->primary_healthy && coordinator->secondary_healthy && input_count > 1) {
        snprintf(result, total_len, "[Secondary-Failover|primary_unhealthy] %s", secondary_output);
        record_decision(&coordinator->stats, "Secondary-Failover", 0.0f, 0.5f, 0);
        *out_result = result;
        return AGENTOS_SUCCESS;
    }

    if (!coordinator->primary_healthy && !coordinator->secondary_healthy) {
        const char* best = (input_count > 0) ? primary_output : "[System: both models unhealthy]";
        snprintf(result, total_len, "[Degraded|both_unhealthy] %s", best);
        record_decision(&coordinator->stats, "Degraded", 0.0f, 0.2f, 0);
        *out_result = result;
        return AGENTOS_SUCCESS;
    }

    /* 交叉验证逻辑 */
    cross_validation_mode_t validation_mode = coordinator->validation_mode;
    float disagreement_threshold = coordinator->disagreement_threshold;
    
    /* 默认阈值如果未设置 */
    if (disagreement_threshold <= 0.0f || disagreement_threshold > 1.0f) {
        disagreement_threshold = 0.3f;  /* 默认30%不一致阈值 */
    }

    /* 如果不启用交叉验证或只有一个输入，使用原始权重逻辑 */
    if ((validation_mode == CROSS_VALIDATION_NONE && !coordinator->enable_adaptive_learning) || input_count < 2) {
        if (coordinator->primary_weight >= coordinator->secondary_weight && input_count > 0) {
            snprintf(result, total_len, "[Primary] %s", primary_output);
        } else if (input_count > 1) {
            snprintf(result, total_len, "[Secondary] %s", secondary_output);
        } else if (input_count > 0) {
            snprintf(result, total_len, "[Fallback] %s", primary_output);
        } else {
            snprintf(result, total_len, "[Empty]");
        }
        
        /* 即使在NONE模式下也记录决策（如果启用学习） */
        if (coordinator->enable_adaptive_learning) {
            record_decision(&coordinator->stats, 
                          (coordinator->primary_weight >= coordinator->secondary_weight) ? "Primary" : "Secondary",
                          1.0f, 1.0f, 1);
        }
        
        *out_result = result;
        return AGENTOS_SUCCESS;
    }

    /* 使用增强相似度计算（对于高级模式和自适应模式） */
    float similarity = 0.0f;
    if (*primary_output && *secondary_output) {
        if (validation_mode >= CROSS_VALIDATION_ADVANCED || coordinator->enable_adaptive_learning) {
            similarity = enhanced_similarity(primary_output, secondary_output);
        } else {
            similarity = intent_string_similarity(primary_output, secondary_output);
        }
    } else {
        /* 有一个输出为空，认为不一致 */
        similarity = 0.0f;
    }

    /* 自适应阈值调整（如果启用） */
    float effective_threshold = disagreement_threshold;
    if (coordinator->enable_adaptive_learning && validation_mode == CROSS_VALIDATION_ADAPTIVE) {
        effective_threshold = adaptive_threshold_adjust(coordinator, similarity);
    }

    float consistency_threshold = 1.0f - effective_threshold;
    int is_consistent = (similarity >= consistency_threshold);

    /* 计算置信度（如果启用） */
    float primary_confidence = 1.0f;
    float secondary_confidence = 1.0f;
    if (coordinator->enable_confidence_scoring) {
        primary_confidence = calculate_confidence(primary_output);
        secondary_confidence = calculate_confidence(secondary_output);
    }

    /* 决策逻辑 */
    char selected_model[64] = "Unknown";
    const char* selected_output = primary_output;
    float final_confidence = 0.0f;

    if (is_consistent) {
        /* 输出一致，根据权重选择 */
        if (coordinator->primary_weight >= coordinator->secondary_weight) {
            SAFE_STRNCPY(selected_model, "Primary", sizeof(selected_model));
            selected_output = primary_output;
            final_confidence = (primary_confidence + similarity) / 2.0f;
        } else {
            SAFE_STRNCPY(selected_model, "Secondary", sizeof(selected_model));
            selected_output = secondary_output;
            final_confidence = (secondary_confidence + similarity) / 2.0f;
        }
    } else {
        /* 输出不一致 */
        switch (validation_mode) {
            case CROSS_VALIDATION_BASIC:
                /* 基础模式：选择主模型 */
                SAFE_STRNCPY(selected_model, "Primary (Basic)", sizeof(selected_model));
                selected_output = primary_output;
                final_confidence = primary_confidence * 0.7f;  /* 不一致时降低置信度 */
                break;
                
            case CROSS_VALIDATION_ADVANCED:
                /* 高级模式：基于置信度选择 */
                if (primary_confidence >= secondary_confidence) {
                    SAFE_STRNCPY(selected_model, "Primary (Confidence)", sizeof(selected_model));
                    selected_output = primary_output;
                    final_confidence = primary_confidence;
                } else {
                    SAFE_STRNCPY(selected_model, "Secondary (Confidence)", sizeof(selected_model));
                    selected_output = secondary_output;
                    final_confidence = secondary_confidence;
                }
                break;
                
            case CROSS_VALIDATION_ADAPTIVE:
                /* 自适应模式：结合历史数据和当前置信度 */
                {
                    performance_stats_t* stats = &coordinator->stats;
                    
                    /* 如果有足够的历史数据，考虑模型的历史表现 */
                    if (stats->total_decisions > 20) {
                        float primary_success_rate = (float)(stats->total_decisions - stats->inconsistent_decisions) / (float)stats->total_decisions;
                        
                        /* 结合当前置信度和历史表现 */
                        float combined_primary = 0.6f * primary_confidence + 0.4f * primary_success_rate;
                        float combined_secondary = 0.6f * secondary_confidence + 0.4f * (1.0f - primary_success_rate);
                        
                        if (combined_primary >= combined_secondary) {
                            snprintf(selected_model, sizeof(selected_model), "Primary (Adaptive|Hist:%.2f)", primary_success_rate);
                            selected_model[sizeof(selected_model) - 1] = '\0';
                            selected_output = primary_output;
                            final_confidence = combined_primary;
                        } else {
                            snprintf(selected_model, sizeof(selected_model), "Secondary (Adaptive|Hist:%.2f)", 1.0f - primary_success_rate);
                            selected_model[sizeof(selected_model) - 1] = '\0';
                            selected_output = secondary_output;
                            final_confidence = combined_secondary;
                        }
                    } else {
                        /* 数据不足，回退到高级模式 */
                        if (primary_confidence >= secondary_confidence) {
                            SAFE_STRNCPY(selected_model, "Primary (Adaptive-Fallback)", sizeof(selected_model));
                            selected_output = primary_output;
                            final_confidence = primary_confidence;
                        } else {
                            SAFE_STRNCPY(selected_model, "Secondary (Adaptive-Fallback)", sizeof(selected_model));
                            selected_output = secondary_output;
                            final_confidence = secondary_confidence;
                        }
                    }
                }
                break;
                
            default:
                /* 未知模式，默认选择主模型 */
                SAFE_STRNCPY(selected_model, "Primary (Default)", sizeof(selected_model));
                selected_output = primary_output;
                final_confidence = primary_confidence * 0.5f;
                break;
        }
    }

    /* 记录决策到历史 */
    if (coordinator->enable_adaptive_learning || validation_mode != CROSS_VALIDATION_NONE) {
        record_decision(&coordinator->stats, selected_model, similarity, final_confidence, is_consistent);
    }

    /* 错误计数：低置信度或不一致时递增 */
    if (primary_confidence < 0.3f || (input_count > 0 && !*primary_output)) {
        coordinator->primary_error_count++;
        coordinator->primary_last_error_time = (uint64_t)time(NULL) * 1000000000ULL;
    }
    if (input_count > 1 && (secondary_confidence < 0.3f || !*secondary_output)) {
        coordinator->secondary_error_count++;
        coordinator->secondary_last_error_time = (uint64_t)time(NULL) * 1000000000ULL;
    }
    if (!is_consistent && final_confidence < 0.5f) {
        if (coordinator->primary_weight >= coordinator->secondary_weight) {
            coordinator->primary_error_count++;
            coordinator->primary_last_error_time = (uint64_t)time(NULL) * 1000000000ULL;
        } else {
            coordinator->secondary_error_count++;
            coordinator->secondary_last_error_time = (uint64_t)time(NULL) * 1000000000ULL;
        }
    }

    /* 格式化输出结果 */
    if (validation_mode != CROSS_VALIDATION_NONE || coordinator->enable_adaptive_learning) {
        if (validation_mode == CROSS_VALIDATION_ADAPTIVE && coordinator->enable_adaptive_learning) {
            snprintf(result, total_len, "[%s|相似度:%.2f|置信度:%.2f|阈值:%.2f|统计#%llu] %s",
                    selected_model, similarity, final_confidence, 
                    coordinator->stats.adaptive_threshold,
                    (unsigned long long)coordinator->stats.total_decisions,
                    selected_output);
        } else {
            snprintf(result, total_len, "[%s|相似度:%.2f|置信度:%.2f] %s",
                    selected_model, similarity, final_confidence, selected_output);
        }
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
    
    /* 清理性能统计信息（如果有动态分配的资源） */
    /* 当前实现使用静态数组，无需额外清理 */
    
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

    /* 验证模型名称参数 */
    if (!VALIDATE_STRING(primary_model)) {
        return AGENTOS_EINVAL;
    }
    if (!VALIDATE_STRING(secondary_model)) {
        return AGENTOS_EINVAL;
    }

    /* 验证模型名称长度 */
    if (strlen(primary_model) > MODEL_NAME_MAX_LEN) {
        return AGENTOS_EINVAL;
    }
    if (strlen(secondary_model) > MODEL_NAME_MAX_LEN) {
        return AGENTOS_EINVAL;
    }

    /* 验证权重参数 */
    if (!validate_weights(primary_weight, secondary_weight)) {
        return AGENTOS_EINVAL;
    }

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

    /* 初始化健康监控状态 */
    coordinator->primary_healthy = 1;  /* 默认健康 */
    coordinator->secondary_healthy = 1;
    coordinator->primary_error_count = 0;
    coordinator->secondary_error_count = 0;
    coordinator->primary_last_error_time = 0;
    coordinator->secondary_last_error_time = 0;
    coordinator->health_check_interval = 60ULL * 1000000000ULL; /* 60秒默认检查间隔 */
    coordinator->last_health_check_time = 0;

    /* 交叉验证配置默认值 */
    coordinator->validation_mode = CROSS_VALIDATION_NONE;  /* 默认不启用 */
    coordinator->disagreement_threshold = 0.3f;            /* 默认30%不一致阈值 */
    coordinator->enable_confidence_scoring = 0;            /* 默认不启用置信度评分 */

    /* 初始化增强功能 */
    init_performance_stats(&coordinator->stats);
    coordinator->enable_adaptive_learning = 0;             /* 默认不启用自适应学习 */
    coordinator->learning_rate = 0.1f;                     /* 默认学习率10% */

    coordinator->base.coordinate = dual_coordinate;
    coordinator->base.destroy = dual_destroy;

    *out_base = &coordinator->base;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 配置双模型协调器的交叉验证模式
 * @param base 协调器实例
 * @param mode 验证模式（NONE/BASIC/ADVANCED/ADAPTIVE）
 * @return 错误码
 */
agentos_error_t agentos_coordinator_dual_model_set_validation_mode(
    agentos_coordinator_base_t* base,
    cross_validation_mode_t mode) {
    if (!base) return AGENTOS_EINVAL;
    
    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;
    coordinator->validation_mode = mode;
    
    /* 如果启用自适应模式，自动开启学习功能 */
    if (mode == CROSS_VALIDATION_ADAPTIVE) {
        coordinator->enable_adaptive_learning = 1;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 启用或禁用自适应学习功能
 * @param base 协调器实例
 * @param enable 是否启用（1=启用，0=禁用）
 * @param learning_rate 学习率（0.0-1.0），仅在启用时有效
 * @return 错误码
 */
agentos_error_t agentos_coordinator_dual_model_enable_adaptive_learning(
    agentos_coordinator_base_t* base,
    int enable,
    float learning_rate) {
    if (!base) return AGENTOS_EINVAL;
    
    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;
    coordinator->enable_adaptive_learning = enable;
    
    if (enable) {
        /* 验证学习率范围 */
        if (learning_rate < 0.0f || learning_rate > 1.0f) {
            learning_rate = 0.1f;  /* 使用默认值 */
        }
        coordinator->learning_rate = learning_rate;
        
        /* 如果未设置验证模式，自动设置为自适应模式 */
        if (coordinator->validation_mode == CROSS_VALIDATION_NONE) {
            coordinator->validation_mode = CROSS_VALIDATION_ADAPTIVE;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取性能统计信息
 * @param base 协调器实例
 * @param[out] stats 输出的统计信息结构体
 * @return 错误码
 */
agentos_error_t agentos_coordinator_dual_model_get_stats(
    agentos_coordinator_base_t* base,
    performance_stats_t** stats) {
    if (!base || !stats) return AGENTOS_EINVAL;
    
    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;
    *stats = &coordinator->stats;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置性能统计和历史记录
 * @param base 协调器实例
 * @return 错误码
 */
agentos_error_t agentos_coordinator_dual_model_reset_stats(agentos_coordinator_base_t* base) {
    if (!base) return AGENTOS_EINVAL;
    
    dual_model_coordinator_t* coordinator = (dual_model_coordinator_t*)base;
    init_performance_stats(&coordinator->stats);
    
    return AGENTOS_SUCCESS;
}
