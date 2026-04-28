// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file taskflow.h
 * @brief AgentOS TaskFlow Graph Execution System Main Header
 * 
 * TaskFlow 图执行系统主头文件，提供基于 Pregel 超步模型的分布式计算框架。
 * 支持大规模图计算的容错、分布式执行和状态管理。
 * 
 * 主要特性：
 * 1. 有向无环图（DAG）执行引擎
 * 2. Pregel 超步迭代计算模型
 * 3. 分布式图分区与消息传递
 * 4. 容错与状态检查点
 * 5. 高性能批处理与流水线
 */

#ifndef AGENTOS_TASKFLOW_H
#define AGENTOS_TASKFLOW_H

#include "taskflow_types.h"

/**
 * @defgroup taskflow TaskFlow Graph Execution System
 * @brief 图执行系统模块
 * 
 * 基于 Pregel 模型的分布式图计算框架，提供大规模图分析的执行引擎。
 * 支持容错、检查点和分布式执行。
 * 
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 错误码定义
// ============================================================================

/**
 * @brief TaskFlow 错误码枚举
 */
typedef enum {
    TASKFLOW_SUCCESS = 0,           /**< 成功 */
    TASKFLOW_ERROR_INVALID_ARG,     /**< 无效参数 */
    TASKFLOW_ERROR_MEMORY,          /**< 内存不足 */
    TASKFLOW_ERROR_NOT_INITIALIZED, /**< 未初始化 */
    TASKFLOW_ERROR_ALREADY_INITIALIZED, /**< 已初始化 */
    TASKFLOW_ERROR_GRAPH_TOO_LARGE, /**< 图过大 */
    TASKFLOW_ERROR_PARTITION,       /**< 分区错误 */
    TASKFLOW_ERROR_CHECKPOINT,      /**< 检查点错误 */
    TASKFLOW_ERROR_TIMEOUT,         /**< 超时错误 */
    TASKFLOW_ERROR_FAULT_DETECTED,  /**< 检测到故障 */
    TASKFLOW_ERROR_COMMUNICATION,   /**< 通信错误 */
    TASKFLOW_ERROR_INTERNAL,        /**< 内部错误 */
    TASKFLOW_ERROR_NO_ACTIVE_VERTICES, /**< 无活跃顶点(计算完成) */
    TASKFLOW_ERROR_ALREADY_RUNNING  /**< 引擎已在运行 */
} taskflow_error_code_t;

// ============================================================================
// TaskFlow 引擎句柄
// ============================================================================

/**
 * @brief TaskFlow 引擎句柄（不透明类型）
 */
typedef struct taskflow_engine_s* taskflow_handle_t;

/**
 * @brief 图句柄（不透明类型）
 */
typedef struct taskflow_graph_s* taskflow_graph_handle_t;

/**
 * @brief 分区句柄（不透明类型）
 */
typedef struct taskflow_partition_s* taskflow_partition_handle_t;

// ============================================================================
// 核心API：引擎管理
// ============================================================================

/**
 * @brief 创建 TaskFlow 引擎实例
 * @param config 引擎配置参数
 * @return 引擎句柄，失败返回 NULL
 */
taskflow_handle_t taskflow_engine_create(const taskflow_config_t* config);

/**
 * @brief 销毁 TaskFlow 引擎实例
 * @param engine 引擎句柄
 */
void taskflow_engine_destroy(taskflow_handle_t engine);

/**
 * @brief 初始化 TaskFlow 引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_engine_init(taskflow_handle_t engine);

/**
 * @brief 启动 TaskFlow 引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_engine_start(taskflow_handle_t engine);

/**
 * @brief 停止 TaskFlow 引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_engine_stop(taskflow_handle_t engine);

/**
 * @brief 暂停 TaskFlow 引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_engine_pause(taskflow_handle_t engine);

/**
 * @brief 恢复 TaskFlow 引擎
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_engine_resume(taskflow_handle_t engine);

// ============================================================================
// 核心API：图管理
// ============================================================================

/**
 * @brief 创建空图
 * @param engine 引擎句柄
 * @return 图句柄，失败返回 NULL
 */
