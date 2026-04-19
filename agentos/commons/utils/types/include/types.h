/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file types.h
 * @brief AgentOS 统一类型定义 - 核心基础类型
 * 
 * @details
 * 本文件定义了 AgentOS 系统范围内使用的所有核心数据类型。
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - K-2 接口契约化：所有类型都有明确的语义和所有权规则
 * - E-5 命名语义化：类型名称精确表达其用途
 * 
 * 类型分类：
 * 1. 基础类型：错误码、状态枚举、结果类型
 * 2. 任务类型：任务状态、优先级、结果
 * 3. 记忆类型：记忆层级、存储结构
 * 4. 会话类型：会话状态、上下文
 * 5. Agent类型：Agent契约、能力定义
 * 6. 可观测性类型：指标、追踪、日志
 * 
 * @author Spharx AgentOS Team
 * @date 2026-04-03
 * @version 1.0.0.7
 * 
 * @note 线程安全：本文件定义的类型均为值类型或不可变类型，线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md K-2 接口契约化原则
 * @see syscall_api_contract.md 系统调用 API 契约
 * @see agent_contract.md Agent 契约规范
 */

#ifndef AGENTOS_TYPES_H
#define AGENTOS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../../platform/include/platform.h"
#include "../../include/agentos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 第一部分：基础类型定义
 * ============================================================================ */

/**
 * @defgroup BasicTypes 基础类型
 * @brief 系统范围内使用的基础数据类型
 * @{
 */

/**
 * @brief 错误码类型
 * @details 所有错误码为负值，成功为0。遵循 syscall_api_contract.md 规范。
 */
typedef int32_t agentos_error_t;

/**
 * @brief 成功返回值
 */
#define AGENTOS_SUCCESS         0

/**
 * @brief 通用错误码定义
 */
#define AGENTOS_EINVAL         (-1)    /**< 参数无效 */
#define AGENTOS_ENOMEM         (-2)    /**< 内存不足 */
#define AGENTOS_EBUSY          (-3)    /**< 资源忙碌 */
#define AGENTOS_ENOENT         (-4)    /**< 资源不存在 */
#define AGENTOS_EPERM          (-5)    /**< 权限不足 */
#define AGENTOS_ETIMEDOUT      (-6)    /**< 操作超时 */
#define AGENTOS_EIO            (-7)    /**< I/O 错误 */
#define AGENTOS_EEXIST         (-8)    /**< 资源已存在 */
#define AGENTOS_ENOTINIT       (-9)    /**< 引擎未初始化 */
#define AGENTOS_ECANCELLED     (-10)   /**< 操作已取消 */
#define AGENTOS_ENOTSUP        (-11)   /**< 操作不支持 */
#define AGENTOS_EOVERFLOW      (-12)   /**< 溢出错误 */
#define AGENTOS_EPROTO         (-13)   /**< 协议错误 */
#define AGENTOS_ENOTCONN       (-14)   /**< 未连接 */
#define AGENTOS_ECONNRESET     (-15)   /**< 连接重置 */
#define AGENTOS_EUNKNOWN       (-99)   /**< 未知错误 */

/**
 * @brief 时间戳类型（纳秒）
 * @details 使用 Unix 时间戳，纳秒精度
 */
typedef uint64_t agentos_timestamp_t;

/**
 * @brief 毫秒时间类型
 */
typedef uint64_t agentos_millis_t;

/**
 * @brief 唯一标识符类型
 * @details 用于 task_id, session_id, agent_id 等标识符
 */
typedef char agentos_uuid_t[37];

/**
 * @brief 优先级枚举
 */
typedef enum {
    AGENTOS_PRIORITY_LOW       = 0,    /**< 低优先级 */
    AGENTOS_PRIORITY_NORMAL    = 1,    /**< 普通优先级 */
    AGENTOS_PRIORITY_HIGH      = 2,    /**< 高优先级 */
    AGENTOS_PRIORITY_CRITICAL  = 3     /**< 关键优先级 */
} agentos_priority_t;

