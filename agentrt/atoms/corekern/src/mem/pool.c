/**
 * @file pool.c
 * @brief 内存池分配器（含双重释放检测与指针归属验证）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


#define POOL_MAGIC 0x504F4F4C
#define BLOCK_ALLOCATED 0xA110CA7E
#define BLOCK_FREED 0xDEADBEEF

typedef struct pool_block {
    struct pool_block *next;
} pool_block_t;

struct agentrt_mem_pool {
    uint32_t magic;
    pool_block_t *free_list;
    void *raw_memory;
    size_t block_size;
    size_t actual_block_size;
    uint32_t block_count;
    uint32_t free_count;
    uint32_t *block_tags;
    agentrt_mutex_t *lock;
};

static inline int32_t pool_block_index(agentrt_mem_pool_t *pool, void *ptr)
{
    uint8_t *base = (uint8_t *)pool->raw_memory;
    uint8_t *block = (uint8_t *)ptr;
    if (block < base)
        ATM_RET_ERR(AGENTRT_EINVAL);
    size_t offset = (size_t)(block - base);
    if (offset % pool->actual_block_size != 0)
        ATM_RET_ERR(AGENTRT_EINVAL);
    uint32_t index = (uint32_t)(offset / pool->actual_block_size);
    if (index >= pool->block_count)
        ATM_RET_ERR(AGENTRT_EINVAL);
    return (int32_t)index;
}

agentrt_mem_pool_t *agentrt_mem_pool_create(size_t block_size, uint32_t block_count)
{
    if (block_size < sizeof(void *) || block_count == 0) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    if (block_size > SIZE_MAX - 7) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }
    size_t actual_block_size = (block_size + 7) & ~(size_t)7;

    if (block_count > SIZE_MAX / actual_block_size) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }
    size_t total_size = actual_block_size * block_count;

    void *raw = agentrt_mem_aligned_alloc(total_size, 8);
    if (!raw) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_mem_pool_t *pool =
        (agentrt_mem_pool_t *)AGENTRT_CALLOC(1, sizeof(struct agentrt_mem_pool));
    if (!pool) {
        agentrt_mem_aligned_free(raw);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    pool->block_tags = (uint32_t *)AGENTRT_CALLOC(block_count, sizeof(uint32_t));
    if (!pool->block_tags) {
        AGENTRT_FREE(pool);
        agentrt_mem_aligned_free(raw);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    pool->magic = POOL_MAGIC;
    pool->block_size = block_size;
    pool->actual_block_size = actual_block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    pool->raw_memory = raw;
    pool->lock = agentrt_mutex_create();

    uint8_t *blocks = (uint8_t *)raw;
    pool->free_list = NULL;
    for (uint32_t i = block_count; i > 0; i--) {
        uint32_t idx = i - 1;
        pool_block_t *block = (pool_block_t *)(blocks + idx * actual_block_size);
        block->next = pool->free_list;
        pool->free_list = block;
        pool->block_tags[idx] = BLOCK_FREED;
    }

    AGENTRT_LOG_DEBUG("P3.15: Pool: CREATE OK (block_size=%zu, block_count=%u, total=%zu)",
                      block_size, block_count, total_size);

    return pool;
}

void *agentrt_mem_pool_alloc(agentrt_mem_pool_t *pool_handle)
{
    if (!pool_handle) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    agentrt_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    if (pool->lock) {
        agentrt_mutex_lock(pool->lock);
    }

    if (!pool->free_list) {
        if (pool->lock) {
            agentrt_mutex_unlock(pool->lock);
        }
        AGENTRT_LOG_WARN("P3.15: Pool: EXHAUSTED (block_size=%zu, block_count=%u)",
                         pool->block_size, pool->block_count);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    pool_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->free_count--;

    int32_t idx = pool_block_index(pool, block);
    if (idx >= 0) {
        pool->block_tags[idx] = BLOCK_ALLOCATED;
    }

    __builtin_memset(block, 0, pool->block_size);

    if (pool->lock) {
        agentrt_mutex_unlock(pool->lock);
    }

    return block;
}

agentrt_error_t agentrt_mem_pool_free(agentrt_mem_pool_t *pool_handle, void *ptr)
{
    if (!pool_handle || !ptr)
        ATM_RET_ERR(AGENTRT_EINVAL);
    agentrt_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC)
        ATM_RET_ERR(AGENTRT_EINVAL);

    if (pool->lock) {
        agentrt_mutex_lock(pool->lock);
    }

    int32_t idx = pool_block_index(pool, ptr);
    if (idx < 0) {
        if (pool->lock) {
            agentrt_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (pool->block_tags[idx] == BLOCK_FREED) {
        if (pool->lock) {
            agentrt_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTRT_EALREADY);
    }

    if (pool->block_tags[idx] != BLOCK_ALLOCATED) {
        if (pool->lock) {
            agentrt_mutex_unlock(pool->lock);
        }
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    pool->block_tags[idx] = BLOCK_FREED;

    pool_block_t *block = (pool_block_t *)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_count++;

    if (pool->lock) {
        agentrt_mutex_unlock(pool->lock);
    }

    return AGENTRT_SUCCESS;
}

void agentrt_mem_pool_destroy(agentrt_mem_pool_t *pool_handle)
{
    if (!pool_handle)
        return;
    agentrt_mem_pool_t *pool = pool_handle;
    if (pool->magic != POOL_MAGIC)
        return;

    AGENTRT_LOG_DEBUG("P3.15: Pool: DESTROY (block_size=%zu, block_count=%u, free_count=%u)",
                      pool->block_size, pool->block_count, pool->free_count);

    pool->magic = 0;

    if (pool->block_tags) {
        AGENTRT_FREE(pool->block_tags);
        pool->block_tags = NULL;
    }

    if (pool->lock) {
        agentrt_mutex_free(pool->lock);
        pool->lock = NULL;
    }

    if (pool->raw_memory) {
        agentrt_mem_aligned_free(pool->raw_memory);
        pool->raw_memory = NULL;
    }

    AGENTRT_FREE(pool);
}
