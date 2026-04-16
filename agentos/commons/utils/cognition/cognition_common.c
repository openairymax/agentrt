/**
 * @file cognition_common.c
 * @brief 认知模块通用功能实现
 * 
 * 提供认知模块共享的功能，包括计划、调度、协调等
 * 减少认知模块之间的代码重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition_common.h"
#include <memory_common.h>
#include <platform.h>
#include <agentos_time.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/**
 * @brief 初始化Agent信息
 * @param agent Agent信息指针
 * @param agent_id Agent ID
 * @return 0 成功，非0 失败
 */
int agent_info_init(agent_info_t* agent, const char* agent_id) {
    if (!agent || !agent_id) {
        return -1;
    }
    
    agent->agent_id = memory_safe_strdup(agent_id);
    if (!agent->agent_id) {
        return -1;
    }
    
    agent->weight = 1.0;
    agent->success_rate = 0.0;
    agent->total_tasks = 0;
    agent->successful_tasks = 0;
    agent->avg_latency = 0.0;
    agent->last_used = agentos_time_monotonic_ms();
    
    return 0;
}

/**
 * @brief 清理Agent信息
 * @param agent Agent信息指针
 */
void agent_info_cleanup(agent_info_t* agent) {
    if (!agent) {
        return;
    }
    
    if (agent->agent_id) {
        memory_safe_free(agent->agent_id);
        agent->agent_id = NULL;
    }
    
    agent->weight = 0.0;
    agent->success_rate = 0.0;
    agent->total_tasks = 0;
    agent->successful_tasks = 0;
    agent->avg_latency = 0.0;
    agent->last_used = 0;
}

/**
 * @brief 更新Agent性能统计
 * @param agent Agent信息指针
 * @param success 是否成功
 * @param latency 延迟时间
 */
void agent_info_update_stats(agent_info_t* agent, bool success, uint64_t latency) {
    if (!agent) {
        return;
    }
    
    agent->total_tasks++;
    if (success) {
        agent->successful_tasks++;
    }
    
    // 更新成功率
    agent->success_rate = (double)agent->successful_tasks / agent->total_tasks;
    
    // 更新平均延迟
    agent->avg_latency = (agent->avg_latency * (agent->total_tasks - 1) + latency) / agent->total_tasks;
    
    // 更新最后使用时间
    agent->last_used = agentos_time_monotonic_ms();
    
    // 更新权重
    agent->weight = agent_info_calculate_weight(agent);
}

/**
 * @brief 计算Agent权重
 * @param agent Agent信息指针
 * @return 计算后的权重
 */
double agent_info_calculate_weight(const agent_info_t* agent) {
    if (!agent) {
        return 0.0;
    }
    
    // 基于成功率和延迟计算权重
    double success_factor = agent->success_rate;
    double latency_factor = 1.0 / (1.0 + agent->avg_latency / 1000.0); // 延迟越低，因子越高
    
    // 结合两个因子，成功率权重更高
    double weight = (success_factor * 0.7) + (latency_factor * 0.3);
    
    return weight;
}

/**
 * @brief 初始化任务信息
 * @param task 任务信息指针
 * @param task_id 任务ID
 * @param task_type 任务类型
 * @param task_content 任务内容
 * @return 0 成功，非0 失败
 */
int task_info_init(task_info_t* task, const char* task_id, 
                  const char* task_type, const char* task_content) {
    if (!task || !task_id || !task_type || !task_content) {
        return -1;
    }
    
    task->task_id = memory_safe_strdup(task_id);
    task->task_type = memory_safe_strdup(task_type);
    task->task_content = memory_safe_strdup(task_content);
    
    if (!task->task_id || !task->task_type || !task->task_content) {
        task_info_cleanup(task);
        return -1;
    }
    
    task->priority = 0;
    task->deadline = 0;
    
    return 0;
}

/**
 * @brief 清理任务信息
 * @param task 任务信息指针
 */
