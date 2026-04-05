/**
 * @file strategy_common.c
 * @brief 策略模式共享工具 - 实现
 * 
 * 实现策略模式的通用功能，消除跨模块的代码重复。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "include/strategy_common.h"
#include <stdio.h>
#include <float.h>

/**
 * @brief 计算加权评分
 */
float strategy_compute_weighted_score(
    const strategy_agent_info_t* agent,
    const weighted_config_t* manager) {
    if (!agent || !manager) {
        return 0.0f;
    }
    
    float cost_score = 1.0f / (agent->cost_estimate + 1.0f);
    float perf_score = agent->success_rate;
    float trust_score = agent->trust_score;
    
    return manager->cost_weight * cost_score +
           manager->perf_weight * perf_score +
           manager->trust_weight * trust_score;
}

/**
 * @brief 从代理数组中选择最佳代理
 */
int strategy_select_best_agent(
    const strategy_agent_info_t* agents,
    size_t agent_count,
    const weighted_config_t* manager,
    strategy_result_t* result) {
    if (!agents || !manager || !result || agent_count == 0) {
        return -1;
    }
    
    int best_index = -1;
    float best_score = -FLT_MAX;
    
    for (size_t i = 0; i < agent_count; i++) {
        float score = strategy_compute_weighted_score(&agents[i], manager);
        if (score > best_score) {
            best_score = score;
            best_index = (int)i;
        }
    }
    
    result->selected_index = best_index;
    result->best_score = best_score;
    result->success = (best_index >= 0);
    
    return 0;
}

/**
 * @brief 创建默认加权配置
 */
weighted_config_t strategy_create_default_weighted_config(void) {
    weighted_config_t manager = {
        .cost_weight = 0.33f,
        .perf_weight = 0.34f,
        .trust_weight = 0.33f
    };
    return manager;
}

/**
 * @brief 验证加权配置
 */
bool strategy_validate_weighted_config(const weighted_config_t* manager) {
    if (!manager) {
        return false;
    }
    
    if (manager->cost_weight < 0.0f || manager->cost_weight > 1.0f) {
        return false;
    }
    if (manager->perf_weight < 0.0f || manager->perf_weight > 1.0f) {
        return false;
    }
    if (manager->trust_weight < 0.0f || manager->trust_weight > 1.0f) {
        return false;
    }
    
    return true;
}

/**
 * @brief 归一化权重
 */
weighted_config_t strategy_normalize_weights(const weighted_config_t* manager) {
    weighted_config_t normalized = *manager;
    float total = manager->cost_weight + manager->perf_weight + manager->trust_weight;
    
    if (total > 0.0f) {
        normalized.cost_weight /= total;
        normalized.perf_weight /= total;
        normalized.trust_weight /= total;
    } else {
        normalized = strategy_create_default_weighted_config();
    }
    
    return normalized;
}

/**
 * @brief 策略数据结构通用清理函数
 */
void strategy_cleanup_data(void* data, void (*free_func)(void*)) {
    if (data && free_func) {
        free_func(data);
    }
}

/**
 * @brief 策略名称生成器
 */
char* strategy_generate_name(const char* base_name, const char* suffix) {
    if (!base_name) {
        return NULL;
    }
    
    size_t base_len = strlen(base_name);
    size_t suffix_len = suffix ? strlen(suffix) : 0;
    size_t total_len = base_len + suffix_len + 2; /* 1 for '_' and 1 for null terminator */
    
    char* name = (char*)AGENTOS_MALLOC(total_len);
    if (!name) {
        return NULL;
    }
    
    if (suffix && suffix_len > 0) {
        snprintf(name, total_len, "%s_%s", base_name, suffix);
    } else {
        snprintf(name, total_len, "%s", base_name);
    }
    
    return name;
}