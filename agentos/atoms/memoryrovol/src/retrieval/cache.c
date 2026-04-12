/**
 * @file cache.c
 * @brief 检索缓存（增强型LRU实现，支持TTL和统计）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/retrieval.h"
#include "agentos.h"
#include <stdlib.h>
#include <time.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

#define DEFAULT_CACHE_TTL_SEC 3600  /* 默认TTL：1小时 */

typedef struct cache_node {
    char* key;                          /**< 查询键（查询文本的SHA1或其他哈希） */
    char** result_ids;                  /**< 缓存的检索结果ID数组 */
    size_t result_count;                /**< 结果数量 */
    uint64_t timestamp;                  /**< 最近使用时间戳（纳秒级） */
    uint64_t created_at;                 /**< 创建时间戳（纳秒级） */
    uint32_t access_count;               /**< 访问次数（用于LFU辅助决策） */
    uint32_t ttl_seconds;                /**< 生存时间（秒），0表示永不过期 */
    struct cache_node* next;
    struct cache_node* prev;
} cache_node_t;

/**
 * @brief 缓存性能统计
 */
typedef struct cache_stats {
    uint64_t hits;                       /**< 命中次数 */
    uint64_t misses;                     /**< 未命中次数 */
    uint64_t evictions;                 /**< 淘汰次数 */
    uint64_t expirations;                /**< 过期次数 */
    uint64_t total_accesses;             /**< 总访问次数 */
    double hit_rate;                     /**< 命中率 (0.0-1.0) */
    double avg_access_time_ns;          /**< 平均访问时间（纳秒） */
    uint64_t last_stats_reset;           /**< 上次重置统计的时间 */
} cache_stats_t;

struct agentos_retrieval_cache {
    cache_node_t* head;                  /**< LRU 链表头（最近使用） */
    cache_node_t* tail;                  /**< LRU 链表尾（最久未用） */
    size_t max_size;                     /**< 最大条目数 */
    size_t current_size;                  /**< 当前条目数 */
    uint32_t default_ttl;                /**< 默认TTL（秒） */
    int enable_stats;                    /**< 是否启用统计 */
    int enable_lfu;                      /**< 是否启用LFU辅助（结合LRU+LFU） */
    cache_stats_t stats;                 /**< 性能统计 */
    agentos_mutex_t* lock;
};

agentos_error_t agentos_retrieval_cache_create(
    size_t max_size,
    agentos_retrieval_cache_t** out_cache) {

    if (!out_cache) return AGENTOS_EINVAL;

    agentos_retrieval_cache_t* cache = (agentos_retrieval_cache_t*)AGENTOS_CALLOC(1, sizeof(agentos_retrieval_cache_t));
    if (!cache) {
        AGENTOS_LOG_ERROR("Failed to allocate cache");
        return AGENTOS_ENOMEM;
    }

    cache->max_size = max_size;
    cache->default_ttl = DEFAULT_CACHE_TTL_SEC;
    cache->enable_stats = 1;  /* 默认启用统计 */
    cache->enable_lfu = 0;    /* 默认不启用LFU */
    
    /* 初始化统计信息 */
    memset(&cache->stats, 0, sizeof(cache_stats_t));
    cache->stats.last_stats_reset = agentos_time_monotonic_ns();
    
    cache->lock = agentos_mutex_create();
    if (!cache->lock) {
        AGENTOS_FREE(cache);
        return AGENTOS_ENOMEM;
    }

    *out_cache = cache;
    return AGENTOS_SUCCESS;
}

void agentos_retrieval_cache_destroy(agentos_retrieval_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(cache->lock);
    cache_node_t* node = cache->head;
    while (node) {
        cache_node_t* next = node->next;
        AGENTOS_FREE(node->key);
        if (node->result_ids) {
            for (size_t i = 0; i < node->result_count; i++) {
                AGENTOS_FREE(node->result_ids[i]);
            }
            AGENTOS_FREE(node->result_ids);
        }
        AGENTOS_FREE(node);
        node = next;
    }
    agentos_mutex_unlock(cache->lock);
    agentos_mutex_destroy(cache->lock);
    AGENTOS_FREE(cache);
}

