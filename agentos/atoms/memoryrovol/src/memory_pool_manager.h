/**
 * @file memory_pool_manager.h
 * @brief 内存池管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef MEMORY_POOL_MANAGER_H
#define MEMORY_POOL_MANAGER_H

#include "memory_pool.h"

#ifndef MAX_MEMORY_POOLS
#define MAX_MEMORY_POOLS 16
#endif

#ifdef __cplusplus
extern "C" {
#endif

agentos_error_t memory_pool_create(const char* name, uint64_t size, memory_pool_t** out_pool);
void memory_pool_destroy(memory_pool_t* pool);
void* memory_pool_alloc(memory_pool_t* pool, size_t size);
agentos_error_t memory_pool_free(memory_pool_t* pool, void* ptr);
uint64_t memory_pool_get_used_size(memory_pool_t* pool);
uint64_t memory_pool_get_total_size(memory_pool_t* pool);
double memory_pool_get_fragmentation(memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_POOL_MANAGER_H */
