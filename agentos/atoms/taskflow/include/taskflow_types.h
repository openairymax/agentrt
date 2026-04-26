// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file taskflow_types.h
 * @brief TaskFlow Graph Execution System Type Definitions
 * 
 * 定义 TaskFlow 图执行系统的核心数据类型和结构。
 * 基于 Pregel 超步模型的分布式计算框架基础类型。
 */

#ifndef AGENTOS_TASKFLOW_TYPES_H
#define AGENTOS_TASKFLOW_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 基础类型定义
// ============================================================================

/**
 * @brief 顶点ID类型
 */
typedef uint64_t vertex_id_t;

/**
 * @brief 边ID类型
 */
typedef uint64_t edge_id_t;

/**
 * @brief 分区ID类型
 */
typedef uint32_t partition_id_t;

/**
 * @brief 超步ID类型
 */
typedef uint32_t superstep_t;

/**
 * @brief 任务ID类型
 */
typedef uint64_t task_id_t;

/**
 * @brief 消息ID类型
 */
typedef uint64_t message_id_t;

/**
 * @brief 错误码类型
 */
typedef int32_t taskflow_error_t;

// ============================================================================
// 核心数据结构
// ============================================================================

/**
 * @brief 顶点状态枚举
 */
typedef enum {
    VERTEX_ACTIVE = 0,      /**< 活跃状态，参与计算 */
    VERTEX_INACTIVE,        /**< 非活跃状态，跳过计算 */
    VERTEX_HALTED,          /**< 停止状态，计算完成 */
    VERTEX_FAULTED,         /**< 故障状态，需要恢复 */
    VERTEX_SUSPENDED        /**< 挂起状态，等待条件 */
} vertex_state_t;

/**
 * @brief 消息传递方向枚举
 */
typedef enum {
    MESSAGE_OUTGOING = 0,   /**< 出站消息 */
    MESSAGE_INCOMING        /**< 入站消息 */
} message_direction_t;

/**
 * @brief 分区策略枚举
 */
typedef enum {
    PARTITION_HASH = 0,     /**< 哈希分区 */
    PARTITION_RANGE,        /**< 范围分区 */
    PARTITION_CUSTOM        /**< 自定义分区策略 */
} partition_strategy_t;

/**
 * @brief 图顶点结构
 */
typedef struct {
    vertex_id_t id;                 /**< 顶点唯一ID */
    vertex_state_t state;           /**< 顶点当前状态 */
    void* value;                    /**< 顶点值（用户定义） */
    size_t value_size;              /**< 顶点值大小 */
    uint32_t out_degree;            /**< 出度 */
    uint32_t in_degree;             /**< 入度 */
    void* user_data;                /**< 用户私有数据 */
} graph_vertex_t;

/**
 * @brief 图边结构
 */
typedef struct {
    edge_id_t id;                   /**< 边唯一ID */
    vertex_id_t source;             /**< 源顶点ID */
    vertex_id_t target;             /**< 目标顶点ID */
    void* weight;                   /**< 边权重（用户定义） */
    size_t weight_size;             /**< 边权重大小 */
    void* user_data;                /**< 用户私有数据 */
} graph_edge_t;

/**
 * @brief 图消息结构
 */
typedef struct graph_message {
    message_id_t id;                /**< 消息唯一ID */
    vertex_id_t sender;             /**< 发送者顶点ID */
    vertex_id_t receiver;           /**< 接收者顶点ID */
    void* payload;                  /**< 消息负载数据 */
    size_t payload_size;            /**< 消息负载大小 */
    superstep_t step;               /**< 发送超步 */
    message_direction_t direction;  /**< 消息方向 */
    struct graph_message* next;     /**< 链表指针 */
} graph_message_t;

/**
 * @brief 图分区结构
 */