void task_info_cleanup(task_info_t* task) {
    if (!task) {
        return;
    }
    
    if (task->task_id) {
        memory_safe_free(task->task_id);
        task->task_id = NULL;
    }
    
    if (task->task_type) {
        memory_safe_free(task->task_type);
        task->task_type = NULL;
    }
    
    if (task->task_content) {
        memory_safe_free(task->task_content);
        task->task_content = NULL;
    }
    
    task->priority = 0;
    task->deadline = 0;
}

/**
 * @brief 初始化计划结果
 * @param result 计划结果指针
 * @return 0 成功，非0 失败
 */
int plan_result_init(plan_result_t* result) {
    if (!result) {
        return -1;
    }
    
    result->success = false;
    result->plan = NULL;
    result->plan_size = 0;
    result->error = NULL;
    result->error_size = 0;
    
    return 0;
}

/**
 * @brief 清理计划结果
 * @param result 计划结果指针
 */
void plan_result_cleanup(plan_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->plan) {
        memory_safe_free(result->plan);
        result->plan = NULL;
    }
    
    if (result->error) {
        memory_safe_free(result->error);
        result->error = NULL;
    }
    
    result->success = false;
    result->plan_size = 0;
    result->error_size = 0;
}

/**
 * @brief 初始化调度结果
 * @param result 调度结果指针
 * @return 0 成功，非0 失败
 */
int dispatch_result_init(dispatch_result_t* result) {
    if (!result) {
        return -1;
    }
    
    result->success = false;
    result->selected_agent = NULL;
    result->confidence = 0.0;
    result->error = NULL;
    result->error_size = 0;
    
    return 0;
}

/**
 * @brief 清理调度结果
 * @param result 调度结果指针
 */
void dispatch_result_cleanup(dispatch_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->selected_agent) {
        memory_safe_free(result->selected_agent);
        result->selected_agent = NULL;
    }
    
    if (result->error) {
        memory_safe_free(result->error);
        result->error = NULL;
    }
    
    result->success = false;
    result->confidence = 0.0;
    result->error_size = 0;
}

/**
 * @brief 初始化协调结果
 * @param result 协调结果指针
 * @return 0 成功，非0 失败
 */
int coordination_result_init(coordination_result_t* result) {
    if (!result) {
        return -1;
    }
    
    result->success = false;
    result->decision = NULL;
    result->decision_size = 0;
    result->error = NULL;
    result->error_size = 0;
    
    return 0;
}

/**
 * @brief 清理协调结果
 * @param result 协调结果指针
 */
void coordination_result_cleanup(coordination_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->decision) {
        memory_safe_free(result->decision);
        result->decision = NULL;
    }
    
    if (result->error) {
        memory_safe_free(result->error);
        result->error = NULL;
    }
    
    result->success = false;
    result->decision_size = 0;
    result->error_size = 0;
}

/**
 * @brief 选择最佳Agent
 * @param agents Agent数组
 * @param agent_count Agent数量
 * @param task 任务信息
 * @param result 调度结果
 * @return 0 成功，非0 失败
 */
