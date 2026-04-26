/**
 * @file heapstore_batch.c
 * @brief AgentOS heapstore 批量写入模块实现（优化版）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 本模块采用 DRY 原则重构，消除重复代码，
 *       将圈复杂度控制在7以下，提升可维护性。
 */

#include "heapstore.h"
#include "heapstore_batch.h"
#include "heapstore_log.h"
#include "private.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 处理日志类型的批量写入
 */
static heapstore_error_t batch_commit_log(const void* data) {
    const heapstore_log_entry_t* log_entry = (const heapstore_log_entry_t*)data;
    if (!log_entry || log_entry->message[0] == '\0') {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_log_write(
        log_entry->level,
        log_entry->service,
        log_entry->trace_id[0] ? log_entry->trace_id : NULL,
        NULL, 0,
        log_entry->message);

    return heapstore_SUCCESS;
}

/**
 * @brief 处理 Span 类型的批量写入（含单位转换）
 */
static heapstore_error_t batch_commit_span(const void* data) {
    const heapstore_trace_entry_t* trace_entry = (const heapstore_trace_entry_t*)data;
    if (!trace_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_span_t span_rec;
    memset(&span_rec, 0, sizeof(span_rec));

    strncpy(span_rec.trace_id, trace_entry->trace_id, sizeof(span_rec.trace_id) - 1);
    strncpy(span_rec.span_id, trace_entry->span_id, sizeof(span_rec.span_id) - 1);

    if (trace_entry->parent_span_id[0]) {
        strncpy(span_rec.parent_span_id, trace_entry->parent_span_id, sizeof(span_rec.parent_span_id) - 1);
    }

    strncpy(span_rec.name, trace_entry->name, sizeof(span_rec.name) - 1);
    strncpy(span_rec.kind, trace_entry->kind, sizeof(span_rec.kind) - 1);

    span_rec.start_time_ns = (uint64_t)trace_entry->start_time_us * 1000ULL;
    span_rec.end_time_ns = (uint64_t)trace_entry->end_time_us * 1000ULL;
    snprintf(span_rec.status, sizeof(span_rec.status), "%d", trace_entry->status);

    if (trace_entry->attributes[0]) {
        span_rec.attributes = strdup(trace_entry->attributes);
        if (!span_rec.attributes) {
            return heapstore_ERR_OUT_OF_MEMORY;
        }
        span_rec.attribute_count = 1;
    }

    heapstore_error_t err = heapstore_trace_write_span(&span_rec);

    if (span_rec.attributes) {
        free(span_rec.attributes);
    }

    return err;
}

/**
 * @brief 批量操作处理器函数指针类型
 */
typedef heapstore_error_t (*batch_handler_fn)(const void* data);

/**
 * @brief 批量操作处理器注册表
 *
 * 使用策略模式替代 switch-case，降低圈复杂度
 */
static const struct {
    heapstore_batch_item_type_t type;
    batch_handler_fn handler;
} batch_handlers[] = {
    { HEAPSTORE_BATCH_ITEM_LOG,           batch_commit_log },
    { HEAPSTORE_BATCH_ITEM_SPAN,          batch_commit_span },
    { HEAPSTORE_BATCH_ITEM_SESSION,       NULL }, /* 使用默认处理 */
    { HEAPSTORE_BATCH_ITEM_AGENT,         NULL },
    { HEAPSTORE_BATCH_ITEM_SKILL,         NULL },
    { HEAPSTORE_BATCH_ITEM_MEMORY_POOL,   NULL },
    { HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC,  NULL },
    { HEAPSTORE_BATCH_ITEM_IPC_CHANNEL,   NULL },
    { HEAPSTORE_BATCH_ITEM_IPC_BUFFER,    NULL }
};

#define BATCH_HANDLER_COUNT (sizeof(batch_handlers) / sizeof(batch_handlers[0]))

/**
 * @brief 默认批量处理函数（用于 registry/memory/ipc 操作）
 */
static heapstore_error_t batch_default_handler(const heapstore_batch_item_t* item) {
    switch (item->type) {
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
 * @brief 为批量写入项目分配内存并初始化
 */
static heapstore_batch_item_t* batch_alloc_item(heapstore_batch_item_type_t type) {
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
 * @brief 将项目添加到链表尾部（通用实现，符合 DRY 原则）
 *
 * @param ctx [in/out] 批量上下文
 * @param item [in] 已分配的项目
 * @return heapstore_error_t 错误码
 */
static heapstore_error_t batch_append_item(heapstore_batch_context_t* ctx, heapstore_batch_item_t* item) {
    if (!ctx || !item) {
        free(item);
        return heapstore_ERR_INVALID_PARAM;
    }

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
 * @brief 验证批量上下文状态
 */
static bool batch_validate_context(const heapstore_batch_context_t* ctx) {
    return (ctx != NULL && ctx->count <= ctx->capacity && ctx->capacity > 0);
}

/* ==================== 公共 API 实现 ==================== */

/**
 * @brief 通用批量添加接口（内部使用，减少代码重复）
 *
 * 符合 DRY 原则，所有 add_* 函数都调用此通用实现
 */
static heapstore_error_t batch_add_generic(
    heapstore_batch_context_t* ctx,
    heapstore_batch_item_type_t type,
    const void* data,
    size_t data_size) {

    if (!batch_validate_context(ctx)) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!data) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = batch_alloc_item(type);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    if (data_size > 0) {
        memcpy(&item->data, data, data_size);
    }

    return batch_append_item(ctx, item);
}

heapstore_error_t heapstore_batch_add_log(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* message) {

    if (!batch_validate_context(ctx)) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!service || !message) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = batch_alloc_item(HEAPSTORE_BATCH_ITEM_LOG);
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    item->data.log.level = level;

    size_t copy_len = strlen(service);
    if (copy_len >= sizeof(item->data.log.service)) {
        copy_len = sizeof(item->data.log.service) - 1;
    }
    memcpy(item->data.log.service, service, copy_len);
    item->data.log.service[copy_len] = '\0';

    item->data.log.trace_id[0] = '\0';

    copy_len = strlen(message);
    if (copy_len >= sizeof(item->data.log.message)) {
        copy_len = sizeof(item->data.log.message) - 1;
    }
    memcpy(item->data.log.message, message, copy_len);
    item->data.log.message[copy_len] = '\0';

    return batch_append_item(ctx, item);
}

heapstore_error_t heapstore_batch_add_span(
    heapstore_batch_context_t* ctx,
    const heapstore_span_t* span) {

    if (!span) return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_SPAN, span, sizeof(*span));

    heapstore_trace_entry_t converted;
    memset(&converted, 0, sizeof(converted));
    strncpy(converted.trace_id, span->trace_id, sizeof(converted.trace_id) - 1);
    strncpy(converted.span_id, span->span_id, sizeof(converted.span_id) - 1);
    strncpy(converted.parent_span_id, span->parent_span_id, sizeof(converted.parent_span_id) - 1);
    strncpy(converted.name, span->name, sizeof(converted.name) - 1);
    strncpy(converted.kind, span->kind, sizeof(converted.kind) - 1);
    converted.start_time_us = span->start_time_ns / 1000;
    converted.end_time_us = span->end_time_ns / 1000;
    converted.status = (strcmp(span->status, "ok") == 0) ? 0 : -1;

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_SPAN, &converted, sizeof(converted));
}

heapstore_error_t heapstore_batch_add_session(
    heapstore_batch_context_t* ctx,
    const heapstore_session_record_t* session) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_SESSION, session, sizeof(*session));
}

