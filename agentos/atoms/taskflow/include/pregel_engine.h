// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file pregel_engine.h
 * @brief Pregel Engine for Distributed Graph Computation
 * 
 * Pregel 超步引擎，实现 Google Pregel 模型的分布式图计算。
 * 支持迭代计算、消息传递和容错恢复。
 */

#ifndef AGENTOS_PREGEL_ENGINE_H
#define AGENTOS_PREGEL_ENGINE_H

#include "taskflow_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Pregel 引擎句柄
// ============================================================================

/**
 * @brief Pregel 引擎句柄（不透明类型）
 */
typedef struct pregel_engine_s* pregel_engine_handle_t;

// ============================================================================
// 超步回调函数类型
// ============================================================================

/**
 * @brief 顶点计算回调函数
 * @param vertex_id 顶点ID
 * @param vertex_value 顶点值
 * @param vertex_value_size 顶点值大小
 * @param incoming_messages 入站消息数组
 * @param message_count 消息数量
 * @param user_context 用户上下文
 */
typedef void (*pregel_compute_func_t)(
    vertex_id_t vertex_id,
    void* vertex_value,
    size_t vertex_value_size,
    graph_message_t* incoming_messages,
    size_t message_count,
    void* user_context
);

/**
 * @brief 消息发送回调函数
 * @param source 源顶点ID
 * @param target 目标顶点ID
 * @param payload 消息负载
 * @param payload_size 负载大小
 * @param user_context 用户上下文
 */
typedef void (*pregel_send_func_t)(
    vertex_id_t source,
    vertex_id_t target,
    const void* payload,
    size_t payload_size,
    void* user_context
);

/**
 * @brief 超步开始回调函数
 * @param superstep 超步ID
 * @param user_context 用户上下文
 */
typedef void (*pregel_superstep_start_func_t)(
    superstep_t superstep,
    void* user_context
);

/**
 * @brief 超步结束回调函数
 * @param superstep 超步ID
 * @param user_context 用户上下文
 */
typedef void (*pregel_superstep_end_func_t)(
    superstep_t superstep,
    void* user_context
);

// ============================================================================
// Pregel 配置
// ============================================================================

/**
 * @brief Pregel 引擎配置
 */
typedef struct {
    size_t max_workers;                     /**< 最大工作线程数 */
    size_t message_buffer_size;             /**< 消息缓冲区大小 */
    uint32_t superstep_timeout_ms;          /**< 超步超时时间（毫秒） */
    
    // 回调函数
    pregel_compute_func_t compute_func;     /**< 顶点计算函数 */
    pregel_send_func_t send_func;           /**< 消息发送函数 */
    pregel_superstep_start_func_t start_func; /**< 超步开始函数 */
    pregel_superstep_end_func_t end_func;   /**< 超步结束函数 */
    void* user_context;                     /**< 用户上下文 */
    
    // 容错配置
    bool enable_fault_tolerance;            /**< 是否启用容错 */
    size_t checkpoint_interval;             /**< 检查点间隔（超步数） */
    
    // 性能优化
    bool enable_message_combining;          /**< 是否启用消息合并 */
    bool enable_edge_caching;               /**< 是否启用边缓存 */
    size_t batch_size;                      /**< 批处理大小 */
} pregel_config_t;

// ============================================================================
// Pregel 引擎API
// ============================================================================

/**
 * @brief 创建 Pregel 引擎实例
 * @param config 引擎配置
 * @return 引擎句柄，失败返回 NULL
 */
pregel_engine_handle_t pregel_engine_create(const pregel_config_t* config);

/**
 * @brief 销毁 Pregel 引擎实例
 * @param engine 引擎句柄
 */
void pregel_engine_destroy(pregel_engine_handle_t engine);

/**
 * @brief 初始化 Pregel 引擎
 * @param engine 引擎句柄
 * @param graph_engine 图引擎句柄
 * @return 错误码
 */
taskflow_error_t pregel_engine_init(pregel_engine_handle_t engine,
                                   graph_engine_handle_t graph_engine);