/**
 * @brief 通用结果类型
 * @details 用于返回操作结果和错误信息
 */
typedef struct {
    agentos_error_t code;           /**< 错误码 */
    const char* message;            /**< 错误消息 */
    const char* detail;             /**< 详细信息 */
} agentos_result_t;

/** @} */ /* end of BasicTypes */

/* ============================================================================
 * 第二部分：任务类型定义
 * ============================================================================ */

/**
 * @defgroup TaskTypes 任务类型
 * @brief 任务管理相关的数据类型
 * @{
 */

/**
 * @brief 任务状态枚举
 * @details 定义任务的生命周期状态
 */
#ifndef AGENTOS_TASK_STATUS_T_DEFINED
#define AGENTOS_TASK_STATUS_T_DEFINED
typedef enum {
    AGENTOS_TASK_PENDING     = 0,
    AGENTOS_TASK_RUNNING     = 1,
    AGENTOS_TASK_SUCCEEDED   = 2,
    AGENTOS_TASK_FAILED      = 3,
    AGENTOS_TASK_CANCELLED   = 4,
    AGENTOS_TASK_TIMEOUT     = 5,
    AGENTOS_TASK_RETRYING    = 6
} agentos_task_status_t;
#endif

/**
 * @brief 任务类型枚举
 */
typedef enum {
    AGENTOS_TASKTYPE_ONESHOT    = 0,    /**< 单次任务 */
    AGENTOS_TASKTYPE_RECURRING  = 1,    /**< 周期任务 */
    AGENTOS_TASKTYPE_CONDITIONAL = 2    /**< 条件触发任务 */
} agentos_task_type_t;

/**
 * @brief 任务句柄类型
 * @details 用于引用任务实例
 */
#ifndef AGENTOS_TASK_T_DEFINED
#define AGENTOS_TASK_T_DEFINED
typedef struct agentos_task agentos_task_t;
#endif

/**
 * @brief 任务配置结构
 */
typedef struct {
    const char* input;              /**< 任务输入（自然语言描述） */
    size_t input_len;               /**< 输入长度 */
    uint32_t timeout_ms;            /**< 超时时间（毫秒） */
    agentos_priority_t priority;    /**< 任务优先级 */
    agentos_task_type_t type;       /**< 任务类型 */
    const char* agent_id;           /**< 指定执行的 Agent ID（可选） */
    const char* session_id;         /**< 关联的会话 ID（可选） */
    const char* parent_task_id;     /**< 父任务 ID（用于子任务） */
} agentos_task_config_t;

/**
 * @brief 任务结果结构
 */
typedef struct {
    char* task_id;                  /**< 任务 ID */
    agentos_task_status_t status;   /**< 任务状态 */
    char* output;                   /**< 输出结果（JSON 字符串） */
    size_t output_len;              /**< 输出长度 */
    agentos_timestamp_t start_time; /**< 开始时间 */
    agentos_timestamp_t end_time;   /**< 结束时间 */
    uint32_t tokens_used;           /**< 消耗的 Token 数 */
    double cost_usd;                /**< 成本（美元） */
    agentos_error_t error_code;     /**< 错误码 */
    char* error_message;            /**< 错误消息 */
} agentos_task_result_t;

/** @} */ /* end of TaskTypes */

/* ============================================================================
 * 第三部分：记忆类型定义
 * ============================================================================ */

/**
 * @defgroup MemoryTypes 记忆类型
 * @brief 记忆管理相关的数据类型
 * @{
 */

/**
 * @brief 记忆层级枚举
 * @details 四层记忆卷载结构
 */
typedef enum {
    AGENTOS_MEM_LAYER1_RAW       = 0,    /**< Layer1: 原始记忆 */
    AGENTOS_MEM_LAYER2_WORKING   = 1,    /**< Layer2: 工作记忆 */
    AGENTOS_MEM_LAYER3_EPISODIC  = 2,    /**< Layer3: 情景记忆 */
    AGENTOS_MEM_LAYER4_SEMANTIC  = 3     /**< Layer4: 语义记忆 */
} agentos_memory_layer_t;

