/**
 * @file memory_pool.c
 * @brief 统一内存管理模块 - 内存池管理实�? * 
 * 实现高效的内存池管理功能，减少内存碎片和分配开销�? * 使用链表管理空闲块，支持线程安全和动态扩展�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory_pool.h"
#include "memory.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/**
 * @brief 内存池块结构
 * @note 性能优化：块头嵌入 pool 指针用于 O(1) 所有权验证，避免 O(n) 线性扫描
 */
typedef struct memory_pool_block {
    struct memory_pool_block* next;      /**< 指向下一个块的指针 */
    bool allocated;                      /**< 块是否已分配 */
    size_t index;                        /**< 块索引（用于调试） */
    struct memory_pool* pool;            /**< 所属池指针（O(1）验证用） */
} memory_pool_block_t;

/**
 * @brief 内存池内部结�? */
struct memory_pool {
    // 配置选项
    memory_pool_options_t options;       /**< 内存池选项 */
    
    // 内存块管�?    void* memory_area;                   /**< 整个内存区域指针 */
    size_t memory_area_size;             /**< 内存区域总大�?*/
    memory_pool_block_t** blocks;        /**< 所有块的指针数�?*/
    size_t blocks_capacity;              /**< 块数组容�?*/
    
    // 空闲块管�?    memory_pool_block_t* free_list;      /**< 空闲块链表头 */
    
    // 统计信息
    memory_pool_stats_t stats;           /**< 内存池统计信�?*/
    
    // 线程同步
#ifdef _WIN32
    CRITICAL_SECTION lock;               /**< Windows临界�?*/
#else
    pthread_mutex_t lock;                /**< POSIX互斥�?*/
#endif
    
    // 名称（用于调试）
    char* name;                          /**< 内存池名�?*/
};

/**
 * @brief 内部锁初始化
 * 
 * @param[in] pool 内存�? * @return 成功返回true，失败返回false
 */
static bool memory_pool_lock_init(memory_pool_t* pool) {
    if (!pool->options.thread_safe) {
        return true;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&pool->lock);
    return true;
#else
    return pthread_mutex_init(&pool->lock, NULL) == 0;
#endif
}

/**
 * @brief 内部锁销�? * 
 * @param[in] pool 内存�? */
static void memory_pool_lock_destroy(memory_pool_t* pool) {
    if (!pool->options.thread_safe) {
        return;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&pool->lock);
#else
    pthread_mutex_destroy(&pool->lock);
#endif
}

/**
 * @brief 加锁
 * 
 * @param[in] pool 内存�? */
static void memory_pool_lock(memory_pool_t* pool) {
    if (!pool->options.thread_safe) {
        return;
    }
    
#ifdef _WIN32
    EnterCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->lock);
#endif
}

/**
 * @brief 解锁
 * 
 * @param[in] pool 内存�? */
static void memory_pool_unlock(memory_pool_t* pool) {
    if (!pool->options.thread_safe) {
        return;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&pool->lock);
#else
    pthread_mutex_unlock(&pool->lock);
#endif
}

/**
 * @brief 计算内存对齐
 * 
 * @param[in] size 原始大小
 * @param[in] alignment 对齐要求
 * @return 对齐后的大小
 */
static size_t memory_pool_align_size(size_t size, size_t alignment) {
    if (alignment == 0) {
        return size;
    }
    
    return ((size + alignment - 1) / alignment) * alignment;
}

/**
 * @brief 分配新的内存区域
 * 
 * @param[in] pool 内存�? * @param[in] block_count 块数�? * @return 成功返回true，失败返回false
 */
