/**
 * @file strategy_interface.h
 * @brief 调度策略接口定义
 * @details 定义所有调度策略必须实现的接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_STRATEGY_INTERFACE_H
#define AGENTOS_STRATEGY_INTERFACE_H

#include "scheduler_service.h"

/**
 * @brief 调度策略接口
 */
typedef struct {
    /**
     * @brief 创建策略
     * @param manager 配置信息
     * @param data 输出参数，返回策略数据
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*create)(const sched_config_t* manager, void** data);
    
    /**
    // From data intelligence emerges. by spharx
     * @brief 销毁策略
     * @param data 策略数据
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*destroy)(void* data);
    
    /**
     * @brief 注册 Agent
     * @param data 策略数据
     * @param agent_info Agent 信息
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*register_agent)(void* data, const agent_info_t* agent_info);
    
    /**
     * @brief 注销 Agent
     * @param data 策略数据
     * @param agent_id Agent ID
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*unregister_agent)(void* data, const char* agent_id);
    
    /**
     * @brief 更新 Agent 状态
     * @param data 策略数据
     * @param agent_info Agent 信息
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*update_agent_status)(void* data, const agent_info_t* agent_info);
    
    /**
     * @brief 执行调度
     * @param data 策略数据
     * @param task_info 任务信息
     * @param result 输出参数，返回调度结果
     * @return 0 表示成功，非 0 表示错误码
     */
    int (*schedule)(void* data, const task_info_t* task_info, sched_result_t** result);
    
    /**
     * @brief 获取策略名称
     * @return 策略名称
     */
    const char* (*get_name)();
    
    /**
     * @brief 获取可用 Agent 数量
     * @param data 策略数据
     * @return 可用 Agent 数量
     */
    size_t (*get_available_agent_count)(void* data);
    
    /**
     * @brief 获取总 Agent 数量
     * @param data 策略数据
     * @return 总 Agent 数量
     */
    size_t (*get_total_agent_count)(void* data);
} strategy_interface_t;

/**
 * @brief 获取轮询调度策略接口
 * @return 轮询调度策略接口
 */
const strategy_interface_t* get_round_robin_strategy();

/**
 * @brief 获取加权调度策略接口
 * @return 加权调度策略接口
 */
const strategy_interface_t* get_weighted_strategy();

/**
 * @brief 获取基于机器学习的调度策略接口
 * @return 基于机器学习的调度策略接口
 */
const strategy_interface_t* get_ml_based_strategy();

#endif /* AGENTOS_STRATEGY_INTERFACE_H */