/**
 * @brief 记忆类型枚举
 */
#ifndef AGENTOS_MEMORY_TYPE_T_DEFINED
#define AGENTOS_MEMORY_TYPE_T_DEFINED
typedef enum {
    AGENTOS_MEMTYPE_TEXT      = 0,
    AGENTOS_MEMTYPE_EMBEDDING = 1,
    AGENTOS_MEMTYPE_STRUCTURED = 2,
    AGENTOS_MEMTYPE_BINARY    = 3
} agentos_memory_type_t;
#endif

#ifndef AGENTOS_MEMORY_T_DEFINED
#define AGENTOS_MEMORY_T_DEFINED
/**
 * @brief 记忆句柄类型（不透明指针，用于跨模块传递）
 */
typedef struct agentos_memory* agentos_memory_t;
#endif /* AGENTOS_MEMORY_T_DEFINED */

/**
 * @brief 记忆条目结构
 */
typedef struct {
    char* memory_id;                /**< 记忆 ID */
    agentos_memory_layer_t layer;   /**< 记忆层级 */
    agentos_memory_type_t type;     /**< 记忆类型 */
    char* content;                  /**< 记忆内容 */
    size_t content_len;             /**< 内容长度 */
    float* embedding;               /**< 向量嵌入（可选） */
    size_t embedding_dim;           /**< 嵌入维度 */
    float importance;               /**< 重要性分数 (0-1) */
    float decay_rate;               /**< 衰减率 */
    uint32_t access_count;          /**< 访问次数 */
    agentos_timestamp_t created_at; /**< 创建时间 */
    agentos_timestamp_t last_access;/**< 最后访问时间 */
    char* session_id;               /**< 关联会话 ID */
    char* task_id;                  /**< 关联任务 ID */
    char** tags;                    /**< 标签列表 */
    size_t tag_count;               /**< 标签数量 */
} agentos_memory_entry_t;

/**
 * @brief 记忆搜索配置
 */
typedef struct {
    const char* query;              /**< 搜索查询 */
    size_t query_len;               /**< 查询长度 */
    agentos_memory_layer_t layer;   /**< 搜索层级（可选） */
    uint32_t top_k;                 /**< 返回数量 */
    float threshold;                /**< 相似度阈值 */
    const char** tags;              /**< 过滤标签 */
    size_t tag_count;               /**< 标签数量 */
} agentos_memory_search_t;

/**
 * @brief 记忆搜索结果
 */
#ifndef AGENTOS_MEMORY_RESULT_T_DEFINED
#define AGENTOS_MEMORY_RESULT_T_DEFINED
typedef struct {
    agentos_memory_entry_t* entries;
    size_t count;
    float* scores;
} agentos_memory_result_t;
#endif

/** @} */ /* end of MemoryTypes */

/* ============================================================================
 * 第四部分：会话类型定义
 * ============================================================================ */

/**
 * @defgroup SessionTypes 会话类型
 * @brief 会话管理相关的数据类型
 * @{
 */

/**
 * @brief 会话状态枚举
 */
typedef enum {
    AGENTOS_SESSION_ACTIVE     = 0,    /**< 活跃状态 */
    AGENTOS_SESSION_IDLE       = 1,    /**< 空闲状态 */
    AGENTOS_SESSION_CLOSED     = 2,    /**< 已关闭 */
    AGENTOS_SESSION_EXPIRED    = 3     /**< 已过期 */
} agentos_session_status_t;

/**
 * @brief 会话句柄类型
 */
typedef struct agentos_session* agentos_session_t;

/**
 * @brief 会话配置结构
 */
typedef struct {
    const char* user_id;            /**< 用户 ID */
    const char* project_id;         /**< 项目 ID（可选） */
    const char* context;            /**< 初始上下文（可选） */
    uint32_t ttl_seconds;           /**< 会话 TTL（秒） */
    agentos_priority_t priority;    /**< 默认优先级 */
} agentos_session_config_t;

/**
 * @brief 会话信息结构
 */