typedef struct {
    partition_id_t id;              /**< 分区ID */
    vertex_id_t start_vertex;       /**< 起始顶点ID */
    vertex_id_t end_vertex;         /**< 结束顶点ID */
    size_t vertex_count;            /**< 顶点数量 */
    size_t edge_count;              /**< 边数量 */
    partition_strategy_t strategy;  /**< 分区策略 */
    void* metadata;                 /**< 分区元数据 */
} graph_partition_t;

/**
 * @brief 检查点结构
 */
typedef struct {
    uint64_t checkpoint_id;         /**< 检查点ID */
    superstep_t superstep;          /**< 对应的超步 */
    uint64_t timestamp;             /**< 时间戳 */
    size_t data_size;               /**< 数据大小 */
    void* snapshot_data;            /**< 快照数据 */
    bool is_consistent;             /**< 是否一致 */
} checkpoint_t;

/**
 * @brief 顶点计算函数类型
 */
typedef void (*vertex_compute_func_t)(
    vertex_id_t vertex_id,
    void* vertex_value,
    size_t vertex_value_size,
    graph_message_t* incoming_messages,
    size_t message_count,
    void* user_context
);

/**
 * @brief 消息发送函数类型
 */
typedef void (*message_send_func_t)(
    vertex_id_t source,
    vertex_id_t target,
    const void* payload,
    size_t payload_size,
    void* user_context
);

/**
 * @brief 聚合器函数类型
 */
typedef void (*aggregator_func_t)(
    const void* values,
    size_t count,
    void* result,
    size_t result_size,
    void* user_context
);

// ============================================================================
// 配置结构
// ============================================================================

/**
 * @brief TaskFlow 引擎配置
 */
typedef struct {
    size_t max_vertices;            /**< 最大顶点数 */
    size_t max_edges;               /**< 最大边数 */
    size_t max_messages;            /**< 最大消息数 */
    size_t worker_threads;          /**< 工作线程数 */
    size_t partition_count;         /**< 分区数量 */
    partition_strategy_t partition_strategy; /**< 分区策略 */
    
    // 超步控制
    size_t max_supersteps;          /**< 最大超步数 */
    uint32_t superstep_timeout_ms;  /**< 超步超时时间（毫秒） */
    
    // 检查点配置
    size_t checkpoint_interval;     /**< 检查点间隔（超步数） */
    bool enable_incremental_checkpoint; /**< 是否启用增量检查点 */
    
    // 内存管理
    size_t message_buffer_size;     /**< 消息缓冲区大小 */
    size_t vertex_buffer_size;      /**< 顶点缓冲区大小 */
    size_t edge_buffer_size;        /**< 边缓冲区大小 */
    
    // 容错配置
    bool enable_fault_tolerance;    /**< 是否启用容错 */
    size_t max_failures;            /**< 最大故障数 */
    
    // 性能调优
    bool enable_message_combining;  /**< 是否启用消息合并 */
    bool enable_edge_caching;       /**< 是否启用边缓存 */
    size_t batch_size;              /**< 批处理大小 */
    
    // 用户回调
    vertex_compute_func_t compute_func; /**< 顶点计算函数 */
    message_send_func_t send_func;      /**< 消息发送函数 */
    aggregator_func_t aggregator_func;  /**< 聚合器函数 */
    void* user_context;                 /**< 用户上下文 */
} taskflow_config_t;

/**
 * @brief 执行统计信息
 */
typedef struct {
    size_t total_vertices;          /**< 总顶点数 */
    size_t total_edges;             /**< 总边数 */
    size_t total_messages;          /**< 总消息数 */
    size_t active_supersteps;       /**< 活跃超步数 */
    size_t completed_supersteps;    /**< 已完成超步数 */
    size_t checkpoints_taken;       /**< 已创建的检查点数 */
    size_t failures_recovered;      /**< 已恢复的故障数 */
    uint64_t total_compute_time_ms; /**< 总计算时间（毫秒） */
    uint64_t total_communication_time_ms; /**< 总通信时间（毫秒） */
    size_t peak_memory_usage;       /**< 峰值内存使用量 */
} execution_stats_t;

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_TASKFLOW_TYPES_H