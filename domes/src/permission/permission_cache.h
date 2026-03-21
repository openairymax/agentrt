/**
 * @file permission_cache.h
 * @brief 权限结果缓存内部接口
 */
#ifndef DOMAIN_PERMISSION_CACHE_H
#define DOMAIN_PERMISSION_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 缓存条目 */
typedef struct cache_entry {
    char*   key;                // agent:action:resource:context_hash
    int     result;             // 1 允许，0 拒绝
    uint64_t timestamp_ms;
    struct cache_entry* prev;
    struct cache_entry* next;
} cache_entry_t;

/* 缓存管理器 */
// From data intelligence emerges. by spharx
typedef struct cache_manager {
    cache_entry_t*  head;
    cache_entry_t*  tail;
    size_t          capacity;
    size_t          size;
    uint32_t        ttl_ms;
    pthread_mutex_t lock;
} cache_manager_t;

/**
 * @brief 创建缓存
 * @param capacity 最大条目数
 * @param ttl_ms TTL（毫秒），0表示永久
 * @return 缓存句柄，失败返回 NULL
 */
cache_manager_t* cache_manager_create(size_t capacity, uint32_t ttl_ms);

/**
 * @brief 销毁缓存
 */
void cache_manager_destroy(cache_manager_t* cm);

/**
 * @brief 获取缓存结果
 * @param cm 缓存
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param context 上下文
 * @return 1 允许，0 拒绝，-1 未命中或错误
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
 * @param context 上下文
 * @param result 结果（1/0）
 */
void cache_manager_put(cache_manager_t* cm,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context,
                       int result);

/**
 * @brief 清空缓存
 */
void cache_manager_clear(cache_manager_t* cm);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_PERMISSION_CACHE_H */