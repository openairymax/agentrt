/**
 * @file async_storage_queue.h
 * @brief 异步存储引擎队列管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef ASYNC_STORAGE_QUEUE_H
#define ASYNC_STORAGE_QUEUE_H

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 默认队列大小（条目数） */
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
agentos_error_t async_queue_enqueue(async_queue_t* queue, write_request_t* request,
                                     uint32_t timeout_ms);
write_request_t* async_queue_dequeue(async_queue_t* queue, uint32_t timeout_ms);
size_t async_queue_get_count(async_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_STORAGE_QUEUE_H */
