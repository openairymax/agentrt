#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include "agentos_memory.h"

#include <error.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef AGENTOS_MEMORY_STATS_T_DEFINED
#define AGENTOS_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;
} memory_stats_t;
#endif

typedef enum {
    MEMORY_STRATEGY_DEFAULT = 0,
    MEMORY_STRATEGY_PERFORMANCE,
    MEMORY_STRATEGY_SAFETY,
    MEMORY_STRATEGY_LOW_LATENCY
} memory_strategy_t;

/* 注意：旧版 memory_pool_t / memory_pool_config_t / memory_pool_init /
 *       memory_pool_alloc 等已移除，与 memory_pool.h 中的新版实现冲突。
 *       新版内存池 API 详见 memory_pool.h（memory_pool_create / memory_pool_alloc 等）。
 *       保留的 memory_pool_t 定义在 memory_pool.h 中（不透明指针 struct memory_pool *）。
 */

void *memory_safe_alloc(size_t size);

void *memory_safe_realloc(void *ptr, size_t size);

void memory_safe_free(void *ptr);

char *memory_safe_strdup(const char *src);

void memory_get_global_stats(memory_stats_t *stats);

void memory_reset_global_stats(void);

void memory_set_strategy(memory_strategy_t strategy);

memory_strategy_t memory_get_strategy(void);

#endif
