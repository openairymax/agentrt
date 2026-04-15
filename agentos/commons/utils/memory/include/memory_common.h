#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include <error.h>
#include <agentos/memory.h>

typedef enum {
    MEMORY_STRATEGY_DEFAULT = 0,
    MEMORY_STRATEGY_PERFORMANCE,
    MEMORY_STRATEGY_SAFETY,
    MEMORY_STRATEGY_LOW_LATENCY
} memory_strategy_t;

typedef struct {
    size_t block_size;
    size_t block_count;
    memory_strategy_t strategy;
    bool thread_safe;
} memory_pool_config_t;

typedef struct {
    void* pool;
    memory_pool_config_t manager;
    size_t used_blocks;
    size_t peak_usage;
} memory_pool_t;

memory_pool_config_t memory_create_default_pool_config(void);

agentos_error_t memory_pool_init(memory_pool_t* pool, const memory_pool_config_t* manager);

void* memory_pool_alloc(memory_pool_t* pool, size_t size);

void memory_pool_free(memory_pool_t* pool, void* ptr);

void memory_pool_cleanup(memory_pool_t* pool);

void memory_pool_get_stats(const memory_pool_t* pool, memory_stats_t* stats);

void* memory_safe_alloc(size_t size);

void* memory_safe_realloc(void* ptr, size_t size);

void memory_safe_free(void* ptr);

char* memory_safe_strdup(const char* src);

void memory_get_global_stats(memory_stats_t* stats);

void memory_reset_global_stats(void);

void memory_set_strategy(memory_strategy_t strategy);

memory_strategy_t memory_get_strategy(void);

#endif
