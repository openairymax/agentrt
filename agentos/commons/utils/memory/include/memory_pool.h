/**
 * @file memory_pool.h
 * @brief 统一内存管理模块 - 内存池管理
 * 
 * 提供高效的内存池管理功能，减少内存碎片和分配开销。
 * 适用于频繁分配和释放相同大小内存块的场景。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORY_POOL_H
#define AGENTOS_MEMORY_POOL_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_pool_api 内存池API
 * @{
 */

/**
 * @brief 内存池选项
 */
typedef struct {
    size_t block_size;                   /**< 内存块大小（字节） */
    size_t initial_blocks;               /**< 初始预分配块数 */
    size_t max_blocks;                   /**< 最大块数（0表示无限制） */
    size_t expansion_size;               /**< 池满时扩展的块数 */
    bool thread_safe;                    /**< 是否线程安全 */
    const char* name;                    /**< 内存池名称（用于调试） */
} memory_pool_options_t;

/**
 * @brief 内存池句柄（不透明类型）
 */
typedef struct memory_pool memory_pool_t;

/**
 * @brief 内存池统计信息
 */
typedef struct {
    size_t block_size;                   /**< 内存块大小 */
    size_t total_blocks;                 /**< 总块数 */
    size_t allocated_blocks;             /**< 已分配块数 */
    size_t free_blocks;                  /**< 空闲块数 */
    size_t total_memory;                 /**< 总内存（字节） */
    size_t used_memory;                  /**< 已使用内存（字节） */
    size_t allocation_count;             /**< 分配次数 */
    size_t free_count;                   /**< 释放次数 */
    size_t hit_count;                    /**< 缓存命中次数 */
    size_t miss_count;                   /**< 缓存未命中次数 */
} memory_pool_stats_t;

/**
 * @brief 创建内存池
 * 
 * @param[in] options 内存池选项（不能为NULL）
 * @return 成功返回内存池句柄，失败返回NULL
 * 
 * @note 内存池创建后会预分配initial_blocks个内存块
 */
memory_pool_t* memory_pool_create(const memory_pool_options_t* options);

/**
 * @brief 销毁内存池
 * 
 * @param[in] pool 内存池句柄
 * 
 * @note 如果池中还有未释放的内存块，会输出警告信息
 * @note 如果启用了内存调试，会检查内存泄漏
 */
void memory_pool_destroy(memory_pool_t* pool);

/**
 * @brief 从内存池分配内存块
 * 
 * @param[in] pool 内存池句柄
 * @return 成功返回内存块指针，失败返回NULL
 * 
 * @note 分配的内存块大小为创建时指定的block_size
 * @note 分配的内存块未初始化
 */
void* memory_pool_alloc(memory_pool_t* pool);

/**
 * @brief 从内存池分配并清零内存块
 * 
 * @param[in] pool 内存池句柄
 * @return 成功返回内存块指针，失败返回NULL
 * 
 * @note 分配的内存块会被清零
 */
void* memory_pool_calloc(memory_pool_t* pool);

/**
 * @brief 释放内存块回内存池
 * 
 * @param[in] pool 内存池句柄
 * @param[in] ptr 要释放的内存块指针
 * 
 * @note 如果ptr为NULL，函数无操作
 * @note 只能释放从同一内存池分配的内存块
 */
void memory_pool_free(memory_pool_t* pool, void* ptr);

/**
 * @brief 安全释放内存块并将指针置为NULL
 * 
 * @param[in] pool 内存池句柄
 * @param[inout] ptr_ptr 指向内存块指针的指针
 * 
 * @note 释放后会自动将指针置为NULL
 */
#define MEMORY_POOL_FREE_SAFE(pool, ptr_ptr) \
    do { \
        if ((ptr_ptr) != NULL && *(ptr_ptr) != NULL) { \
            memory_pool_free((pool), *(ptr_ptr)); \
            *(ptr_ptr) = NULL; \
        } \
    } while (0)

