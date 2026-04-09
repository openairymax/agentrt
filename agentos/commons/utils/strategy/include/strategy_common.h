/**
 * @file strategy_common.h
 * @brief 策略模式共享工具 - 消除策略相关代码重复
 * 
 * 提供策略模式的通用实现，包括：
 * - 加权评分算法
 * - 调度策略通用结构
 * - 规划策略通用结构
 * - 跨模块共享的策略工具函数
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_STRATEGY_COMMON_H
#define AGENTOS_STRATEGY_COMMON_H

#include <stddef.h>
#include <stdbool.h>

/* Unified base library compatibility layer */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"
#include <string.h>

/**
 * @brief 加权评分配置
 */
typedef struct weighted_config {
    float cost_weight;     /**< 成本权重 */
    float perf_weight;     /**< 性能权重 */
    float trust_weight;    /**< 信任度权重 */
} weighted_config_t;

/**
 * @brief 代理信息结构（策略通用）
 */
typedef struct strategy_agent_info {
    float cost_estimate;   /**< 成本估计 */
    float success_rate;    /**< 成功率 */
    float trust_score;     /**< 信任度评分 */
    const char* name;      /**< 代理名称 */
    void* user_data;       /**< 用户数据 */
} strategy_agent_info_t;

/**
 * @brief 策略结果结构
 */
typedef struct strategy_result {
    int selected_index;    /**< 选中的索引 */
    float best_score;      /**< 最佳评分 */
    bool success;          /**< 是否成功 */
} strategy_result_t;

/**
 * @brief 计算加权评分
 * @param agent 代理信息
 * @param manager 加权配置
 * @return 计算的评分
 */
float strategy_compute_weighted_score(
    const strategy_agent_info_t* agent,
    const weighted_config_t* manager);

/**
 * @brief 从代理数组中选择最佳代理
 * @param agents 代理数组
 * @param agent_count 代理数量
 * @param manager 加权配置
 * @param result 输出结果
 * @return 0表示成功，非0表示失败
 */
int strategy_select_best_agent(
    const strategy_agent_info_t* agents,
    size_t agent_count,
    const weighted_config_t* manager,
    strategy_result_t* result);

/**
 * @brief 创建默认加权配置
 * @return 默认加权配置
 */
weighted_config_t strategy_create_default_weighted_config(void);

/**
 * @brief 验证加权配置
 * @param manager 加权配置
 * @return true表示有效，false表示无效
 */
bool strategy_validate_weighted_config(const weighted_config_t* manager);

/**
 * @brief 归一化权重
 * @param manager 加权配置
 * @return 归一化后的配置
 */
weighted_config_t strategy_normalize_weights(const weighted_config_t* manager);

/**
 * @brief 策略数据结构通用清理函数
 * @param data 数据指针
 * @param free_func 释放函数
 */
void strategy_cleanup_data(void* data, void (*free_func)(void*));

/**
 * @brief 策略名称生成器
 * @param base_name 基础名称
 * @param suffix 后缀
 * @return 生成的策略名称（需要调用AGENTOS_FREE释放）
 */
char* strategy_generate_name(const char* base_name, const char* suffix);

#endif /* AGENTOS_STRATEGY_COMMON_H */