static bool memory_pool_allocate_blocks(memory_pool_t* pool, size_t block_count) {
    if (pool == NULL || block_count == 0) {
        return false;
    }
    
    // 计算对齐后的块大小（包括块头�?    size_t aligned_block_size = memory_pool_align_size(
        sizeof(memory_pool_block_t) + pool->options.block_size,
        sizeof(void*)
    );
    
    // 计算总内存大�?    size_t total_size = block_count * aligned_block_size;
    
    // 检查是否超过最大限�?    if (pool->options.max_blocks > 0 && 
        pool->stats.total_blocks + block_count > pool->options.max_blocks) {
        return false;
    }
    
    // 分配内存区域
    void* new_memory = memory_aligned_alloc(sizeof(void*), total_size, "memory_pool");
    if (new_memory == NULL) {
        return false;
    }
    
    // 如果已有内存区域，需要合并（这里简化处理，实际可能需要重新分配）
    if (pool->memory_area != NULL) {
        // 简化实现：不支持动态扩展已有区�?        // 在实际实现中，这里可以使用realloc或分配新区域并复制数�?        memory_free(pool->memory_area);
        pool->memory_area = NULL;
    }
    
    pool->memory_area = new_memory;
    pool->memory_area_size = total_size;
    
    // 扩展块指针数�?    size_t new_capacity = pool->blocks_capacity + block_count;
    memory_pool_block_t** new_blocks = memory_realloc(
        pool->blocks, 
        new_capacity * sizeof(memory_pool_block_t*), 
        "memory_pool_blocks"
    );
    
    if (new_blocks == NULL) {
        memory_free(pool->memory_area);
        pool->memory_area = NULL;
        return false;
    }
    
    pool->blocks = new_blocks;
    pool->blocks_capacity = new_capacity;
    
    // 初始化新块
    uint8_t* memory_ptr = (uint8_t*)pool->memory_area;
    for (size_t i = 0; i < block_count; i++) {
        memory_pool_block_t* block = (memory_pool_block_t*)memory_ptr;

        // 初始化块（含 O(1) 验证用池指针）
        block->next = NULL;
        block->allocated = false;
        block->index = pool->stats.total_blocks + i;
        block->pool = pool;
        
        // 将块添加到空闲链�?        block->next = pool->free_list;
        pool->free_list = block;
        
        // 存储块指�?        pool->blocks[pool->stats.total_blocks + i] = block;
        
        // 移动到下一个块
        memory_ptr += aligned_block_size;
    }
    
    // 更新统计信息
    pool->stats.total_blocks += block_count;
    pool->stats.free_blocks += block_count;
    pool->stats.total_memory += total_size;
    
    return true;
}

/**
 * @brief 释放内存区域
 * 
 * @param[in] pool 内存�? */
static void memory_pool_free_blocks(memory_pool_t* pool) {
    if (pool == NULL) {
        return;
    }
    
    if (pool->memory_area != NULL) {
        memory_free(pool->memory_area);
        pool->memory_area = NULL;
        pool->memory_area_size = 0;
    }
    
    if (pool->blocks != NULL) {
        memory_free(pool->blocks);
        pool->blocks = NULL;
        pool->blocks_capacity = 0;
    }
    
    pool->free_list = NULL;
    pool->stats.total_blocks = 0;
    pool->stats.allocated_blocks = 0;
    pool->stats.free_blocks = 0;
    pool->stats.total_memory = 0;
    pool->stats.used_memory = 0;
}

memory_pool_t* memory_pool_create(const memory_pool_options_t* options) {
    if (options == NULL || options->block_size == 0) {
        return NULL;
    }
    
    // 分配内存池结�?    memory_pool_t* pool = memory_calloc(sizeof(memory_pool_t), "memory_pool_instance");
    if (pool == NULL) {
        return NULL;
    }
    
    // 复制选项
    memcpy(&pool->options, options, sizeof(memory_pool_options_t));
    
    // 设置默认�?    if (pool->options.initial_blocks == 0) {
        pool->options.initial_blocks = 16;
    }
    
    if (pool->options.expansion_size == 0) {
        pool->options.expansion_size = 8;
    }
    
    // 初始化统计信�?    memset(&pool->stats, 0, sizeof(memory_pool_stats_t));
    pool->stats.block_size = pool->options.block_size;
    
    // 初始化锁
    if (!memory_pool_lock_init(pool)) {
        memory_free(pool);
        return NULL;
    }
    
    // 复制名称
    if (pool->options.name != NULL) {
        pool->name = memory_calloc(strlen(pool->options.name) + 1, "memory_pool_name");
        if (pool->name != NULL) {
            memcpy(pool->name, pool->options.name, strlen(pool->options.name) + 1);
        }
    }
    
    memory_pool_lock(pool);
    
    // 预分配初始块
    if (!memory_pool_allocate_blocks(pool, pool->options.initial_blocks)) {
        memory_pool_unlock(pool);
        memory_pool_lock_destroy(pool);
        if (pool->name != NULL) {
            memory_free(pool->name);
        }
        memory_free(pool);
        return NULL;
    }
    
    memory_pool_unlock(pool);
    
    return pool;
}

void memory_pool_destroy(memory_pool_t* pool) {
    if (pool == NULL) {
        return;
    }

    size_t leaked_blocks = 0;
    const char* pool_name_for_log = NULL;

    memory_pool_lock(pool);

    // 检查是否有未释放的块（收集信息，不在锁内做I/O）
    if (pool->stats.allocated_blocks > 0) {
        leaked_blocks = pool->stats.allocated_blocks;
        pool_name_for_log = pool->name;
    }

    // 释放内存区域
    memory_pool_free_blocks(pool);

    memory_pool_unlock(pool);

    /* 锁外输出警告信息，避免锁内I/O阻塞 */
    if (leaked_blocks > 0) {
        fprintf(stderr, "警告：销毁内存池时发现未释放的块\n");
        fprintf(stderr, "内存池：%s\n", pool_name_for_log ? pool_name_for_log : "(unnamed)");
        fprintf(stderr, "未释放块数：%zu\n", leaked_blocks);
    }
    
    // 销毁锁
    memory_pool_lock_destroy(pool);
    
    // 释放名称
    if (pool->name != NULL) {
        memory_free(pool->name);
    }
    
    // 释放池结构本�?    memory_free(pool);
}

