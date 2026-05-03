/**
 * @file layer1_raw_internal.h
 * @brief L1 原始卷内部共享定义（不对外暴露）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本头文件包含 layer1_raw 模块内部 .c 文件之间共享的结构体完整定义。
 * 外部模块应使用 layer1_raw.h 中的前向声明和 API 接口。
 */

#ifndef AGENTOS_LAYER1_RAW_INTERNAL_H
#define AGENTOS_LAYER1_RAW_INTERNAL_H

#include "layer1_raw.h"
#include "platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_queue async_queue_t;
typedef struct worker_thread worker_thread_t;
typedef struct shard_manager shard_manager_t;

struct agentos_layer1_raw {
    char* storage_path;
    async_queue_t* queue;
    worker_thread_t* workers;
    uint32_t worker_count;
    agentos_mutex_t lock;
    agentos_cond_t cond;
    int shutdown;

    shard_manager_t* shards;
    uint32_t shard_count;

    uint64_t total_writes;
    uint64_t total_reads;
    uint64_t total_errors;

    void* metadata_db;

    void* inner;
};

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LAYER1_RAW_INTERNAL_H */
