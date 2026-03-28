/**
 * @file cognition_common.h
 * @brief 认知模块通用功能定义
 * 
 * 提供认知模块共享的功能，包括计划、调度、协调等
 * 减少认知模块之间的代码重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef COGNITION_COMMON_H
#define COGNITION_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Agent信息结构
 */
typedef struct {
    char* agent_id;           /**< Agent ID */
    double weight;            /**< 权重 */
    double success_rate;      /**< 成功率 */
    uint64_t total_tasks;     /**< 总任务数 */
    uint64_t successful_tasks; /**< 成功任务数 */
    double avg_latency;       /**< 平均延迟 */
    uint64_t last_used;       /**< 最后使用时间 */
} agent_info_t;

/**
 * @brief 任务信息结构
 */
typedef struct {
    char* task_id;            /**< 任务ID */
    char* task_type;          /**< 任务类型 */
    char* task_content;       /**< 任务内容 */
    uint64_t priority;        /**< 优先级 */
    uint64_t deadline;        /**< 截止时间 */
} task_info_t;

/**
 * @brief 计划结果结构
 */
typedef struct {
    bool success;             /**< 是否成功 */
    char* plan;               /**< 计划内容 */
    size_t plan_size;         /**< 计划大小 */
    char* error;              /**< 错误信息 */
    size_t error_size;        /**< 错误信息大小 */
} plan_result_t;

/**
 * @brief 调度结果结构
 */
typedef struct {
    bool success;             /**< 是否成功 */
    char* selected_agent;     /**< 选中的Agent */
    double confidence;        /**< 置信度 */
    char* error;              /**< 错误信息 */
    size_t error_size;        /**< 错误信息大小 */
} dispatch_result_t;

/**
 * @brief 协调结果结构
 */
typedef struct {
    bool success;             /**< 是否成功 */
    char* decision;           /**< 决策结果 */
    size_t decision_size;     /**< 决策结果大小 */
    char* error;              /**< 错误信息 */
    size_t error_size;        /**< 错误信息大小 */
} coordination_result_t;

/**
 * @brief 初始化Agent信息
 * @param agent Agent信息指针
 * @param agent_id Agent ID
 * @return 0 成功，非0 失败
 */
int agent_info_init(agent_info_t* agent, const char* agent_id);

/**
 * @brief 清理Agent信息
 * @param agent Agent信息指针
 */
void agent_info_cleanup(agent_info_t* agent);

/**
 * @brief 更新Agent性能统计
 * @param agent Agent信息指针
 * @param success 是否成功
 * @param latency 延迟时间
 */
void agent_info_update_stats(agent_info_t* agent, bool success, uint64_t latency);

/**
 * @brief 计算Agent权重
 * @param agent Agent信息指针
 * @return 计算后的权重
 */
double agent_info_calculate_weight(const agent_info_t* agent);

/**
 * @brief 初始化任务信息
 * @param task 任务信息指针
 * @param task_id 任务ID
 * @param task_type 任务类型
 * @param task_content 任务内容
 * @return 0 成功，非0 失败
 */
int task_info_init(task_info_t* task, const char* task_id, 
                  const char* task_type, const char* task_content);

/**
 * @brief 清理任务信息
 * @param task 任务信息指针
 */
void task_info_cleanup(task_info_t* task);

/**
 * @brief 初始化计划结果
 * @param result 计划结果指针
 * @return 0 成功，非0 失败
 */
int plan_result_init(plan_result_t* result);

/**
 * @brief 清理计划结果
 * @param result 计划结果指针
 */
void plan_result_cleanup(plan_result_t* result);

/**
 * @brief 初始化调度结果
 * @param result 调度结果指针
 * @return 0 成功，非0 失败
 */
int dispatch_result_init(dispatch_result_t* result);

/**
 * @brief 清理调度结果
 * @param result 调度结果指针
 */
void dispatch_result_cleanup(dispatch_result_t* result);

/**
 * @brief 初始化协调结果
 * @param result 协调结果指针
 * @return 0 成功，非0 失败
 */
int coordination_result_init(coordination_result_t* result);

/**
 * @brief 清理协调结果
 * @param result 协调结果指针
 */
void coordination_result_cleanup(coordination_result_t* result);

/**
 * @brief 选择最佳Agent
 * @param agents Agent数组
 * @param agent_count Agent数量
 * @param task 任务信息
 * @param result 调度结果
 * @return 0 成功，非0 失败
 */
int cognition_select_best_agent(agent_info_t* agents, size_t agent_count, 
                              const task_info_t* task, dispatch_result_t* result);

/**
 * @brief 生成计划
 * @param task 任务信息
 * @param result 计划结果
 * @return 0 成功，非0 失败
 */
int cognition_generate_plan(const task_info_t* task, plan_result_t* result);

/**
 * @brief 协调多个Agent的结果
 * @param agent_results 多个Agent的结果
 * @param result_count 结果数量
 * @param result 协调结果
 * @return 0 成功，非0 失败
 */
int cognition_coordinate_results(const char** agent_results, size_t result_count, 
                               coordination_result_t* result);

/**
 * @brief 计算任务优先级
 * @param task 任务信息
 * @return 优先级值
 */
uint64_t cognition_calculate_task_priority(const task_info_t* task);

/**
 * @brief 评估计划质量
 * @param plan 计划内容
 * @param task 任务信息
 * @return 质量分数（0-100）
 */
int cognition_evaluate_plan_quality(const char* plan, const task_info_t* task);

#ifdef __cplusplus
}
#endif

#endif // COGNITION_COMMON_H