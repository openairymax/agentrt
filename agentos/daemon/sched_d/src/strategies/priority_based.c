/**
 * @file priority_based.c
 * @brief 基于优先级的调度策略实现
 * @details 根据任务优先级和Agent权重进行智能调度
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

/**
 * @brief 基于优先级的调度策略数据
 */
typedef struct {
    agent_info_t** agents;      /**< Agent 列表 */
    size_t agent_count;         /**< Agent 数量 */
    size_t max_agents;          /**< 最大 Agent 数量 */
    float* priority_weights;    /**< 优先级权重表 */
    size_t priority_levels;     /**< 优先级级别数量 */
} priority_based_data_t;

/**
 * @brief 创建基于优先级的调度策略
 * @param manager 配置信息
 * @param data 输出参数，返回策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_create(const sched_config_t* manager, void** data) {
    priority_based_data_t* pbd = (priority_based_data_t*)malloc(sizeof(priority_based_data_t));
    if (!pbd) {
        return -1;
    }

    pbd->max_agents = manager->max_agents;
    pbd->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * pbd->max_agents);
    if (!pbd->agents) {
        free(pbd);
        return -1;
    }

    pbd->agent_count = 0;
    
    // 初始化优先级权重表（默认：优先级越高，权重越大）
    pbd->priority_levels = 10; // 假设有10个优先级级别
    pbd->priority_weights = (float*)malloc(sizeof(float) * pbd->priority_levels);
    if (pbd->priority_weights) {
        for (size_t i = 0; i < pbd->priority_levels; i++) {
            // 指数权重：优先级越高，权重增长越快
            pbd->priority_weights[i] = expf((float)i / 2.0f);
        }
    }

    *data = pbd;
    return 0;
}

/**
 * @brief 销毁基于优先级的调度策略
 * @param data 策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_destroy(void* data) {
    if (!data) {
        return 0;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;
    
    if (pbd->agents) {
        for (size_t i = 0; i < pbd->agent_count; i++) {
            if (pbd->agents[i]) {
                free(pbd->agents[i]->agent_id);
                free(pbd->agents[i]->agent_name);
                free(pbd->agents[i]);
            }
        }
        free(pbd->agents);
    }

    if (pbd->priority_weights) {
        free(pbd->priority_weights);
    }

    free(pbd);
    return 0;
}

/**
 * @brief 注册 Agent
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_register_agent(void* data, const agent_info_t* agent_info) {
    if (!data || !agent_info) {
        return -1;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;

    if (pbd->agent_count >= pbd->max_agents) {
        return -2;
    }

    // 检查是否已存在
    for (size_t i = 0; i < pbd->agent_count; i++) {
        if (strcmp(pbd->agents[i]->agent_id, agent_info->agent_id) == 0) {
            // 更新现有 Agent
            free(pbd->agents[i]->agent_id);
            free(pbd->agents[i]->agent_name);
            
            pbd->agents[i]->agent_id = strdup(agent_info->agent_id);
            pbd->agents[i]->agent_name = strdup(agent_info->agent_name);
            pbd->agents[i]->load_factor = agent_info->load_factor;
            pbd->agents[i]->success_rate = agent_info->success_rate;
            pbd->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            pbd->agents[i]->is_available = agent_info->is_available;
            pbd->agents[i]->weight = agent_info->weight;
            
            return 0;
        }
    }

    // 添加新 Agent
    agent_info_t* new_agent = (agent_info_t*)malloc(sizeof(agent_info_t));
    if (!new_agent) {
        return -1;
    }

    new_agent->agent_id = strdup(agent_info->agent_id);
    new_agent->agent_name = strdup(agent_info->agent_name);
    new_agent->load_factor = agent_info->load_factor;
    new_agent->success_rate = agent_info->success_rate;
    new_agent->avg_response_time_ms = agent_info->avg_response_time_ms;
    new_agent->is_available = agent_info->is_available;
    new_agent->weight = agent_info->weight;

    pbd->agents[pbd->agent_count++] = new_agent;
    return 0;
}

/**
 * @brief 注销 Agent
 * @param data 策略数据
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_unregister_agent(void* data, const char* agent_id) {
    if (!data || !agent_id) {
        return -1;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;

    for (size_t i = 0; i < pbd->agent_count; i++) {
        if (strcmp(pbd->agents[i]->agent_id, agent_id) == 0) {
            // 释放 Agent 资源
            free(pbd->agents[i]->agent_id);
            free(pbd->agents[i]->agent_name);
            free(pbd->agents[i]);

            // 移动剩余 Agent
            for (size_t j = i; j < pbd->agent_count - 1; j++) {
                pbd->agents[j] = pbd->agents[j + 1];
            }

            pbd->agent_count--;
            return 0;
        }
    }

    return -2;  // Agent 不存在
}

/**
 * @brief 更新 Agent 状态
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_update_agent_status(void* data, const agent_info_t* agent_info) {
    return priority_based_register_agent(data, agent_info);
}

/**
 * @brief 计算任务-Agent匹配分数
 * @param agent Agent信息
 * @param task 任务信息
 * @param priority_weight 优先级权重
 * @return 匹配分数
 */