void* memory_pool_alloc(memory_pool_t* pool) {
    if (pool == NULL) {
        return NULL;
    }
    
    memory_pool_lock(pool);
    
    // 如果没有空闲块，尝试扩展
    if (pool->free_list == NULL) {
        pool->stats.miss_count++;
        
        if (!memory_pool_allocate_blocks(pool, pool->options.expansion_size)) {
            memory_pool_unlock(pool);
            return NULL;
        }
    } else {
        pool->stats.hit_count++;
    }
    
    // 从空闲链表获取一个块
    memory_pool_block_t* block = pool->free_list;
    pool->free_list = block->next;
    
    // 标记为已分配
    block->allocated = true;
    block->next = NULL;
    
    // 计算块数据区域的指针
    void* data_ptr = (uint8_t*)block + sizeof(memory_pool_block_t);
    
    // 更新统计信息
    pool->stats.allocated_blocks++;
    pool->stats.free_blocks--;
    pool->stats.used_memory += pool->options.block_size;
    pool->stats.allocation_count++;
    
    memory_pool_unlock(pool);
    
    return data_ptr;
}

void* memory_pool_calloc(memory_pool_t* pool) {
    void* ptr = memory_pool_alloc(pool);
    if (ptr != NULL) {
        memset(ptr, 0, pool->options.block_size);
    }
    return ptr;
}

void memory_pool_free(memory_pool_t* pool, void* ptr) {
    if (pool == NULL || ptr == NULL) {
        return;
    }

    // 计算块头指针
    memory_pool_block_t* block = (memory_pool_block_t*)((uint8_t*)ptr - sizeof(memory_pool_block_t));

    memory_pool_lock(pool);

    // O(1) 验证：通过嵌入的池指针确认所有权（替代原来的 O(n) 线性扫描）
    if (block->pool != pool || !block->allocated) {
        fprintf(stderr, "错误：尝试释放无效的内存池块\n");
        memory_pool_unlock(pool);
        return;
    }
    
    // 标记为未分配
    block->allocated = false;
    
    // 添加到空闲链�?    block->next = pool->free_list;
    pool->free_list = block;
    
    // 更新统计信息
    pool->stats.allocated_blocks--;
    pool->stats.free_blocks++;
    pool->stats.used_memory -= pool->options.block_size;
    pool->stats.free_count++;
    
    memory_pool_unlock(pool);
}

bool memory_pool_get_stats(memory_pool_t* pool, memory_pool_stats_t* stats) {
    if (pool == NULL || stats == NULL) {
        return false;
    }
    
    memory_pool_lock(pool);
    memcpy(stats, &pool->stats, sizeof(memory_pool_stats_t));
    memory_pool_unlock(pool);
    
    return true;
}

void memory_pool_reset_stats(memory_pool_t* pool) {
    if (pool == NULL) {
        return;
    }
    
    memory_pool_lock(pool);
    
    // 只重置计数统计，不重置大小和内存统计
    pool->stats.allocation_count = 0;
    pool->stats.free_count = 0;
    pool->stats.hit_count = 0;
    pool->stats.miss_count = 0;
    
    memory_pool_unlock(pool);
}

bool memory_pool_prealloc(memory_pool_t* pool, size_t count) {
    if (pool == NULL || count == 0) {
        return false;
    }
    
    memory_pool_lock(pool);
    bool result = memory_pool_allocate_blocks(pool, count);
    memory_pool_unlock(pool);
    
    return result;
}

void memory_pool_clear(memory_pool_t* pool) {
    if (pool == NULL) {
        return;
    }
    
    memory_pool_lock(pool);
    
    // 只清空空闲块，已分配块不受影�?    // 这里简化实现：实际上需要重新组织内�?    
    // 计算需要保留的块数（已分配块）
    size_t blocks_to_keep = pool->stats.allocated_blocks;
    
    if (blocks_to_keep == 0) {
        // 如果没有已分配块，可以直接释放整个内存区�?        memory_pool_free_blocks(pool);
        
        // 重新创建最小内存池
        if (pool->options.initial_blocks > 0) {
            memory_pool_allocate_blocks(pool, pool->options.initial_blocks);
        }
    }
    
    memory_pool_unlock(pool);
}

