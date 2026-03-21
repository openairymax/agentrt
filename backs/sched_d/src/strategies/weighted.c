/**
 * @file weighted.c
 * @brief 加权调度策略实现
 * @details 根据 Agent 的权重和状态进行调度
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

/**
 * @brief 加权调度策略数据
 */
typedef struct {
    agent_info_t** agents;      /**< Agent 列表 */
    size_t agent_count;         /**< Agent 数量 */
    size_t max_agents;          /**< 最大 Agent 数量 */
    float total_weight;         /**< 总权重 */
} weighted_data_t;

/**
 * @brief 创建加权调度策略
 * @param config 配置信息
 * @param data 输出参数，返回策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int weighted_create(const sched_config_t* config, void** data) {
    weighted_data_t* wd = (weighted_data_t*)malloc(sizeof(weighted_data_t));
    if (!wd) {
        return -1;
    }

    wd->max_agents = config->max_agents;
    wd->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * wd->max_agents);
    if (!wd->agents) {
        free(wd);
        return -1;
    }

    wd->agent_count = 0;
    wd->total_weight = 0.0f;

    *data = wd;
    return 0;
}

/**
 * @brief 销毁加权调度策略
 * @param data 策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int weighted_destroy(void* data) {
    if (!data) {
        return 0;
    }

    weighted_data_t* wd = (weighted_data_t*)data;
    
    if (wd->agents) {
        for (size_t i = 0; i < wd->agent_count; i++) {
            if (wd->agents[i]) {
                free(wd->agents[i]->agent_id);
                free(wd->agents[i]->agent_name);
                free(wd->agents[i]);
            }
        }
        free(wd->agents);
    }

    free(wd);
    return 0;
}

/**
 * @brief 注册 Agent
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int weighted_register_agent(void* data, const agent_info_t* agent_info) {
    if (!data || !agent_info) {
        return -1;
    }

    weighted_data_t* wd = (weighted_data_t*)data;

    if (wd->agent_count >= wd->max_agents) {
        return -2;
    }

    // 检查是否已存在
    for (size_t i = 0; i < wd->agent_count; i++) {
        if (strcmp(wd->agents[i]->agent_id, agent_info->agent_id) == 0) {
            // 更新现有 Agent
            float old_weight = wd->agents[i]->weight;
            float new_weight = agent_info->weight;
            
            // 更新权重
            wd->total_weight = wd->total_weight - old_weight + new_weight;
            
            // 更新 Agent 信息
            free(wd->agents[i]->agent_id);
            free(wd->agents[i]->agent_name);
            
            wd->agents[i]->agent_id = strdup(agent_info->agent_id);
            wd->agents[i]->agent_name = strdup(agent_info->agent_name);
            wd->agents[i]->load_factor = agent_info->load_factor;
            wd->agents[i]->success_rate = agent_info->success_rate;
            wd->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            wd->agents[i]->is_available = agent_info->is_available;
            wd->agents[i]->weight = new_weight;
            
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

    wd->agents[wd->agent_count++] = new_agent;
    wd->total_weight += agent_info->weight;

    return 0;
}

/**
 * @brief 注销 Agent
 * @param data 策略数据
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
static int weighted_unregister_agent(void* data, const char* agent_id) {
    if (!data || !agent_id) {
        return -1;
    }

    weighted_data_t* wd = (weighted_data_t*)data;

    for (size_t i = 0; i < wd->agent_count; i++) {
        if (strcmp(wd->agents[i]->agent_id, agent_id) == 0) {
            // 更新总权重
            wd->total_weight -= wd->agents[i]->weight;
            
            // 释放 Agent 资源
            free(wd->agents[i]->agent_id);
            free(wd->agents[i]->agent_name);
            free(wd->agents[i]);

            // 移动剩余 Agent
            for (size_t j = i; j < wd->agent_count - 1; j++) {
                wd->agents[j] = wd->agents[j + 1];
            }

            wd->agent_count--;
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
static int weighted_update_agent_status(void* data, const agent_info_t* agent_info) {
    return weighted_register_agent(data, agent_info);
}

/**
 * @brief 执行加权调度
 * @param data 策略数据
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
static int weighted_schedule(void* data, const task_info_t* task_info, sched_result_t** result) {
    if (!data || !task_info || !result) {
        return -1;
    }

    weighted_data_t* wd = (weighted_data_t*)data;

    if (wd->agent_count == 0) {
        return -2;  // 无可用 Agent
    }

    // 计算可用 Agent 的总权重
    float available_weight = 0.0f;
    for (size_t i = 0; i < wd->agent_count; i++) {
        agent_info_t* agent = wd->agents[i];
        if (agent->is_available && agent->load_factor < 0.9) {
            available_weight += agent->weight;
        }
    }

    if (available_weight == 0.0f) {
        return -3;  // 无可用 Agent
    }

    // 生成随机数
    float random = (float)rand() / RAND_MAX * available_weight;
    float current_weight = 0.0f;

    // 根据权重选择 Agent
    for (size_t i = 0; i < wd->agent_count; i++) {
        agent_info_t* agent = wd->agents[i];
        
        if (agent->is_available && agent->load_factor < 0.9) {
            current_weight += agent->weight;
            
            if (random <= current_weight) {
                // 创建调度结果
                sched_result_t* res = (sched_result_t*)malloc(sizeof(sched_result_t));
                if (!res) {
                    return -1;
                }

                res->selected_agent_id = strdup(agent->agent_id);
                res->confidence = agent->weight / available_weight;
                res->estimated_time_ms = agent->avg_response_time_ms;

                *result = res;
                return 0;
            }
        }
    }

    return -4;  // 未找到可用 Agent
}

/**
 * @brief 获取加权调度策略名称
 * @return 策略名称
 */
static const char* weighted_get_name() {
    return "weighted";
}

/**
 * @brief 获取可用 Agent 数量
 * @param data 策略数据
 * @return 可用 Agent 数量
 */
static size_t weighted_get_available_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    weighted_data_t* wd = (weighted_data_t*)data;
    size_t count = 0;

    for (size_t i = 0; i < wd->agent_count; i++) {
        if (wd->agents[i]->is_available) {
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
static size_t weighted_get_total_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    weighted_data_t* wd = (weighted_data_t*)data;
    return wd->agent_count;
}

/**
 * @brief 加权调度策略接口
 */
static const strategy_interface_t weighted_strategy = {
    .create = weighted_create,
    .destroy = weighted_destroy,
    .register_agent = weighted_register_agent,
    .unregister_agent = weighted_unregister_agent,
    .update_agent_status = weighted_update_agent_status,
    .schedule = weighted_schedule,
    .get_name = weighted_get_name,
    .get_available_agent_count = weighted_get_available_agent_count,
    .get_total_agent_count = weighted_get_total_agent_count
};

/**
 * @brief 获取加权调度策略接口
 * @return 加权调度策略接口
 */
const strategy_interface_t* get_weighted_strategy() {
    return &weighted_strategy;
}
