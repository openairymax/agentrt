/**
 * @file strategy.h
 * @brief 规划策略创建函数的统一声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_PLANNER_STRATEGIES_H
#define AGENTRT_PLANNER_STRATEGIES_H

#include "cognition.h"
#include "llm_client.h"

#include <stddef.h>

typedef struct agentrt_memory_engine agentrt_memory_engine_t;
typedef struct agentrt_planner_base agentrt_planner_base_t;
typedef struct agentrt_planning_context agentrt_planning_context_t;
typedef struct agentrt_plan agentrt_plan_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建分层规划策略
 * @param llm LLM服务客户端句柄（用于生成子任务分解）
 * @param max_depth 最大分解深度
 * @return 策略对象，失败返回NULL
 */
agentrt_plan_strategy_t *agentrt_plan_hierarchical_create(agentrt_llm_service_t *llm,
                                                          int max_depth);

/**
 * @brief 创建反应式规划策略
 * @param llm LLM服务客户端句柄（用于直接生成计划）
 * @return 策略对象
 */
agentrt_plan_strategy_t *agentrt_plan_reactive_create(agentrt_llm_service_t *llm);

/**
 * @brief 创建反思式规划策略
 * @param llm LLM服务客户端句柄
 * @param memory_engine 记忆引擎（用于获取历史经验）
 * @return 策略对象
 */
agentrt_plan_strategy_t *agentrt_plan_reflective_create(agentrt_llm_service_t *llm,
                                                        agentrt_memory_engine_t *memory_engine);

/**
 * @brief 创建基于机器学习的规划策略
 * @param model_path 模型文件路径
 * @param llm LLM服务客户端（可选，用于回退）
 * @return 策略对象
 */
agentrt_plan_strategy_t *agentrt_plan_ml_create(const char *model_path, agentrt_llm_service_t *llm);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_PLANNER_STRATEGIES_H */