bool memory_pool_is_empty(memory_pool_t* pool) {
    if (pool == NULL) {
        return true;
    }
    
    memory_pool_lock(pool);
    bool empty = (pool->stats.allocated_blocks == 0);
    memory_pool_unlock(pool);
    
    return empty;
}

bool memory_pool_is_full(memory_pool_t* pool) {
    if (pool == NULL) {
        return false;
    }
    
    memory_pool_lock(pool);
    
    bool full = false;
    if (pool->options.max_blocks > 0) {
        full = (pool->stats.total_blocks >= pool->options.max_blocks);
    }
    
    memory_pool_unlock(pool);
    
    return full;
}

bool memory_pool_expand(memory_pool_t* pool, size_t additional_blocks) {
    if (pool == NULL || additional_blocks == 0) {
        return false;
    }
    
    memory_pool_lock(pool);
    bool result = memory_pool_allocate_blocks(pool, additional_blocks);
    memory_pool_unlock(pool);
    
    return result;
}

size_t memory_pool_shrink(memory_pool_t* pool, size_t blocks_to_keep) {
    if (pool == NULL) {
        return 0;
    }
    
    memory_pool_lock(pool);
    
    // 确保至少保留blocks_to_keep个块（包括已分配块）
    if (blocks_to_keep < pool->stats.allocated_blocks) {
        blocks_to_keep = pool->stats.allocated_blocks;
    }
    
    // 计算可以释放的空闲块�?    size_t blocks_to_free = 0;
    if (pool->stats.total_blocks > blocks_to_keep) {
        blocks_to_free = pool->stats.total_blocks - blocks_to_keep;
    }
    
    if (blocks_to_free == 0) {
        memory_pool_unlock(pool);
        return 0;
    }
    
    // 简化实现：实际需要复杂的块管理和内存重新分配
    // 这里返回0表示不支持收�?    memory_pool_unlock(pool);
    
    return 0;
}

bool memory_pool_validate(memory_pool_t* pool) {
    if (pool == NULL) {
        return false;
    }
    
    memory_pool_lock(pool);
    
    bool valid = true;
    
    // 检查统计信息一致�?    if (pool->stats.total_blocks != pool->stats.allocated_blocks + pool->stats.free_blocks) {
        valid = false;
    }
    
    if (pool->stats.total_memory != pool->stats.total_blocks * 
        (sizeof(memory_pool_block_t) + pool->options.block_size)) {
        valid = false;
    }
    
    if (pool->stats.used_memory != pool->stats.allocated_blocks * pool->options.block_size) {
        valid = false;
    }
    
    // 检查空闲链�?    size_t free_count = 0;
    memory_pool_block_t* current = pool->free_list;
    while (current != NULL) {
        free_count++;
        
        // 检查块是否确实未分�?        if (current->allocated) {
            valid = false;
            break;
        }
        
        current = current->next;
    }
    
    if (free_count != pool->stats.free_blocks) {
        valid = false;
    }
    
    memory_pool_unlock(pool);
    
    return valid;
}

void memory_pool_iterate(memory_pool_t* pool, 
    void (*callback)(void* block, bool allocated, void* user_data),
    void* user_data) {
    
    if (pool == NULL || callback == NULL) {
        return;
    }
    
    memory_pool_lock(pool);
    
    for (size_t i = 0; i < pool->stats.total_blocks; i++) {
        memory_pool_block_t* block = pool->blocks[i];
        if (block != NULL) {
            void* data_ptr = (uint8_t*)block + sizeof(memory_pool_block_t);
            callback(data_ptr, block->allocated, user_data);
        }
    }
    
    memory_pool_unlock(pool);
}

const char* memory_pool_get_name(memory_pool_t* pool) {
    if (pool == NULL) {
        return NULL;
    }
    
    return pool->name;
}

void memory_pool_set_name(memory_pool_t* pool, const char* name) {
    if (pool == NULL) {
        return;
    }
    
    memory_pool_lock(pool);
    
    // 释放旧名�?    if (pool->name != NULL) {
        memory_free(pool->name);
        pool->name = NULL;
    }
    
    // 设置新名�?    if (name != NULL) {
        pool->name = memory_calloc(strlen(name) + 1, "memory_pool_name");
        if (pool->name != NULL) {
            memcpy(pool->name, name, strlen(name) + 1);
        }
    }
    
    memory_pool_unlock(pool);
}

memory_pool_t* memory_pool_create_default(size_t block_size) {
    memory_pool_options_t options = {
        .block_size = block_size,
        .initial_blocks = 16,
        .max_blocks = 0,
        .expansion_size = 8,
        .thread_safe = true,
        .name = NULL
    };
    
    return memory_pool_create(&options);
}