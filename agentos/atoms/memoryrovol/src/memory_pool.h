/**
 * @file memory_pool.h
 * @brief 内存池管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H
#define MEMORY_POOL_T_DEFINED

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_ALIGNMENT 64
#define MIN_ALLOCATION_SIZE 64

typedef struct memory_block_header {
    uint64_t size;
    uint64_t magic;
    uint8_t in_use;
    uint8_t pool_id;
    uint16_t flags;
    uint32_t ref_count;
    uint64_t alloc_time_ns;
    uint64_t last_access_ns;
    struct memory_block_header* next;
    struct memory_block_header* prev;
} memory_block_header_t;

typedef struct memory_pool {
    uint8_t pool_id;
    char* pool_name;
    uint8_t* base_address;
    uint64_t total_size;
    uint64_t used_size;
    uint64_t peak_size;
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t alloc_failures;
    memory_block_header_t* free_list;
    memory_block_header_t* used_list;
    agentos_mutex_t* lock;
    uint32_t flags;
} memory_pool_t;

memory_pool_t* memory_pool_create(uint8_t pool_id, const char* name, uint64_t size);
void memory_pool_destroy(memory_pool_t* pool);
void* memory_pool_alloc(memory_pool_t* pool, size_t size);
void memory_pool_free(memory_pool_t* pool, void* ptr);
uint64_t memory_pool_get_used_size(memory_pool_t* pool);
uint64_t memory_pool_get_peak_size(memory_pool_t* pool);
double memory_pool_get_fragmentation_ratio(memory_pool_t* pool);
uint64_t memory_pool_get_alloc_count(memory_pool_t* pool);
uint64_t memory_pool_get_free_count(memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_POOL_H */
