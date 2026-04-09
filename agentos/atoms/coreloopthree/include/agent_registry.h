/**
 * @file agent_registry.h
 * @brief Agent 注册中心接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_AGENT_REGISTRY_H
#define AGENTOS_AGENT_REGISTRY_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Agent 信息结构（由注册中心提供）
 */
typedef struct agent_info {
    char* agent_id;                 /**< Agent 唯一标识 */
    char* role;                     /**< 角色 */
    float cost_estimate;            /**< 预估成本（越小越好） */
    float success_rate;             /**< 成功率（0-1） */
    float trust_score;              /**< 信任度（0-1） */
    int priority;                   /**< 优先级（越大越高） */
} agent_info_t;

/**
 * @brief 获取可用 Agent 列表的函数类型
 * @param ctx 注册中心上下文
 * @param role 按角色过滤（可为 NULL）
 * @param out_agents 输出 agent_info_t 数组（由调用者释放数组，但不释放内部指针）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
typedef agentos_error_t (*agent_registry_get_agents_func)(
    void* ctx,
    const char* role,
    agent_info_t*** out_agents,
    size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_AGENT_REGISTRY_H */
