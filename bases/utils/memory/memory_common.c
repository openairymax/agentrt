#include "include/memory_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 全局内存统计信息
 */
static memory_stats_t g_memory_stats = {
    .total_allocated = 0,
    .total_freed = 0,
    .current_usage = 0,
    .peak_usage = 0,
    .allocation_count = 0,
    .free_count = 0
};

/**
 * @brief 当前内存分配策略
 */
static memory_strategy_t g_memory_strategy = MEMORY_STRATEGY_DEFAULT;

/**
 * @brief 创建默认内存池配置
 * @return 默认内存池配置
 */
memory_pool_config_t memory_create_default_pool_config(void) {
    memory_pool_config_t manager = {
        .block_size = 128,
        .block_count = 1024,
        .strategy = MEMORY_STRATEGY_DEFAULT,
        .thread_safe = true
    };
    return manager;
}

/**
 * @brief 初始化内存池
 * @param pool 内存池结构体
 * @param manager 内存池配置
 * @return 错误码
 */
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
    
    // 分配内存池
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

/**
 * @brief 从内存池分配内存
 * @param pool 内存池
 * @param size 分配大小
 * @return 分配的内存指针
 */
void* memory_pool_alloc(memory_pool_t* pool, size_t size) {
    if (!pool || !pool->pool) {
        return NULL;
    }
    
    // 简单的内存池分配实现
    // 实际项目中应该使用更复杂的内存管理算法
    if (size > pool->manager.block_size || pool->used_blocks >= pool->manager.block_count) {
        // 内存池不足，使用标准分配
        void* ptr = AGENTOS_MALLOC(size);
        if (ptr) {
            // 更新统计信息
            g_memory_stats.total_allocated += size;
            g_memory_stats.current_usage += size;
            g_memory_stats.allocation_count++;
            if (g_memory_stats.current_usage > g_memory_stats.peak_usage) {
                g_memory_stats.peak_usage = g_memory_stats.current_usage;
            }
        }
        return ptr;
    }
    
    // 从内存池分配
    size_t offset = pool->used_blocks * pool->manager.block_size;
    void* ptr = (char*)pool->pool + offset;
    pool->used_blocks++;
    
    if (pool->used_blocks > pool->peak_usage) {
        pool->peak_usage = pool->used_blocks;
    }
    
    return ptr;
}

/**
 * @brief 释放内存池中的内存
 * @param pool 内存池
 * @param ptr 内存指针
 */
void memory_pool_free(memory_pool_t* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    // 检查是否是内存池中的内存
    if (ptr >= pool->pool && ptr < (char*)pool->pool + pool->manager.block_size * pool->manager.block_count) {
        // 简单的内存池释放实现
        // 实际项目中应该使用更复杂的内存管理算法
        pool->used_blocks--;
    } else {
        // 释放标准分配的内存
        size_t size = malloc_usable_size(ptr);
        AGENTOS_FREE(ptr);
        
        // 更新统计信息
        g_memory_stats.total_freed += size;
        g_memory_stats.current_usage -= size;
        g_memory_stats.free_count++;
    }
}

/**
 * @brief 清理内存池
 * @param pool 内存池
 */
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

/**
 * @brief 获取内存池统计信息
 * @param pool 内存池
 * @param stats 统计信息输出
 */
void memory_pool_get_stats(const memory_pool_t* pool, memory_stats_t* stats) {
    if (!pool || !stats) {
        return;
    }
    
    stats->total_allocated = pool->manager.block_size * pool->used_blocks;
    stats->total_freed = 0;
    stats->current_usage = pool->manager.block_size * pool->used_blocks;
    stats->peak_usage = pool->manager.block_size * pool->peak_usage;
    stats->allocation_count = pool->used_blocks;
    stats->free_count = 0;
}

/**
 * @brief 安全内存分配
 * @param size 分配大小
 * @return 分配的内存指针，失败返回NULL
 */
void* memory_safe_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void* ptr = AGENTOS_MALLOC(size);
    if (ptr) {
        // 更新统计信息
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_usage += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_usage > g_memory_stats.peak_usage) {
            g_memory_stats.peak_usage = g_memory_stats.current_usage;
        }
    }
    
    return ptr;
}

/**
 * @brief 安全内存重分配
 * @param ptr 原内存指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回NULL
 */
void* memory_safe_realloc(void* ptr, size_t size) {
    if (size == 0) {
        memory_safe_free(ptr);
        return NULL;
    }
    
    void* new_ptr = AGENTOS_REALLOC(ptr, size);
    if (new_ptr) {
        // 更新统计信息
        if (ptr) {
            size_t old_size = malloc_usable_size(ptr);
            g_memory_stats.total_freed += old_size;
            g_memory_stats.current_usage -= old_size;
        }
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_usage += size;
        g_memory_stats.allocation_count++;
        g_memory_stats.free_count++;
        if (g_memory_stats.current_usage > g_memory_stats.peak_usage) {
            g_memory_stats.peak_usage = g_memory_stats.current_usage;
        }
    }
    
    return new_ptr;
}

/**
 * @brief 安全内存释放
 * @param ptr 内存指针
 */
void memory_safe_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    size_t size = malloc_usable_size(ptr);
    AGENTOS_FREE(ptr);
    
    // 更新统计信息
    g_memory_stats.total_freed += size;
    g_memory_stats.current_usage -= size;
    g_memory_stats.free_count++;
}

/**
 * @brief 安全字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 目标缓冲区指针
 */
char* memory_safe_strdup(const char* src) {
    if (!src) {
        return NULL;
    }
    
    size_t len = strlen(src) + 1;
    char* dest = memory_safe_alloc(len);
    if (dest) {
        strcpy(dest, src);
    }
    
    return dest;
}

/**
 * @brief 获取全局内存统计信息
 * @param stats 统计信息输出
 */
void memory_get_global_stats(memory_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    *stats = g_memory_stats;
}

/**
 * @brief 重置全局内存统计信息
 */
void memory_reset_global_stats(void) {
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
}

/**
 * @brief 设置内存分配策略
 * @param strategy 内存分配策略
 */
void memory_set_strategy(memory_strategy_t strategy) {
    g_memory_strategy = strategy;
}

/**
 * @brief 获取当前内存分配策略
 * @return 当前内存分配策略
 */
memory_strategy_t memory_get_strategy(void) {
    return g_memory_strategy;
}