typedef struct {
    char* session_id;               /**< 会话 ID */
    agentos_session_status_t status;/**< 会话状态 */
    char* user_id;                  /**< 用户 ID */
    char* project_id;               /**< 项目 ID */
    agentos_timestamp_t created_at; /**< 创建时间 */
    agentos_timestamp_t last_active;/**< 最后活跃时间 */
    uint32_t ttl_seconds;           /**< TTL */
    uint32_t task_count;            /**< 任务数量 */
    uint32_t memory_count;          /**< 记忆数量 */
    uint64_t tokens_used;           /**< 总 Token 消耗 */
    double cost_usd;                /**< 总成本 */
} agentos_session_info_t;

/**
 * @brief 执行上下文结构
 * @details 用于传递请求链路的上下文信息
 */
typedef struct {
    char* agent_id;                 /**< Agent ID */
    char* session_id;               /**< 会话 ID */
    char* trace_id;                 /**< 追踪 ID */
    char* parent_span_id;           /**< 父 Span ID */
    agentos_timestamp_t timestamp;  /**< 时间戳 */
    agentos_priority_t priority;    /**< 优先级 */
    char* user_id;                  /**< 用户 ID */
    char* project_id;               /**< 项目 ID */
    void* user_data;                /**< 用户自定义数据 */
} agentos_context_t;

/** @} */ /* end of SessionTypes */

/* ============================================================================
 * 第五部分：Agent 类型定义
 * ============================================================================ */

/**
 * @defgroup AgentTypes Agent 类型
 * @brief Agent 契约和能力相关的数据类型
 * @{
 */

/**
 * @brief Agent 维护级别枚举
 */
typedef enum {
    AGENTOS_AGENT_COMMUNITY  = 0,    /**< 社区维护 */
    AGENTOS_AGENT_VERIFIED   = 1,    /**< 已验证 */
    AGENTOS_AGENT_OFFICIAL   = 2     /**< 官方维护 */
} agentos_agent_level_t;

/**
 * @brief Agent 能力结构
 */
typedef struct {
    char* name;                     /**< 能力名称 */
    char* description;              /**< 能力描述 */
    char* input_schema;             /**< 输入 Schema（JSON） */
    char* output_schema;            /**< 输出 Schema（JSON） */
    uint32_t estimated_tokens;      /**< 预估 Token 数 */
    uint32_t avg_duration_ms;       /**< 平均执行时间 */
    float success_rate;             /**< 成功率 */
} agentos_capability_t;

/**
 * @brief Agent 模型配置
 */
typedef struct {
    char* system1;                  /**< System 1 模型（快速） */
    char* system2;                  /**< System 2 模型（深度） */
} agentos_models_t;

/**
 * @brief Agent 成本概览
 */
typedef struct {
    uint32_t token_per_task_avg;    /**< 单任务平均 Token */
    double api_cost_per_task;       /**< 单任务 API 成本 */
    agentos_agent_level_t level;    /**< 维护级别 */
} agentos_cost_profile_t;

/**
 * @brief Agent 信任指标
 */
typedef struct {
    uint32_t install_count;         /**< 安装次数 */
    float rating;                   /**< 用户评分 (1-5) */
    bool verified_provider;         /**< 是否认证提供商 */
    char* last_audit;               /**< 上次审计日期 */
} agentos_trust_metrics_t;

/**
 * @brief Agent 契约结构
 * @details 完整的 Agent 元数据定义
 */
typedef struct {
    char* schema_version;           /**< 契约版本 */
    char* agent_id;                 /**< Agent ID */
    char* agent_name;               /**< Agent 名称 */
    char* version;                  /**< Agent 版本 */
    char* role;                     /**< 角色分类 */
    char* description;              /**< 描述 */
    agentos_capability_t* capabilities; /**< 能力列表 */
    size_t capability_count;        /**< 能力数量 */
    agentos_models_t models;        /**< 模型配置 */
    char** required_permissions;    /**< 所需权限 */
    size_t permission_count;        /**< 权限数量 */
    agentos_cost_profile_t cost;    /**< 成本概览 */
    agentos_trust_metrics_t trust;  /**< 信任指标 */
    char* extensions;               /**< 扩展字段（JSON） */
} agentos_agent_contract_t;