taskflow_graph_handle_t taskflow_graph_create(taskflow_handle_t engine);

/**
 * @brief 销毁图
 * @param graph 图句柄
 */
void taskflow_graph_destroy(taskflow_graph_handle_t graph);

/**
 * @brief 添加顶点到图
 * @param graph 图句柄
 * @param vertex 顶点结构
 * @return 错误码
 */
taskflow_error_t taskflow_graph_add_vertex(taskflow_graph_handle_t graph, const graph_vertex_t* vertex);

/**
 * @brief 从图移除顶点
 * @param graph 图句柄
 * @param vertex_id 顶点ID
 * @return 错误码
 */
taskflow_error_t taskflow_graph_remove_vertex(taskflow_graph_handle_t graph, vertex_id_t vertex_id);

/**
 * @brief 添加边到图
 * @param graph 图句柄
 * @param edge 边结构
 * @return 错误码
 */
taskflow_error_t taskflow_graph_add_edge(taskflow_graph_handle_t graph, const graph_edge_t* edge);

/**
 * @brief 从图移除边
 * @param graph 图句柄
 * @param edge_id 边ID
 * @return 错误码
 */
taskflow_error_t taskflow_graph_remove_edge(taskflow_graph_handle_t graph, edge_id_t edge_id);

/**
 * @brief 获取图顶点数量
 * @param graph 图句柄
 * @return 顶点数量
 */
size_t taskflow_graph_get_vertex_count(taskflow_graph_handle_t graph);

/**
 * @brief 获取图边数量
 * @param graph 图句柄
 * @return 边数量
 */
size_t taskflow_graph_get_edge_count(taskflow_graph_handle_t graph);

// ============================================================================
// 核心API：分区管理
// ============================================================================

/**
 * @brief 对图进行分区
 * @param graph 图句柄
 * @param strategy 分区策略
 * @param partition_count 分区数量
 * @return 错误码
 */
taskflow_error_t taskflow_graph_partition(taskflow_graph_handle_t graph,
                                         partition_strategy_t strategy,
                                         size_t partition_count);

/**
 * @brief 获取图的分区列表
 * @param graph 图句柄
 * @param partitions 分区句柄数组（输出）
 * @param max_count 数组最大容量
 * @return 实际分区数量
 */
size_t taskflow_graph_get_partitions(taskflow_graph_handle_t graph,
                                    taskflow_partition_handle_t* partitions,
                                    size_t max_count);

// ============================================================================
// 核心API：执行控制
// ============================================================================

/**
 * @brief 执行图计算（同步）
 * @param engine 引擎句柄
 * @param graph 图句柄
 * @param max_supersteps 最大超步数
 * @return 错误码
 */
taskflow_error_t taskflow_execute_sync(taskflow_handle_t engine,
                                      taskflow_graph_handle_t graph,
                                      size_t max_supersteps);

/**
 * @brief 执行图计算（异步）
 * @param engine 引擎句柄
 * @param graph 图句柄
 * @param max_supersteps 最大超步数
 * @param callback 完成回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
taskflow_error_t taskflow_execute_async(taskflow_handle_t engine,
                                       taskflow_graph_handle_t graph,
                                       size_t max_supersteps,
                                       void (*callback)(taskflow_error_t result, void* user_data),
                                       void* user_data);

/**
 * @brief 取消正在执行的计算
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_execute_cancel(taskflow_handle_t engine);

/**
 * @brief 等待计算完成
 * @param engine 引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
taskflow_error_t taskflow_execute_wait(taskflow_handle_t engine, uint32_t timeout_ms);

// ============================================================================
// 核心API：消息传递
// ============================================================================

/**
 * @brief 发送消息到顶点
 * @param engine 引擎句柄
 * @param source 源顶点ID
 * @param target 目标顶点ID
 * @param payload 消息负载
 * @param payload_size 负载大小
 * @return 错误码
 */
