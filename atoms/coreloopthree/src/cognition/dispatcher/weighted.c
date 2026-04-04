/**
 * @file weighted.c
 * @brief 加权调度策略 - 基于多维度权重评分
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agent_registry.h"
#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"

/**
 * @brief 加权调度策略内部数据结构
 */
typedef struct weighted_data {
    weighted_config_t config;                    /**< 权重配置 */
    void* registry_ctx;                          /**< 注册中心上下文 */
    agent_registry_get_agents_func get_agents;   /**< 获取候选Agent列表的函数 */
    agentos_mutex_t* lock;                       /**< 线程安全锁 */
} weighted_data_t;

/**
 * @brief 计算单个Agent的综合加权得分
 *
 * 根据配置的成本权重、性能权重和信任权重计算综合得分。
 *
 * @param agent [in] Agent信息指针
 * @param data [in] 加权调度上下文
 * @return 综合得分（越高越好）
 */
static float compute_weighted_score(
    const agent_info_t* agent,
    const weighted_data_t* data)
{
    if (!agent || !data) return 0.0f;

    float cost_score = 1.0f;
    float perf_score = 1.0f;
    float trust_score = 1.0f;

    /* 成本得分：成本越低越好，归一化到 [0, 1] */
    if (agent->cost_estimate > 0) {
        cost_score = 1.0f / (1.0f + agent->cost_estimate);
    }

    /* 性能得分：直接使用成功率 */
    perf_score = agent->success_rate;

    /* 信任得分：直接使用信任分数 */
    trust_score = agent->trust_score;

    /* 加权综合得分 */
    float total = data->config.cost_weight * cost_score +
                  data->config.perf_weight * perf_score +
                  data->config.trust_weight * trust_score;

    return total;
}

/**
 * @brief 销毁加权调度策略实例
 * @param strategy [in] 要销毁的策略实例
 */
static void weighted_destroy(agentos_dispatching_strategy_t* strategy)
{
    if (!strategy) return;
    weighted_data_t* data = (weighted_data_t*)strategy->context;
    if (data) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

/**
 * @brief 使用加权策略选择最优Agent
 *
 * @param task [in] 任务描述
 * @param candidates [in] 候选Agent列表
 * @param count [in] 候选数量
 * @param context [in] 策略上下文（weighted_data_t*）
 * @param out_agent_id [out] 选中的Agent ID
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t weighted_select(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id)
{
    weighted_data_t* data = (weighted_data_t*)context;
    if (!data || !task || !out_agent_id) return AGENTOS_EINVAL;

    agent_info_t** agents = NULL;
    size_t agent_count = 0;
    agentos_error_t err;

    if (candidates && count > 0) {
        agents = (agent_info_t**)candidates;
        agent_count = count;
    } else {
        err = data->get_agents(data->registry_ctx, task->agent_role, &agents, &agent_count);
        if (err != AGENTOS_SUCCESS) return err;
        if (agent_count == 0) return AGENTOS_ENOENT;
    }

    float best_score = -FLT_MAX;
    int best_index = -1;

    for (size_t i = 0; i < agent_count; i++) {
        agent_info_t* agent = agents[i];
        if (!agent) continue;
        float score = compute_weighted_score(agent, data);
        if (score > best_score) {
            best_score = score;
            best_index = (int)i;
        }
    }

    if (best_index >= 0 && agents[best_index]) {
        *out_agent_id = AGENTOS_STRDUP(agents[best_index]->name);
        return *out_agent_id ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }

    return AGENTOS_ENOENT;
}

/**
 * @brief 创建加权调度策略实例
 *
 * @param config [in] 权重配置（若为NULL使用默认值）
 * @param registry_ctx [in] 注册中心上下文
 * @param get_agents_func [in] 获取候选Agent列表的函数
 * @param out_strategy [out] 输出策略实例
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存分配失败
 */
agentos_error_t agentos_dispatching_weighted_create(
    const weighted_config_t* config,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func,
    agentos_dispatching_strategy_t** out_strategy)
{
    if (!registry_ctx || !get_agents_func || !out_strategy) {
        return AGENTOS_EINVAL;
    }

    weighted_data_t* data = (weighted_data_t*)AGENTOS_CALLOC(1, sizeof(weighted_data_t));
    if (!data) return AGENTOS_ENOMEM;

    /* 使用提供的配置或默认值 */
    if (config) {
        data->config = *config;
    } else {
        data->config.cost_weight = 0.3f;
        data->config.perf_weight = 0.4f;
        data->config.trust_weight = 0.3f;
    }

    data->registry_ctx = registry_ctx;
    data->get_agents = get_agents_func;
    data->lock = agentos_mutex_create();

    agentos_dispatching_strategy_t* strategy =
        (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(*strategy));
    if (!strategy) {
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
        return AGENTOS_ENOMEM;
    }

    strategy->context = data;
    strategy->select_agent = weighted_select;
    strategy->destroy = weighted_destroy;

    *out_strategy = strategy;
    return AGENTOS_SUCCESS;
}