/**
 * @brief 获取内存池统计信息
 * 
 * @param[in] pool 内存池句柄
 * @param[out] stats 统计信息输出缓冲区
 * @return 成功返回true，失败返回false
 */
bool memory_pool_get_stats(memory_pool_t* pool, memory_pool_stats_t* stats);

/**
 * @brief 重置内存池统计信息
 * 
 * @param[in] pool 内存池句柄
 */
void memory_pool_reset_stats(memory_pool_t* pool);

/**
 * @brief 预分配内存块
 * 
 * @param[in] pool 内存池句柄
 * @param[in] count 要预分配的块数
 * @return 成功返回true，失败返回false
 * 
 * @note 预分配可以提高后续分配的性能
 */
bool memory_pool_prealloc(memory_pool_t* pool, size_t count);

/**
 * @brief 清空内存池中的所有空闲块
 * 
 * @param[in] pool 内存池句柄
 * 
 * @note 只清空空闲块，已分配块不受影响
 * @note 调用后，内存池的大小会减少到只包含已分配块
 */
void memory_pool_clear(memory_pool_t* pool);

/**
 * @brief 检查内存池是否为空
 * 
 * @param[in] pool 内存池句柄
 * @return 如果内存池中没有已分配块，返回true
 */
bool memory_pool_is_empty(memory_pool_t* pool);

/**
 * @brief 检查内存池是否已满
 * 
 * @param[in] pool 内存池句柄
 * @return 如果内存池已满（达到max_blocks限制），返回true
 */
bool memory_pool_is_full(memory_pool_t* pool);

/**
 * @brief 扩展内存池
 * 
 * @param[in] pool 内存池句柄
 * @param[in] additional_blocks 要添加的块数
 * @return 成功返回true，失败返回false
 */
bool memory_pool_expand(memory_pool_t* pool, size_t additional_blocks);

/**
 * @brief 收缩内存池
 * 
 * @param[in] pool 内存池句柄
 * @param[in] blocks_to_keep 要保留的最小块数（包括已分配块）
 * @return 成功释放的块数
 * 
 * @note 只能收缩空闲块，确保至少保留blocks_to_keep个块
 */
size_t memory_pool_shrink(memory_pool_t* pool, size_t blocks_to_keep);

/**
 * @brief 验证内存池完整性
 * 
 * @param[in] pool 内存池句柄
 * @return 内存池完整返回true，损坏返回false
 * 
 * @note 需要启用内存调试功能
 */
bool memory_pool_validate(memory_pool_t* pool);

/**
 * @brief 遍历内存池中的所有块
 * 
 * @param[in] pool 内存池句柄
 * @param[in] callback 回调函数，对每个块调用
 * @param[in] user_data 用户数据，传递给回调函数
 * 
 * @note 回调函数原型：void callback(void* block, bool allocated, void* user_data)
 * @note 主要用于调试和监控
 */
void memory_pool_iterate(memory_pool_t* pool, 
    void (*callback)(void* block, bool allocated, void* user_data),
    void* user_data);

/**
 * @brief 获取内存池名称
 * 
 * @param[in] pool 内存池句柄
 * @return 内存池名称（可能为NULL）
 */
const char* memory_pool_get_name(memory_pool_t* pool);

/**
 * @brief 设置内存池名称
 * 
 * @param[in] pool 内存池句柄
 * @param[in] name 新名称（可为NULL）
 */
void memory_pool_set_name(memory_pool_t* pool, const char* name);

/**
 * @brief 创建默认选项的内存池
 * 
 * @param[in] block_size 内存块大小
 * @return 成功返回内存池句柄，失败返回NULL
 * 
 * @note 使用默认选项：initial_blocks=16, max_blocks=0, expansion_size=8, thread_safe=true
 */
memory_pool_t* memory_pool_create_default(size_t block_size);

/** @} */ // end of memory_pool_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_POOL_H */