/**
 * @brief 启动 Pregel 计算
 * @param engine 引擎句柄
 * @param max_supersteps 最大超步数
 * @return 错误码
 */
taskflow_error_t pregel_engine_start(pregel_engine_handle_t engine,
                                    size_t max_supersteps);

/**
 * @brief 停止 Pregel 计算
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t pregel_engine_stop(pregel_engine_handle_t engine);

/**
 * @brief 暂停 Pregel 计算
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t pregel_engine_pause(pregel_engine_handle_t engine);

/**
 * @brief 恢复 Pregel 计算
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t pregel_engine_resume(pregel_engine_handle_t engine);

/**
 * @brief 获取当前超步
 * @param engine 引擎句柄
 * @return 当前超步ID
 */
superstep_t pregel_engine_get_current_superstep(pregel_engine_handle_t engine);

/**
 * @brief 获取活跃顶点数量
 * @param engine 引擎句柄
 * @return 活跃顶点数量
 */
size_t pregel_engine_get_active_vertices(pregel_engine_handle_t engine);

/**
 * @brief 获取队列中的消息数量
 * @param engine 引擎句柄
 * @return 消息数量
 */
size_t pregel_engine_get_queued_messages(pregel_engine_handle_t engine);

/**
 * @brief 发送消息
 * @param engine 引擎句柄
 * @param source 源顶点ID
 * @param target 目标顶点ID
 * @param payload 消息负载
 * @param payload_size 负载大小
 * @return 错误码
 */
taskflow_error_t pregel_engine_send_message(pregel_engine_handle_t engine,
                                           vertex_id_t source,
                                           vertex_id_t target,
                                           const void* payload,
                                           size_t payload_size);

/**
 * @brief 广播消息
 * @param engine 引擎句柄
 * @param source 源顶点ID
 * @param payload 消息负载
 * @param payload_size 负载大小
 * @return 错误码
 */
taskflow_error_t pregel_engine_broadcast_message(pregel_engine_handle_t engine,
                                                vertex_id_t source,
                                                const void* payload,
                                                size_t payload_size);

/**
 * @brief 获取顶点投票结果
 * @param engine 引擎句柄
 * @param vertex_id 顶点ID
 * @return 是否投票停止（true: 停止, false: 继续）
 */
bool pregel_engine_get_vote_to_halt(pregel_engine_handle_t engine,
                                   vertex_id_t vertex_id);

/**
 * @brief 设置顶点投票结果
 * @param engine 引擎句柄
 * @param vertex_id 顶点ID
 * @param vote_to_halt 是否投票停止
 * @return 错误码
 */
taskflow_error_t pregel_engine_set_vote_to_halt(pregel_engine_handle_t engine,
                                              vertex_id_t vertex_id,
                                              bool vote_to_halt);

/**
 * @brief 创建检查点
 * @param engine 引擎句柄
 * @return 检查点ID，失败返回 0
 */
uint64_t pregel_engine_create_checkpoint(pregel_engine_handle_t engine);

/**
 * @brief 恢复检查点
 * @param engine 引擎句柄
 * @param checkpoint_id 检查点ID
 * @return 错误码
 */
taskflow_error_t pregel_engine_restore_checkpoint(pregel_engine_handle_t engine,
                                                 uint64_t checkpoint_id);

/**
 * @brief 获取执行统计信息
 * @param engine 引擎句柄
 * @param stats 统计信息结构（输出）
 * @return 错误码
 */
taskflow_error_t pregel_engine_get_stats(pregel_engine_handle_t engine,
                                        execution_stats_t* stats);

/**
 * @brief 重置统计信息
 * @param engine 引擎句柄
 * @return 错误码
 */
taskflow_error_t pregel_engine_reset_stats(pregel_engine_handle_t engine);

/**
 * @brief 等待计算完成
 * @param engine 引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
taskflow_error_t pregel_engine_wait_for_completion(pregel_engine_handle_t engine,
                                                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_PREGEL_ENGINE_H