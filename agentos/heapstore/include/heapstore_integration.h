/**
 * @file heapstore_integration.h
 * @brief heapstore 与 AgentOS 核心模块集成接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @details
 * 本文件定义 heapstore 数据分区存储系统与 AgentOS 核心模块的集成接口。
 * 遵循架构原则：
 * - S-2 层次分解原则：heapstore 作为底层存储引擎
 * - K-2 接口契约化原则：所有接口有完整契约定义
 * - E-2 可观测性原则：集成可观测性数据采集
 *
 * 集成架构：
 * ```
 * syscall/ ──────────▶ heapstore（注册表、追踪数据存储）
 * memoryrovol/ ──────▶ heapstore（记忆数据持久化）
 * corekern/ipc/ ─────▶ heapstore（IPC 数据存储）
 * agentos/commons/logging/ ──▶ heapstore（日志存储）
 * ```
 */

#ifndef AGENTOS_heapstore_INTEGRATION_H
#define AGENTOS_heapstore_INTEGRATION_H

#include "heapstore.h"
#include "heapstore_log.h"
#include "heapstore_registry.h"
#include "heapstore_trace.h"
#include "heapstore_ipc.h"
#include "heapstore_memory.h"
#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 系统初始化集成 ==================== */

/**
 * @brief 初始化 heapstore 并与 AgentOS 核心集成
 *
 * @param root_path [in] 数据分区根路径，为 NULL 使用默认路径
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有资源
 * @threadsafe 否，必须在程序启动时调用
 * @reentrant 否
 *
 * @note 此函数应在 agentos_core_init() 之后调用
 * @note 自动初始化所有子系统（日志、注册表、追踪、IPC、内存）
 *
 * @see heapstore_integration_shutdown()
 */
AGENTOS_API agentos_error_t heapstore_integration_init(const char* root_path);

/**
 * @brief 关闭 heapstore 集成并清理资源
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否，必须在程序退出前调用
 * @reentrant 否
 *
 * @note 此函数应在 agentos_core_shutdown() 之前调用
 *
 * @see heapstore_integration_init()
 */
AGENTOS_API void heapstore_integration_shutdown(void);

/* ==================== syscall 层集成 ==================== */

/**
 * @brief 为 syscall 层提供会话持久化接口
 *
 * @param session_id [in] 会话 ID
 * @param metadata [in] 会话元数据（JSON 格式）
 * @param created_ns [in] 创建时间（纳秒）
 * @param last_active_ns [in] 最后活动时间（纳秒）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责所有参数的生命周期
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_session_save(
    const char* session_id,
    const char* metadata,
    uint64_t created_ns,
    uint64_t last_active_ns);

/**
 * @brief 为 syscall 层提供会话加载接口
 *
 * @param session_id [in] 会话 ID
 * @param out_metadata [out] 输出元数据（需调用者释放）
 * @param out_created_ns [out] 输出创建时间
 * @param out_last_active_ns [out] 输出最后活动时间
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_metadata
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_session_load(
    const char* session_id,
    char** out_metadata,
    uint64_t* out_created_ns,
    uint64_t* out_last_active_ns);

/**
 * @brief 为 syscall 层提供会话删除接口
 *
 * @param session_id [in] 会话 ID
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_session_delete(const char* session_id);

/**
 * @brief 为 syscall 层提供会话列表接口
 *
 * @param out_sessions [out] 输出会话 ID 数组（需调用者释放）
 * @param out_count [out] 输出会话数量
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_sessions 及其元素
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_session_list(
    char*** out_sessions,
    size_t* out_count);

/* ==================== telemetry 层集成 ==================== */

/**
 * @brief 为 telemetry 层提供追踪数据存储接口
 *
 * @param trace_id [in] 追踪 ID
 * @param span_id [in] Span ID
 * @param parent_id [in] 父 Span ID（可为 NULL）
 * @param name [in] Span 名称
 * @param start_time_us [in] 开始时间（微秒）
 * @param end_time_us [in] 结束时间（微秒）
 * @param status [in] 状态（0=运行中, 1=完成, 2=错误）
 * @param events_json [in] 事件 JSON 数组（可为 NULL）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责所有参数的生命周期
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_trace_save(
    const char* trace_id,
    const char* span_id,
    const char* parent_id,
    const char* name,
    int64_t start_time_us,
    int64_t end_time_us,
    int status,
    const char* events_json);

/**
 * @brief 为 telemetry 层提供追踪数据导出接口
 *
 * @param out_traces [out] 输出追踪数据 JSON 数组（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_traces
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_syscall_trace_export(char** out_traces);

/* ==================== memoryrovol 层集成 ==================== */

/**
 * @brief 为 memoryrovol 层提供原始记忆数据存储接口
 *
 * @param data [in] 原始数据
 * @param len [in] 数据长度
 * @param metadata [in] 元数据（JSON 格式，可为 NULL）
 * @param out_record_id [out] 输出记录 ID（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_record_id
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_memoryrovol_save(
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);

/**
 * @brief 为 memoryrovol 层提供原始记忆数据加载接口
 *
 * @param record_id [in] 记录 ID
 * @param out_data [out] 输出数据（需调用者释放）
 * @param out_len [out] 输出数据长度
 * @param out_metadata [out] 输出元数据（需调用者释放，可为 NULL）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_data 和 out_metadata
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_memoryrovol_load(
    const char* record_id,
    void** out_data,
    size_t* out_len,
    char** out_metadata);

/**
 * @brief 为 memoryrovol 层提供原始记忆数据删除接口
 *
 * @param record_id [in] 记录 ID
 * @return agentos_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_memoryrovol_delete(const char* record_id);

/* ==================== corekern IPC 层集成 ==================== */

/**
 * @brief 为 corekern IPC 层提供通道状态存储接口
 *
 * @param channel_id [in] 通道 ID
 * @param state_json [in] 通道状态 JSON
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责所有参数的生命周期
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_ipc_channel_save(
    const char* channel_id,
    const char* state_json);

/**
 * @brief 为 corekern IPC 层提供通道状态加载接口
 *
 * @param channel_id [in] 通道 ID
 * @param out_state [out] 输出通道状态 JSON（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_state
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_ipc_channel_load(
    const char* channel_id,
    char** out_state);

/* ==================== commons logging 层集成 ==================== */

/**
 * @brief 为 commons logging 层提供日志存储接口
 *
 * @param module [in] 模块名称
 * @param level [in] 日志级别
 * @param trace_id [in] 追踪 ID（可为 NULL）
 * @param message [in] 日志消息
 * @param timestamp_ns [in] 时间戳（纳秒）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责所有参数的生命周期
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 此接口同时支持快速路径和慢速路径
 */
AGENTOS_API agentos_error_t heapstore_logging_write(
    const char* module,
    int level,
    const char* trace_id,
    const char* message,
    uint64_t timestamp_ns);

/* ==================== 健康检查与可观测性 ==================== */

/**
 * @brief 获取 heapstore 集成健康状态
 *
 * @param out_health_json [out] 输出健康状态 JSON（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_health_json
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_integration_health_check(char** out_health_json);

/**
 * @brief 获取 heapstore 集成统计信息
 *
 * @param out_stats_json [out] 输出统计信息 JSON（需调用者释放）
 * @return agentos_error_t 错误码
 *
 * @ownership 调用者负责释放 out_stats_json
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t heapstore_integration_get_stats(char** out_stats_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_INTEGRATION_H */
