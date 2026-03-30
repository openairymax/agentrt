/**
 * @file cache.h
 * @brief LRU 缓存接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_CACHE_H
#define LLM_CACHE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cache cache_t;

cache_t* cache_create(size_t capacity, int ttl_sec);
void cache_destroy(cache_t* cache);
int cache_get(cache_t* cache, const char* key, char** out_value);
void cache_put(cache_t* cache, const char* key, const char* value);
void cache_clear(cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* LLM_CACHE_H */