/** @} */ /* end of AgentTypes */

/* ============================================================================
 * 第六部分：可观测性类型定义
 * ============================================================================ */

/**
 * @defgroup ObservabilityTypes 可观测性类型
 * @brief 指标、追踪、日志相关的数据类型
 * @{
 */

/**
 * @brief 日志级别枚举
 */
#ifndef AGENTOS_LOG_LEVEL_T_DEFINED
#define AGENTOS_LOG_LEVEL_T_DEFINED
typedef enum {
    AGENTOS_LOG_LEVEL_DEBUG   = 0,
    AGENTOS_LOG_LEVEL_INFO    = 1,
    AGENTOS_LOG_LEVEL_WARN    = 2,
    AGENTOS_LOG_LEVEL_ERROR   = 3,
    AGENTOS_LOG_LEVEL_FATAL   = 4
} agentos_log_level_t;
#endif

/**
 * @brief 指标类型枚举
 */
typedef enum {
    AGENTOS_METRIC_COUNTER   = 0,    /**< 计数器 */
    AGENTOS_METRIC_GAUGE     = 1,    /**< 仪表 */
    AGENTOS_METRIC_HISTOGRAM = 2,    /**< 直方图 */
    AGENTOS_METRIC_SUMMARY   = 3     /**< 摘要 */
} agentos_metric_type_t;

/**
 * @brief Span 类型枚举
 */
typedef enum {
    AGENTOS_SPAN_INTERNAL = 0,   /**< 内部操作 */
    AGENTOS_SPAN_CLIENT   = 1,   /**< 客户端调用 */
    AGENTOS_SPAN_SERVER   = 2,   /**< 服务端处理 */
    AGENTOS_SPAN_PRODUCER = 3,   /**< 消息生产者 */
    AGENTOS_SPAN_CONSUMER = 4    /**< 消息消费者 */
} agentos_span_kind_t;

/**
 * @brief Span 状态枚举
 */
typedef enum {
    AGENTOS_SPAN_UNSET  = 0,     /**< 未设置 */
    AGENTOS_SPAN_OK     = 1,     /**< 成功 */
    AGENTOS_SPAN_ERROR  = 2      /**< 错误 */
} agentos_span_status_t;

/**
 * @brief 指标数据结构
 */
typedef struct {
    char* name;                     /**< 指标名称 */
    agentos_metric_type_t type;     /**< 指标类型 */
    char* description;              /**< 描述 */
    char* unit;                     /**< 单位 */
    double value;                   /**< 当前值 */
    char** labels;                  /**< 标签键值对 */
    size_t label_count;             /**< 标签数量 */
    agentos_timestamp_t timestamp;  /**< 时间戳 */
} agentos_metric_t;

/**
 * @brief Span 数据结构
 */
typedef struct {
    char* trace_id;                 /**< 追踪 ID */
    char* span_id;                  /**< Span ID */
    char* parent_span_id;           /**< 父 Span ID */
    char* name;                     /**< Span 名称 */
    agentos_span_kind_t kind;       /**< Span 类型 */
    agentos_timestamp_t start_time; /**< 开始时间 */
    agentos_timestamp_t end_time;   /**< 结束时间 */
    agentos_span_status_t status;   /**< Span 状态 */
    char* status_message;           /**< 状态消息 */
    char** attributes;              /**< 属性键值对 */
    size_t attribute_count;         /**< 属性数量 */
    char* events;                   /**< 事件列表（JSON） */
} agentos_span_t;

/**
 * @brief 遥测数据结构
 */
typedef struct {
    agentos_metric_t* metrics;      /**< 指标数组 */
    size_t metric_count;            /**< 指标数量 */
    agentos_span_t* spans;          /**< Span 数组 */
    size_t span_count;              /**< Span 数量 */
    char* logs;                     /**< 日志数据（JSON） */
} agentos_telemetry_t;

