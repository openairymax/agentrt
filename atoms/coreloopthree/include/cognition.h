/**
 * @file cognition.h
 * @brief 认知层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_COGNITION_H
#define AGENTOS_COGNITION_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_cognition_engine agentos_cognition_engine_t;
typedef struct agentos_intent agentos_intent_t;
typedef struct agentos_task_plan agentos_task_plan_t;
typedef struct agentos_plan_strategy agentos_plan_strategy_t;
typedef struct agentos_coordinator_strategy agentos_coordinator_strategy_t;
typedef struct agentos_dispatching_strategy agentos_dispatching_strategy_t;

/**
 * @brief 认知引擎配置
 */
typedef struct agentos_cognition_config {
    uint32_t default_timeout_ms;   /**< 默认任务超时（毫秒） */
    uint32_t max_retries;          /**< 最大重试次数 */
} agentos_cognition_config_t;

/**
 * @brief 意图结构，表示解析后的用户输入
 */
typedef struct agentos_intent {
    char* raw_text;                 /**< 原始输入文本 */
    size_t raw_len;                 /**< 原始文本长度 */
    char* goal;                      /**< 提取的核心目标 */
    size_t goal_len;                 /**< 目标长度 */
    uint32_t flags;                  /**< 标志位（如紧急、复杂等） */
    void* context;                   /**< 附加上下文（需由上层释放） */
} agentos_intent_t;

/**
 * @brief 任务计划节点（DAG中的一个节点）
 */
typedef struct agentos_task_node {
    char* task_id;                   /**< 任务ID */
    size_t id_len;                    /**< ID长度 */
    char* agent_role;                 /**< 需要的Agent角色 */
    size_t role_len;                  /**< 角色长度 */
    char** depends_on;                /**< 依赖的任务ID数组 */
    size_t depends_count;             /**< 依赖数量 */
    uint32_t timeout_ms;              /**< 超时时间 */
    uint8_t priority;                 /**< 优先级 */
    void* input;                      /**< 输入数据（由策略管理） */
    void* output;                     /**< 输出数据 */
} agentos_task_node_t;

/**
 * @brief 任务计划（DAG）
 */
typedef struct agentos_task_plan {
    char* plan_id;                    /**< 计划ID */
    size_t id_len;                     /**< ID长度 */
    agentos_task_node_t** nodes;       /**< 节点数组 */
    size_t node_count;                 /**< 节点数量 */
    char** entry_points;               /**< 入口点节点ID数组 */
    size_t entry_count;                /**< 入口点数量 */
} agentos_task_plan_t;

/* ==================== 规划策略接口 ==================== */

/**
 * @brief 规划策略函数类型，根据意图生成任务计划
 * @param intent 解析后的意图
 * @param context 上下文
 * @param out_plan 输出计划（需由调用者释放）
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_plan_func_t)(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan);

/**
 * @brief 规划策略释放函数
 * @param strategy 策略对象
 */
typedef void (*agentos_plan_destroy_t)(agentos_plan_strategy_t* strategy);

/**
 * @brief 规划策略对象
 */
struct agentos_plan_strategy {
    agentos_plan_func_t plan;          /**< 规划函数 */
    agentos_plan_destroy_t destroy;    /**< 释放函数 */
    void* data;                         /**< 私有数据 */
};

/* ==================== 协同策略接口 ==================== */

/**
 * @brief 协同策略函数类型，协调多个模型输出
 * @param prompts 多个模型的输入提示
 * @param count 模型数量
 * @param context 上下文
 * @param out_result 输出协调结果
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_coordinate_func_t)(
    const char** prompts,
    size_t count,
    void* context,
    char** out_result);

/**
 * @brief 协同策略释放函数
 */
typedef void (*agentos_coordinator_destroy_t)(agentos_coordinator_strategy_t* strategy);

/**
 * @brief 协同策略对象
 */
struct agentos_coordinator_strategy {
    agentos_coordinate_func_t coordinate;
    agentos_coordinator_destroy_t destroy;
    void* data;
};

/* ==================== 调度策略接口 ==================== */

/**
 * @brief 调度策略函数类型，从候选Agent中选择最合适的一个
 * @param task 待分配的任务节点
 * @param candidates 候选Agent信息数组（格式由策略定义）
 * @param count 候选数量
 * @param context 上下文
 * @param out_agent_id 输出选中的Agent ID
 * @return agentos_error_t
 */
typedef agentos_error_t (*agentos_dispatch_func_t)(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id);

/**
 * @brief 调度策略释放函数
 */
typedef void (*agentos_dispatch_destroy_t)(agentos_dispatching_strategy_t* strategy);

/**
 * @brief 调度策略对象
 */
struct agentos_dispatching_strategy {
    agentos_dispatch_func_t dispatch;
    agentos_dispatch_destroy_t destroy;
    void* data;
};

/* ==================== 认知引擎接口 ==================== */

/**
 * @brief 创建认知引擎（使用默认配置）
 * @param plan_strategy 规划策略（可选，若为NULL则使用默认策略）
 * @param coord_strategy 协同策略（可选）
 * @param disp_strategy 调度策略（可选）
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_cognition_create(
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine);

/**
 * @brief 创建认知引擎（带配置）
 * @param config 配置（若为NULL使用默认）
 * @param plan_strategy 规划策略（可选）
 * @param coord_strategy 协同策略（可选）
 * @param disp_strategy 调度策略（可选）
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_cognition_create_ex(
    const agentos_cognition_config_t* config,
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine);

/**
 * @brief 销毁认知引擎
 * @param engine 引擎句柄
 */
void agentos_cognition_destroy(agentos_cognition_engine_t* engine);

/**
 * @brief 设置回退规划策略
 * @param engine 认知引擎
 * @param fallback 回退策略
 */
void agentos_cognition_set_fallback_plan(
    agentos_cognition_engine_t* engine,
    agentos_plan_strategy_t* fallback);

/**
 * @brief 处理用户输入，生成任务计划
 * @param engine 认知引擎
 * @param input 原始输入字符串
 * @param input_len 输入长度
 * @param out_plan 输出任务计划（需调用 agentos_task_plan_free 释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_cognition_process(
    agentos_cognition_engine_t* engine,
    const char* input,
    size_t input_len,
    agentos_task_plan_t** out_plan);

/**
 * @brief 释放任务计划
 * @param plan 要释放的计划
 */
void agentos_task_plan_free(agentos_task_plan_t* plan);

/**
 * @brief 设置认知引擎的全局上下文
 * @param engine 引擎句柄
 * @param context 上下文指针
 * @param destroy 上下文释放函数（可为NULL）
 */
void agentos_cognition_set_context(
    agentos_cognition_engine_t* engine,
    void* context,
    void (*destroy)(void*));

/**
 * @brief 获取认知引擎的当前统计信息
 * @param engine 引擎句柄
 * @param out_stats 输出统计字符串（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
agentos_error_t agentos_cognition_stats(
    agentos_cognition_engine_t* engine,
    char** out_stats,
    size_t* out_len);

/**
 * @brief 获取认知引擎健康状态
 * @param engine 认知引擎句柄
 * @param out_json 输出 JSON 状态字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_cognition_health_check(
    agentos_cognition_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COGNITION_H */