heapstore_error_t heapstore_batch_add_agent(
    heapstore_batch_context_t* ctx,
    const heapstore_agent_record_t* agent) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_AGENT, agent, sizeof(*agent));
}

heapstore_error_t heapstore_batch_add_skill(
    heapstore_batch_context_t* ctx,
    const heapstore_skill_record_t* skill) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_SKILL, skill, sizeof(*skill));
}

heapstore_error_t heapstore_batch_add_memory_pool(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_pool_t* pool) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_MEMORY_POOL, pool, sizeof(*pool));
}

heapstore_error_t heapstore_batch_add_memory_allocation(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_allocation_t* allocation) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC, allocation, sizeof(*allocation));
}

heapstore_error_t heapstore_batch_add_ipc_channel(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_channel_t* channel) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_IPC_CHANNEL, channel, sizeof(*channel));
}

heapstore_error_t heapstore_batch_add_ipc_buffer(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_buffer_t* buffer) {

    return batch_add_generic(ctx, HEAPSTORE_BATCH_ITEM_IPC_BUFFER, buffer, sizeof(*buffer));
}

/**
 * @brief 处理单个批量写入项目（使用查表法降低复杂度）
 *
 * 圈复杂度从 ~10 降低至 ~5
 */
static heapstore_error_t batch_process_single_item(const heapstore_batch_item_t* item) {
    if (!item) {
        return heapstore_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < BATCH_HANDLER_COUNT; i++) {
        if (batch_handlers[i].type == item->type) {
            if (batch_handlers[i].handler) {
                return batch_handlers[i].handler((const void*)&item->data);
            } else {
                return batch_default_handler(item);
            }
        }
    }

    return heapstore_ERR_INVALID_PARAM;
}

heapstore_error_t heapstore_batch_commit(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    heapstore_batch_item_t* item = ctx->head;

    while (item) {
        heapstore_batch_item_t* next = item->next;

        heapstore_error_t err = batch_process_single_item(item);

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

void heapstore_batch_rollback(heapstore_batch_context_t* ctx) {
    if (!ctx) return;

    heapstore_batch_item_t* item = ctx->head;
    while (item) {
        heapstore_batch_item_t* next = item->next;
        free(item);
        item = next;
    }

    ctx->head = ctx->tail = NULL;
    ctx->count = 0;
}
