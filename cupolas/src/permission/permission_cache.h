/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_cache.h - Permission Cache Internal Interface: Hash-based LRU Implementation
 */

#ifndef CUPOLAS_PERMISSION_CACHE_H
#define CUPOLAS_PERMISSION_CACHE_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cache entry structure for LRU management
 * 
 * Design principles:
 * - O(1) lookup complexity via hash table
 * - LRU eviction policy for memory efficiency
 * - TTL-based expiration for stale data cleanup
 * - Thread-safe with fine-grained locking
 */
typedef struct cache_entry {
    char*                   key;            /**< Hash key (agent_id:action:resource:context) */
    int                     result;         /**< Cached permission result (1=allow, 0=deny) */
    uint64_t                timestamp_ms;   /**< Creation timestamp (milliseconds) */
    uint32_t                hash;           /**< Pre-computed hash value */
    struct cache_entry*     prev;           /**< Previous entry in LRU list */
    struct cache_entry*     next;           /**< Next entry in LRU list */
    struct cache_entry*     hnext;          /**< Next entry in hash bucket chain */
} cache_entry_t;

/**
 * @brief Cache manager structure
 * 
 * Provides high-performance permission caching with:
 * - Configurable capacity limits
 * - Automatic TTL expiration
 * - Hit/miss statistics for monitoring
 */
typedef struct cache_manager {
    cache_entry_t**     buckets;        /**< Hash table buckets */
    size_t              bucket_count;   /**< Number of hash buckets */
    cache_entry_t*      head;           /**< LRU list head (most recently used) */
    cache_entry_t*      tail;           /**< LRU list tail (least recently used) */
    size_t              capacity;       /**< Maximum number of entries */
    size_t              size;           /**< Current number of entries */
    uint32_t            ttl_ms;         /**< Time-to-live in milliseconds (0=permanent) */
    cupolas_mutex_t     lock;           /**< Mutex for thread safety */
    cupolas_atomic64_t  hit_count;      /**< Cache hit counter */
    cupolas_atomic64_t  miss_count;     /**< Cache miss counter */
} cache_manager_t;

/**
 * @brief Create permission cache
 * @param[in] capacity Maximum number of cache entries
 * @param[in] ttl_ms Time-to-live in milliseconds (0 = permanent/no expiration)
 * @return Cache manager handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership Returns owned pointer: caller must call cache_manager_destroy()
 */
cache_manager_t* cache_manager_create(size_t capacity, uint32_t ttl_ms);

/**
 * @brief Destroy cache manager and free all resources
 * @param[in] cm Cache manager handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership cm: transferred to this function, will be freed
 */
void cache_manager_destroy(cache_manager_t* cm);

/**
 * @brief Get cached permission result
 * @param[in] cm Cache manager handle
 * @param[in] agent_id Agent identifier
 * @param[in] action Action being performed
 * @param[in] resource Resource being accessed
 * @param[in] context Context information
 * @return 1=allowed, 0=denied, -1=cache miss or error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership All parameters: caller retains ownership, may be NULL
 */
int cache_manager_get(cache_manager_t* cm,
                      const char* agent_id,
                      const char* action,
                      const char* resource,
                      const char* context);

/**
 * @brief Store permission result in cache
 * @param[in] cm Cache manager handle
 * @param[in] agent_id Agent identifier
 * @param[in] action Action being performed
 * @param[in] resource Resource being accessed
 * @param[in] context Context information
 * @param[in] result Permission result (1=allow, 0=deny)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership All string parameters: caller retains ownership
 */
void cache_manager_put(cache_manager_t* cm,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context,
                       int result);

/**
 * @brief Clear all cache entries
 * @param[in] cm Cache manager handle
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void cache_manager_clear(cache_manager_t* cm);

/**
 * @brief Get cache statistics
 * @param[in] cm Cache manager handle
 * @param[out] hit_count Cache hit counter (may be NULL)
 * @param[out] miss_count Cache miss counter (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
void cache_manager_stats(cache_manager_t* cm, uint64_t* hit_count, uint64_t* miss_count);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_PERMISSION_CACHE_H */
