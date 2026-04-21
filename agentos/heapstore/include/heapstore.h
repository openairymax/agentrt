/**
 * @file heapstore.h
 * @brief AgentOS 数据分区核心接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_H
#define AGENTOS_heapstore_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 错误码定义
 */
typedef enum {
    heapstore_SUCCESS = 0,
    heapstore_ERR_INVALID_PARAM = -1,
    heapstore_ERR_NOT_INITIALIZED = -2,
    heapstore_ERR_ALREADY_INITIALIZED = -3,
    heapstore_ERR_DIR_CREATE_FAILED = -4,
    heapstore_ERR_DIR_NOT_FOUND = -5,
    heapstore_ERR_PERMISSION_DENIED = -6,
    heapstore_ERR_OUT_OF_MEMORY = -7,
    heapstore_ERR_DB_INIT_FAILED = -8,
    heapstore_ERR_DB_QUERY_FAILED = -9,
    heapstore_ERR_FILE_OPEN_FAILED = -10,
    heapstore_ERR_CONFIG_INVALID = -11,
    heapstore_ERR_NOT_FOUND = -12,
    heapstore_ERR_FILE_OPERATION_FAILED = -13,
    heapstore_ERR_FILE_NOT_FOUND = -14,
    heapstore_ERR_CIRCUIT_OPEN = -15,
    heapstore_ERR_TIMEOUT = -16,
    heapstore_ERR_INTERNAL = -99
} heapstore_error_t;

/* 共享类型定义 - 必须在子模块包含之前加载 */
#include "heapstore_types.h"

/* 子模块头文件 - 提供各子系统 API 声明 */
#include "heapstore_registry.h"
#include "heapstore_trace.h"
#include "heapstore_ipc.h"
#include "heapstore_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数据分区路径类型
 */
typedef enum {
    heapstore_PATH_KERNEL,         /* 内核数据路径 */
    heapstore_PATH_LOGS,           /* 日志文件路径 */
    heapstore_PATH_REGISTRY,       /* 注册表路径 */
    heapstore_PATH_SERVICES,       /* 服务数据路径 */
    heapstore_PATH_TRACES,         /* 追踪数据路径 */
    heapstore_PATH_KERNEL_IPC,     /* 内核 IPC 数据 */
    heapstore_PATH_KERNEL_MEMORY,  /* 内核内存数据 */
    heapstore_PATH_MAX
} heapstore_path_type_t;

/**
 * @brief 熔断器状态
 */
typedef enum {
    heapstore_CIRCUIT_CLOSED = 0,  /* 正常状态 */
    heapstore_CIRCUIT_OPEN,        /* 熔断器打开 */
    heapstore_CIRCUIT_HALF_OPEN    /* 半开状态 */
} heapstore_circuit_state_t;

/**
 * @brief 配置项结构
 */
typedef struct heapstore_config {
    const char* root_path;            /* 数据分区根路径 */
    size_t max_log_size_mb;           /* 最大日志文件大小(MB) */
    int log_retention_days;           /* 日志保留天数 */
    int trace_retention_days;         /* 追踪数据保留天数 */
    bool enable_auto_cleanup;          /* 启用自动清理 */
    bool enable_log_rotation;          /* 启用日志轮转 */
    bool enable_trace_export;          /* 启用追踪导出 */
    int db_vacuum_interval_days;      /* 数据库 Vacuum 间隔(天) */
    uint32_t circuit_breaker_threshold; /* 熔断器阈值（失败次数） */
    uint32_t circuit_breaker_timeout_sec; /* 熔断器超时（秒） */
} heapstore_config_t;

/**
 * @brief 统计信息结构
 */
typedef struct heapstore_stats {
    uint64_t total_disk_usage_bytes;  /* 总磁盘使用量 */
    uint64_t log_usage_bytes;         /* 日志使用量 */
    uint64_t registry_usage_bytes;     /* 注册表使用量 */
    uint64_t trace_usage_bytes;        /* 追踪数据使用量 */
    uint64_t ipc_usage_bytes;          /* IPC 数据使用量 */
    uint64_t memory_usage_bytes;       /* 内存数据使用量 */
    uint32_t log_file_count;           /* 日志文件数量 */
    uint32_t trace_file_count;         /* 追踪文件数量 */
} heapstore_stats_t;

/**
 * @brief 性能指标结构
 */