/** @} */ /* end of ObservabilityTypes */

/* ============================================================================
 * 第七部分：IPC 类型定义
 * ============================================================================ */

/**
 * @defgroup IPCTypes IPC 类型
 * @brief 进程间通信相关的数据类型
 * @{
 */

/**
 * @brief IPC 通道类型枚举
 */
typedef enum {
    AGENTOS_IPC_PIPE     = 0,   /**< 管道 */
    AGENTOS_IPC_SOCKET   = 1,   /**< Unix Socket / Named Pipe */
    AGENTOS_IPC_SHM      = 2,   /**< 共享内存 */
    AGENTOS_IPC_MQ       = 3,   /**< 消息队列 */
    AGENTOS_IPC_RPC      = 4    /**< RPC 调用 */
} agentos_ipc_type_t;

/**
 * @brief IPC 消息标志
 */
typedef enum {
    AGENTOS_IPC_FLAG_NONE     = 0,     /**< 无标志 */
    AGENTOS_IPC_FLAG_NONBLOCK = 1,     /**< 非阻塞 */
    AGENTOS_IPC_FLAG_PRIORITY = 2,     /**< 优先级消息 */
    AGENTOS_IPC_FLAG_BROADCAST = 4     /**< 广播消息 */
} agentos_ipc_flag_t;

/* agentos_ipc_header_t 现在由 agentos_types.h 提供 */

/* agentos_ipc_message_t 现在由 agentos_types.h 提供 */

/**
 * @brief IPC 通道句柄类型
 * @note 内核级IPC通道类型，完整定义见corekern/include/ipc.h
 *       应用层应使用commons/utils/ipc/include/ipc_common.h中的ipc_channel_t
 */

/**
 * @brief IPC 通道配置
 */
typedef struct {
    agentos_ipc_type_t type;        /**< 通道类型 */
    const char* name;               /**< 通道名称 */
    uint32_t buffer_size;           /**< 缓冲区大小 */
    uint32_t max_message_size;      /**< 最大消息大小 */
    uint32_t timeout_ms;            /**< 默认超时 */
    bool nonblocking;               /**< 是否非阻塞 */
} agentos_ipc_config_t;

/** @} */ /* end of IPCTypes */

/* ============================================================================
 * 第八部分：网络类型定义
 * ============================================================================ */

/**
 * @defgroup NetworkTypes 网络类型
 * @brief 网络通信相关的数据类型
 * @{
 */

/**
 * @brief 协议类型枚举
 */
typedef enum {
    AGENTOS_PROTO_TCP   = 0,    /**< TCP 协议 */
    AGENTOS_PROTO_UDP   = 1,    /**< UDP 协议 */
    AGENTOS_PROTO_HTTP  = 2,    /**< HTTP 协议 */
    AGENTOS_PROTO_HTTPS = 3,    /**< HTTPS 协议 */
    AGENTOS_PROTO_WS    = 4,    /**< WebSocket 协议 */
    AGENTOS_PROTO_WSS   = 5     /**< WebSocket Secure 协议 */
} agentos_protocol_t;

/**
 * @brief 连接状态枚举
 */
typedef enum {
    AGENTOS_CONN_DISCONNECTED = 0,  /**< 已断开 */
    AGENTOS_CONN_CONNECTING   = 1,  /**< 连接中 */
    AGENTOS_CONN_CONNECTED    = 2,  /**< 已连接 */
    AGENTOS_CONN_CLOSING      = 3,  /**< 关闭中 */
    AGENTOS_CONN_ERROR        = 4   /**< 错误状态 */
} agentos_conn_state_t;

/**
 * @brief Socket 句柄类型
 * 定义在 platform.h 中
 */
// typedef struct agentos_socket* agentos_socket_t;

/**
 * @brief 连接端点结构
 */
typedef struct {
    char* host;                     /**< 主机名或 IP */
    uint16_t port;                  /**< 端口号 */
    agentos_protocol_t protocol;    /**< 协议类型 */
    char* path;                     /**< 路径（用于 HTTP/WebSocket） */
} agentos_endpoint_t;

