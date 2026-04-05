/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * sanitizer_cache.h - Sanitizer Cache Internal Interface
 */

#ifndef CUPOLAS_SANITIZER_CACHE_H
#define CUPOLAS_SANITIZER_CACHE_H

#include "../platform/platform.h"
#include "sanitizer.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sanitizer cache structure
 * 
 * Design principles:
 * - LRU caching for sanitized strings
 * - Thread-safe with fine-grained locking
 * - Memory-efficient with bounded capacity
 */
typedef struct sanitizer_cache sanitizer_cache_t;

/**
 * @brief Create sanitizer cache
 * @param[in] capacity Maximum number of cached entries
 * @return Cache handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call sanitizer_cache_destroy()
 */
sanitizer_cache_t* sanitizer_cache_create(size_t capacity);

/**
 * @brief Destroy sanitizer cache and free all resources
 * @param[in] cache Cache handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership cache: transferred to this function, will be freed
 */
void sanitizer_cache_destroy(sanitizer_cache_t* cache);

/**
 * @brief Get cached sanitized result
 * @param[in] cache Cache handle
 * @param[in] input Original input string
 * @param[in] level Sanitization level
 * @return Sanitized string (cached or newly computed), NULL on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership Returns owned pointer: caller must free with free()
 * @ownership input: caller retains ownership
 */
char* sanitizer_cache_get(sanitizer_cache_t* cache, const char* input, sanitize_level_t level);

/**
 * @brief Store sanitized result in cache
 * @param[in] cache Cache handle
 * @param[in] input Original input string
 * @param[in] output Sanitized output string
 * @param[in] level Sanitization level
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership input and output: caller retains ownership
 */
void sanitizer_cache_put(sanitizer_cache_t* cache, const char* input, const char* output, sanitize_level_t level);

/**
 * @brief Clear all cache entries
 * @param[in] cache Cache handle
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void sanitizer_cache_clear(sanitizer_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SANITIZER_CACHE_H */