typedef struct heapstore_metrics {
    uint64_t total_operations;        /* 总操作次数 */
    uint64_t failed_operations;        /* 失败操作次数 */
    uint64_t fast_path_operations;     /* 快速路径操作次数 */
    uint64_t slow_path_operations;     /* 慢速路径操作次数 */
    uint64_t circuit_breaker_trips;    /* 熔断器触发次数 */
    double avg_operation_time_ns;      /* 平均操作时间（纳秒） */
    uint64_t peak_concurrent_ops;      /* 峰值并发操作数 */
} heapstore_metrics_t;

/**
 * @brief 熔断器状态信息
 */
typedef struct heapstore_circuit_info {
    heapstore_circuit_state_t state;  /* 当前状态 */
    uint32_t failure_count;            /* 连续失败次数 */
    uint64_t last_failure_time;        /* 上次失败时间 */
    uint32_t threshold;                /* 触发阈值 */
    uint32_t timeout_sec;              /* 超时时间 */
} heapstore_circuit_info_t;

/**
 * @brief 初始化数据分区
 *
 * @param manager [in] 配置参数（如果为 NULL，使用默认配置）
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 manager 的生命周期
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @note 必须在使用其他 API 前调用此函数
 *
 * @see heapstore_shutdown()
 * @since v1.0.0
 */
heapstore_error_t heapstore_init(const heapstore_config_t* manager);

/**
 * @brief 关闭数据分区并清理资源
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @note 调用后所有 API 将返回 heapstore_ERR_NOT_INITIALIZED
 *
 * @see heapstore_init()
 * @since v1.0.0
 */
void heapstore_shutdown(void);

/**
 * @brief 检查数据分区是否已初始化
 *
 * @return bool 已初始化返回 true
 *
 * @threadsafe 是
 * @reentrant 是
 * @since v1.0.0
 */
bool heapstore_is_initialized(void);

/**
 * @brief 获取数据分区根路径
 *
 * @return const char* 根路径字符串
 *
 * @ownership 返回内部字符串，调用者不应释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 未初始化时返回空字符串
 * @since v1.0.0
 */
const char* heapstore_get_root(void);

/**
 * @brief 获取指定类型的路径
 *
 * @param type [in] 路径类型
 * @return const char* 路径字符串（不包含根路径前缀）
 *
 * @ownership 返回内部字符串，调用者不应释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 无效类型返回 NULL
 */
const char* heapstore_get_path(heapstore_path_type_t type);

/**
 * @brief 获取完整路径
 *
 * @param type [in] 路径类型
 * @param buffer [out] 输出缓冲区
 * @param buffer_size [in] 缓冲区大小
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 buffer 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 缓冲区不足时返回 heapstore_ERR_BUFFER_TOO_SMALL
 */
heapstore_error_t heapstore_get_full_path(heapstore_path_type_t type, char* buffer, size_t buffer_size);

/**
 * @brief 获取统计信息
 *
 * @param stats [out] 输出统计信息结构
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 stats 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats);

/**
 * @brief 快速路径：异步写入日志（无锁路径）
 *
 * @param service [in] 服务名称
 * @param level [in] 日志级别
 * @param message [in] 日志消息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 message 的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 此为快速路径，适用于高频日志写入场景
 * @note 如果熔断器打开，将返回 heapstore_ERR_CIRCUIT_OPEN
 *
 * @see heapstore_log_write_slow()
 */
heapstore_error_t heapstore_log_write_fast(const char* service, int level, const char* message);

/**
 * @brief 慢速路径：同步写入日志（完整检查路径）
 *
 * @param service [in] 服务名称
 * @param level [in] 日志级别
 * @param message [in] 日志消息
 * @param trace_id [in] 追踪 ID（可为空）
 * @param timeout_ms [in] 超时时间（毫秒）
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 message 的生命周期
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 此为慢速路径，适用于重要日志写入场景
 * @note 包含完整的参数验证和错误处理
 *
 * @see heapstore_log_write_fast()
 * @since v1.0.0
 */
heapstore_error_t heapstore_log_write_slow(const char* service, int level, const char* message, const char* trace_id, uint32_t timeout_ms);

/**
 * @brief 清理过期数据
 *
 * @param dry_run [in] 如果为 true，仅返回将清理的数据量，不实际清理
 * @param freed_bytes [out] 输出实际释放的字节数（可为 NULL）
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 freed_bytes 的分配和释放
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 清理规则基于配置中的 log_retention_days 和 trace_retention_days
 */
heapstore_error_t heapstore_cleanup(bool dry_run, uint64_t* freed_bytes);

/**
 * @brief 获取错误码对应的描述字符串
 *
 * @param err [in] 错误码
 * @return const char* 错误描述
 *
 * @ownership 返回内部字符串，调用者不应释放
 * @threadsafe 是
 * @reentrant 是
 */
