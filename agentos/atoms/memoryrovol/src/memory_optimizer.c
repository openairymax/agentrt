/**
 * @file memory_optimizer.c
 * @brief MemoryRovol 内存优化器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 内存优化器负责四层记忆系统的内存管理和优化。
 * 本文件是主入口，整合了内存池和LRU缓存模块。
 *
 * 架构：
 * - memory_pool.c/h: 内存池管理
 * - lru_cache.c/h: LRU缓存管理
 * - memory_optimizer.c: 优化器主逻辑（本文件）
 */

#include "memoryrovol.h"
#include "memory_pool.h"
#include "lru_cache.h"
#include "config.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MEMORY_POOLS 16
#define DEFAULT_POOL_SIZE (64 * 1024 * 1024)
#define DEFAULT_CACHE_SIZE (128 * 1024 * 1024)
#define DEFRAGMENT_THRESHOLD 0.3

struct agentos_memory_optimizer {
    char* optimizer_id;
    memory_pool_t* pools[MAX_MEMORY_POOLS];
    uint32_t pool_count;
    lru_cache_t* caches[MAX_MEMORY_POOLS];
    uint32_t cache_count;
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t peak_usage;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t defrag_operations;
    double fragmentation_ratio;
    agentos_mutex_t* lock;
    agentos_observability_t* obs;
    uint64_t last_defrag_time_ns;
    uint32_t defrag_interval_ms;
    uint8_t auto_defrag;
    uint8_t auto_compress;
};
typedef struct agentos_memory_optimizer agentos_memory_optimizer_t;

static agentos_memory_optimizer_t* g_memory_optimizer = NULL;

agentos_memory_optimizer_t* agentos_memory_optimizer_create(void) {
    agentos_memory_optimizer_t* opt = (agentos_memory_optimizer_t*)AGENTOS_CALLOC(1, sizeof(agentos_memory_optimizer_t));
    if (!opt) return NULL;

    opt->optimizer_id = AGENTOS_STRDUP("memory_optimizer_default");
    opt->pool_count = 0;
    opt->cache_count = 0;
    opt->total_allocated = 0;
    opt->total_freed = 0;
    opt->peak_usage = 0;
    opt->cache_hits = 0;
    opt->cache_misses = 0;
    opt->defrag_operations = 0;
    opt->fragmentation_ratio = 0.0;
    opt->defrag_interval_ms = 60000;
    opt->auto_defrag = 1;
    opt->auto_compress = 1;

    opt->lock = agentos_mutex_create();
    if (!opt->lock) {
        AGENTOS_FREE(opt->optimizer_id);
        AGENTOS_FREE(opt);
        return NULL;
    }

    opt->obs = agentos_observability_create();
    if (opt->obs) {
        agentos_observability_register_metric(opt->obs, "memory_allocated_total", AGENTOS_METRIC_COUNTER, "Total memory allocated");
        agentos_observability_register_metric(opt->obs, "memory_freed_total", AGENTOS_METRIC_COUNTER, "Total memory freed");
        agentos_observability_register_metric(opt->obs, "memory_peak_usage", AGENTOS_METRIC_GAUGE, "Peak memory usage");
        agentos_observability_register_metric(opt->obs, "cache_hit_rate", AGENTOS_METRIC_GAUGE, "Cache hit rate");
    }

    g_memory_optimizer = opt;
    return opt;
}

void agentos_memory_optimizer_destroy(agentos_memory_optimizer_t* opt) {
    if (!opt) return;

    if (opt == g_memory_optimizer) {
        g_memory_optimizer = NULL;
    }

    for (uint32_t i = 0; i < opt->pool_count; i++) {
        if (opt->pools[i]) {
            memory_pool_destroy(opt->pools[i]);
        }
    }

    for (uint32_t i = 0; i < opt->cache_count; i++) {
        if (opt->caches[i]) {
            lru_cache_destroy(opt->caches[i]);
        }
    }

    if (opt->obs) {
        agentos_observability_destroy(opt->obs);
    }

    if (opt->lock) {
        agentos_mutex_destroy(opt->lock);
    }

    if (opt->optimizer_id) {
        AGENTOS_FREE(opt->optimizer_id);
    }

    AGENTOS_FREE(opt);
}

agentos_error_t agentos_memory_optimizer_create_pool(
    agentos_memory_optimizer_t* opt,
    uint8_t pool_id,
    const char* name,
    uint64_t size) {

    if (!opt || !name) return AGENTOS_EINVAL;
    if (pool_id >= MAX_MEMORY_POOLS) return AGENTOS_EINVAL;

    agentos_mutex_lock(opt->lock);

    if (opt->pools[pool_id]) {
        agentos_mutex_unlock(opt->lock);
        return AGENTOS_EEXIST;
    }

    memory_pool_t* pool = memory_pool_create(pool_id, name, size);
    if (!pool) {
        agentos_mutex_unlock(opt->lock);
        return AGENTOS_ENOMEM;
    }

    opt->pools[pool_id] = pool;
    opt->pool_count++;

    agentos_mutex_unlock(opt->lock);
    return AGENTOS_SUCCESS;
}