static float calculate_match_score(const agent_info_t* agent, const task_info_t* task, float priority_weight) {
    if (!agent->is_available || agent->load_factor >= 0.9) {
        return -1.0f; // 不可用或负载过高
    }
    
    float score = 0.0f;
    
    // 基础成功率权重
    score += agent->success_rate * 0.4f;
    
    // 负载因子权重（负载越低越好）
    score += (1.0f - agent->load_factor) * 0.3f;
    
    // 响应时间权重（响应时间越短越好）
    float response_factor = 1.0f / (1.0f + agent->avg_response_time_ms / 1000.0f);
    score += response_factor * 0.2f;
    
    // Agent权重（配置的优先级）
    score += agent->weight * 0.1f;
    
    // 应用任务优先级权重
    score *= priority_weight;
    
    return score;
}

/**
 * @brief 执行基于优先级的调度
 * @param data 策略数据
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
static int priority_based_schedule(void* data, const task_info_t* task_info, sched_result_t** result) {
    if (!data || !task_info || !result) {
        return -1;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;

    if (pbd->agent_count == 0) {
        return -2;  // 无可用 Agent
    }

    // 获取任务优先级（假设task_info中有priority字段）
    // 如果没有，使用默认优先级
    int task_priority = 5; // 默认中等优先级
    if (task_info->priority >= 0) {
        task_priority = task_info->priority;
    }
    
    // 限制优先级在有效范围内
    if (task_priority < 0) task_priority = 0;
    if (task_priority >= (int)pbd->priority_levels) task_priority = pbd->priority_levels - 1;
    
    float priority_weight = 1.0f;
    if (pbd->priority_weights && task_priority < (int)pbd->priority_levels) {
        priority_weight = pbd->priority_weights[task_priority];
    }

    agent_info_t* best_agent = NULL;
    float best_score = -1.0f;

    for (size_t i = 0; i < pbd->agent_count; i++) {
        agent_info_t* agent = pbd->agents[i];
        float score = calculate_match_score(agent, task_info, priority_weight);
        
        if (score > best_score) {
            best_score = score;
            best_agent = agent;
        }
    }

    if (!best_agent) {
        return -3;  // 无可用 Agent
    }

    // 创建调度结果
    sched_result_t* res = (sched_result_t*)malloc(sizeof(sched_result_t));
    if (!res) {
        return -1;
    }

    res->selected_agent_id = strdup(best_agent->agent_id);
    res->confidence = best_score;
    res->estimated_time_ms = best_agent->avg_response_time_ms;

    *result = res;
    return 0;
}

/**
 * @brief 获取基于优先级的调度策略名称
 * @return 策略名称
 */
static const char* priority_based_get_name() {
    return "priority_based";
}

/**
 * @brief 获取可用 Agent 数量
 * @param data 策略数据
 * @return 可用 Agent 数量
 */
static size_t priority_based_get_available_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;
    size_t count = 0;

    for (size_t i = 0; i < pbd->agent_count; i++) {
        if (pbd->agents[i]->is_available) {
            count++;
        }
    }

    return count;
}

/**
 * @brief 获取总 Agent 数量
 * @param data 策略数据
 * @return 总 Agent 数量
 */
static size_t priority_based_get_total_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    priority_based_data_t* pbd = (priority_based_data_t*)data;
    return pbd->agent_count;
}

/**
 * @brief 基于优先级的调度策略接口
 */
static const strategy_interface_t priority_based_strategy = {
    .create = priority_based_create,
    .destroy = priority_based_destroy,
    .register_agent = priority_based_register_agent,
    .unregister_agent = priority_based_unregister_agent,
    .update_agent_status = priority_based_update_agent_status,
    .schedule = priority_based_schedule,
    .get_name = priority_based_get_name,
    .get_available_agent_count = priority_based_get_available_agent_count,
    .get_total_agent_count = priority_based_get_total_agent_count
};

/**
 * @brief 获取基于优先级的调度策略接口
 * @return 基于优先级的调度策略接口
 */
const strategy_interface_t* get_priority_based_strategy() {
    return &priority_based_strategy;
}