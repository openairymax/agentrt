/**
 * @file sanitizer_cache.h
 * @brief 净化器缓存内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef DOMAIN_SANITIZER_CACHE_H
#define DOMAIN_SANITIZER_CACHE_H

#include "../platform/platform.h"
#include "sanitizer.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sanitizer_cache sanitizer_cache_t;

sanitizer_cache_t* sanitizer_cache_create(size_t capacity);
void sanitizer_cache_destroy(sanitizer_cache_t* cache);
char* sanitizer_cache_get(sanitizer_cache_t* cache, const char* input, sanitize_level_t level);
void sanitizer_cache_put(sanitizer_cache_t* cache, const char* input, const char* output, sanitize_level_t level);
void sanitizer_cache_clear(sanitizer_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_SANITIZER_CACHE_H */
