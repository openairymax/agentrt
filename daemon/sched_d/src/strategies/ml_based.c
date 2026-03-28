/**
 * @file ml_based.c
 * @brief 基于机器学习的调度策略实现
 * @details 使用机器学习模型预测最佳 Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

/**
 * @brief 机器学习调度策略数据
 */
typedef struct {
    agent_info_t** agents;      /**< Agent 列表 */
    size_t agent_count;         /**< Agent 数量 */
    size_t max_agents;          /**< 最大 Agent 数量 */
    char* model_path;           /**< 模型路径 */
    void* model;                /**< 机器学习模型 */
    bool model_loaded;          /**< 模型是否加载 */
} ml_based_data_t;

// From data intelligence emerges. by spharx
/**
 * @brief 创建基于机器学习的调度策略
 * @param manager 配置信息
 * @param data 输出参数，返回策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int ml_based_create(const sched_config_t* manager, void** data) {
    ml_based_data_t* mld = (ml_based_data_t*)malloc(sizeof(ml_based_data_t));
    if (!mld) {
        return -1;
    }

    mld->max_agents = manager->max_agents;
    mld->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * mld->max_agents);
    if (!mld->agents) {
        free(mld);
        return -1;
    }

    mld->agent_count = 0;
    mld->model_path = manager->ml_model_path ? strdup(manager->ml_model_path) : NULL;
    mld->model = NULL;
    mld->model_loaded = false;

    // 尝试加载模型
    if (mld->model_path) {
        // 这里应该加载机器学习模型
        // 由于是示例，我们只是模拟加载
        mld->model_loaded = true;
        printf("ML model loaded from: %s\n", mld->model_path);
    }

    *data = mld;
    return 0;
}

/**
 * @brief 销毁基于机器学习的调度策略
 * @param data 策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int ml_based_destroy(void* data) {
    if (!data) {
        return 0;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;
    
    if (mld->agents) {
        for (size_t i = 0; i < mld->agent_count; i++) {
            if (mld->agents[i]) {
                free(mld->agents[i]->agent_id);
                free(mld->agents[i]->agent_name);
                free(mld->agents[i]);
            }
        }
        free(mld->agents);
    }

    if (mld->model_path) {
        free(mld->model_path);
    }

    if (mld->model) {
        // 释放模型资源
        free(mld->model);
    }

    free(mld);
    return 0;
}

/**
 * @brief 注册 Agent
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int ml_based_register_agent(void* data, const agent_info_t* agent_info) {
    if (!data || !agent_info) {
        return -1;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;

    if (mld->agent_count >= mld->max_agents) {
        return -2;
    }

    // 检查是否已存在
    for (size_t i = 0; i < mld->agent_count; i++) {
        if (strcmp(mld->agents[i]->agent_id, agent_info->agent_id) == 0) {
            // 更新现有 Agent
            free(mld->agents[i]->agent_id);
            free(mld->agents[i]->agent_name);
            
            mld->agents[i]->agent_id = strdup(agent_info->agent_id);
            mld->agents[i]->agent_name = strdup(agent_info->agent_name);
            mld->agents[i]->load_factor = agent_info->load_factor;
            mld->agents[i]->success_rate = agent_info->success_rate;
            mld->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            mld->agents[i]->is_available = agent_info->is_available;
            mld->agents[i]->weight = agent_info->weight;
            
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

    mld->agents[mld->agent_count++] = new_agent;
    return 0;
}

/**
 * @brief 注销 Agent
 * @param data 策略数据
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
static int ml_based_unregister_agent(void* data, const char* agent_id) {
    if (!data || !agent_id) {
        return -1;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;

    for (size_t i = 0; i < mld->agent_count; i++) {
        if (strcmp(mld->agents[i]->agent_id, agent_id) == 0) {
            // 释放 Agent 资源
            free(mld->agents[i]->agent_id);
            free(mld->agents[i]->agent_name);
            free(mld->agents[i]);

            // 移动剩余 Agent
            for (size_t j = i; j < mld->agent_count - 1; j++) {
                mld->agents[j] = mld->agents[j + 1];
            }

            mld->agent_count--;
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
static int ml_based_update_agent_status(void* data, const agent_info_t* agent_info) {
    return ml_based_register_agent(data, agent_info);
}

/**
 * @brief 执行基于机器学习的调度
 * @param data 策略数据
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
static int ml_based_schedule(void* data, const task_info_t* task_info, sched_result_t** result) {
    if (!data || !task_info || !result) {
        return -1;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;

    if (mld->agent_count == 0) {
        return -2;  // 无可用 Agent
    }

    // 检查模型是否加载
    if (!mld->model_loaded) {
        // 模型未加载，使用默认策略（选择成功率最高的可用 Agent）
        agent_info_t* best_agent = NULL;
        float best_score = 0.0f;

        for (size_t i = 0; i < mld->agent_count; i++) {
            agent_info_t* agent = mld->agents[i];
            
            if (agent->is_available && agent->load_factor < 0.9) {
                // 计算综合评分
                float score = agent->success_rate * 0.7 + (1.0 - agent->load_factor) * 0.3;
                
                if (score > best_score) {
                    best_score = score;
                    best_agent = agent;
                }
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

    // 模型已加载，使用机器学习模型进行预测
    // 这里应该使用实际的机器学习模型进行预测
    // 由于是示例，我们模拟预测过程
    agent_info_t* best_agent = NULL;
    float best_score = 0.0f;

    for (size_t i = 0; i < mld->agent_count; i++) {
        agent_info_t* agent = mld->agents[i];
        
        if (agent->is_available && agent->load_factor < 0.9) {
            // 模拟模型预测得分
            float score = agent->success_rate * 0.5 + 
                         (1.0 - agent->load_factor) * 0.3 + 
                         (1.0 / (1.0 + agent->avg_response_time_ms / 1000.0)) * 0.2;
            
            if (score > best_score) {
                best_score = score;
                best_agent = agent;
            }
        }
    }

    if (!best_agent) {
        return -4;  // 无可用 Agent
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
 * @brief 获取基于机器学习的调度策略名称
 * @return 策略名称
 */
static const char* ml_based_get_name() {
    return "ml_based";
}

/**
 * @brief 获取可用 Agent 数量
 * @param data 策略数据
 * @return 可用 Agent 数量
 */
static size_t ml_based_get_available_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;
    size_t count = 0;

    for (size_t i = 0; i < mld->agent_count; i++) {
        if (mld->agents[i]->is_available) {
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
static size_t ml_based_get_total_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    ml_based_data_t* mld = (ml_based_data_t*)data;
    return mld->agent_count;
}

/**
 * @brief 基于机器学习的调度策略接口
 */
static const strategy_interface_t ml_based_strategy = {
    .create = ml_based_create,
    .destroy = ml_based_destroy,
    .register_agent = ml_based_register_agent,
    .unregister_agent = ml_based_unregister_agent,
    .update_agent_status = ml_based_update_agent_status,
    .schedule = ml_based_schedule,
    .get_name = ml_based_get_name,
    .get_available_agent_count = ml_based_get_available_agent_count,
    .get_total_agent_count = ml_based_get_total_agent_count
};

/**
 * @brief 获取基于机器学习的调度策略接口
 * @return 基于机器学习的调度策略接口
 */
const strategy_interface_t* get_ml_based_strategy() {
    return &ml_based_strategy;
}
