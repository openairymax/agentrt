#include "memory_common.h"

#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define malloc_usable_size(ptr) _msize(ptr)
#else
#include <malloc.h>
#endif

static memory_stats_t g_memory_stats = {.total_allocated = 0,
                                        .total_freed = 0,
                                        .current_allocated = 0,
                                        .peak_allocated = 0,
                                        .allocation_count = 0,
                                        .free_count = 0,
                                        .leak_count = 0};

static memory_strategy_t g_memory_strategy = MEMORY_STRATEGY_DEFAULT;

/* 注意：旧版 memory_create_default_pool_config / memory_pool_init /
 *       memory_pool_alloc / memory_pool_free / memory_pool_cleanup /
 *       memory_pool_get_stats 已移除，与 memory_pool.c 中的新版实现冲突。
 *       新版内存池 API 详见 memory_pool.h。
 */

void *memory_safe_alloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = AGENTRT_MALLOC(size);
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

void *memory_safe_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        memory_safe_free(ptr);
        return NULL;
    }

    size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
    void *new_ptr = AGENTRT_REALLOC(ptr, size);
    if (new_ptr) {
        if (old_size > 0) {
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

void memory_safe_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    size_t size = malloc_usable_size(ptr);
    AGENTRT_FREE(ptr);

    g_memory_stats.total_freed += size;
    g_memory_stats.current_allocated -= size;
    g_memory_stats.free_count++;
}

char *memory_safe_strdup(const char *src)
{
    if (!src) {
        return NULL;
    }

    size_t len = strlen(src) + 1;
    char *dest = memory_safe_alloc(len);
    if (dest) {
        __builtin_memcpy(dest, src, len);
    }

    return dest;
}

void memory_get_global_stats(memory_stats_t *stats)
{
    if (!stats) {
        return;
    }

    *stats = g_memory_stats;
}

void memory_reset_global_stats(void)
{
    AGENTRT_MEMSET(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void memory_set_strategy(memory_strategy_t strategy)
{
    g_memory_strategy = strategy;
}

memory_strategy_t memory_get_strategy(void)
{
    return g_memory_strategy;
}
