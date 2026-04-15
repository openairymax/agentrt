#include "include/memory_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define malloc_usable_size(ptr) _msize(ptr)
#else
#include <malloc.h>
#endif

static memory_stats_t g_memory_stats = {
    .total_allocated = 0,
    .total_freed = 0,
    .current_allocated = 0,
    .peak_allocated = 0,
    .allocation_count = 0,
    .free_count = 0,
    .leak_count = 0
};

static memory_strategy_t g_memory_strategy = MEMORY_STRATEGY_DEFAULT;

memory_pool_config_t memory_create_default_pool_config(void) {
    memory_pool_config_t manager = {
        .block_size = 128,
        .block_count = 1024,
        .strategy = MEMORY_STRATEGY_DEFAULT,
        .thread_safe = true
    };
    return manager;
}

agentos_error_t memory_pool_init(memory_pool_t* pool, const memory_pool_config_t* manager) {
    if (!pool) {
        return AGENTOS_EINVAL;
    }
    
    memset(pool, 0, sizeof(memory_pool_t));
    
    if (manager) {
        pool->manager = *manager;
    } else {
        pool->manager = memory_create_default_pool_config();
    }
    
    size_t pool_size = pool->manager.block_size * pool->manager.block_count;
    pool->pool = AGENTOS_MALLOC(pool_size);
    if (!pool->pool) {
        return AGENTOS_ENOMEM;
    }
    
    memset(pool->pool, 0, pool_size);
    pool->used_blocks = 0;
    pool->peak_usage = 0;
    
    return AGENTOS_SUCCESS;
}

void* memory_pool_alloc(memory_pool_t* pool, size_t size) {
    if (!pool || !pool->pool) {
        return NULL;
    }
    
    if (size > pool->manager.block_size || pool->used_blocks >= pool->manager.block_count) {
        void* ptr = AGENTOS_MALLOC(size);
        if (ptr) {
            g_memory_stats.total_allocated += size;
            g_memory_stats.current_allocated += size;
            g_memory_stats.allocation_count++;
            if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated) {
                g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
            }
        }
        return ptr;
    }
    
    size_t offset = pool->used_blocks * pool->manager.block_size;
    void* ptr = (char*)pool->pool + offset;
    pool->used_blocks++;
    
    if (pool->used_blocks > pool->peak_usage) {
        pool->peak_usage = pool->used_blocks;
    }
    
    return ptr;
}

void memory_pool_free(memory_pool_t* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    if (ptr >= pool->pool && ptr < (char*)pool->pool + pool->manager.block_size * pool->manager.block_count) {
        pool->used_blocks--;
    } else {
        size_t size = malloc_usable_size(ptr);
        AGENTOS_FREE(ptr);
        
        g_memory_stats.total_freed += size;
        g_memory_stats.current_allocated -= size;
        g_memory_stats.free_count++;
    }
}

void memory_pool_cleanup(memory_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    if (pool->pool) {
        AGENTOS_FREE(pool->pool);
        pool->pool = NULL;
    }
    
    pool->used_blocks = 0;
    pool->peak_usage = 0;
}

void memory_pool_get_stats(const memory_pool_t* pool, memory_stats_t* stats) {
    if (!pool || !stats) {
        return;
    }
    
    stats->total_allocated = pool->manager.block_size * pool->used_blocks;
    stats->total_freed = 0;
    stats->current_allocated = pool->manager.block_size * pool->used_blocks;
    stats->peak_allocated = pool->manager.block_size * pool->peak_usage;
    stats->allocation_count = pool->used_blocks;
    stats->free_count = 0;
    stats->leak_count = 0;
}

void* memory_safe_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void* ptr = AGENTOS_MALLOC(size);
    if (ptr) {
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_allocated += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
        }
    }
    
    return ptr;
}

void* memory_safe_realloc(void* ptr, size_t size) {
    if (size == 0) {
        memory_safe_free(ptr);
        return NULL;
    }
    
    void* new_ptr = AGENTOS_REALLOC(ptr, size);
    if (new_ptr) {
        if (ptr) {
            size_t old_size = malloc_usable_size(ptr);
            g_memory_stats.total_freed += old_size;
            g_memory_stats.current_allocated -= old_size;
        }
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_allocated += size;
        g_memory_stats.allocation_count++;
        g_memory_stats.free_count++;
        if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
        }
    }
    
    return new_ptr;
}

void memory_safe_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    size_t size = malloc_usable_size(ptr);
    AGENTOS_FREE(ptr);
    
    g_memory_stats.total_freed += size;
    g_memory_stats.current_allocated -= size;
    g_memory_stats.free_count++;
}

char* memory_safe_strdup(const char* src) {
    if (!src) {
        return NULL;
    }
    
    size_t len = strlen(src) + 1;
    char* dest = memory_safe_alloc(len);
    if (dest) {
        memcpy(dest, src, len);
    }
    
    return dest;
}

void memory_get_global_stats(memory_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    *stats = g_memory_stats;
}

void memory_reset_global_stats(void) {
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void memory_set_strategy(memory_strategy_t strategy) {
    g_memory_strategy = strategy;
}

memory_strategy_t memory_get_strategy(void) {
    return g_memory_strategy;
}
