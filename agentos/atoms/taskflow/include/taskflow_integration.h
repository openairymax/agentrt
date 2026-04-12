// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file taskflow_integration.h
 * @brief TaskFlow与Atoms任务系统集成接口
 *
 * 提供TaskFlow作为Atoms执行单元的适配器接口，
 * 允许Atoms任务系统执行图计算任务。
 */

#ifndef AGENTOS_TASKFLOW_INTEGRATION_H
#define AGENTOS_TASKFLOW_INTEGRATION_H

#include "taskflow.h"
#include "../../coreloopthree/include/execution.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TaskFlow执行单元配置
// ============================================================================

/**
 * @brief TaskFlow执行单元配置结构
 */
typedef struct {
    taskflow_config_t taskflow_config;   /**< TaskFlow引擎配置 */
    size_t max_concurrent_graphs;        /**< 最大并发图数量 */
    uint32_t default_timeout_ms;         /**< 默认超时时间（毫秒） */
    bool enable_auto_checkpoint;         /**< 是否启用自动检查点 */
    size_t auto_checkpoint_interval;     /**< 自动检查点间隔（超步数） */
} taskflow_unit_config_t;

/**
 * @brief TaskFlow任务输入格式
 */
typedef struct {
    taskflow_graph_handle_t graph;       /**< 图句柄（可选） */
    taskflow_config_t graph_config;      /**< 图配置（如果graph为NULL） */
    size_t vertex_count;                 /**< 顶点数量 */
    graph_vertex_t* vertices;            /**< 顶点数组 */
    size_t edge_count;                   /**< 边数量 */
    graph_edge_t* edges;                 /**< 边数组 */
    size_t max_supersteps;               /**< 最大超步数 */
    void* user_context;                  /**< 用户上下文 */
} taskflow_task_input_t;

/**
 * @brief TaskFlow任务输出格式
 */
typedef struct {
    taskflow_error_t result;             /**< 执行结果 */
    superstep_t completed_supersteps;    /**< 已完成的超步数 */
    size_t active_vertices;              /**< 活跃顶点数量 */
    execution_stats_t stats;             /**< 执行统计信息 */
    void* result_data;                   /**< 结果数据（用户定义） */
    size_t result_data_size;             /**< 结果数据大小 */
} taskflow_task_output_t;

// ============================================================================
// TaskFlow执行单元API
// ============================================================================

/**
 * @brief 创建TaskFlow执行单元
 * @param config 单元配置
 * @return 执行单元句柄，失败返回NULL
 */
agentos_execution_unit_t* taskflow_unit_create(const taskflow_unit_config_t* config);

/**
 * @brief 销毁TaskFlow执行单元
 * @param unit 执行单元句柄
 */
void taskflow_unit_destroy(agentos_execution_unit_t* unit);

/**
 * @brief 创建TaskFlow任务输入
 * @param graph 图句柄（可选，如果为NULL则从vertices/edges创建）
 * @param vertices 顶点数组
 * @param vertex_count 顶点数量
 * @param edges 边数组
 * @param edge_count 边数量
 * @param max_supersteps 最大超步数
 * @return 任务输入句柄，失败返回NULL
 */
taskflow_task_input_t* taskflow_task_input_create(
    taskflow_graph_handle_t graph,
    const graph_vertex_t* vertices,
    size_t vertex_count,
    const graph_edge_t* edges,
    size_t edge_count,
    size_t max_supersteps);

/**
 * @brief 销毁TaskFlow任务输入
 * @param input 任务输入句柄
 */
void taskflow_task_input_destroy(taskflow_task_input_t* input);

/**
 * @brief 创建TaskFlow任务输出
 * @return 任务输出句柄，失败返回NULL
 */
taskflow_task_output_t* taskflow_task_output_create(void);

/**
 * @brief 销毁TaskFlow任务输出
 * @param output 任务输出句柄
 */
void taskflow_task_output_destroy(taskflow_task_output_t* output);

/**
 * @brief 从AgentOS任务结构解析TaskFlow任务输入
 * @param agentos_task AgentOS任务结构
 * @return TaskFlow任务输入句柄，失败返回NULL
 */
taskflow_task_input_t* taskflow_parse_task_input(const agentos_task_t* agentos_task);

/**
 * @brief 将TaskFlow任务输出打包到AgentOS任务结构
 * @param output TaskFlow任务输出
 * @param agentos_task AgentOS任务结构（输出）
 * @return 错误码
 */
taskflow_error_t taskflow_pack_task_output(const taskflow_task_output_t* output,
                                          agentos_task_t* agentos_task);

// ============================================================================
// 注册辅助函数
// ============================================================================

/**
 * @brief 注册TaskFlow执行单元到执行引擎
 * @param engine 执行引擎句柄
 * @param unit_name 单元名称
 * @param config 单元配置
 * @return AgentOS错误码
 */
agentos_error_t taskflow_register_unit(
    agentos_execution_engine_t* engine,
    const char* unit_name,
    const taskflow_unit_config_t* config);

/**
 * @brief 注销TaskFlow执行单元
 * @param engine 执行引擎句柄
 * @param unit_name 单元名称
 */
void taskflow_unregister_unit(
    agentos_execution_engine_t* engine,
    const char* unit_name);

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_TASKFLOW_INTEGRATION_H
