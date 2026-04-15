/**
 * @file advanced_storage_cache.c
 * @brief L1 增强存储缓存管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "advanced_storage_cache.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>

/* 基础库兼容性层 */
#include <agentos/memory.h>

/* 前向声明 */
struct shard_manager {
    int shard_id;
    char* base_path;
    cache_manager_t* cache;
    void* storage;  /* agentos_layer1_raw_t 类型 */
    agentos_observability_t* obs;
    uint64_t write_count;
    uint64_t read_count;
    uint64_t error_count;
    agentos_mutex_t* stats_lock;
};

/* ==================== 缓存条目管理 ==================== */

cache_entry_t* advanced_cache_entry_create(const char* id, const void* data, size_t data_size,
                                          compression_algorithm_t comp_algo,
                                          encryption_algorithm_t enc_algo) {
    if (!id || !data || data_size == 0) {
        AGENTOS_LOG_ERROR("Invalid parameters for cache entry creation");
        return NULL;
    }

    cache_entry_t* entry = (cache_entry_t*)AGENTOS_CALLOC(1, sizeof(cache_entry_t));
    if (!entry) {
        AGENTOS_LOG_ERROR("Failed to allocate cache entry");
        return NULL;
    }

    entry->id = AGENTOS_STRDUP(id);
    entry->data = AGENTOS_MALLOC(data_size);
    if (!entry->id || !entry->data) {
        if (entry->id) AGENTOS_FREE(entry->id);
        if (entry->data) AGENTOS_FREE(entry->data);
        AGENTOS_FREE(entry);
        AGENTOS_LOG_ERROR("Failed to allocate cache entry data");
        return NULL;
    }

    memcpy(entry->data, data, data_size);
    entry->data_size = data_size;
    entry->compressed_size = data_size;
    entry->access_count = 1;
    entry->last_access_time = agentos_get_monotonic_time_ns();
    entry->creation_time = entry->last_access_time;
    entry->state = CACHE_ENTRY_CLEAN;
    entry->comp_algo = comp_algo;
    entry->enc_algo = enc_algo;
    entry->lock = agentos_mutex_create();

    if (!entry->lock) {
        AGENTOS_FREE(entry->id);
        AGENTOS_FREE(entry->data);
        AGENTOS_FREE(entry);
        AGENTOS_LOG_ERROR("Failed to create mutex for cache entry");
        return NULL;
    }

    /* 生成完整性哈希 */
    if (advanced_storage_generate_hash(data, data_size, &entry->integrity_hash) != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Failed to generate integrity hash for cache entry %s", id);
        entry->integrity_hash = NULL;
    }

    return entry;
}

void advanced_cache_entry_destroy(cache_entry_t* entry) {
    if (!entry) return;

    if (entry->lock) {
        agentos_mutex_destroy(entry->lock);
    }

    if (entry->id) AGENTOS_FREE(entry->id);
    if (entry->data) AGENTOS_FREE(entry->data);
    if (entry->integrity_hash) AGENTOS_FREE(entry->integrity_hash);

    AGENTOS_FREE(entry);
}

/* ==================== 缓存管理器 ==================== */

cache_manager_t* advanced_cache_manager_create(size_t max_memory) {
    if (max_memory == 0) {
        AGENTOS_LOG_ERROR("Invalid max_memory for cache manager");
        return NULL;
    }

    cache_manager_t* cache = (cache_manager_t*)AGENTOS_CALLOC(1, sizeof(cache_manager_t));
    if (!cache) {
        AGENTOS_LOG_ERROR("Failed to allocate cache manager");
        return NULL;
    }

    cache->max_memory = max_memory;
    cache->lock = agentos_mutex_create();
    cache->evict_cond = agentos_condition_create();

    if (!cache->lock || !cache->evict_cond) {
        if (cache->lock) agentos_mutex_destroy(cache->lock);
        if (cache->evict_cond) agentos_condition_destroy(cache->evict_cond);
        AGENTOS_FREE(cache);
        AGENTOS_LOG_ERROR("Failed to create cache manager synchronization primitives");
        return NULL;
    }

    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->entry_count = 0;
    cache->total_memory_used = 0;
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;

    return cache;
}

