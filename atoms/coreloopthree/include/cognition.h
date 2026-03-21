/**
 * @file cognition.h
 * @brief 认知层公共接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COGNITION_H
#define AGENTOS_COGNITION_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define COGNITION_API_VERSION_MAJOR 1
#define COGNITION_API_VERSION_MINOR 0
#define COGNITION_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// From data intelligence emerges. by spharx

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
    uint32_t cognition_default_timeout_ms;   /**< 默认任务超时（毫秒） */
    uint32_t cognition_max_retries;          /**< 最大重试次数 */
} agentos_cognition_config_t;

/**
 * @brief 意图结构，表示解析后的用户输入
 */
typedef struct agentos_intent {
    char* intent_raw_text;                 /**< 原始输入文本 */
    size_t intent_raw_len;                 /**< 原始文本长度 */
    char* intent_goal;                     /**< 提取的核心目标 */
    size_t intent_goal_len;                /**< 目标长度 */
    uint32_t intent_flags;                 /**< 标志位（如紧急、复杂等） */
    void* intent_context;                  /**< 附加上下文（需由上层释放） */
} agentos_intent_t;

/**
 * @brief 任务计划节点（DAG中的一个节点）
 */
typedef struct agentos_task_node {
    char* task_node_id;                   /**< 任务ID */
    size_t task_node_id_len;              /**< ID长度 */
    char* task_node_agent_role;           /**< 需要的Agent角色 */
    size_t task_node_role_len;            /**< 角色长度 */
    char** task_node_depends_on;          /**< 依赖的任务ID数组 */
    size_t task_node_depends_count;       /**< 依赖数量 */
    uint32_t task_node_timeout_ms;        /**< 超时时间 */
    uint8_t task_node_priority;           /**< 优先级 */
    void* task_node_input;                /**< 输入数据（由策略管理） */
    void* task_node_output;               /**< 输出数据 */
} agentos_task_node_t;

/**
 * @brief 任务计划（DAG）
 */
typedef struct agentos_task_plan {
    char* task_plan_id;                    /**< 计划ID */
    size_t task_plan_id_len;               /**< ID长度 */
    agentos_task_node_t** task_plan_nodes;  /**< 节点数组 */
    size_t task_plan_node_count;           /**< 节点数量 */
    char** task_plan_entry_points;         /**< 入口点节点ID数组 */
    size_t task_plan_entry_count;          /**< 入口点数量 */
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
 * 
 * @param plan_strategy [in] 规划策略（可选，若为NULL则使用默认策略）
 * @param coord_strategy [in] 协同策略（可选）
 * @param disp_strategy [in] 调度策略（可选）
 * @param out_engine [out] 输出引擎句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_engine 由调用者负责通过 agentos_cognition_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * 
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中创建认知引擎
 * - 创建完成后，引擎实例可以被多个线程使用，但需要外部同步
 * 
 * @see agentos_cognition_create_ex(), agentos_cognition_destroy()
 */
AGENTOS_API agentos_error_t agentos_cognition_create(
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine);

/**
 * @brief 创建认知引擎（带配置）
 * 
 * @param config [in] 配置（若为NULL使用默认）
 * @param plan_strategy [in] 规划策略（可选）
 * @param coord_strategy [in] 协同策略（可选）
 * @param disp_strategy [in] 调度策略（可选）
 * @param out_engine [out] 输出引擎句柄（调用者负责销毁）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_engine 由调用者负责通过 agentos_cognition_destroy() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * 
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中创建认知引擎
 * - 创建完成后，引擎实例可以被多个线程使用，但需要外部同步
 * 
 * @see agentos_cognition_create(), agentos_cognition_destroy()
 */
AGENTOS_API agentos_error_t agentos_cognition_create_ex(
    const agentos_cognition_config_t* config,
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine);

/**
 * @brief 销毁认知引擎
 * 
 * @param engine [in] 引擎句柄（非NULL）
 * 
 * @ownership 释放 engine 及其内部资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * 
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在所有使用引擎的线程都已完成操作后再调用此函数
 * - 销毁后，引擎实例不应再被任何线程使用
 * 
 * @see agentos_cognition_create(), agentos_cognition_create_ex()
 */
AGENTOS_API void agentos_cognition_destroy(agentos_cognition_engine_t* engine);

/**
 * @brief 设置回退规划策略
 * 
 * @param engine [in] 认知引擎（非NULL）
 * @param fallback [in] 回退策略（可为NULL）
 * 
 * @ownership 引擎不接管 fallback 的所有权，调用者仍需负责其生命周期
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_fallback_plan(
    agentos_cognition_engine_t* engine,
    agentos_plan_strategy_t* fallback);

/**
 * @brief 处理用户输入，生成任务计划
 * 
 * @param engine [in] 认知引擎（非NULL）
 * @param input [in] 原始输入字符串（非NULL）
 * @param input_len [in] 输入长度
 * @param out_plan [out] 输出任务计划（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_plan 由调用者负责通过 agentos_task_plan_free() 释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * 
 * @concurrency 并发合约：
 * - 该函数不是线程安全的，不应在多个线程同时调用
 * - 调用者应确保在单个线程中处理输入，或使用外部同步机制
 * - 处理过程中，引擎实例不应被其他线程修改
 * 
 * @see agentos_task_plan_free()
 */
AGENTOS_API agentos_error_t agentos_cognition_process(
    agentos_cognition_engine_t* engine,
    const char* input,
    size_t input_len,
    agentos_task_plan_t** out_plan);

/**
 * @brief 释放任务计划
 * 
 * @param plan [in] 要释放的计划（可为NULL）
 * 
 * @ownership 释放 plan 及其内部所有资源
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 * @see agentos_cognition_process()
 */
AGENTOS_API void agentos_task_plan_free(agentos_task_plan_t* plan);

/**
 * @brief 设置认知引擎的全局上下文
 * 
 * @param engine [in] 引擎句柄（非NULL）
 * @param context [in] 上下文指针（可为NULL）
 * @param destroy [in] 上下文释放函数（可为NULL）
 * 
 * @ownership 如果提供了 destroy 函数，引擎会在销毁时调用它来释放 context
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API void agentos_cognition_set_context(
    agentos_cognition_engine_t* engine,
    void* context,
    void (*destroy)(void*));

/**
 * @brief 获取认知引擎的当前统计信息
 * 
 * @param engine [in] 引擎句柄（非NULL）
 * @param out_stats [out] 输出统计字符串（调用者负责释放）
 * @param out_len [out] 输出长度
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_stats 由调用者负责释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_cognition_stats(
    agentos_cognition_engine_t* engine,
    char** out_stats,
    size_t* out_len);

/**
 * @brief 获取认知引擎健康状态
 * 
 * @param engine [in] 认知引擎句柄（非NULL）
 * @param out_json [out] 输出 JSON 状态字符串（调用者负责释放）
 * @return agentos_error_t AGENTOS_SUCCESS 成功，其他为错误码
 * 
 * @ownership out_json 由调用者负责释放
 * @threadsafe 否（内部未使用线程安全措施）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t agentos_cognition_health_check(
    agentos_cognition_engine_t* engine,
    char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COGNITION_H */