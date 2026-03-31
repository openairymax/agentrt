/**
 * @file scheduler_service.h
 * @brief 调度服务接口定义
 * @details 负责任务调度，选择最合适的 Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SCHEDULER_SERVICE_H
#define AGENTOS_SCHEDULER_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 调度策略类型
 */
typedef enum {
    SCHED_STRATEGY_ROUND_ROBIN,  /**< 轮询调度 */
    SCHED_STRATEGY_WEIGHTED,     /**< 加权调度 */
    SCHED_STRATEGY_ML_BASED,     /**< 基于机器学习的调度 */
    SCHED_STRATEGY_COUNT
} sched_strategy_t;

/**
 * @brief 任务优先级
 */
typedef enum {
    TASK_PRIORITY_LOW,     /**< 低优先级 */
    TASK_PRIORITY_NORMAL,  /**< 正常优先级 */
    TASK_PRIORITY_HIGH,    /**< 高优先级 */
    TASK_PRIORITY_URGENT,  /**< 紧急优先级 */
    TASK_PRIORITY_COUNT
} task_priority_t;

/**
 * @brief 任务信息
 */
typedef struct {
    char* task_id;          /**< 任务 ID */
    char* task_description;  /**< 任务描述 */
    task_priority_t priority; /**< 任务优先级 */
    uint32_t timeout_ms;     /**< 超时时间（毫秒） */
    void* task_data;         /**< 任务数据 */
    size_t task_data_size;   /**< 任务数据大小 */
} task_info_t;

/**
 * @brief Agent 信息
 */
typedef struct {
    char* agent_id;         /**< Agent ID */
    char* agent_name;       /**< Agent 名称 */
    float load_factor;      /**< 负载因子（0.0-1.0） */
    float success_rate;     /**< 成功率 */
    uint32_t avg_response_time_ms; /**< 平均响应时间（毫秒） */
    bool is_available;      /**< 是否可用 */
    float weight;           /**< 权重（用于加权调度） */
} agent_info_t;

/**
 * @brief 调度结果
 */
typedef struct {
    char* selected_agent_id; /**< 选中的 Agent ID */
    float confidence;        /**< 置信度（0.0-1.0） */
    uint32_t estimated_time_ms; /**< 估计执行时间（毫秒） */
} sched_result_t;

/**
 * @brief 调度服务配置
 */
typedef struct {
    sched_strategy_t strategy;    /**< 调度策略 */
    uint32_t health_check_interval_ms; /**< 健康检查间隔（毫秒） */
    uint32_t stats_report_interval_ms; /**< 统计报告间隔（毫秒） */
    bool enable_ml_strategy;      /**< 是否启用机器学习策略 */
    char* ml_model_path;          /**< 机器学习模型路径 */
    uint32_t max_agents;          /**< 最大 Agent 数量 */
} sched_config_t;

/**
 * @brief 调度服务句柄
 */
typedef struct sched_service sched_service_t;

/**
 * @brief 创建调度服务
 * @param manager 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_create(const sched_config_t* manager, sched_service_t** service);

/**
 * @brief 销毁调度服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_destroy(sched_service_t* service);

/**
 * @brief 注册 Agent
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_register_agent(sched_service_t* service, const agent_info_t* agent_info);

/**
 * @brief 注销 Agent
 * @param service 服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_unregister_agent(sched_service_t* service, const char* agent_id);

/**
 * @brief 更新 Agent 状态
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_update_agent_status(sched_service_t* service, const agent_info_t* agent_info);

/**
 * @brief 调度任务
 * @param service 服务句柄
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_schedule_task(sched_service_t* service, const task_info_t* task_info, sched_result_t** result);

/**
 * @brief 获取调度统计信息
 * @param service 服务句柄
 * @param stats 输出参数，返回统计信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_get_stats(sched_service_t* service, void** stats);

/**
 * @brief 健康检查
 * @param service 服务句柄
 * @param health_status 输出参数，返回健康状态
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_health_check(sched_service_t* service, bool* health_status);

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param manager 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_reload_config(sched_service_t* service, const sched_config_t* manager);

#endif /* AGENTOS_SCHEDULER_SERVICE_H */
