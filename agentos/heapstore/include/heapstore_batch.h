/**
 * @file heapstore_batch.h
 * @brief AgentOS heapstore 批量写入接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_HEAPSTORE_BATCH_H
#define AGENTOS_HEAPSTORE_BATCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "heapstore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 批量写入项目类型
 */
typedef enum {
    HEAPSTORE_BATCH_ITEM_LOG = 0,
    HEAPSTORE_BATCH_ITEM_SPAN,
    HEAPSTORE_BATCH_ITEM_SESSION,
    HEAPSTORE_BATCH_ITEM_AGENT,
    HEAPSTORE_BATCH_ITEM_SKILL,
    HEAPSTORE_BATCH_ITEM_MEMORY_POOL,
    HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC,
    HEAPSTORE_BATCH_ITEM_IPC_CHANNEL,
    HEAPSTORE_BATCH_ITEM_IPC_BUFFER,
    HEAPSTORE_BATCH_ITEM_TYPE_COUNT
} heapstore_batch_item_type_t;

/**
 * @brief 批量写入项目联合数据
 */
typedef struct {
    heapstore_log_entry_t log;
    heapstore_trace_entry_t trace;
    heapstore_session_record_t session;
    heapstore_agent_record_t agent;
    heapstore_skill_record_t skill;
    heapstore_memory_pool_t memory_pool;
    heapstore_memory_allocation_t memory_alloc;
    heapstore_ipc_channel_t ipc_channel;
    heapstore_ipc_buffer_t ipc_buffer;
} heapstore_batch_item_data_t;

/**
 * @brief 批量写入项目节点
 */
typedef struct heapstore_batch_item {
    heapstore_batch_item_type_t type;
    heapstore_batch_item_data_t data;
    struct heapstore_batch_item* next;
} heapstore_batch_item_t;

/**
 * @brief 批量写入上下文
 */
typedef struct heapstore_batch_context {
    heapstore_batch_item_t* head;
    heapstore_batch_item_t* tail;
    size_t count;
    size_t capacity;
} heapstore_batch_context_t;

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

 * @since v1.0.0.5*/
heapstore_error_t heapstore_batch_add_log(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* message);

/**
 * @brief 添加带追踪ID的日志到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_log_with_trace(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* trace_id,
    const char* message);

/**
 * @brief 添加追踪Span到批量写入缓冲区
 */
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

/**
 * @brief 添加会话记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_session(
    heapstore_batch_context_t* ctx,
    const heapstore_session_record_t* record);

/**
 * @brief 添加Agent记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_agent(
    heapstore_batch_context_t* ctx,
    const heapstore_agent_record_t* record);

/**
 * @brief 添加Skill记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_skill(
    heapstore_batch_context_t* ctx,
    const heapstore_skill_record_t* record);

/**
 * @brief 添加内存池记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_memory_pool(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_pool_t* pool);

/**
 * @brief 添加内存分配记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_allocation(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_allocation_t* allocation);

/**
 * @brief 添加IPC通道记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_ipc_channel(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_channel_t* channel);

/**
 * @brief 添加IPC缓冲区记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_ipc_buffer(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_buffer_t* buffer);

/**
 * @brief 添加Span记录到批量写入缓冲区
 */
heapstore_error_t heapstore_batch_add_span(
    heapstore_batch_context_t* ctx,
    const heapstore_span_t* span);

/**
 * @brief 提交批量写入
 *
 * @param ctx [in/out] 批量写入上下文
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否

 * @since v1.0.0.5*/
heapstore_error_t heapstore_batch_commit(heapstore_batch_context_t* ctx);

/**
 * @brief 回滚批量写入
 *
 * @param ctx [in/out] 批量写入上下文
 *
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0.5*/
void heapstore_batch_rollback(heapstore_batch_context_t* ctx);

/**
 * @brief 销毁批量写入上下文
 *
 * @param ctx [in] 批量写入上下文
 *
 * @threadsafe 是
 * @reentrant 是

 * @since v1.0.0.5*/
void heapstore_batch_context_destroy(heapstore_batch_context_t* ctx);

/**
 * @brief 获取当前批量项目数量
 *
 * @param ctx [in] 批量写入上下文
 * @return size_t 项目数量
 */
size_t heapstore_batch_get_count(const heapstore_batch_context_t* ctx);

/**
 * @brief 获取批量上下文容量
 *
 * @param ctx [in] 批量写入上下文
 * @return size_t 容量大小
 */
size_t heapstore_batch_get_capacity(const heapstore_batch_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HEAPSTORE_BATCH_H */
