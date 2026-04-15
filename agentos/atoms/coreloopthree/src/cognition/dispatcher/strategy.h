/**
 * @file strategy.h
 * @brief 调度策略创建函数的统一声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DISPATCHER_STRATEGIES_H
#define AGENTOS_DISPATCHER_STRATEGIES_H

#include "cognition.h"
#include "agent_registry.h"
#include <stddef.h>

/* Forward declarations for dispatcher base types */
struct agentos_dispatcher_base;
typedef struct agentos_dispatcher_base agentos_dispatcher_base_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 加权调度配置
 */
typedef struct weighted_config {
    float cost_weight;
    float perf_weight;
    float trust_weight;
} weighted_config_t;

/**
 * @brief 创建加权调度策略
 * @param manager 权重配置（若为NULL使用默认）
 * @param registry_ctx 注册中心上下文
 * @param get_agents_func 获取候选Agent列表的函数
 * @return 策略对象，失败返回NULL
 */
agentos_dispatching_strategy_t* agentos_dispatching_weighted_create(
    const weighted_config_t* manager,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func);

/**
 * @brief 创建轮询调度策略
 * @param registry_ctx 注册中心上下文
 * @param get_agents_func 获取候选Agent列表的函数
 * @return 策略对象
 */
agentos_dispatching_strategy_t* agentos_dispatching_round_robin_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func);

/**
 * @brief 创建基于机器学习的调度策略
 * @param model_path 模型文件路径
 * @param registry_ctx 注册中心上下文
 * @param get_agents_func 获取候选Agent列表的函数
 * @return 策略对象
 */
agentos_dispatching_strategy_t* agentos_dispatching_ml_create(
    const char* model_path,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func);

/**
 * @brief 创建优先级调度策略
 * @param registry_ctx 注册中心上下文
 * @param get_agents_func 获取候选Agent列表的函数
 * @return 策略对象
 */
agentos_error_t agentos_dispatching_priority_create(
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func,
    agentos_dispatching_strategy_t** out_strategy);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DISPATCHER_STRATEGIES_H */
