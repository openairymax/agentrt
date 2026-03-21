/**
 * @file round_robin.c
 * @brief 轮询调度策略实现
 * @details 按照注册顺序依次选择可用的 Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

/**
 * @brief 轮询调度策略数据
 */
typedef struct {
    agent_info_t** agents;      /**< Agent 列表 */
    size_t agent_count;         /**< Agent 数量 */
    size_t current_index;       /**< 当前索引 */
    size_t max_agents;          /**< 最大 Agent 数量 */
} round_robin_data_t;

/**
 * @brief 创建轮询调度策略
 * @param config 配置信息
 * @param data 输出参数，返回策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_create(const sched_config_t* config, void** data) {
    round_robin_data_t* rrd = (round_robin_data_t*)malloc(sizeof(round_robin_data_t));
    if (!rrd) {
        return -1;
    }

    rrd->max_agents = config->max_agents;
    rrd->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * rrd->max_agents);
    if (!rrd->agents) {
        free(rrd);
        return -1;
    }

    rrd->agent_count = 0;
    rrd->current_index = 0;

    *data = rrd;
    return 0;
}

/**
 * @brief 销毁轮询调度策略
 * @param data 策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_destroy(void* data) {
    if (!data) {
        return 0;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    
    if (rrd->agents) {
        for (size_t i = 0; i < rrd->agent_count; i++) {
            if (rrd->agents[i]) {
                free(rrd->agents[i]->agent_id);
                free(rrd->agents[i]->agent_name);
                free(rrd->agents[i]);
            }
        }
        free(rrd->agents);
    }

    free(rrd);
    return 0;
}

/**
 * @brief 注册 Agent
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_register_agent(void* data, const agent_info_t* agent_info) {
    if (!data || !agent_info) {
        return -1;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    if (rrd->agent_count >= rrd->max_agents) {
        return -2;
    }

    // 检查是否已存在
    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (strcmp(rrd->agents[i]->agent_id, agent_info->agent_id) == 0) {
            // 更新现有 Agent
            free(rrd->agents[i]->agent_id);
            free(rrd->agents[i]->agent_name);
            
            rrd->agents[i]->agent_id = strdup(agent_info->agent_id);
            rrd->agents[i]->agent_name = strdup(agent_info->agent_name);
            rrd->agents[i]->load_factor = agent_info->load_factor;
            rrd->agents[i]->success_rate = agent_info->success_rate;
            rrd->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            rrd->agents[i]->is_available = agent_info->is_available;
            rrd->agents[i]->weight = agent_info->weight;
            
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

    rrd->agents[rrd->agent_count++] = new_agent;
    return 0;
}

/**
 * @brief 注销 Agent
 * @param data 策略数据
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_unregister_agent(void* data, const char* agent_id) {
    if (!data || !agent_id) {
        return -1;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (strcmp(rrd->agents[i]->agent_id, agent_id) == 0) {
            // 释放 Agent 资源
            free(rrd->agents[i]->agent_id);
            free(rrd->agents[i]->agent_name);
            free(rrd->agents[i]);

            // 移动剩余 Agent
            for (size_t j = i; j < rrd->agent_count - 1; j++) {
                rrd->agents[j] = rrd->agents[j + 1];
            }

            rrd->agent_count--;

            // 调整当前索引
            if (rrd->current_index >= rrd->agent_count && rrd->agent_count > 0) {
                rrd->current_index = 0;
            }

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
static int round_robin_update_agent_status(void* data, const agent_info_t* agent_info) {
    return round_robin_register_agent(data, agent_info);
}

/**
 * @brief 执行轮询调度
 * @param data 策略数据
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_schedule(void* data, const task_info_t* task_info, sched_result_t** result) {
    if (!data || !task_info || !result) {
        return -1; // 无效参数
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    if (rrd->agent_count == 0) {
        return -2;  // 无 Agent
    }

    // 查找可用的 Agent
    size_t start_index = rrd->current_index;
    size_t attempts = 0;

    while (attempts < rrd->agent_count) {
        agent_info_t* agent = rrd->agents[rrd->current_index];

        if (!agent) {
            // 跳过无效 Agent
            rrd->current_index = (rrd->current_index + 1) % rrd->agent_count;
            attempts++;
            continue;
        }

        if (agent->is_available && agent->load_factor < 0.9) {  // 负载小于 90%
            // 创建调度结果
            sched_result_t* res = (sched_result_t*)malloc(sizeof(sched_result_t));
            if (!res) {
                return -4; // 内存分配失败
            }

            res->selected_agent_id = strdup(agent->agent_id);
            if (!res->selected_agent_id) {
                free(res);
                return -4; // 内存分配失败
            }

            res->confidence = 0.7;  // 轮询调度的置信度
            res->estimated_time_ms = agent->avg_response_time_ms;

            // 更新当前索引
            rrd->current_index = (rrd->current_index + 1) % rrd->agent_count;

            *result = res;
            return 0;
        }

        // 移动到下一个 Agent
        rrd->current_index = (rrd->current_index + 1) % rrd->agent_count;
        attempts++;
    }

    return -3;  // 无可用 Agent
}

/**
 * @brief 获取轮询调度策略名称
 * @return 策略名称
 */
static const char* round_robin_get_name() {
    return "round_robin";
}

/**
 * @brief 获取可用 Agent 数量
 * @param data 策略数据
 * @return 可用 Agent 数量
 */
static size_t round_robin_get_available_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    size_t count = 0;

    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (rrd->agents[i]->is_available) {
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
static size_t round_robin_get_total_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    return rrd->agent_count;
}

/**
 * @brief 轮询调度策略接口
 */
static const strategy_interface_t round_robin_strategy = {
    .create = round_robin_create,
    .destroy = round_robin_destroy,
    .register_agent = round_robin_register_agent,
    .unregister_agent = round_robin_unregister_agent,
    .update_agent_status = round_robin_update_agent_status,
    .schedule = round_robin_schedule,
    .get_name = round_robin_get_name,
    .get_available_agent_count = round_robin_get_available_agent_count,
    .get_total_agent_count = round_robin_get_total_agent_count
};

/**
 * @brief 获取轮询调度策略接口
 * @return 轮询调度策略接口
 */
const strategy_interface_t* get_round_robin_strategy() {
    return &round_robin_strategy;
}
