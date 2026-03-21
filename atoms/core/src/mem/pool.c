/**
 * @file pool.c
 * @brief 内存池实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "mem.h"
#include <stdint.h>

typedef struct pool_block {
    struct pool_block* next;
} pool_block_t;

typedef struct {
    size_t block_size;
    size_t block_count;
    pool_block_t* free_list;
    agentos_mutex_t* lock;
} mem_pool_t;

void* agentos_mem_pool_create(size_t block_size, uint32_t block_count) {
    mem_pool_t* pool = (mem_pool_t*)agentos_mem_alloc(sizeof(mem_pool_t));
    if (!pool) return NULL;

    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->free_list = NULL;
    pool->lock = agentos_mutex_create();
    if (!pool->lock) {
        agentos_mem_free(pool);
        return NULL;
    }

    // 预分配所有块
    for (uint32_t i = 0; i < block_count; i++) {
        pool_block_t* block = (pool_block_t*)agentos_mem_aligned_alloc(block_size, 8);
        if (!block) {
            // 清理已分配的块
            while (pool->free_list) {
                void* tmp = pool->free_list;
                pool->free_list = pool->free_list->next;
                agentos_mem_free(tmp);
            }
            agentos_mutex_destroy(pool->lock);
            agentos_mem_free(pool);
            return NULL;
        }
        block->next = pool->free_list;
        pool->free_list = block;
    }

    return (void*)pool;
}

void* agentos_mem_pool_alloc(void* pool_handle) {
    mem_pool_t* pool = (mem_pool_t*)pool_handle;
    if (!pool) return NULL;

    agentos_mutex_lock(pool->lock);
    pool_block_t* block = pool->free_list;
    if (block) {
        pool->free_list = block->next;
    }
    agentos_mutex_unlock(pool->lock);

    return block;
}

void agentos_mem_pool_free(void* pool_handle, void* ptr) {
    if (!ptr) return;
    mem_pool_t* pool = (mem_pool_t*)pool_handle;
    if (!pool) return;

    pool_block_t* block = (pool_block_t*)ptr;
    agentos_mutex_lock(pool->lock);
    block->next = pool->free_list;
    pool->free_list = block;
    agentos_mutex_unlock(pool->lock);
}

void agentos_mem_pool_destroy(void* pool_handle) {
    mem_pool_t* pool = (mem_pool_t*)pool_handle;
    if (!pool) return;

    // 释放所有块
    while (pool->free_list) {
        void* tmp = pool->free_list;
        pool->free_list = pool->free_list->next;
        agentos_mem_free(tmp);
    }
    agentos_mutex_destroy(pool->lock);
    agentos_mem_free(pool);
}