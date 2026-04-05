/**
 * @file cache_manager.h
 * @brief LRU缓存管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 最大缓存条目数 */
#define MAX_CACHE_ENTRIES 10000

/** @brief 默认缓存TTL（秒） */
#define DEFAULT_CACHE_TTL_SECONDS 3600

typedef struct cache_entry {
    char* key;
    void* value;
    size_t value_size;
    uint64_t access_count;
    uint64_t last_access_ns;
    uint64_t create_time_ns;
    uint64_t expire_time_ns;
    uint32_t flags;
    struct cache_entry* prev;
    struct cache_entry* next;
    struct cache_entry* hash_next;
} cache_entry_t;

typedef struct lru_cache {
    char* cache_name;
    uint64_t max_size;
    uint64_t current_size;
    uint64_t entry_count;
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t evict_count;
    cache_entry_t** hash_table;
    size_t hash_table_size;
    cache_entry_t* head;
    cache_entry_t* tail;
    void* lock;
    uint32_t default_ttl_seconds;
} lru_cache_t;

agentos_error_t lru_cache_create(const char* name, uint64_t max_size, lru_cache_t** out_cache);
void lru_cache_destroy(lru_cache_t* cache);
agentos_error_t lru_cache_get(lru_cache_t* cache, const char* key, void** out_value, size_t* out_size);
agentos_error_t lru_cache_put(lru_cache_t* cache, const char* key, const void* value, size_t size);
agentos_error_t lru_cache_remove(lru_cache_t* cache, const char* key);
void lru_cache_clear(lru_cache_t* cache);
uint64_t lru_cache_get_hit_count(lru_cache_t* cache);
uint64_t lru_cache_get_miss_count(lru_cache_t* cache);
double lru_cache_get_hit_rate(lru_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_MANAGER_H */