taskflow_error_t taskflow_send_message(taskflow_handle_t engine,
                                      vertex_id_t source,
                                      vertex_id_t target,
                                      const void* payload,
                                      size_t payload_size);

/**
 * @brief 广播消息到所有顶点
 * @param engine 引擎句柄
 * @param source 源顶点ID
 * @param payload 消息负载
 * @param payload_size 负载大小
 * @return 错误码
 */
taskflow_error_t taskflow_broadcast_message(taskflow_handle_t engine,
                                           vertex_id_t source,
                                           const void* payload,
                                           size_t payload_size);

/**
 * @brief 获取顶点的入站消息
 * @param engine 引擎句柄
 * @param vertex_id 顶点ID
 * @param messages 消息数组（输出）
 * @param max_count 数组最大容量
 * @return 实际消息数量
 */
size_t taskflow_get_incoming_messages(taskflow_handle_t engine,
                                     vertex_id_t vertex_id,
                                     graph_message_t* messages,
                                     size_t max_count);

/**
 * @brief 清空顶点的消息队列
 * @param engine 引擎句柄
 * @param vertex_id 顶点ID
 * @return 错误码
 */
taskflow_error_t taskflow_clear_messages(taskflow_handle_t engine, vertex_id_t vertex_id);

// ============================================================================
// 核心API：检查点与容错
// ============================================================================

/**
 * @brief 创建检查点
 * @param engine 引擎句柄
 * @return 检查点ID，失败返回 0
 */
uint64_t taskflow_create_checkpoint(taskflow_handle_t engine);

/**
 * @brief 恢复检查点
 * @param engine 引擎句柄
 * @param checkpoint_id 检查点ID
 * @return 错误码
 */
taskflow_error_t taskflow_restore_checkpoint(taskflow_handle_t engine, uint64_t checkpoint_id);

/**
 * @brief 删除检查点
 * @param engine 引擎句柄
 * @param checkpoint_id 检查点ID
 * @return 错误码
 */
taskflow_error_t taskflow_delete_checkpoint(taskflow_handle_t engine, uint64_t checkpoint_id);

/**
 * @brief 获取可用的检查点列表
 * @param engine 引擎句柄
 * @param checkpoints 检查点ID数组（输出）
 * @param max_count 数组最大容量
 * @return 实际检查点数量
 */
size_t taskflow_list_checkpoints(taskflow_handle_t engine,
                                uint64_t* checkpoints,
                                size_t max_count);

// ============================================================================
// 核心API：统计与监控
// ============================================================================

/**
 * @brief 获取执行统计信息
 * @param engine 引擎句柄
 * @param stats 统计信息结构（输出）
 * @return 错误码
 */
taskflow_error_t taskflow_get_stats(taskflow_handle_t engine, execution_stats_t* stats);

/**
 * @brief 重置统计信息
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t taskflow_reset_stats(taskflow_handle_t engine);

/**
 * @brief 获取当前超步
 * @param engine 引擎句柄
 * @return 当前超步ID，未运行返回 0
 */
superstep_t taskflow_get_current_superstep(taskflow_handle_t engine);

/**
 * @brief 获取活跃顶点数量
 * @param engine 引擎句柄
 * @return 活跃顶点数量
 */
size_t taskflow_get_active_vertex_count(taskflow_handle_t engine);

/**
 * @brief 获取队列中的消息数量
 * @param engine 引擎句柄
 * @return 消息数量
 */
size_t taskflow_get_queued_message_count(taskflow_handle_t engine);

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 获取错误码描述
 * @param error 错误码
 * @return 错误描述字符串
 */
const char* taskflow_error_to_string(taskflow_error_t error);

/**
 * @brief 获取 TaskFlow 版本信息
 * @return 版本字符串
 */
const char* taskflow_get_version(void);

/**
 * @brief 设置日志回调函数
 * @param callback 日志回调函数
 * @param user_data 用户数据
 */
void taskflow_set_log_callback(void (*callback)(const char* message, void* user_data),
                              void* user_data);

#ifdef __cplusplus
}
#endif

/** @} */ // end of taskflow group

#endif // AGENTOS_TASKFLOW_H