/**
 * @file pool.c
 * @brief 内存池分配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include <stdlib.h>
#include <string.h>

typedef struct pool_block {
    struct pool_block* next;
} pool_block_t;

typedef struct {
    pool_block_t* free_list;
    void* raw_memory;
    size_t block_size;
    size_t actual_block_size;
    uint32_t block_count;
    uint32_t free_count;
} pool_t;

void* agentos_mem_pool_create(size_t block_size, uint32_t block_count) {
    if (block_size < sizeof(void*) || block_count == 0) return NULL;

    /* 检查 block_size + 7 是否溢出 */
    if (block_size > SIZE_MAX - 7) return NULL;
    size_t actual_block_size = (block_size + 7) & ~(size_t)7;

    /* 检查 actual_block_size * block_count 是否溢出 */
    if (block_count > SIZE_MAX / actual_block_size) return NULL;
    size_t total_size = actual_block_size * block_count;

    void* raw = agentos_mem_aligned_alloc(total_size, 8);
    if (!raw) return NULL;

    pool_t* pool = (pool_t*)malloc(sizeof(pool_t));
    if (!pool) {
        agentos_mem_aligned_free(raw);
        return NULL;
    }

    pool->block_size = block_size;
    pool->actual_block_size = actual_block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    pool->raw_memory = raw;

    uint8_t* blocks = (uint8_t*)raw;
    pool->free_list = NULL;
    for (uint32_t i = block_count; i > 0; i--) {
        pool_block_t* block = (pool_block_t*)(blocks + (i - 1) * actual_block_size);
        block->next = pool->free_list;
        pool->free_list = block;
    }

    return pool;
}

void* agentos_mem_pool_alloc(void* pool_handle) {
    if (!pool_handle) return NULL;
    pool_t* pool = (pool_t*)pool_handle;

    if (!pool->free_list) return NULL;

    pool_block_t* block = pool->free_list;
    pool->free_list = block->next;
    pool->free_count--;

    memset(block, 0, pool->block_size);
    return block;
}

void agentos_mem_pool_free(void* pool_handle, void* ptr) {
    if (!pool_handle || !ptr) return;
    pool_t* pool = (pool_t*)pool_handle;

    pool_block_t* block = (pool_block_t*)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_count++;
}

void agentos_mem_pool_destroy(void* pool_handle) {
    if (!pool_handle) return;
    pool_t* pool = (pool_t*)pool_handle;

    if (pool->raw_memory) {
        agentos_mem_aligned_free(pool->raw_memory);
    }
    free(pool);
}