void* agentos_memory_optimizer_alloc(
    agentos_memory_optimizer_t* opt,
    uint8_t pool_id,
    size_t size) {

    if (!opt || pool_id >= MAX_MEMORY_POOLS) return NULL;

    memory_pool_t* pool = opt->pools[pool_id];
    if (!pool) return NULL;

    void* ptr = memory_pool_alloc(pool, size);
    if (ptr) {
        agentos_mutex_lock(opt->lock);
        opt->total_allocated += size;
        if (opt->total_allocated > opt->peak_usage) {
            opt->peak_usage = opt->total_allocated;
        }
        agentos_mutex_unlock(opt->lock);
    }

    return ptr;
}

void agentos_memory_optimizer_free(
    agentos_memory_optimizer_t* opt,
    uint8_t pool_id,
    void* ptr) {

    if (!opt || pool_id >= MAX_MEMORY_POOLS || !ptr) return;

    memory_pool_t* pool = opt->pools[pool_id];
    if (!pool) return;

    memory_pool_free(pool, ptr);
}

agentos_error_t agentos_memory_optimizer_create_cache(
    agentos_memory_optimizer_t* opt,
    uint32_t cache_id,
    const char* name,
    uint64_t max_size) {

    if (!opt || !name) return AGENTOS_EINVAL;
    if (cache_id >= MAX_MEMORY_POOLS) return AGENTOS_EINVAL;

    agentos_mutex_lock(opt->lock);

    if (opt->caches[cache_id]) {
        agentos_mutex_unlock(opt->lock);
        return AGENTOS_EEXIST;
    }

    lru_cache_t* cache = lru_cache_create(name, max_size, 1024);
    if (!cache) {
        agentos_mutex_unlock(opt->lock);
        return AGENTOS_ENOMEM;
    }

    opt->caches[cache_id] = cache;
    opt->cache_count++;

    agentos_mutex_unlock(opt->lock);
    return AGENTOS_SUCCESS;
}

void* agentos_memory_optimizer_cache_get(
    agentos_memory_optimizer_t* opt,
    uint32_t cache_id,
    const char* key,
    size_t* out_size) {

    if (!opt || cache_id >= MAX_MEMORY_POOLS || !key) return NULL;

    lru_cache_t* cache = opt->caches[cache_id];
    if (!cache) return NULL;

    void* result = lru_cache_get(cache, key, out_size);

    agentos_mutex_lock(opt->lock);
    if (result) {
        opt->cache_hits++;
    } else {
        opt->cache_misses++;
    }
    agentos_mutex_unlock(opt->lock);

    return result;
}

int agentos_memory_optimizer_cache_put(
    agentos_memory_optimizer_t* opt,
    uint32_t cache_id,
    const char* key,
    const void* value,
    size_t size,
    uint32_t ttl_seconds) {

    if (!opt || cache_id >= MAX_MEMORY_POOLS || !key || !value) return -1;

    lru_cache_t* cache = opt->caches[cache_id];
    if (!cache) return -1;

    return lru_cache_put(cache, key, value, size, ttl_seconds);
}

agentos_error_t agentos_memory_optimizer_defragment(
    agentos_memory_optimizer_t* opt,
    uint8_t pool_id) {

    if (!opt || pool_id >= MAX_MEMORY_POOLS) return AGENTOS_EINVAL;

    memory_pool_t* pool = opt->pools[pool_id];
    if (!pool) return AGENTOS_ENOENT;

    double frag = memory_pool_get_fragmentation_ratio(pool);
    if (frag < DEFRAGMENT_THRESHOLD) {
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_lock(opt->lock);
    opt->defrag_operations++;
    agentos_mutex_unlock(opt->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memory_optimizer_get_stats(
    agentos_memory_optimizer_t* opt,
    char* out_json,
    size_t json_size) {

    if (!opt || !out_json) return AGENTOS_EINVAL;

    agentos_mutex_lock(opt->lock);

    int written = snprintf(out_json, json_size,
        "{"
        "\"optimizer_id\": \"%s\","
        "\"pool_count\": %u,"
        "\"cache_count\": %u,"
        "\"total_allocated\": %lu,"
        "\"total_freed\": %lu,"
        "\"peak_usage\": %lu,"
        "\"cache_hits\": %lu,"
        "\"cache_misses\": %lu,"
        "\"defrag_operations\": %lu,"
        "\"cache_hit_rate\": %.3f"
        "}",
        opt->optimizer_id ? opt->optimizer_id : "unknown",
        opt->pool_count,
        opt->cache_count,
        (unsigned long)opt->total_allocated,
        (unsigned long)opt->total_freed,
        (unsigned long)opt->peak_usage,
        (unsigned long)opt->cache_hits,
        (unsigned long)opt->cache_misses,
        (unsigned long)opt->defrag_operations,
        (opt->cache_hits + opt->cache_misses) > 0 ?
            (double)opt->cache_hits / (opt->cache_hits + opt->cache_misses) : 0.0
    );

    agentos_mutex_unlock(opt->lock);

    if (written < 0 || (size_t)written >= json_size) {
        return AGENTOS_ERANGE;
    }

    return AGENTOS_SUCCESS;
}

agentos_memory_optimizer_t* agentos_memory_optimizer_get_global(void) {
    return g_memory_optimizer;
}
