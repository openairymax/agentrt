/**
 * @file advanced_storage_async.c
 * @brief L1 增强存储异步操作管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "advanced_storage_async.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>

/* 基础库兼容性层 */
#include "memory_compat.h"
#include "string_compat.h"

/* ==================== 异步操作管理 ==================== */

async_operation_t* advanced_async_op_create(async_operation_type_t type,
                                           const char* id,
                                           const void* data, size_t data_size,
                                           void (*callback)(async_operation_t*),
                                           void* user_context) {
    if (!id) {
        AGENTOS_LOG_ERROR("Invalid async operation ID");
        return NULL;
    }

    async_operation_t* op = (async_operation_t*)AGENTOS_CALLOC(1, sizeof(async_operation_t));
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to allocate async operation");
        return NULL;
    }

    op->id = AGENTOS_STRDUP(id);
    op->type = type;
    op->state = ASYNC_OP_PENDING;
    op->result = AGENTOS_SUCCESS;
    op->start_time = agentos_get_monotonic_time_ns();
    op->callback = callback;
    op->user_context = user_context;
    op->lock = agentos_mutex_create();
    op->cond = agentos_condition_create();

    if (!op->id || !op->lock || !op->cond) {
        if (op->id) AGENTOS_FREE(op->id);
        if (op->lock) agentos_mutex_destroy(op->lock);
        if (op->cond) agentos_condition_destroy(op->cond);
        AGENTOS_FREE(op);
        AGENTOS_LOG_ERROR("Failed to initialize async operation resources");
        return NULL;
    }

    /* 复制输入数据（如果有） */
    if (data && data_size > 0) {
        op->input_data = AGENTOS_MALLOC(data_size);
        if (!op->input_data) {
            AGENTOS_FREE(op->id);
            agentos_mutex_destroy(op->lock);
            agentos_condition_destroy(op->cond);
            AGENTOS_FREE(op);
            AGENTOS_LOG_ERROR("Failed to allocate async operation input data");
            return NULL;
        }
        memcpy(op->input_data, data, data_size);
        op->input_size = data_size;
    }

    return op;
}

void advanced_async_op_destroy(async_operation_t* op) {
    if (!op) return;

    if (op->lock) agentos_mutex_destroy(op->lock);
    if (op->cond) agentos_condition_destroy(op->cond);

    if (op->id) AGENTOS_FREE(op->id);
    if (op->input_data) AGENTOS_FREE(op->input_data);
    if (op->output_data) AGENTOS_FREE(op->output_data);

    AGENTOS_FREE(op);
}

agentos_error_t advanced_async_op_wait(async_operation_t* op, uint32_t timeout_ms) {
    if (!op || !op->lock || !op->cond) return AGENTOS_EINVAL;

    agentos_mutex_lock(op->lock);

    while (op->state == ASYNC_OP_PENDING || op->state == ASYNC_OP_RUNNING) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(op->lock);
            return AGENTOS_ETIMEDOUT;
        }

        agentos_condition_wait(op->cond, op->lock, timeout_ms);

        /* 简化超时处理 */
        if (timeout_ms > 0) {
            agentos_mutex_unlock(op->lock);
            return AGENTOS_ETIMEDOUT;
        }
    }

    agentos_error_t result = op->result;
    agentos_mutex_unlock(op->lock);

    return result;
}

/* ==================== 异步队列管理 ==================== */

async_queue_t* advanced_async_queue_create(size_t capacity) {
    if (capacity == 0) {
        AGENTOS_LOG_ERROR("Invalid capacity for async queue");
        return NULL;
    }

    async_queue_t* queue = (async_queue_t*)AGENTOS_CALLOC(1, sizeof(async_queue_t));
    if (!queue) {
        AGENTOS_LOG_ERROR("Failed to allocate async queue");
        return NULL;
    }

    queue->operations = (async_operation_t**)AGENTOS_MALLOC(sizeof(async_operation_t*) * capacity);
    if (!queue->operations) {
        AGENTOS_FREE(queue);
        AGENTOS_LOG_ERROR("Failed to allocate operations array");
        return NULL;
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->lock = agentos_mutex_create();
    queue->not_empty = agentos_condition_create();
    queue->not_full = agentos_condition_create();

    if (!queue->lock || !queue->not_empty || !queue->not_full) {
        if (queue->lock) agentos_mutex_destroy(queue->lock);
        if (queue->not_empty) agentos_condition_destroy(queue->not_empty);
        if (queue->not_full) agentos_condition_destroy(queue->not_full);
        AGENTOS_FREE(queue->operations);
        AGENTOS_FREE(queue);
        AGENTOS_LOG_ERROR("Failed to create queue synchronization primitives");
        return NULL;
    }

    return queue;
}

void advanced_async_queue_destroy(async_queue_t* queue) {
    if (!queue) return;

    /* 释放所有未完成的操作 */
    while (!advanced_async_queue_is_empty(queue)) {
        async_operation_t* op = advanced_async_queue_pop(queue, 0);
        if (op) advanced_async_op_destroy(op);
    }

    if (queue->lock) agentos_mutex_destroy(queue->lock);
    if (queue->not_empty) agentos_condition_destroy(queue->not_empty);
    if (queue->not_full) agentos_condition_destroy(queue->not_full);

    AGENTOS_FREE(queue->operations);
    AGENTOS_FREE(queue);
}

agentos_error_t advanced_async_queue_push(async_queue_t* queue, async_operation_t* op, uint32_t timeout_ms) {
    if (!queue || !op) return AGENTOS_EINVAL;

    agentos_mutex_lock(queue->lock);

    /* 等待队列非满 */
    while (queue->size >= queue->capacity) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(queue->lock);
            return AGENTOS_ETIMEDOUT;
        }
        agentos_condition_wait(queue->not_full, queue->lock, timeout_ms);
    }

    /* 添加到队列尾部 */
    queue->operations[queue->tail] = op;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;

    /* 通知等待的消费者 */
    agentos_condition_signal(queue->not_empty);
    agentos_mutex_unlock(queue->lock);

    return AGENTOS_SUCCESS;
}

async_operation_t* advanced_async_queue_pop(async_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;

    agentos_mutex_lock(queue->lock);

    /* 等待队列非空 */
    while (queue->size == 0) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(queue->lock);
            return NULL;
        }
        agentos_condition_wait(queue->not_empty, queue->lock, timeout_ms);
    }

    /* 从队列头部取出 */
    async_operation_t* op = queue->operations[queue->head];
    queue->operations[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;

    /* 通知等待的生产者 */
    agentos_condition_signal(queue->not_full);
    agentos_mutex_unlock(queue->lock);

    return op;
}

size_t advanced_async_queue_size(async_queue_t* queue) {
    if (!queue) return 0;

    agentos_mutex_lock(queue->lock);
    size_t size = queue->size;
    agentos_mutex_unlock(queue->lock);

    return size;
}

int advanced_async_queue_is_empty(async_queue_t* queue) {
    if (!queue) return 1;

    agentos_mutex_lock(queue->lock);
    int is_empty = (queue->size == 0);
    agentos_mutex_unlock(queue->lock);

    return is_empty;
}
