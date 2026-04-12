// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file workflow_patterns.h
 * @brief 基础工作流模式实现
 * 
 * 提供常见的工作流模式，如顺序执行、并行执行、条件分支等。
 * 基于TaskFlow图引擎构建，支持复杂工作流编排。
 */

#ifndef AGENTOS_WORKFLOW_PATTERNS_H
#define AGENTOS_WORKFLOW_PATTERNS_H

#include "taskflow.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 工作流模式类型定义
// ============================================================================

/**
 * @brief 工作流模式类型枚举
 */
typedef enum {
    WORKFLOW_SEQUENTIAL = 0,    /**< 顺序执行模式 */
    WORKFLOW_PARALLEL,          /**< 并行执行模式 */
    WORKFLOW_CONDITIONAL,       /**< 条件分支模式 */
    WORKFLOW_LOOP,              /**< 循环模式 */
    WORKFLOW_FORK_JOIN,         /**< Fork-Join模式 */
    WORKFLOW_PIPELINE,          /**< 流水线模式 */
    WORKFLOW_CUSTOM             /**< 自定义模式 */
} workflow_pattern_type_t;

/**
 * @brief 工作流节点类型枚举
 */
typedef enum {
    NODE_TASK = 0,              /**< 任务节点 */
    NODE_CONDITION,             /**< 条件节点 */
    NODE_LOOP_START,            /**< 循环开始节点 */
    NODE_LOOP_END,              /**< 循环结束节点 */
    NODE_PARALLEL_START,        /**< 并行开始节点 */
    NODE_PARALLEL_END,          /**< 并行结束节点 */
    NODE_SUBWORKFLOW            /**< 子工作流节点 */
} workflow_node_type_t;

/**
 * @brief 工作流节点结构
 */
typedef struct {
    vertex_id_t node_id;                /**< 节点ID */
    workflow_node_type_t node_type;     /**< 节点类型 */
    char* node_name;                    /**< 节点名称 */
    void* task_data;                    /**< 任务数据（类型相关） */
    size_t task_data_size;              /**< 任务数据大小 */
    void (*task_executor)(void* data);  /**< 任务执行函数 */
    void* user_context;                 /**< 用户上下文 */
} workflow_node_t;

/**
 * @brief 工作流边结构（带条件）
 */
typedef struct {
    edge_id_t edge_id;                  /**< 边ID */
    vertex_id_t source_node;            /**< 源节点ID */
    vertex_id_t target_node;            /**< 目标节点ID */
    bool (*condition_func)(void* ctx);  /**< 条件函数（可选） */
    void* condition_context;            /**< 条件上下文 */
    char* edge_label;                   /**< 边标签（用于调试） */
} workflow_edge_t;

/**
 * @brief 工作流模式配置
 */
typedef struct {
    workflow_pattern_type_t pattern_type;   /**< 模式类型 */
    size_t max_nodes;                       /**< 最大节点数 */
    size_t max_edges;                       /**< 最大边数 */
    bool enable_tracing;                    /**< 是否启用执行追踪 */
    bool enable_checkpoint;                 /**< 是否启用检查点 */
    size_t checkpoint_interval;             /**< 检查点间隔（节点数） */
} workflow_pattern_config_t;

/**
 * @brief 工作流执行上下文
 */
typedef struct {
    workflow_pattern_config_t config;       /**< 工作流配置 */
    taskflow_handle_t taskflow_engine;      /**< TaskFlow引擎句柄 */
    taskflow_graph_handle_t workflow_graph; /**< 工作流图句柄 */
    workflow_node_t* nodes;                 /**< 节点数组 */
    size_t node_count;                      /**< 节点数量 */
    workflow_edge_t* edges;                 /**< 边数组 */
    size_t edge_count;                      /**< 边数量 */
    vertex_id_t start_node;                 /**< 起始节点ID */
    vertex_id_t end_node;                   /**< 结束节点ID */
    void* execution_state;                  /**< 执行状态（内部使用） */
} workflow_context_t;

// ============================================================================
// 工作流模式API
// ============================================================================

/**
 * @brief 创建工作流上下文
 * @param config 工作流配置
 * @param taskflow_engine TaskFlow引擎句柄（可为NULL，内部创建）
 * @return 工作流上下文句柄，失败返回NULL
 */
workflow_context_t* workflow_context_create(
    const workflow_pattern_config_t* config,
    taskflow_handle_t taskflow_engine);

/**
 * @brief 销毁工作流上下文
 * @param context 工作流上下文句柄
 */
void workflow_context_destroy(workflow_context_t* context);

/**
 * @brief 添加节点到工作流
 * @param context 工作流上下文
 * @param node 节点结构
 * @return 错误码
 */
