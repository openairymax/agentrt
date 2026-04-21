/**
 * @file advanced_storage_cache.h
 * @brief L1 增强存储缓存管理 - LRU 缓存、脏数据写回
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ADVANCED_STORAGE_CACHE_H
#define AGENTOS_ADVANCED_STORAGE_CACHE_H

#include "../../../atoms/corekern/include/agentos.h"
#include "advanced_storage_utils.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 缓存条目状态
 */
typedef enum {
    CACHE_ENTRY_CLEAN = 0,          /**< 干净状态，与磁盘一致 */
    CACHE_ENTRY_DIRTY,              /**< 脏状态，需要写回磁盘 */
    CACHE_ENTRY_LOCKED,             /**< 锁定状态，正在操作 */
    CACHE_ENTRY_EVICTED,            /**< 已驱逐，等待释放 */
    CACHE_ENTRY_CORRUPTED           /**< 数据损坏 */
} cache_entry_state_t;

/**
 * @brief 缓存条目结构
 */
typedef struct cache_entry {
    char* id;                       /**< 数据 ID */
    void* data;                     /**< 数据指针 */
    size_t data_size;               /**< 数据大小 */
    size_t compressed_size;         /**< 压缩后大小 */
    uint64_t access_count;          /**< 访问计数 */
    uint64_t last_access_time;      /**< 最后访问时间 */
    uint64_t creation_time;         /**< 创建时间 */
    cache_entry_state_t state;      /**< 状态 */
    compression_algorithm_t comp_algo; /**< 压缩算法 */
    encryption_algorithm_t enc_algo;   /**< 加密算法 */
    char* integrity_hash;           /**< 完整性哈希 */
    struct cache_entry* prev;       /**< 前一个条目 */
    struct cache_entry* next;       /**< 后一个条目 */
    agentos_mutex_t* lock;          /**< 条目锁 */
} cache_entry_t;

/**
 * @brief 缓存管理器结构
 */
typedef struct cache_manager {
    cache_entry_t* lru_head;        /**< LRU 链表头 */
    cache_entry_t* lru_tail;        /**< LRU 链表尾 */
    size_t entry_count;             /**< 条目数量 */
    size_t total_memory_used;       /**< 总内存使用 */
    size_t max_memory;              /**< 最大内存限制 */
    agentos_mutex_t* lock;          /**< 缓存锁 */
    agentos_condition_t* evict_cond; /**< 驱逐条件变量 */
    uint64_t hit_count;             /**< 命中次数 */
    uint64_t miss_count;            /**< 未命中次数 */
    uint64_t eviction_count;        /**< 驱逐次数 */
} cache_manager_t;

/**
 * @brief 分片管理器（前向声明）
 */
typedef struct shard_manager shard_manager_t;

/**
 * @brief 创建缓存条目
 * @param id 数据 ID
 * @param data 数据
 * @param data_size 数据大小
 * @param comp_algo 压缩算法
 * @param enc_algo 加密算法
 * @return 缓存条目，失败返回 NULL
 */
cache_entry_t* advanced_cache_entry_create(const char* id, const void* data, size_t data_size,
                                          compression_algorithm_t comp_algo,
                                          encryption_algorithm_t enc_algo);

/**
 * @brief 销毁缓存条目
 * @param entry 缓存条目
 */
void advanced_cache_entry_destroy(cache_entry_t* entry);

/**
 * @brief 创建缓存管理器
 * @param max_memory 最大内存限制
 * @return 缓存管理器，失败返回 NULL
 */
cache_manager_t* advanced_cache_manager_create(size_t max_memory);

/**
 * @brief 销毁缓存管理器
 * @param cache 缓存管理器
 */
void advanced_cache_manager_destroy(cache_manager_t* cache);

/**
 * @brief 访问缓存条目，更新 LRU 位置
 * @param cache 缓存管理器
 * @param entry 缓存条目
 */
void advanced_cache_entry_access(cache_manager_t* cache, cache_entry_t* entry);

/**
 * @brief 从缓存中驱逐最久未使用的条目
 * @param cache 缓存管理器
 * @param required_space 需要释放的空间
 * @return 释放的空间大小
 */
size_t advanced_cache_evict_lru(cache_manager_t* cache, size_t required_space);

/**
 * @brief 将脏缓存条目写回磁盘
 * @param cache 缓存管理器
 * @param shard 分片管理器
 * @return 写回的条目数量
 */
size_t advanced_cache_flush_dirty(cache_manager_t* cache, shard_manager_t* shard);

/**
 * @brief 获取缓存命中率
 * @param cache 缓存管理器
 * @return 命中率 (0.0-1.0)
 */
double advanced_cache_get_hit_rate(cache_manager_t* cache);

#endif /* AGENTOS_ADVANCED_STORAGE_CACHE_H */
