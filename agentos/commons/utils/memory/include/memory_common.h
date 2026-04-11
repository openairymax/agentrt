#ifndef MEMORY_COMMON_H
#define MEMORY_COMMON_H

#include <error.h>
#include <memory_compat.h>

/**
 * @brief 内存分配策略枚举
 */
typedef enum {
    MEMORY_STRATEGY_DEFAULT = 0,
    MEMORY_STRATEGY_PERFORMANCE,
    MEMORY_STRATEGY_SAFETY,
    MEMORY_STRATEGY_LOW_LATENCY
} memory_strategy_t;

/**
 * @brief 内存池配置结构体
 */
typedef struct {
    size_t block_size;
    size_t block_count;
    memory_strategy_t strategy;
    bool thread_safe;
} memory_pool_config_t;

/**
 * @brief 内存池结构体
 */
typedef struct {
    void* pool;
    memory_pool_config_t manager;
    size_t used_blocks;
    size_t peak_usage;
} memory_pool_t;

/**
 * @brief 内存分配统计信息
 */
#ifndef AGENTOS_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    size_t allocation_count;
    size_t free_count;
} memory_stats_t;
#endif

/**
 * @brief 创建默认内存池配置
 * @return 默认内存池配置
 */
memory_pool_config_t memory_create_default_pool_config(void);

/**
 * @brief 初始化内存池
 * @param pool 内存池结构体
 * @param manager 内存池配置
 * @return 错误码
 */
agentos_error_t memory_pool_init(memory_pool_t* pool, const memory_pool_config_t* manager);

/**
 * @brief 从内存池分配内存
 * @param pool 内存池
 * @param size 分配大小
 * @return 分配的内存指针
 */
void* memory_pool_alloc(memory_pool_t* pool, size_t size);

/**
 * @brief 释放内存池中的内存
 * @param pool 内存池
 * @param ptr 内存指针
 */
void memory_pool_free(memory_pool_t* pool, void* ptr);

/**
 * @brief 清理内存池
 * @param pool 内存池
 */
void memory_pool_cleanup(memory_pool_t* pool);

/**
 * @brief 获取内存池统计信息
 * @param pool 内存池
 * @param stats 统计信息输出
 */
void memory_pool_get_stats(const memory_pool_t* pool, memory_stats_t* stats);

/**
 * @brief 安全内存分配
 * @param size 分配大小
 * @return 分配的内存指针，失败返回NULL
 */
void* memory_safe_alloc(size_t size);

/**
 * @brief 安全内存重分配
 * @param ptr 原内存指针
 * @param size 新大小
 * @return 重新分配的内存指针，失败返回NULL
 */
void* memory_safe_realloc(void* ptr, size_t size);

/**
 * @brief 安全内存释放
 * @param ptr 内存指针
 */
void memory_safe_free(void* ptr);

/**
 * @brief 安全字符串复制
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区大小
 * @return 目标缓冲区指针
 */
char* memory_safe_strdup(const char* src);

/**
 * @brief 获取全局内存统计信息
 * @param stats 统计信息输出
 */
void memory_get_global_stats(memory_stats_t* stats);

/**
 * @brief 重置全局内存统计信息
 */
void memory_reset_global_stats(void);

/**
 * @brief 设置内存分配策略
 * @param strategy 内存分配策略
 */
void memory_set_strategy(memory_strategy_t strategy);

/**
 * @brief 获取当前内存分配策略
 * @return 当前内存分配策略
 */
memory_strategy_t memory_get_strategy(void);

#endif // MEMORY_COMMON_H