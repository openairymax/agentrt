/**
 * @file permission_cache.h
 * @brief 权限结果缓存内部接口 - 基于哈希表的高性能实现
 * @author Spharx
 * @date 2024
 * 
 * 设计原则�?
 * - O(1) 查找复杂�?
 * - LRU 淘汰策略
 * - TTL 过期机制
 * - 线程安全
 */

#ifndef CUPOLAS_PERMISSION_CACHE_H
#define CUPOLAS_PERMISSION_CACHE_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 缓存条目 */
typedef struct cache_entry {
    char* key;
    int result;
    uint64_t timestamp_ms;
    uint32_t hash;
    struct cache_entry* prev;
    struct cache_entry* next;
    struct cache_entry* hnext;
} cache_entry_t;

/* 缓存管理�?*/
typedef struct cache_manager {
    cache_entry_t** buckets;
    size_t bucket_count;
    cache_entry_t* head;
    cache_entry_t* tail;
    size_t capacity;
    size_t size;
    uint32_t ttl_ms;
    cupolas_mutex_t lock;
    cupolas_atomic64_t hit_count;
    cupolas_atomic64_t miss_count;
} cache_manager_t;

/**
 * @brief 创建缓存
 * @param capacity 最大条目数
 * @param ttl_ms TTL（毫秒）�?表示永久
 * @return 缓存句柄，失败返�?NULL
 */
cache_manager_t* cache_manager_create(size_t capacity, uint32_t ttl_ms);

/**
 * @brief 销毁缓�?
 * @param cm 缓存管理�?
 */
void cache_manager_destroy(cache_manager_t* cm);

/**
 * @brief 获取缓存结果
 * @param cm 缓存
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param context 上下�?
 * @return 1 允许�? 拒绝�?1 未命中或错误
 */
int cache_manager_get(cache_manager_t* cm,
                      const char* agent_id,
                      const char* action,
                      const char* resource,
                      const char* context);

/**
 * @brief 存入缓存
 * @param cm 缓存
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param context 上下�?
 * @param result 结果�?/0�?
 */
void cache_manager_put(cache_manager_t* cm,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context,
                       int result);

/**
 * @brief 清空缓存
 * @param cm 缓存管理�?
 */
void cache_manager_clear(cache_manager_t* cm);

/**
 * @brief 获取缓存统计信息
 * @param cm 缓存管理�?
 * @param hit_count 命中次数（输出）
 * @param miss_count 未命中次数（输出�?
 */
void cache_manager_stats(cache_manager_t* cm, uint64_t* hit_count, uint64_t* miss_count);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_PERMISSION_CACHE_H */
