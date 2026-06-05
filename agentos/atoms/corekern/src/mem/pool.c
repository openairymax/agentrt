/**
 * @file pool.c
 * @brief 内存池分配器（含双重释放检测与指针归属验证）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)


#define POOL_MAGIC 0x504F4F4C
#define BLOCK_ALLOCATED 0xA110CA7E
#define BLOCK_FREED 0xDEADBEEF

typedef struct pool_block {
    struct pool_block *next;
} pool_block_t;

struct agentos_mem_pool {
    uint32_t magic;
    pool_block_t *free_list;
    void *raw_memory;
    size_t block_size;
    size_t actual_block_size;
    uint32_t block_count;
    uint32_t free_count;
    uint32_t *block_tags;
    agentos_mutex_t *lock;
};

static inline int32_t pool_block_index(agentos_mem_pool_t *pool, void *ptr)
{
    uint8_t *base = (uint8_t *)pool->raw_memory;
    uint8_t *block = (uint8_t *)ptr;
    if (block < base)
        ATM_RET_ERR(AGENTOS_EINVAL);
    size_t offset = (size_t)(block - base);
    if (offset % pool->actual_block_size != 0)
        ATM_RET_ERR(AGENTOS_EINVAL);
    uint32_t index = (uint32_t)(offset / pool->actual_block_size);
    if (index >= pool->block_count)
        ATM_RET_ERR(AGENTOS_EINVAL);
    return (int32_t)index;
}

agentos_mem_pool_t *agentos_mem_pool_create(size_t block_size, uint32_t block_count)
{
    if (block_size < sizeof(void *) || block_count == 0) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
        return NULL;
    }

    if (block_size > SIZE_MAX - 7) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
        return NULL;
    }
    size_t actual_block_size = (block_size + 7) & ~(size_t)7;

    if (block_count > SIZE_MAX / actual_block_size) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
        return NULL;
    }
    size_t total_size = actual_block_size * block_count;

    void *raw = agentos_mem_aligned_alloc(total_size, 8);
    if (!raw) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    agentos_mem_pool_t *pool =
        (agentos_mem_pool_t *)AGENTOS_CALLOC(1, sizeof(struct agentos_mem_pool));
    if (!pool) {
        agentos_mem_aligned_free(raw);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    pool->block_tags = (uint32_t *)AGENTOS_CALLOC(block_count, sizeof(uint32_t));
    if (!pool->block_tags) {
        AGENTOS_FREE(pool);
        agentos_mem_aligned_free(raw);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    pool->magic = POOL_MAGIC;
    pool->block_size = block_size;
    pool->actual_block_size = actual_block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    pool->raw_memory = raw;
    pool->lock = agentos_mutex_create();

    uint8_t *blocks = (uint8_t *)raw;
    pool->free_list = NULL;
    for (uint32_t i = block_count; i > 0; i--) {
        uint32_t idx = i - 1;
        pool_block_t *block = (pool_block_t *)(blocks + idx * actual_block_size);
        block->next = pool->free_list;
        pool->free_list = block;
        pool->block_tags[idx] = BLOCK_FREED;
    }

    return pool;
}

void *agentos_mem_pool_alloc(agentos_mem_pool_t *pool_handle)
{
    if (!pool_handle) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    agentos_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    if (pool->lock) {
        agentos_mutex_lock(pool->lock);
    }

    if (!pool->free_list) {
        if (pool->lock) {
            agentos_mutex_unlock(pool->lock);
        }
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    pool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->free_count--;

    int32_t idx = pool_block_index(pool, block);
    if (idx >= 0) {
        pool->block_tags[idx] = BLOCK_ALLOCATED;
    }

    AGENTOS_MEMSET(block, 0, pool->block_size);

    if (pool->lock) {
        agentos_mutex_unlock(pool->lock);
    }

    return block;
}

agentos_error_t agentos_mem_pool_free(agentos_mem_pool_t *pool_handle, void *ptr)
{
    if (!pool_handle || !ptr)
        ATM_RET_ERR(AGENTOS_EINVAL);
    agentos_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC)
        ATM_RET_ERR(AGENTOS_EINVAL);

    if (pool->lock) {
        agentos_mutex_lock(pool->lock);
    }

    int32_t idx = pool_block_index(pool, ptr);
    if (idx < 0) {
        if (pool->lock) {
            agentos_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    if (pool->block_tags[idx] == BLOCK_FREED) {
        if (pool->lock) {
            agentos_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTOS_EALREADY);
    }

    if (pool->block_tags[idx] != BLOCK_ALLOCATED) {
        if (pool->lock) {
            agentos_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    pool->block_tags[idx] = BLOCK_FREED;

    pool_block_t *block = (pool_block_t *)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_count++;

    if (pool->lock) {
        agentos_mutex_unlock(pool->lock);
    }

    return AGENTOS_SUCCESS;
}

void agentos_mem_pool_destroy(agentos_mem_pool_t *pool_handle)
{
    if (!pool_handle)
        return;
    agentos_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC)
        return;

    pool->magic = 0;

    if (pool->block_tags) {
        AGENTOS_FREE(pool->block_tags);
        pool->block_tags = NULL;
    }

    if (pool->raw_memory) {
        agentos_mem_aligned_free(pool->raw_memory);
        pool->raw_memory = NULL;
    }

    AGENTOS_FREE(pool);
}
