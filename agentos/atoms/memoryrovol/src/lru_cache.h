/**
 * @file lru_cache.h
 * @brief LRU 缓存管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../atoms/corekern/include/agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    agentos_mutex_t* lock;
    uint32_t default_ttl_seconds;
} lru_cache_t;

lru_cache_t* lru_cache_create(const char* name, uint64_t max_size, size_t hash_size);
void lru_cache_destroy(lru_cache_t* cache);
void* lru_cache_get(lru_cache_t* cache, const char* key, size_t* out_size);
int lru_cache_put(lru_cache_t* cache, const char* key, const void* value, size_t size, uint32_t ttl_seconds);
int lru_cache_remove(lru_cache_t* cache, const char* key);
void lru_cache_clear(lru_cache_t* cache);
uint64_t lru_cache_get_hit_count(lru_cache_t* cache);
uint64_t lru_cache_get_miss_count(lru_cache_t* cache);
double lru_cache_get_hit_rate(lru_cache_t* cache);
uint64_t lru_cache_get_evict_count(lru_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* LRU_CACHE_H */