/**
 * @brief 连接配置结构
 */
typedef struct {
    agentos_endpoint_t remote;      /**< 远程端点 */
    uint32_t timeout_ms;            /**< 连接超时 */
    uint32_t read_timeout_ms;       /**< 读取超时 */
    uint32_t write_timeout_ms;      /**< 写入超时 */
    uint32_t max_retries;           /**< 最大重试次数 */
    uint32_t retry_delay_ms;        /**< 重试延迟 */
    bool keepalive;                 /**< 是否保持连接 */
    bool verify_ssl;                /**< 是否验证 SSL */
    char* ssl_cert_path;            /**< SSL 证书路径 */
    char* ssl_key_path;             /**< SSL 密钥路径 */
} agentos_conn_config_t;

/**
 * @brief HTTP 请求结构
 */
typedef struct {
    const char* method;             /**< HTTP 方法 */
    const char* path;               /**< 请求路径 */
    const char** headers;           /**< 请求头 */
    size_t header_count;            /**< 请求头数量 */
    const void* body;               /**< 请求体 */
    size_t body_len;                /**< 请求体长度 */
    uint32_t timeout_ms;            /**< 超时时间 */
} agentos_http_request_t;

/**
 * @brief HTTP 响应结构
 */
typedef struct {
    int status_code;                /**< 状态码 */
    char** headers;                 /**< 响应头 */
    size_t header_count;            /**< 响应头数量 */
    void* body;                     /**< 响应体 */
    size_t body_len;                /**< 响应体长度 */
    agentos_error_t error;          /**< 错误码 */
} agentos_http_response_t;

/** @} */ /* end of NetworkTypes */

/* ============================================================================
 * 第九部分：辅助宏定义
 * ============================================================================ */

/**
 * @defgroup HelperMacros 辅助宏
 * @brief 常用的辅助宏定义
 * @{
 */

/**
 * @brief 数组元素数量计算
 */
#define AGENTOS_ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 最小值宏
 */
#define AGENTOS_MIN(a, b)           ((a) < (b) ? (a) : (b))

/**
 * @brief 最大值宏
 */
#define AGENTOS_MAX(a, b)           ((a) > (b) ? (a) : (b))

/**
 * @brief 对齐宏
 */
#define AGENTOS_ALIGN_UP(x, align)  (((x) + (align) - 1) & ~((align) - 1))

/**
 * @brief 字符串化宏
 */
#define AGENTOS_STRINGIFY(x)        #x
#define AGENTOS_TOSTRING(x)         AGENTOS_STRINGIFY(x)

/**
 * @brief 连接宏
 */
#define AGENTOS_CONCAT(a, b)        a##b
#define AGENTOS_CONCAT3(a, b, c)    a##b##c

/**
 * @brief 版本号解析宏
 */
#ifndef AGENTOS_VERSION_MAJOR
#define AGENTOS_VERSION_MAJOR(v)    (((v) >> 24) & 0xFF)
#endif
#ifndef AGENTOS_VERSION_MINOR
#define AGENTOS_VERSION_MINOR(v)    (((v) >> 16) & 0xFF)
#endif
#ifndef AGENTOS_VERSION_PATCH
#define AGENTOS_VERSION_PATCH(v)    (((v) >> 8) & 0xFF)
#endif
#ifndef AGENTOS_MAKE_VERSION
#define AGENTOS_MAKE_VERSION(maj, min, pat) \
    (((maj) << 24) | ((min) << 16) | ((pat) << 8))
#endif

/**
 * @brief 时间转换宏
 */
#define AGENTOS_MS_TO_NS(ms)        ((uint64_t)(ms) * 1000000ULL)
#define AGENTOS_SEC_TO_MS(s)        ((uint64_t)(s) * 1000ULL)
#define AGENTOS_SEC_TO_NS(s)        ((uint64_t)(s) * 1000000000ULL)

/** @} */ /* end of HelperMacros */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TYPES_H */
