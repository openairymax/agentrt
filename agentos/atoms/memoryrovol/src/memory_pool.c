/**
 * @file memory_pool.c
 * @brief 内存池管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory_pool.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static size_t align_size(size_t size) {
    size_t aligned = (size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    return aligned > MIN_ALLOCATION_SIZE ? aligned : MIN_ALLOCATION_SIZE;
}

static double calculate_fragmentation_ratio(memory_pool_t* pool) {
    if (!pool || pool->total_size == 0) return 0.0;
    uint64_t free_space = pool->total_size - pool->used_size;
    uint64_t largest_free_block = 0;

    memory_block_header_t* block = pool->free_list;
    while (block) {
        if (block->size > largest_free_block) {
            largest_free_block = block->size;
        }
        block = block->next;
    }

    if (free_space == 0) return 0.0;
    return 1.0 - ((double)largest_free_block / free_space);
}

memory_pool_t* memory_pool_create(uint8_t pool_id, const char* name, uint64_t size) {
    memory_pool_t* pool = (memory_pool_t*)AGENTOS_CALLOC(1, sizeof(memory_pool_t));
    if (!pool) return NULL;

    pool->base_address = (uint8_t*)aligned_alloc(MEMORY_ALIGNMENT, size);
    if (!pool->base_address) {
        AGENTOS_FREE(pool);
        return NULL;
    }

    pool->pool_id = pool_id;
    pool->pool_name = name ? AGENTOS_STRDUP(name) : NULL;
    pool->total_size = size;
    pool->used_size = 0;
    pool->peak_size = 0;
    pool->alloc_count = 0;
    pool->free_count = 0;
    pool->alloc_failures = 0;
    pool->flags = 0;

    pool->lock = agentos_mutex_create();
    if (!pool->lock) {
        if (pool->pool_name) AGENTOS_FREE(pool->pool_name);
        AGENTOS_FREE(pool->base_address);
        AGENTOS_FREE(pool);
        return NULL;
    }

    memory_block_header_t* initial_block = (memory_block_header_t*)pool->base_address;
    initial_block->size = size - sizeof(memory_block_header_t);
    initial_block->magic = 0xDEADBEEF;
    initial_block->in_use = 0;
    initial_block->pool_id = pool_id;
    initial_block->flags = 0;
    initial_block->ref_count = 0;
    initial_block->alloc_time_ns = 0;
    initial_block->last_access_ns = 0;
    initial_block->next = NULL;
    initial_block->prev = NULL;

    pool->free_list = initial_block;
    pool->used_list = NULL;

    return pool;
}

void memory_pool_destroy(memory_pool_t* pool) {
    if (!pool) return;
    if (pool->lock) agentos_mutex_destroy(pool->lock);
    if (pool->pool_name) AGENTOS_FREE(pool->pool_name);
    if (pool->base_address) AGENTOS_FREE(pool->base_address);
    AGENTOS_FREE(pool);
}

void* memory_pool_alloc(memory_pool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;

    size_t aligned_size = align_size(size + sizeof(memory_block_header_t));
    agentos_mutex_lock(pool->lock);

    memory_block_header_t* block = pool->free_list;
    memory_block_header_t* prev = NULL;

    while (block) {
        if (block->size >= aligned_size) {
            break;
        }
        prev = block;
        block = block->next;
    }

    if (!block) {
        pool->alloc_failures++;
        agentos_mutex_unlock(pool->lock);
        AGENTOS_LOG_WARN("Memory pool %s allocation failed: size=%zu",
                         pool->pool_name, size);
        return NULL;
    }

    if (prev) {
        prev->next = block->next;
    } else {
        pool->free_list = block->next;
    }

    if (block->size > aligned_size + sizeof(memory_block_header_t) + MIN_ALLOCATION_SIZE) {
        memory_block_header_t* new_block = (memory_block_header_t*)((uint8_t*)block + aligned_size);
        new_block->size = block->size - aligned_size;
        new_block->magic = 0xDEADBEEF;
        new_block->in_use = 0;
        new_block->pool_id = pool->pool_id;
        new_block->flags = 0;
        new_block->ref_count = 0;
        new_block->alloc_time_ns = 0;
        new_block->last_access_ns = 0;
        new_block->next = pool->free_list;
        new_block->prev = NULL;
        if (pool->free_list) {
            pool->free_list->prev = new_block;
        }
        pool->free_list = new_block;
        block->size = aligned_size;
    }

    block->in_use = 1;
    block->ref_count = 1;
    block->alloc_time_ns = get_timestamp_ns();
    block->last_access_ns = block->alloc_time_ns;

    block->next = pool->used_list;
    block->prev = NULL;
    if (pool->used_list) {
        pool->used_list->prev = block;
    }
    pool->used_list = block;

    pool->used_size += block->size;
    pool->alloc_count++;
    if (pool->used_size > pool->peak_size) {
        pool->peak_size = pool->used_size;
    }

    agentos_mutex_unlock(pool->lock);
    return (void*)((uint8_t*)block + sizeof(memory_block_header_t));
}

void memory_pool_free(memory_pool_t* pool, void* ptr) {
    if (!pool || !ptr) return;

    memory_block_header_t* block = (memory_block_header_t*)((uint8_t*)ptr - sizeof(memory_block_header_t));
    if (block->magic != 0xDEADBEEF) {
        AGENTOS_LOG_ERROR("Invalid memory block magic: %p", ptr);
        return;
    }

    agentos_mutex_lock(pool->lock);

    if (block->ref_count > 1) {
        block->ref_count--;
        agentos_mutex_unlock(pool->lock);
        return;
    }

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->used_list = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }

    block->in_use = 0;
    block->ref_count = 0;
    block->last_access_ns = get_timestamp_ns();

    block->next = pool->free_list;
    block->prev = NULL;
    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;

    pool->used_size -= block->size;
    pool->free_count++;

    agentos_mutex_unlock(pool->lock);
}

uint64_t memory_pool_get_used_size(memory_pool_t* pool) {
    if (!pool) return 0;
    return pool->used_size;
}

uint64_t memory_pool_get_peak_size(memory_pool_t* pool) {
    if (!pool) return 0;
    return pool->peak_size;
}

double memory_pool_get_fragmentation_ratio(memory_pool_t* pool) {
    if (!pool) return 0.0;
    return calculate_fragmentation_ratio(pool);
}

uint64_t memory_pool_get_alloc_count(memory_pool_t* pool) {
    if (!pool) return 0;
    return pool->alloc_count;
}

uint64_t memory_pool_get_free_count(memory_pool_t* pool) {
    if (!pool) return 0;
    return pool->free_count;
}