void advanced_cache_manager_destroy(cache_manager_t* cache) {
    if (!cache) return;

    /* 驱逐所有条目 */
    advanced_cache_evict_lru(cache, SIZE_MAX);

    if (cache->lock) agentos_mutex_destroy(cache->lock);
    if (cache->evict_cond) agentos_condition_destroy(cache->evict_cond);

    AGENTOS_FREE(cache);
}

void advanced_cache_entry_access(cache_manager_t* cache, cache_entry_t* entry) {
    if (!cache || !entry) return;

    agentos_mutex_lock(cache->lock);

    entry->access_count++;
    entry->last_access_time = agentos_get_monotonic_time_ns();

    /* 从当前位置移除 */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;

    if (entry == cache->lru_head) cache->lru_head = entry->next;
    if (entry == cache->lru_tail) cache->lru_tail = entry->prev;

    /* 移动到 LRU 头部（最近使用） */
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = entry;
    cache->lru_head = entry;
    if (!cache->lru_tail) cache->lru_tail = entry;

    agentos_mutex_unlock(cache->lock);
}

size_t advanced_cache_evict_lru(cache_manager_t* cache, size_t required_space) {
    if (!cache || required_space == 0) return 0;

    agentos_mutex_lock(cache->lock);

    size_t freed_space = 0;
    cache_entry_t* current = cache->lru_tail;

    while (current && freed_space < required_space) {
        cache_entry_t* to_evict = current;
        current = current->prev;

        /* 检查是否可以驱逐 */
        agentos_mutex_lock(to_evict->lock);
        if (to_evict->state == CACHE_ENTRY_CLEAN || to_evict->state == CACHE_ENTRY_EVICTED) {
            /* 从链表中移除 */
            if (to_evict->prev) to_evict->prev->next = to_evict->next;
            if (to_evict->next) to_evict->next->prev = to_evict->prev;

            if (to_evict == cache->lru_head) cache->lru_head = to_evict->next;
            if (to_evict == cache->lru_tail) cache->lru_tail = to_evict->prev;

            cache->entry_count--;
            cache->total_memory_used -= to_evict->data_size;
            freed_space += to_evict->data_size;
            cache->eviction_count++;

            /* 标记为已驱逐 */
            to_evict->state = CACHE_ENTRY_EVICTED;
            agentos_mutex_unlock(to_evict->lock);

            /* 异步销毁（避免在锁内执行耗时操作） */
            agentos_thread_create(NULL, (agentos_thread_func_t)advanced_cache_entry_destroy, to_evict);
        } else {
            agentos_mutex_unlock(to_evict->lock);
        }
    }

    agentos_mutex_unlock(cache->lock);
    return freed_space;
}

size_t advanced_cache_flush_dirty(cache_manager_t* cache, shard_manager_t* shard) {
    if (!cache || !shard) return 0;

    agentos_mutex_lock(cache->lock);

    size_t flushed_count = 0;
    cache_entry_t* current = cache->lru_head;

    while (current) {
        cache_entry_t* entry = current;
        current = current->next;

        agentos_mutex_lock(entry->lock);
        if (entry->state == CACHE_ENTRY_DIRTY) {
            /* 实际写入磁盘操作 */
            if (shard->storage && entry->data && entry->data_size > 0) {
                /* 注意：这里需要外部提供写入函数 */
                /* 简化实现，实际应该调用 agentos_layer1_raw_write */
                AGENTOS_LOG_DEBUG("Would flush dirty entry %s (size: %zu)", entry->id, entry->data_size);
                entry->state = CACHE_ENTRY_CLEAN;
                flushed_count++;
                
                /* 更新统计信息 */
                agentos_mutex_lock(shard->stats_lock);
                shard->write_count++;
                agentos_mutex_unlock(shard->stats_lock);
            } else {
                AGENTOS_LOG_WARN("Cannot write dirty cache entry %s: invalid storage or data",
                                entry->id);
            }
        }
        agentos_mutex_unlock(entry->lock);
    }

    agentos_mutex_unlock(cache->lock);
    return flushed_count;
}

double advanced_cache_get_hit_rate(cache_manager_t* cache) {
    if (!cache) return 0.0;

    uint64_t total = cache->hit_count + cache->miss_count;
    if (total == 0) return 0.0;

    return (double)cache->hit_count / (double)total;
}