int cognition_select_best_agent(agent_info_t* agents, size_t agent_count, 
                              const task_info_t* task, dispatch_result_t* result) {
    if (!agents || agent_count == 0 || !task || !result) {
        return -1;
    }
    
    // 找到权重最高的Agent
    size_t best_agent_index = 0;
    double best_weight = agents[0].weight;
    
    for (size_t i = 1; i < agent_count; i++) {
        if (agents[i].weight > best_weight) {
            best_weight = agents[i].weight;
            best_agent_index = i;
        }
    }
    
    // 设置调度结果
    result->success = true;
    result->selected_agent = memory_safe_strdup(agents[best_agent_index].agent_id);
    result->confidence = best_weight;
    
    if (!result->selected_agent) {
        result->success = false;
        result->error = memory_safe_strdup("Failed to allocate memory for selected agent");
        result->error_size = strlen(result->error);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 生成计划
 * @param task 任务信息
 * @param result 计划结果
 * @return 0 成功，非0 失败
 */
int cognition_generate_plan(const task_info_t* task, plan_result_t* result) {
    if (!task || !result) {
        return -1;
    }
    
    // 简单的计划生成逻辑
    // 实际项目中可能需要更复杂的逻辑
    size_t plan_size = strlen("Execute task: ") + strlen(task->task_content) + 1;
    char* plan = memory_safe_alloc(plan_size);
    
    if (!plan) {
        result->success = false;
        result->error = memory_safe_strdup("Failed to allocate memory for plan");
        result->error_size = strlen(result->error);
        return -1;
    }
    
    snprintf(plan, plan_size, "Execute task: %s", task->task_content);
    
    result->success = true;
    result->plan = plan;
    result->plan_size = plan_size - 1;
    
    return 0;
}

/**
 * @brief 协调多个Agent的结果
 * @param agent_results 多个Agent的结果
 * @param result_count 结果数量
 * @param result 协调结果
 * @return 0 成功，非0 失败
 */
int cognition_coordinate_results(const char** agent_results, size_t result_count, 
                               coordination_result_t* result) {
    if (!agent_results || result_count == 0 || !result) {
        return -1;
    }
    
    // 简单的协调逻辑：选择第一个结果
    // 实际项目中可能需要更复杂的逻辑，如投票、加权等
    size_t decision_size = strlen(agent_results[0]) + 1;
    char* decision = memory_safe_alloc(decision_size);
    
    if (!decision) {
        result->success = false;
        result->error = memory_safe_strdup("Failed to allocate memory for decision");
        result->error_size = strlen(result->error);
        return -1;
    }
    
    memcpy(decision, agent_results[0], decision_size);
    
    result->success = true;
    result->decision = decision;
    result->decision_size = decision_size - 1;
    
    return 0;
}

/**
 * @brief 计算任务优先级
 * @param task 任务信息
 * @return 优先级值
 */
uint64_t cognition_calculate_task_priority(const task_info_t* task) {
    if (!task) {
        return 0;
    }
    
    // 简单的优先级计算逻辑
    // 实际项目中可能需要更复杂的逻辑
    uint64_t priority = 0;
    
    // 基于任务类型设置基础优先级
    if (strcmp(task->task_type, "emergency") == 0) {
        priority = 100;
    } else if (strcmp(task->task_type, "urgent") == 0) {
        priority = 80;
    } else if (strcmp(task->task_type, "normal") == 0) {
        priority = 50;
    } else if (strcmp(task->task_type, "low") == 0) {
        priority = 20;
    }
    
    // 基于截止时间调整优先级
    if (task->deadline > 0) {
        uint64_t now = agentos_time_monotonic_ms();
        uint64_t time_left = task->deadline - now;
        
        if (time_left < 3600000) { // 少于1小时
            priority += 30;
        } else if (time_left < 86400000) { // 少于1天
            priority += 15;
        }
    }
    
    return priority;
}

/**
 * @brief 评估计划质量
 * @param plan 计划内容
 * @param task 任务信息
 * @return 质量分数（0-100）
 */
int cognition_evaluate_plan_quality(const char* plan, const task_info_t* task) {
    if (!plan || !task) {
        return 0;
    }
    
    // 简单的计划质量评估逻辑
    // 实际项目中可能需要更复杂的逻辑
    int score = 50; // 基础分数
    
    // 检查计划是否包含任务内容
    if (strstr(plan, task->task_content)) {
        score += 30;
    }
    
    // 检查计划长度
    size_t plan_length = strlen(plan);
    if (plan_length > 10) {
        score += 20;
    }
    
    // 确保分数在0-100范围内
    if (score > 100) {
        score = 100;
    }
    
    return score;
}