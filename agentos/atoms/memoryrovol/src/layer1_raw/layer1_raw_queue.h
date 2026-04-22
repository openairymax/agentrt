/**
 * @file layer1_raw_queue.h
 * @brief L1 原始卷异步队列管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供线程安全的异步写入请求队列，支持：
 * - 有界阻塞队列（带超时）
 * - 跨平台同步原语（Windows/POSIX）
 * - 生产者-消费者模式
 *
 * 本模块是从 async_storage_engine.c 拆分出来的队列管理子模块。
 */

#ifndef LAYER1_RAW_QUEUE_H
#define LAYER1_RAW_QUEUE_H

#include "memory_compat.h"
#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ASYNC_QUEUE_SIZE 65536

typedef struct write_request {
    char* id;
    void* data;
    size_t data_len;
    uint64_t timestamp_ns;
    uint8_t retry_count;
    uint8_t priority;
    uint32_t flags;
    struct write_request* next;
} write_request_t;

typedef struct async_queue async_queue_t;

async_queue_t* async_queue_create(size_t capacity);
void async_queue_destroy(async_queue_t* queue);
agentos_error_t async_queue_enqueue(async_queue_t* queue, write_request_t* request, uint32_t timeout_ms);
write_request_t* async_queue_dequeue(async_queue_t* queue, uint32_t timeout_ms);
size_t async_queue_get_count(async_queue_t* queue);
int async_queue_is_shutting_down(async_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* LAYER1_RAW_QUEUE_H */