const char* heapstore_strerror(heapstore_error_t err);

/**
 * @brief 重新加载配置
 *
 * @param manager [in] 新配置
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 manager 的生命周期
 * @threadsafe 否
 * @reentrant 否
 *
 * @note 仅更新配置，不影响已初始化的资源
 */
heapstore_error_t heapstore_reload_config(const heapstore_config_t* manager);

/**
 * @brief 强制刷新所有待写入的数据
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_flush(void);

/**
 * @brief 健康检查接口，用于检查各子系统状态
 *
 * @param registry_ok [out] 注册表系统是否健康，可为 NULL
 * @param trace_ok [out] 追踪系统是否健康，可为 NULL
 * @param log_ok [out] 日志系统是否健康，可为 NULL
 * @param ipc_ok [out] IPC 系统是否健康，可为 NULL
 * @param memory_ok [out] 内存系统是否健康，可为 NULL
 * @return heapstore_error_t 错误码，heapstore_SUCCESS 表示整体健康
 *
 * @ownership 调用者负责所有输出参数的分配和释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 所有输出参数均为可选，传入 NULL 表示不检查该子系统
 */
heapstore_error_t heapstore_health_check(bool* registry_ok,
                                       bool* trace_ok,
                                       bool* log_ok,
                                       bool* ipc_ok,
                                       bool* memory_ok);

/**
 * @brief 获取性能指标
 *
 * @param metrics [out] 输出性能指标结构
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 metrics 的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_get_metrics(heapstore_metrics_t* metrics);

/**
 * @brief 重置性能指标
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0*/
heapstore_error_t heapstore_reset_metrics(void);

/**
 * @brief 获取熔断器状态
 *
 * @param info [out] 输出熔断器状态信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 info 的分配和释放
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_get_circuit_state(heapstore_circuit_info_t* info);

/**
 * @brief 手动重置熔断器
 *
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 通常在问题修复后手动调用
 */
heapstore_error_t heapstore_reset_circuit(void);

/* ==================== 批量写入支持 ==================== */

/**
 * @brief 批量写入上下文
 */
typedef struct heapstore_batch_context heapstore_batch_context_t;

/**
 * @brief 创建批量写入上下文
 *
 * @param batch_size [in] 批量大小（默认 100）
 * @return heapstore_batch_context_t* 批量写入上下文指针
 *
 * @ownership 调用者负责释放返回的上下文
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_batch_context_t* heapstore_batch_begin(size_t batch_size);

/**
 * @brief 添加日志到批量写入缓冲区
 *
 * @param ctx [in] 批量写入上下文
 * @param service [in] 服务名称
 * @param level [in] 日志级别
 * @param message [in] 日志消息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责所有参数的生命周期
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0*/
heapstore_error_t heapstore_batch_add_log(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* message);

heapstore_error_t heapstore_batch_add_log_with_trace(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* trace_id,
    const char* message);

heapstore_error_t heapstore_batch_add_trace(
    heapstore_batch_context_t* ctx,
    const char* trace_id,
    const char* span_id,
    const char* parent_id,
    const char* name,
    int64_t start_time_us,
    int64_t end_time_us,
    int status,
    const char* attributes);

heapstore_error_t heapstore_batch_add_session(
    heapstore_batch_context_t* ctx,
    const heapstore_session_record_t* record);
heapstore_error_t heapstore_batch_add_agent(
    heapstore_batch_context_t* ctx,
    const heapstore_agent_record_t* record);
heapstore_error_t heapstore_batch_add_skill(
    heapstore_batch_context_t* ctx,
    const heapstore_skill_record_t* record);
heapstore_error_t heapstore_batch_add_memory_pool(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_pool_t* pool);
heapstore_error_t heapstore_batch_add_allocation(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_allocation_t* allocation);
heapstore_error_t heapstore_batch_add_ipc_channel(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_channel_t* channel);
heapstore_error_t heapstore_batch_add_ipc_buffer(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_buffer_t* buffer);
heapstore_error_t heapstore_batch_add_span(
    heapstore_batch_context_t* ctx,
    const heapstore_span_t* span);

heapstore_error_t heapstore_batch_commit(
    heapstore_batch_context_t* ctx);
void heapstore_batch_rollback(
    heapstore_batch_context_t* ctx);
void heapstore_batch_context_destroy(
    heapstore_batch_context_t* ctx);
size_t heapstore_batch_get_count(
    const heapstore_batch_context_t* ctx);
size_t heapstore_batch_get_capacity(
    const heapstore_batch_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_H */
