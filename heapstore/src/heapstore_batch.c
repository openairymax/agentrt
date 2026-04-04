/**
 * @file heapstore_batch.c
 * @brief AgentOS heapstore 批量写入模块实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore.h"
#include "private.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 处理日志类型的批量写入
 */
static heapstore_error_t heapstore_batch_commit_log(const heapstore_log_entry_t* log_entry) {
    if (!log_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    return heapstore_log_write(
        log_entry->level,
        log_entry->service,
        log_entry->trace_id[0] ? log_entry->trace_id : NULL,
        NULL, 0, log_entry->message);
}

/**
 * @brief 处理 Span 类型的批量写入
 *
 * 包含单位转换（微秒→纳秒）和内存管理
 */
static heapstore_error_t heapstore_batch_commit_span(const heapstore_trace_entry_t* trace_entry) {
    if (!trace_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_span_t span_rec;
    memset(&span_rec, 0, sizeof(span_rec));

    /* ID 字段拷贝 */
    strncpy(span_rec.trace_id, trace_entry->trace_id, sizeof(span_rec.trace_id) - 1);
    strncpy(span_rec.span_id, trace_entry->span_id, sizeof(span_rec.span_id) - 1);
    if (trace_entry->parent_span_id[0]) {
        strncpy(span_rec.parent_span_id, trace_entry->parent_span_id, sizeof(span_rec.parent_span_id) - 1);
    }

    /* 基本属性拷贝 */
    strncpy(span_rec.name, trace_entry->name, sizeof(span_rec.name) - 1);
    strncpy(span_rec.kind, trace_entry->kind, sizeof(span_rec.kind) - 1);

    /* 单位转换：微秒 → 纳秒 */
    span_rec.start_time_ns = (uint64_t)trace_entry->start_time_us * 1000ULL;
    span_rec.end_time_ns = (uint64_t)trace_entry->end_time_us * 1000ULL;

    /* 状态转换：int → string */
    snprintf(span_rec.status, sizeof(span_rec.status), "%d", trace_entry->status);

    /* 属性处理：需要深拷贝 */
    if (trace_entry->attributes[0]) {
        span_rec.attributes = strdup(trace_entry->attributes);
        if (!span_rec.attributes) {
            return heapstore_ERR_OUT_OF_MEMORY;
        }
        span_rec.attribute_count = 1;
    }

    /* 调用 trace 写入 API */
    heapstore_error_t err = heapstore_trace_write_span(&span_rec);

    /* 释放动态分配的属性内存 */
    if (span_rec.attributes) {
        free(span_rec.attributes);
    }

    return err;
}

/**
 * @brief 处理单个批量写入项目
 *
 * 根据项目类型调用相应的存储子系统 API
 *
 * @param item [in] 批量写入项目
 * @return heapstore_error_t 错误码
 */
static heapstore_error_t heapstore_batch_process_single_item(const heapstore_batch_item_t* item) {
    if (!item) {
        return heapstore_ERR_INVALID_PARAM;
    }

    switch (item->type) {
        case HEAPSTORE_BATCH_ITEM_LOG:
            return heapstore_batch_commit_log(&item->data.log);

        case HEAPSTORE_BATCH_ITEM_SPAN:
            return heapstore_batch_commit_span(&item->data.span);

        case HEAPSTORE_BATCH_ITEM_SESSION:
            return heapstore_registry_add_session(&item->data.session);

        case HEAPSTORE_BATCH_ITEM_AGENT:
            return heapstore_registry_add_agent(&item->data.agent);

        case HEAPSTORE_BATCH_ITEM_SKILL:
            return heapstore_registry_add_skill(&item->data.skill);

        case HEAPSTORE_BATCH_ITEM_MEMORY_POOL:
            return heapstore_memory_record_pool(&item->data.memory_pool);

        case HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC:
            return heapstore_memory_record_allocation(&item->data.memory_alloc);

        case HEAPSTORE_BATCH_ITEM_IPC_CHANNEL:
            return heapstore_ipc_record_channel(&item->data.ipc_channel);

        case HEAPSTORE_BATCH_ITEM_IPC_BUFFER:
            return heapstore_ipc_record_buffer(&item->data.ipc_buffer);

        default:
            return heapstore_ERR_INVALID_PARAM;
    }
}

/**
 * @brief 为批量写入项目分配内存
 */
static heapstore_batch_item_t* heapstore_batch_alloc_item(heapstore_batch_item_type_t type) {
    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return NULL;
    }

    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = type;
    item->next = NULL;

    return item;
}

/**
 * @brief 添加日志到批量上下文
 */
heapstore_error_t heapstore_batch_add_log(
    heapstore_batch_context_t* ctx,
    int level,
    const char* service,
    const char* trace_id,
    const char* message) {

    if (!ctx || !service || !message) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_LOG);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    item->data.log.level = level;
    strncpy(item->data.log.service, service, sizeof(item->data.log.service) - 1);
    if (trace_id) {
        strncpy(item->data.log.trace_id, trace_id, sizeof(item->data.log.trace_id) - 1);
    }
    strncpy(item->data.log.message, message, sizeof(item->data.log.message) - 1);

    /* 添加到链表尾部 */
    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 Span 到批量上下文
 */
heapstore_error_t heapstore_batch_add_span(
    heapstore_batch_context_t* ctx,
    const heapstore_trace_entry_t* trace_entry) {

    if (!ctx || !trace_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_SPAN);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.span, trace_entry, sizeof(heapstore_trace_entry_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 Session 到批量上下文
 */
heapstore_error_t heapstore_batch_add_session(
    heapstore_batch_context_t* ctx,
    const heapstore_session_record_t* session) {

    if (!ctx || !session) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_SESSION);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.session, session, sizeof(heapstore_session_record_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 Agent 到批量上下文
 */
heapstore_error_t heapstore_batch_add_agent(
    heapstore_batch_context_t* ctx,
    const heapstore_agent_record_t* agent) {

    if (!ctx || !agent) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_AGENT);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.agent, agent, sizeof(heapstore_agent_record_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 Skill 到批量上下文
 */
heapstore_error_t heapstore_batch_add_skill(
    heapstore_batch_context_t* ctx,
    const heapstore_skill_record_t* skill) {

    if (!ctx || !skill) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_SKILL);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.skill, skill, sizeof(heapstore_skill_record_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加内存池记录到批量上下文
 */
heapstore_error_t heapstore_batch_add_memory_pool(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_pool_t* pool) {

    if (!ctx || !pool) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_MEMORY_POOL);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.memory_pool, pool, sizeof(heapstore_memory_pool_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加内存分配记录到批量上下文
 */
heapstore_error_t heapstore_batch_add_memory_allocation(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_allocation_t* allocation) {

    if (!ctx || !allocation) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.memory_alloc, allocation, sizeof(heapstore_memory_allocation_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 IPC 通道记录到批量上下文
 */
heapstore_error_t heapstore_batch_add_ipc_channel(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_channel_t* channel) {

    if (!ctx || !channel) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_IPC_CHANNEL);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.ipc_channel, channel, sizeof(heapstore_ipc_channel_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 添加 IPC 缓冲区记录到批量上下文
 */
heapstore_error_t heapstore_batch_add_ipc_buffer(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_buffer_t* buffer) {

    if (!ctx || !buffer) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = heapstore_batch_alloc_item(HEAPSTORE_BATCH_ITEM_IPC_BUFFER);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&item->data.ipc_buffer, buffer, sizeof(heapstore_ipc_buffer_t));

    if (!ctx->head) {
        ctx->head = ctx->tail = item;
    } else {
        ctx->tail->next = item;
        ctx->tail = item;
    }

    ctx->count++;
    return heapstore_SUCCESS;
}

/**
 * @brief 提交批量写入
 *
 * 遍历批量上下文中的所有项目，逐个提交到相应的存储子系统
 *
 * @param ctx [in] 批量写入上下文
 * @return heapstore_error_t 错误码
 *
 * @note 采用“尽可能提交”策略，单个失败不影响其他项目
 */
heapstore_error_t heapstore_batch_commit(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    heapstore_batch_item_t* item = ctx->head;

    while (item) {
        heapstore_batch_item_t* next = item->next;

        heapstore_error_t err = heapstore_batch_process_single_item(item);

        if (err != heapstore_SUCCESS && result == heapstore_SUCCESS) {
            result = err;
        }

        free(item);
        item = next;
    }

    ctx->head = ctx->tail = NULL;
    ctx->count = 0;

    return result;
}

/**
 * @brief 回滚批量写入
 *
 * 清空批量上下文，释放所有项目
 *
 * @param ctx [in] 批量写入上下文
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_batch_rollback(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_batch_item_t* item = ctx->head;
    while (item) {
        heapstore_batch_item_t* next = item->next;
        free(item);
        item = next;
    }

    ctx->head = ctx->tail = NULL;
    ctx->count = 0;

    return heapstore_SUCCESS;
}
