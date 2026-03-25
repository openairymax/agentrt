/**
 * @file strategy.h
 * @brief 协同策略创建函数的统一声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COORDINATOR_STRATEGIES_H
#define AGENTOS_COORDINATOR_STRATEGIES_H

#include "cognition.h"
#include "llm_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建双模型协同策略（1主+2辅）
 * @param primary_model 主模型名称
 * @param secondary1 辅模型1名称
 * @param secondary2 辅模型2名称
 * @param llm LLM服务客户端句柄
 * @return 策略对象，失败返回NULL
 */
agentos_coordinator_strategy_t* agentos_dual_model_coordinator_create(
    const char* primary_model,
    const char* secondary1,
    const char* secondary2,
    agentos_llm_service_t* llm);

/**
 * @brief 创建多数投票协同策略
 * @param model_names 模型名称数组
 * @param model_count 模型数量
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_majority_coordinator_create(
    const char** model_names,
    size_t model_count,
    agentos_llm_service_t* llm);

/**
 * @brief 创建加权融合策略
 * @param model_names 模型名称数组
 * @param weights 权重数组（总和不必为1）
 * @param model_count 数量
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_weighted_coordinator_create(
    const char** model_names,
    const float* weights,
    size_t model_count,
    agentos_llm_service_t* llm);

/**
 * @brief 创建外部仲裁策略（模型仲裁）
 * @param arbiter_model 仲裁模型名称
 * @param llm LLM服务客户端
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_model_create(
    const char* arbiter_model,
    agentos_llm_service_t* llm);

/**
 * @brief 创建外部仲裁策略（人工仲裁）
 * @param callback 人工回调函数，接收问题并填充答案
 * @return 策略对象
 */
agentos_coordinator_strategy_t* agentos_arbiter_human_create(
    void (*callback)(const char* question, char* answer, size_t max_len));

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COORDINATOR_STRATEGIES_H */