/* 简单的字符串哈希（djb2�?*/
static unsigned long hash_key(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* 将节点移动到链表头部 */
static void move_to_head(cache_node_t* node, agentos_retrieval_cache_t* cache) {
    if (node == cache->head) return;

    // 从链表中移除
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (cache->tail == node) cache->tail = node->prev;

    // 插入头部
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
}

agentos_error_t agentos_retrieval_cache_get(
    agentos_retrieval_cache_t* cache,
    const char* query,
    char*** out_result_ids,
    size_t* out_count) {

    if (!cache || !query || !out_result_ids || !out_count) return AGENTOS_EINVAL;

    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%lx", hash_key(query));

    uint64_t start_time = 0;
    if (cache->enable_stats) {
        start_time = agentos_time_monotonic_ns();
    }

    agentos_mutex_lock(cache->lock);
    cache_node_t* node = cache->head;
    
    /* 检查过期条目（在查找过程中顺便清理） */
    uint64_t now = agentos_time_monotonic_ns();
    
    while (node) {
        cache_node_t* next_node = node->next;  /* 保存下一个节点 */
        
        /* 检查TTL过期 */
        if (node->ttl_seconds > 0) {
            uint64_t age_sec = (now - node->created_at) / 1000000000ULL;
            if (age_sec >= node->ttl_seconds) {
                /* 条目已过期，移除它 */
                /* 从链表中移除 */
                if (node->prev) node->prev->next = node->next;
                if (node->next) node->next->prev = node->prev;
                if (cache->head == node) cache->head = node->next;
                if (cache->tail == node) cache->tail = node->prev;
                
                /* 释放资源 */
                AGENTOS_FREE(node->key);
                if (node->result_ids) {
                    for (size_t i = 0; i < node->result_count; i++) {
                        AGENTOS_FREE(node->result_ids[i]);
                    }
                    AGENTOS_FREE(node->result_ids);
                }
                AGENTOS_FREE(node);
                cache->current_size--;
                
                if (cache->enable_stats) {
                    cache->stats.expirations++;
                }
                
                node = next_node;
                continue;
            }
        }
        
        if (strcmp(node->key, key_buf) == 0) {
            /* 找到缓存命中 */
            move_to_head(node, cache);
            node->timestamp = now;
            node->access_count++;

            /* 更新统计信息 */
            if (cache->enable_stats) {
                cache->stats.hits++;
                cache->stats.total_accesses++;
                uint64_t elapsed = agentos_time_monotonic_ns() - start_time;
                /* 使用移动平均更新平均访问时间 */
                double alpha = 0.1;
                cache->stats.avg_access_time_ns = 
                    alpha * (double)elapsed + (1.0 - alpha) * cache->stats.avg_access_time_ns;
                /* 更新命中率 */
                cache->stats.hit_rate = (double)cache->stats.hits / (double)cache->stats.total_accesses;
            }

            /* 复制结果 */
            char** ids = (char**)AGENTOS_MALLOC(node->result_count * sizeof(char*));
            if (!ids) {
                agentos_mutex_unlock(cache->lock);
                return AGENTOS_ENOMEM;
            }
            for (size_t i = 0; i < node->result_count; i++) {
                ids[i] = AGENTOS_STRDUP(node->result_ids[i]);
                if (!ids[i]) {
                    for (size_t j = 0; j < i; j++) AGENTOS_FREE(ids[j]);
                    AGENTOS_FREE(ids);
                    agentos_mutex_unlock(cache->lock);
                    return AGENTOS_ENOMEM;
                }
            }
            *out_result_ids = ids;
            *out_count = node->result_count;
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_SUCCESS;
        }
        
        node = next_node;
    }
    
    /* 未找到 */
    if (cache->enable_stats) {
        cache->stats.misses++;
        cache->stats.total_accesses++;
        cache->stats.hit_rate = (double)cache->stats.hits / (double)cache->stats.total_accesses;
        
        uint64_t elapsed = agentos_time_monotonic_ns() - start_time;
        double alpha = 0.1;
        cache->stats.avg_access_time_ns = 
            alpha * (double)elapsed + (1.0 - alpha) * cache->stats.avg_access_time_ns;
    }
    
    agentos_mutex_unlock(cache->lock);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_retrieval_cache_put(
    agentos_retrieval_cache_t* cache,
    const char* query,
    const char** result_ids,
    size_t result_count) {

    if (!cache || !query || !result_ids || result_count == 0) return AGENTOS_EINVAL;

    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%lx", hash_key(query));

    agentos_mutex_lock(cache->lock);

    // 如果已存在，更新并移动到头部
    cache_node_t* node = cache->head;
    while (node) {
        if (strcmp(node->key, key_buf) == 0) {
            // 释放旧结�?
            for (size_t i = 0; i < node->result_count; i++) AGENTOS_FREE(node->result_ids[i]);
            AGENTOS_FREE(node->result_ids);
            // 复制新结�?
            node->result_ids = (char**)AGENTOS_MALLOC(result_count * sizeof(char*));
            if (!node->result_ids) {
                agentos_mutex_unlock(cache->lock);
                return AGENTOS_ENOMEM;
            }
            for (size_t i = 0; i < result_count; i++) {
                node->result_ids[i] = AGENTOS_STRDUP(result_ids[i]);
                if (!node->result_ids[i]) {
                    for (size_t j = 0; j < i; j++) AGENTOS_FREE(node->result_ids[j]);
                    AGENTOS_FREE(node->result_ids);
                    node->result_ids = NULL;
                    node->result_count = 0;
                    agentos_mutex_unlock(cache->lock);
                    return AGENTOS_ENOMEM;
                }
            }
            node->result_count = result_count;
            node->timestamp = agentos_time_monotonic_ns();
            move_to_head(node, cache);
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_SUCCESS;
        }
        node = node->next;
    }

    // 创建新节�?
    node = (cache_node_t*)AGENTOS_CALLOC(1, sizeof(cache_node_t));
    if (!node) {
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    node->key = AGENTOS_STRDUP(key_buf);
    if (!node->key) {
        AGENTOS_FREE(node);
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    node->result_ids = (char**)AGENTOS_MALLOC(result_count * sizeof(char*));
    if (!node->result_ids) {
        AGENTOS_FREE(node->key);
        AGENTOS_FREE(node);
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    for (size_t i = 0; i < result_count; i++) {
        node->result_ids[i] = AGENTOS_STRDUP(result_ids[i]);
        if (!node->result_ids[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(node->result_ids[j]);
            AGENTOS_FREE(node->result_ids);
            AGENTOS_FREE(node->key);
            AGENTOS_FREE(node);
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_ENOMEM;
        }
    }
    node->result_count = result_count;
    node->timestamp = agentos_time_monotonic_ns();
    node->created_at = agentos_time_monotonic_ns();  /* 设置创建时间 */
    node->access_count = 1;                        /* 初始访问次数为1 */
    node->ttl_seconds = cache->default_ttl;         /* 使用默认TTL */

    /* 插入头部 */
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
    cache->current_size++;

    /* 如果超过最大容量，移除尾部 */
    if (cache->current_size > cache->max_size) {
        cache_node_t* tail = cache->tail;
        if (tail) {
            cache->tail = tail->prev;
            if (cache->tail) cache->tail->next = NULL;
            AGENTOS_FREE(tail->key);
            for (size_t i = 0; i < tail->result_count; i++) AGENTOS_FREE(tail->result_ids[i]);
            AGENTOS_FREE(tail->result_ids);
            AGENTOS_FREE(tail);
            cache->current_size--;
            
            if (cache->enable_stats) {
                cache->stats.evictions++;
            }
        }
    }

    agentos_mutex_unlock(cache->lock);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取缓存性能统计信息
 * @param cache 缓存实例
 * @param[out] stats 输出的统计信息结构体
 * @return 错误码
 */
agentos_error_t agentos_retrieval_cache_get_stats(
    agentos_retrieval_cache_t* cache,
    cache_stats_t** stats) {
    
    if (!cache || !stats) return AGENTOS_EINVAL;
    
    if (!cache->enable_stats) {
        /* 统计未启用 */
        *stats = NULL;
        return AGENTOS_EPERM;
    }
    
    agentos_mutex_lock(cache->lock);
    *stats = &cache->stats;
    agentos_mutex_unlock(cache->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置缓存统计信息
 * @param cache 缓存实例
 * @return 错误码
 */
agentos_error_t agentos_retrieval_cache_reset_stats(agentos_retrieval_cache_t* cache) {
    if (!cache) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cache->lock);
    memset(&cache->stats, 0, sizeof(cache_stats_t));
    cache->stats.last_stats_reset = agentos_time_monotonic_ns();
    agentos_mutex_unlock(cache->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 配置缓存默认TTL
 * @param cache 缓存实例
 * @param ttl_seconds TTL值（秒），0表示永不过期
 * @return 错误码
 */
agentos_error_t agentos_retrieval_cache_set_default_ttl(
    agentos_retrieval_cache_t* cache,
    uint32_t ttl_seconds) {
    
    if (!cache) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cache->lock);
    cache->default_ttl = ttl_seconds;
    agentos_mutex_unlock(cache->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 启用或禁用LFU辅助策略
 * @param cache 缓存实例
 * @param enable 是否启用（1=启用，0=禁用）
 * @return 错误码
 */
agentos_error_t agentos_retrieval_cache_enable_lfu(
    agentos_retrieval_cache_t* cache,
    int enable) {
    
    if (!cache) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cache->lock);
    cache->enable_lfu = enable;
    agentos_mutex_unlock(cache->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清除所有过期的缓存条目
 * @param cache 缓存实例
 * @param[out] cleared_count 清除的条目数（可选）
 * @return 错误码
 */
agentos_error_t agentos_retrieval_cache_purge_expired(
    agentos_retrieval_cache_t* cache,
    size_t* cleared_count) {
    
    if (!cache) return AGENTOS_EINVAL;
    
    size_t cleared = 0;
    uint64_t now = agentos_time_monotonic_ns();
    
    agentos_mutex_lock(cache->lock);
    cache_node_t* node = cache->head;
    
    while (node) {
        cache_node_t* next_node = node->next;
        
        if (node->ttl_seconds > 0) {
            uint64_t age_sec = (now - node->created_at) / 1000000000ULL;
            if (age_sec >= node->ttl_seconds) {
                /* 条目已过期，移除它 */
                if (node->prev) node->prev->next = node->next;
                if (node->next) node->next->prev = node->prev;
                if (cache->head == node) cache->head = node->next;
                if (cache->tail == node) cache->tail = node->prev;
                
                AGENTOS_FREE(node->key);
                if (node->result_ids) {
                    for (size_t i = 0; i < node->result_count; i++) {
                        AGENTOS_FREE(node->result_ids[i]);
                    }
                    AGENTOS_FREE(node->result_ids);
                }
                AGENTOS_FREE(node);
                cache->current_size--;
                cleared++;
                
                if (cache->enable_stats) {
                    cache->stats.expirations++;
                }
            }
        }
        
        node = next_node;
    }
    
    agentos_mutex_unlock(cache->lock);
    
    if (cleared_count) {
        *cleared_count = cleared;
    }
    
    return AGENTOS_SUCCESS;
}