taskflow_error_t workflow_add_node(
    workflow_context_t* context,
    const workflow_node_t* node);

/**
 * @brief 添加边到工作流
 * @param context 工作流上下文
 * @param edge 边结构
 * @return 错误码
 */
taskflow_error_t workflow_add_edge(
    workflow_context_t* context,
    const workflow_edge_t* edge);

/**
 * @brief 设置工作流起始节点
 * @param context 工作流上下文
 * @param start_node_id 起始节点ID
 * @return 错误码
 */
taskflow_error_t workflow_set_start_node(
    workflow_context_t* context,
    vertex_id_t start_node_id);

/**
 * @brief 设置工作流结束节点
 * @param context 工作流上下文
 * @param end_node_id 结束节点ID
 * @return 错误码
 */
taskflow_error_t workflow_set_end_node(
    workflow_context_t* context,
    vertex_id_t end_node_id);

// ============================================================================
// 预定义工作流模式构建器
// ============================================================================

/**
 * @brief 创建顺序工作流
 * @param context 工作流上下文
 * @param node_ids 节点ID数组（按顺序）
 * @param node_count 节点数量
 * @return 错误码
 */
taskflow_error_t workflow_build_sequential(
    workflow_context_t* context,
    const vertex_id_t* node_ids,
    size_t node_count);

/**
 * @brief 创建并行工作流
 * @param context 工作流上下文
 * @param start_node_id 起始节点ID
 * @param parallel_node_ids 并行节点ID数组
 * @param parallel_count 并行节点数量
 * @param end_node_id 结束节点ID
 * @return 错误码
 */
taskflow_error_t workflow_build_parallel(
    workflow_context_t* context,
    vertex_id_t start_node_id,
    const vertex_id_t* parallel_node_ids,
    size_t parallel_count,
    vertex_id_t end_node_id);

/**
 * @brief 创建条件分支工作流
 * @param context 工作流上下文
 * @param condition_node_id 条件节点ID
 * @param true_branch_node_id 真分支节点ID
 * @param false_branch_node_id 假分支节点ID
 * @param merge_node_id 合并节点ID
 * @return 错误码
 */
taskflow_error_t workflow_build_conditional(
    workflow_context_t* context,
    vertex_id_t condition_node_id,
    vertex_id_t true_branch_node_id,
    vertex_id_t false_branch_node_id,
    vertex_id_t merge_node_id);

/**
 * @brief 创建循环工作流
 * @param context 工作流上下文
 * @param loop_start_node_id 循环开始节点ID
 * @param loop_body_node_id 循环体节点ID
 * @param loop_condition_node_id 循环条件节点ID
 * @param loop_end_node_id 循环结束节点ID
 * @return 错误码
 */
taskflow_error_t workflow_build_loop(
    workflow_context_t* context,
    vertex_id_t loop_start_node_id,
    vertex_id_t loop_body_node_id,
    vertex_id_t loop_condition_node_id,
    vertex_id_t loop_end_node_id);

// ============================================================================
// 工作流执行控制
// ============================================================================

/**
 * @brief 执行工作流（同步）
 * @param context 工作流上下文
 * @param max_iterations 最大迭代次数（用于循环）
 * @return 错误码
 */
taskflow_error_t workflow_execute_sync(
    workflow_context_t* context,
    size_t max_iterations);

/**
 * @brief 执行工作流（异步）
 * @param context 工作流上下文
 * @param max_iterations 最大迭代次数
 * @param callback 完成回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
taskflow_error_t workflow_execute_async(
    workflow_context_t* context,
    size_t max_iterations,
    void (*callback)(taskflow_error_t result, void* user_data),
    void* user_data);

/**
 * @brief 暂停工作流执行
 * @param context 工作流上下文
 * @return 错误码
 */
taskflow_error_t workflow_pause(workflow_context_t* context);

/**
 * @brief 恢复工作流执行
 * @param context 工作流上下文
 * @return 错误码
 */
taskflow_error_t workflow_resume(workflow_context_t* context);

/**
 * @brief 停止工作流执行
 * @param context 工作流上下文
 * @return 错误码
 */
taskflow_error_t workflow_stop(workflow_context_t* context);

/**
 * @brief 获取工作流执行状态
 * @param context 工作流上下文
 * @param completed_nodes 已完成节点数（输出）
 * @param total_nodes 总节点数（输出）
 * @param current_node 当前节点ID（输出）
 * @return 错误码
 */
taskflow_error_t workflow_get_status(
    workflow_context_t* context,
    size_t* completed_nodes,
    size_t* total_nodes,
    vertex_id_t* current_node);

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_WORKFLOW_PATTERNS_H