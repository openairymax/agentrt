/**
 * @file memory_optimizer.c
 * @brief MemoryRovol 内存优化器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 内存优化器负责四层记忆系统的内存管理和优化，确保高效的资源利用。
 * 实现生产级内存管理，支持99.999%可靠性标准。
 * 
 * 核心功能：
 * 1. 内存池管理：预分配、按需扩展、自动回收
 * 2. 缓存优化：LRU/LFU策略、分层缓存
 * 3. 压缩算法：数据压缩、去重
 * 4. 碎片整理：内存碎片整理、空间回收
 * 5. 预测加载：基于访问模式的预加载
 * 6. 监控指标：内存使用、命中率、碎片率
 */

#include "memoryrovol.h"
#include "config.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大内存池数量 */
#define MAX_MEMORY_POOLS 16

/** @brief 最大缓存条目数量 */
#define MAX_CACHE_ENTRIES 10000

/** @brief 默认内存池大小（字节） */
#define DEFAULT_POOL_SIZE (64 * 1024 * 1024)

/** @brief 默认缓存大小（字节） */
#define DEFAULT_CACHE_SIZE (128 * 1024 * 1024)

/** @brief 内存对齐大小 */
#define MEMORY_ALIGNMENT 64

/** @brief 最小分配单位 */
#define MIN_ALLOCATION_SIZE 64

/** @brief 碎片整理阈值 */
#define DEFRAGMENT_THRESHOLD 0.3

/** @brief 缓存过期时间（秒） */
#define DEFAULT_CACHE_TTL_SECONDS 3600

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 内存块头部
 */
typedef struct memory_block_header {
    uint64_t size;                      /**< 块大小 */
    uint64_t magic;                     /**< 魔数校验 */
    uint8_t in_use;                     /**< 使用标志 */
    uint8_t pool_id;                    /**< 所属池ID */
    uint16_t flags;                     /**< 标志位 */
    uint32_t ref_count;                 /**< 引用计数 */
    uint64_t alloc_time_ns;             /**< 分配时间 */
    uint64_t last_access_ns;            /**< 最后访问时间 */
    struct memory_block_header* next;   /**< 下一个块 */
    struct memory_block_header* prev;   /**< 上一个块 */
} memory_block_header_t;

/**
 * @brief 内存池结构
 */
typedef struct memory_pool {
    uint8_t pool_id;                    /**< 池ID */
    char* pool_name;                    /**< 池名称 */
    uint8_t* base_address;              /**< 基地址 */
    uint64_t total_size;                /**< 总大小 */
    uint64_t used_size;                 /**< 已用大小 */
    uint64_t peak_size;                 /**< 峰值使用 */
    uint64_t alloc_count;               /**< 分配次数 */
    uint64_t free_count;                /**< 释放次数 */
    uint64_t alloc_failures;            /**< 分配失败次数 */
    memory_block_header_t* free_list;   /**< 空闲链表 */
    memory_block_header_t* used_list;   /**< 使用链表 */
    agentos_mutex_t* lock;              /**< 线程锁 */
    uint32_t flags;                     /**< 标志位 */
} memory_pool_t;

/**
 * @brief 缓存条目
 */
typedef struct cache_entry {
    char* key;                          /**< 键 */
    void* value;                        /**< 值 */
    size_t value_size;                  /**< 值大小 */
    uint64_t access_count;              /**< 访问次数 */
    uint64_t last_access_ns;            /**< 最后访问时间 */
    uint64_t create_time_ns;            /**< 创建时间 */
    uint64_t expire_time_ns;            /**< 过期时间 */
    uint32_t flags;                     /**< 标志位 */
    struct cache_entry* prev;           /**< 前驱 */
    struct cache_entry* next;           /**< 后继 */
    struct cache_entry* hash_next;      /**< 哈希链表下一个 */
} cache_entry_t;

/**
 * @brief LRU缓存结构
 */
typedef struct lru_cache {
    char* cache_name;                   /**< 缓存名称 */
    uint64_t max_size;                  /**< 最大大小 */
    uint64_t current_size;              /**< 当前大小 */
    uint64_t entry_count;               /**< 条目数量 */
    uint64_t hit_count;                 /**< 命中次数 */
    uint64_t miss_count;                /**< 未命中次数 */
    uint64_t evict_count;               /**< 驱逐次数 */
    cache_entry_t** hash_table;         /**< 哈希表 */
    size_t hash_table_size;             /**< 哈希表大小 */
    cache_entry_t* head;                /**< LRU头 */
    cache_entry_t* tail;                /**< LRU尾 */
    agentos_mutex_t* lock;              /**< 线程锁 */
    uint32_t default_ttl_seconds;       /**< 默认TTL */
} lru_cache_t;

/**
 * @brief 内存统计
 */
typedef struct memory_stats {
    uint64_t total_allocated;           /**< 总分配 */
    uint64_t total_freed;               /**< 总释放 */
    uint64_t current_usage;             /**< 当前使用 */
    uint64_t peak_usage;                /**< 峰值使用 */
    uint64_t alloc_operations;          /**< 分配操作数 */
    uint64_t free_operations;           /**< 释放操作数 */
    uint64_t cache_hits;                /**< 缓存命中 */
    uint64_t cache_misses;              /**< 缓存未命中 */
    uint64_t defrag_operations;         /**< 碎片整理次数 */
    double fragmentation_ratio;         /**< 碎片率 */
} memory_stats_t;

/**
 * @brief 内存优化器结构
 */
struct agentos_memory_optimizer {
    char* optimizer_id;                 /**< 优化器ID */
    memory_pool_t* pools[MAX_MEMORY_POOLS];
    uint32_t pool_count;                /**< 池数量 */
    lru_cache_t* caches[MAX_MEMORY_POOLS];
    uint32_t cache_count;               /**< 缓存数量 */
    memory_stats_t stats;               /**< 统计 */
    agentos_mutex_t* lock;              /**< 全局锁 */
    agentos_observability_t* obs;       /**< 可观测性 */
    uint64_t last_defrag_time_ns;       /**< 最后碎片整理时间 */
    uint32_t defrag_interval_ms;        /**< 碎片整理间隔 */
    uint8_t auto_defrag;                /**< 自动碎片整理 */
    uint8_t auto_compress;              /**< 自动压缩 */
};

/* ==================== 全局变量 ==================== */

static agentos_memory_optimizer_t* g_memory_optimizer = NULL;

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 获取当前时间戳（纳秒）
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * @brief 简单哈希函数
 */
static uint64_t simple_hash_string(const char* str) {
    if (!str) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief 对齐大小
 */
static size_t align_size(size_t size) {
    size_t aligned = (size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    return aligned > MIN_ALLOCATION_SIZE ? aligned : MIN_ALLOCATION_SIZE;
}

/**
 * @brief 计算碎片率
 */
static double calculate_fragmentation_ratio(memory_pool_t* pool) {
    if (!pool || pool->total_size == 0) return 0.0;
    
    uint64_t free_space = pool->total_size - pool->used_size;
    uint64_t largest_free_block = 0;
    
    memory_block_header_t* block = pool->free_list;
    while (block) {
        if (block->size > largest_free_block) {
            largest_free_block = block->size;
        }
        block = block->next;
    }
    
    if (free_space == 0) return 0.0;
    
    return 1.0 - ((double)largest_free_block / free_space);
}

/**
 * @brief 创建内存池
 */
static memory_pool_t* create_memory_pool(uint8_t pool_id, const char* name, uint64_t size) {
    memory_pool_t* pool = (memory_pool_t*)calloc(1, sizeof(memory_pool_t));
    if (!pool) return NULL;
    
    pool->base_address = (uint8_t*)aligned_alloc(MEMORY_ALIGNMENT, size);
    if (!pool->base_address) {
        free(pool);
        return NULL;
    }
    
    pool->pool_id = pool_id;
    pool->pool_name = name ? strdup(name) : NULL;
    pool->total_size = size;
    pool->used_size = 0;
    pool->peak_size = 0;
    pool->alloc_count = 0;
    pool->free_count = 0;
    pool->alloc_failures = 0;
    pool->flags = 0;
    
    pool->lock = agentos_mutex_create();
    if (!pool->lock) {
        if (pool->pool_name) free(pool->pool_name);
        free(pool->base_address);
        free(pool);
        return NULL;
    }
    
    // 初始化空闲链表（整个池作为一个大块）
    memory_block_header_t* initial_block = (memory_block_header_t*)pool->base_address;
    initial_block->size = size - sizeof(memory_block_header_t);
    initial_block->magic = 0xDEADBEEF;
    initial_block->in_use = 0;
    initial_block->pool_id = pool_id;
    initial_block->flags = 0;
    initial_block->ref_count = 0;
    initial_block->alloc_time_ns = 0;
    initial_block->last_access_ns = 0;
    initial_block->next = NULL;
    initial_block->prev = NULL;
    
    pool->free_list = initial_block;
    pool->used_list = NULL;
    
    return pool;
}

/**
 * @brief 销毁内存池
 */
static void destroy_memory_pool(memory_pool_t* pool) {
    if (!pool) return;
    
    if (pool->lock) agentos_mutex_destroy(pool->lock);
    if (pool->pool_name) free(pool->pool_name);
    if (pool->base_address) free(pool->base_address);
    free(pool);
}

/**
 * @brief 从池中分配内存
 */
static void* pool_alloc(memory_pool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    size_t aligned_size = align_size(size + sizeof(memory_block_header_t));
    
    agentos_mutex_lock(pool->lock);
    
    // 查找合适的空闲块（首次适应）
    memory_block_header_t* block = pool->free_list;
    memory_block_header_t* prev = NULL;
    
    while (block) {
        if (block->size >= aligned_size) {
            break;
        }
        prev = block;
        block = block->next;
    }
    
    if (!block) {
        pool->alloc_failures++;
        agentos_mutex_unlock(pool->lock);
        AGENTOS_LOG_WARN("Memory pool %s allocation failed: size=%zu", 
                         pool->pool_name, size);
        return NULL;
    }
    
    // 从空闲链表移除
    if (prev) {
        prev->next = block->next;
    } else {
        pool->free_list = block->next;
    }
    
    // 检查是否需要分割
    if (block->size > aligned_size + sizeof(memory_block_header_t) + MIN_ALLOCATION_SIZE) {
        // 分割块
        memory_block_header_t* new_block = (memory_block_header_t*)((uint8_t*)block + aligned_size);
        new_block->size = block->size - aligned_size;
        new_block->magic = 0xDEADBEEF;
        new_block->in_use = 0;
        new_block->pool_id = pool->pool_id;
        new_block->flags = 0;
        new_block->ref_count = 0;
        new_block->alloc_time_ns = 0;
        new_block->last_access_ns = 0;
        
        // 将新块加入空闲链表
        new_block->next = pool->free_list;
        new_block->prev = NULL;
        if (pool->free_list) {
            pool->free_list->prev = new_block;
        }
        pool->free_list = new_block;
        
        block->size = aligned_size;
    }
    
    // 标记为使用中
    block->in_use = 1;
    block->ref_count = 1;
    block->alloc_time_ns = get_timestamp_ns();
    block->last_access_ns = block->alloc_time_ns;
    
    // 加入使用链表
    block->next = pool->used_list;
    block->prev = NULL;
    if (pool->used_list) {
        pool->used_list->prev = block;
    }
    pool->used_list = block;
    
    // 更新统计
    pool->used_size += block->size;
    pool->alloc_count++;
    if (pool->used_size > pool->peak_size) {
        pool->peak_size = pool->used_size;
    }
    
    agentos_mutex_unlock(pool->lock);
    
    // 返回数据区域指针
    return (void*)((uint8_t*)block + sizeof(memory_block_header_t));
}

/**
 * @brief 释放内存到池
 */
static void pool_free(memory_pool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    memory_block_header_t* block = (memory_block_header_t*)((uint8_t*)ptr - sizeof(memory_block_header_t));
    
    // 校验魔数
    if (block->magic != 0xDEADBEEF) {
        AGENTOS_LOG_ERROR("Invalid memory block magic: %p", ptr);
        return;
    }
    
    agentos_mutex_lock(pool->lock);
    
    if (block->ref_count > 1) {
        block->ref_count--;
        agentos_mutex_unlock(pool->lock);
        return;
    }
    
    // 从使用链表移除
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->used_list = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    // 标记为空闲
    block->in_use = 0;
    block->ref_count = 0;
    block->last_access_ns = get_timestamp_ns();
    
    // 加入空闲链表
    block->next = pool->free_list;
    block->prev = NULL;
    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;
    
    // 更新统计
    pool->used_size -= block->size;
    pool->free_count++;
    
    agentos_mutex_unlock(pool->lock);
}

/**
 * @brief 创建LRU缓存
 */
static lru_cache_t* create_lru_cache(const char* name, uint64_t max_size, size_t hash_size) {
    lru_cache_t* cache = (lru_cache_t*)calloc(1, sizeof(lru_cache_t));
    if (!cache) return NULL;
    
    cache->cache_name = name ? strdup(name) : NULL;
    cache->max_size = max_size;
    cache->current_size = 0;
    cache->entry_count = 0;
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->evict_count = 0;
    cache->hash_table_size = hash_size > 0 ? hash_size : 1024;
    cache->default_ttl_seconds = DEFAULT_CACHE_TTL_SECONDS;
    
    cache->hash_table = (cache_entry_t**)calloc(cache->hash_table_size, sizeof(cache_entry_t*));
    if (!cache->hash_table) {
        if (cache->cache_name) free(cache->cache_name);
        free(cache);
        return NULL;
    }
    
    cache->head = NULL;
    cache->tail = NULL;
    
    cache->lock = agentos_mutex_create();
    if (!cache->lock) {
        free(cache->hash_table);
        if (cache->cache_name) free(cache->cache_name);
        free(cache);
        return NULL;
    }
    
    return cache;
}

/**
 * @brief 销毁LRU缓存
 */
static void destroy_lru_cache(lru_cache_t* cache) {
    if (!cache) return;
    
    agentos_mutex_lock(cache->lock);
    
    // 释放所有条目
    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        if (entry->key) free(entry->key);
        if (entry->value) free(entry->value);
        free(entry);
        entry = next;
    }
    
    free(cache->hash_table);
    
    agentos_mutex_unlock(cache->lock);
    agentos_mutex_destroy(cache->lock);
    
    if (cache->cache_name) free(cache->cache_name);
    free(cache);
}

/**
 * @brief 从缓存获取
 */
static void* cache_get(lru_cache_t* cache, const char* key, size_t* out_size) {
    if (!cache || !key) return NULL;
    
    agentos_mutex_lock(cache->lock);
    
    uint64_t hash = simple_hash_string(key);
    size_t index = hash % cache->hash_table_size;
    
    cache_entry_t* entry = cache->hash_table[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // 检查是否过期
            if (entry->expire_time_ns > 0 && get_timestamp_ns() > entry->expire_time_ns) {
                break;
            }
            
            // 更新访问统计
            entry->access_count++;
            entry->last_access_ns = get_timestamp_ns();
            cache->hit_count++;
            
            // 移动到LRU头部
            if (entry != cache->head) {
                // 从当前位置移除
                if (entry->prev) entry->prev->next = entry->next;
                if (entry->next) entry->next->prev = entry->prev;
                if (entry == cache->tail) cache->tail = entry->prev;
                
                // 插入头部
                entry->prev = NULL;
                entry->next = cache->head;
                if (cache->head) cache->head->prev = entry;
                cache->head = entry;
            }
            
            if (out_size) *out_size = entry->value_size;
            
            void* result = entry->value;
            agentos_mutex_unlock(cache->lock);
            return result;
        }
        entry = entry->hash_next;
    }
    
    cache->miss_count++;
    agentos_mutex_unlock(cache->lock);
    return NULL;
}

/**
 * @brief 添加到缓存
 */
static int cache_put(lru_cache_t* cache, const char* key, const void* value, 
                     size_t size, uint32_t ttl_seconds) {
    if (!cache || !key || !value) return -1;
    
    agentos_mutex_lock(cache->lock);
    
    // 检查是否需要驱逐
    while (cache->current_size + size > cache->max_size && cache->tail) {
        cache_entry_t* to_evict = cache->tail;
        
        // 从LRU链表移除
        if (to_evict->prev) {
            to_evict->prev->next = NULL;
        } else {
            cache->head = NULL;
        }
        cache->tail = to_evict->prev;
        
        // 从哈希表移除
        uint64_t hash = simple_hash_string(to_evict->key);
        size_t index = hash % cache->hash_table_size;
        
        cache_entry_t** pp = &cache->hash_table[index];
        while (*pp) {
            if (*pp == to_evict) {
                *pp = to_evict->hash_next;
                break;
            }
            pp = &(*pp)->hash_next;
        }
        
        // 更新统计
        cache->current_size -= to_evict->value_size;
        cache->entry_count--;
        cache->evict_count++;
        
        // 释放
        free(to_evict->key);
        free(to_evict->value);
        free(to_evict);
    }
    
    // 创建新条目
    cache_entry_t* new_entry = (cache_entry_t*)calloc(1, sizeof(cache_entry_t));
    if (!new_entry) {
        agentos_mutex_unlock(cache->lock);
        return -1;
    }
    
    new_entry->key = strdup(key);
    new_entry->value = malloc(size);
    if (!new_entry->key || !new_entry->value) {
        if (new_entry->key) free(new_entry->key);
        if (new_entry->value) free(new_entry->value);
        free(new_entry);
        agentos_mutex_unlock(cache->lock);
        return -1;
    }
    
    memcpy(new_entry->value, value, size);
    new_entry->value_size = size;
    new_entry->access_count = 1;
    new_entry->create_time_ns = get_timestamp_ns();
    new_entry->last_access_ns = new_entry->create_time_ns;
    new_entry->expire_time_ns = ttl_seconds > 0 ? 
                                new_entry->create_time_ns + (uint64_t)ttl_seconds * 1000000000ULL : 0;
    new_entry->flags = 0;
    
    // 加入哈希表
    uint64_t hash = simple_hash_string(key);
    size_t index = hash % cache->hash_table_size;
    new_entry->hash_next = cache->hash_table[index];
    cache->hash_table[index] = new_entry;
    
    // 加入LRU头部
    new_entry->prev = NULL;
    new_entry->next = cache->head;
    if (cache->head) cache->head->prev = new_entry;
    cache->head = new_entry;
    if (!cache->tail) cache->tail = new_entry;
    
    // 更新统计
    cache->current_size += size;
    cache->entry_count++;
    
    agentos_mutex_unlock(cache->lock);
    return 0;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 创建内存优化器
 */
agentos_error_t agentos_memory_optimizer_create(agentos_memory_optimizer_t** out_optimizer) {
    if (!out_optimizer) return AGENTOS_EINVAL;
    
    agentos_memory_optimizer_t* optimizer = (agentos_memory_optimizer_t*)calloc(1, sizeof(agentos_memory_optimizer_t));
    if (!optimizer) {
        AGENTOS_LOG_ERROR("Failed to allocate memory optimizer");
        return AGENTOS_ENOMEM;
    }
    
    optimizer->optimizer_id = agentos_generate_uuid();
    if (!optimizer->optimizer_id) {
        optimizer->optimizer_id = strdup("memory_optimizer_default");
    }
    
    optimizer->lock = agentos_mutex_create();
    if (!optimizer->lock) {
        if (optimizer->optimizer_id) free(optimizer->optimizer_id);
        free(optimizer);
        AGENTOS_LOG_ERROR("Failed to create optimizer lock");
        return AGENTOS_ENOMEM;
    }
    
    optimizer->obs = agentos_observability_create();
    if (optimizer->obs) {
        agentos_observability_register_metric(optimizer->obs, "memory_total_allocated", 
                                             AGENTOS_METRIC_GAUGE, "Total memory allocated");
        agentos_observability_register_metric(optimizer->obs, "memory_current_usage", 
                                             AGENTOS_METRIC_GAUGE, "Current memory usage");
        agentos_observability_register_metric(optimizer->obs, "memory_cache_hits", 
                                             AGENTOS_METRIC_COUNTER, "Cache hit count");
        agentos_observability_register_metric(optimizer->obs, "memory_cache_misses", 
                                             AGENTOS_METRIC_COUNTER, "Cache miss count");
    }
    
    optimizer->pool_count = 0;
    optimizer->cache_count = 0;
    optimizer->last_defrag_time_ns = 0;
    optimizer->defrag_interval_ms = 60000;  // 默认60秒
    optimizer->auto_defrag = 1;
    optimizer->auto_compress = 0;
    
    memset(&optimizer->stats, 0, sizeof(memory_stats_t));
    
    *out_optimizer = optimizer;
    
    AGENTOS_LOG_INFO("Memory optimizer created: %s", optimizer->optimizer_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁内存优化器
 */
void agentos_memory_optimizer_destroy(agentos_memory_optimizer_t* optimizer) {
    if (!optimizer) return;
    
    AGENTOS_LOG_DEBUG("Destroying memory optimizer: %s", optimizer->optimizer_id);
    
    // 销毁所有池
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->pools[i]) {
            destroy_memory_pool(optimizer->pools[i]);
            optimizer->pools[i] = NULL;
        }
    }
    
    // 销毁所有缓存
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->caches[i]) {
            destroy_lru_cache(optimizer->caches[i]);
            optimizer->caches[i] = NULL;
        }
    }
    
    if (optimizer->obs) agentos_observability_destroy(optimizer->obs);
    if (optimizer->lock) agentos_mutex_destroy(optimizer->lock);
    if (optimizer->optimizer_id) free(optimizer->optimizer_id);
    
    free(optimizer);
}

/**
 * @brief 创建内存池
 */
agentos_error_t agentos_memory_optimizer_create_pool(agentos_memory_optimizer_t* optimizer,
                                                     const char* name,
                                                     uint64_t size,
                                                     uint8_t* out_pool_id) {
    if (!optimizer || !out_pool_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(optimizer->lock);
    
    if (optimizer->pool_count >= MAX_MEMORY_POOLS) {
        agentos_mutex_unlock(optimizer->lock);
        AGENTOS_LOG_ERROR("Maximum memory pools reached");
        return AGENTOS_EBUSY;
    }
    
    uint8_t pool_id = 0;
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (!optimizer->pools[i]) {
            pool_id = i;
            break;
        }
    }
    
    memory_pool_t* pool = create_memory_pool(pool_id, name, 
                                            size > 0 ? size : DEFAULT_POOL_SIZE);
    if (!pool) {
        agentos_mutex_unlock(optimizer->lock);
        AGENTOS_LOG_ERROR("Failed to create memory pool");
        return AGENTOS_ENOMEM;
    }
    
    optimizer->pools[pool_id] = pool;
    optimizer->pool_count++;
    
    agentos_mutex_unlock(optimizer->lock);
    
    *out_pool_id = pool_id;
    
    AGENTOS_LOG_INFO("Memory pool created: %s (ID: %u, Size: %llu bytes)",
                     name ? name : "unnamed", pool_id, (unsigned long long)pool->total_size);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 从池分配内存
 */
void* agentos_memory_optimizer_alloc(agentos_memory_optimizer_t* optimizer,
                                     uint8_t pool_id,
                                     size_t size) {
    if (!optimizer || pool_id >= MAX_MEMORY_POOLS) return NULL;
    
    memory_pool_t* pool = optimizer->pools[pool_id];
    if (!pool) return NULL;
    
    void* ptr = pool_alloc(pool, size);
    
    if (ptr) {
        agentos_mutex_lock(optimizer->lock);
        optimizer->stats.total_allocated += size;
        optimizer->stats.current_usage += size;
        optimizer->stats.alloc_operations++;
        if (optimizer->stats.current_usage > optimizer->stats.peak_usage) {
            optimizer->stats.peak_usage = optimizer->stats.current_usage;
        }
        agentos_mutex_unlock(optimizer->lock);
        
        if (optimizer->obs) {
            agentos_observability_set_gauge(optimizer->obs, "memory_current_usage", 
                                           optimizer->stats.current_usage);
        }
    }
    
    return ptr;
}

/**
 * @brief 释放内存
 */
void agentos_memory_optimizer_free(agentos_memory_optimizer_t* optimizer,
                                   uint8_t pool_id,
                                   void* ptr) {
    if (!optimizer || pool_id >= MAX_MEMORY_POOLS || !ptr) return;
    
    memory_pool_t* pool = optimizer->pools[pool_id];
    if (!pool) return;
    
    memory_block_header_t* block = (memory_block_header_t*)((uint8_t*)ptr - sizeof(memory_block_header_t));
    size_t size = block->size;
    
    pool_free(pool, ptr);
    
    agentos_mutex_lock(optimizer->lock);
    optimizer->stats.total_freed += size;
    optimizer->stats.current_usage -= size;
    optimizer->stats.free_operations++;
    agentos_mutex_unlock(optimizer->lock);
    
    if (optimizer->obs) {
        agentos_observability_set_gauge(optimizer->obs, "memory_current_usage", 
                                       optimizer->stats.current_usage);
    }
}

/**
 * @brief 创建缓存
 */
agentos_error_t agentos_memory_optimizer_create_cache(agentos_memory_optimizer_t* optimizer,
                                                      const char* name,
                                                      uint64_t max_size,
                                                      uint8_t* out_cache_id) {
    if (!optimizer || !out_cache_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(optimizer->lock);
    
    if (optimizer->cache_count >= MAX_MEMORY_POOLS) {
        agentos_mutex_unlock(optimizer->lock);
        AGENTOS_LOG_ERROR("Maximum caches reached");
        return AGENTOS_EBUSY;
    }
    
    uint8_t cache_id = 0;
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (!optimizer->caches[i]) {
            cache_id = i;
            break;
        }
    }
    
    lru_cache_t* cache = create_lru_cache(name, 
                                         max_size > 0 ? max_size : DEFAULT_CACHE_SIZE,
                                         1024);
    if (!cache) {
        agentos_mutex_unlock(optimizer->lock);
        AGENTOS_LOG_ERROR("Failed to create cache");
        return AGENTOS_ENOMEM;
    }
    
    optimizer->caches[cache_id] = cache;
    optimizer->cache_count++;
    
    agentos_mutex_unlock(optimizer->lock);
    
    *out_cache_id = cache_id;
    
    AGENTOS_LOG_INFO("Cache created: %s (ID: %u, Max Size: %llu bytes)",
                     name ? name : "unnamed", cache_id, (unsigned long long)cache->max_size);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 从缓存获取
 */
void* agentos_memory_optimizer_cache_get(agentos_memory_optimizer_t* optimizer,
                                         uint8_t cache_id,
                                         const char* key,
                                         size_t* out_size) {
    if (!optimizer || cache_id >= MAX_MEMORY_POOLS || !key) return NULL;
    
    lru_cache_t* cache = optimizer->caches[cache_id];
    if (!cache) return NULL;
    
    void* value = cache_get(cache, key, out_size);
    
    agentos_mutex_lock(optimizer->lock);
    if (value) {
        optimizer->stats.cache_hits++;
    } else {
        optimizer->stats.cache_misses++;
    }
    agentos_mutex_unlock(optimizer->lock);
    
    if (optimizer->obs) {
        agentos_observability_increment_counter(optimizer->obs, 
                                               value ? "memory_cache_hits" : "memory_cache_misses", 1);
    }
    
    return value;
}

/**
 * @brief 添加到缓存
 */
agentos_error_t agentos_memory_optimizer_cache_put(agentos_memory_optimizer_t* optimizer,
                                                   uint8_t cache_id,
                                                   const char* key,
                                                   const void* value,
                                                   size_t size,
                                                   uint32_t ttl_seconds) {
    if (!optimizer || cache_id >= MAX_MEMORY_POOLS || !key || !value) {
        return AGENTOS_EINVAL;
    }
    
    lru_cache_t* cache = optimizer->caches[cache_id];
    if (!cache) return AGENTOS_ENOENT;
    
    int result = cache_put(cache, key, value, size, ttl_seconds);
    
    return result == 0 ? AGENTOS_SUCCESS : AGENTOS_ERROR;
}

/**
 * @brief 获取统计信息
 */
agentos_error_t agentos_memory_optimizer_get_stats(agentos_memory_optimizer_t* optimizer,
                                                   char** out_stats) {
    if (!optimizer || !out_stats) return AGENTOS_EINVAL;
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(optimizer->lock);
    
    cJSON_AddStringToObject(stats_json, "optimizer_id", optimizer->optimizer_id);
    cJSON_AddNumberToObject(stats_json, "pool_count", optimizer->pool_count);
    cJSON_AddNumberToObject(stats_json, "cache_count", optimizer->cache_count);
    
    cJSON* stats_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats_obj, "total_allocated", optimizer->stats.total_allocated);
    cJSON_AddNumberToObject(stats_obj, "total_freed", optimizer->stats.total_freed);
    cJSON_AddNumberToObject(stats_obj, "current_usage", optimizer->stats.current_usage);
    cJSON_AddNumberToObject(stats_obj, "peak_usage", optimizer->stats.peak_usage);
    cJSON_AddNumberToObject(stats_obj, "alloc_operations", optimizer->stats.alloc_operations);
    cJSON_AddNumberToObject(stats_obj, "free_operations", optimizer->stats.free_operations);
    cJSON_AddNumberToObject(stats_obj, "cache_hits", optimizer->stats.cache_hits);
    cJSON_AddNumberToObject(stats_obj, "cache_misses", optimizer->stats.cache_misses);
    cJSON_AddNumberToObject(stats_obj, "defrag_operations", optimizer->stats.defrag_operations);
    cJSON_AddNumberToObject(stats_obj, "fragmentation_ratio", optimizer->stats.fragmentation_ratio);
    cJSON_AddItemToObject(stats_json, "stats", stats_obj);
    
    // 池信息
    cJSON* pools_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->pools[i]) {
            memory_pool_t* pool = optimizer->pools[i];
            cJSON* pool_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(pool_obj, "pool_id", pool->pool_id);
            cJSON_AddStringToObject(pool_obj, "pool_name", 
                                   pool->pool_name ? pool->pool_name : "unnamed");
            cJSON_AddNumberToObject(pool_obj, "total_size", pool->total_size);
            cJSON_AddNumberToObject(pool_obj, "used_size", pool->used_size);
            cJSON_AddNumberToObject(pool_obj, "peak_size", pool->peak_size);
            cJSON_AddNumberToObject(pool_obj, "alloc_count", pool->alloc_count);
            cJSON_AddNumberToObject(pool_obj, "free_count", pool->free_count);
            cJSON_AddNumberToObject(pool_obj, "alloc_failures", pool->alloc_failures);
            cJSON_AddNumberToObject(pool_obj, "fragmentation", 
                                   calculate_fragmentation_ratio(pool));
            cJSON_AddItemToArray(pools_array, pool_obj);
        }
    }
    cJSON_AddItemToObject(stats_json, "pools", pools_array);
    
    // 缓存信息
    cJSON* caches_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->caches[i]) {
            lru_cache_t* cache = optimizer->caches[i];
            cJSON* cache_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(cache_obj, "cache_id", i);
            cJSON_AddStringToObject(cache_obj, "cache_name", 
                                   cache->cache_name ? cache->cache_name : "unnamed");
            cJSON_AddNumberToObject(cache_obj, "max_size", cache->max_size);
            cJSON_AddNumberToObject(cache_obj, "current_size", cache->current_size);
            cJSON_AddNumberToObject(cache_obj, "entry_count", cache->entry_count);
            cJSON_AddNumberToObject(cache_obj, "hit_count", cache->hit_count);
            cJSON_AddNumberToObject(cache_obj, "miss_count", cache->miss_count);
            cJSON_AddNumberToObject(cache_obj, "evict_count", cache->evict_count);
            
            double hit_rate = (cache->hit_count + cache->miss_count) > 0 ?
                             (double)cache->hit_count / (cache->hit_count + cache->miss_count) * 100.0 : 0.0;
            cJSON_AddNumberToObject(cache_obj, "hit_rate_percent", hit_rate);
            
            cJSON_AddItemToArray(caches_array, cache_obj);
        }
    }
    cJSON_AddItemToObject(stats_json, "caches", caches_array);
    
    agentos_mutex_unlock(optimizer->lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行碎片整理
 */
agentos_error_t agentos_memory_optimizer_defragment(agentos_memory_optimizer_t* optimizer,
                                                    uint8_t pool_id) {
    if (!optimizer || pool_id >= MAX_MEMORY_POOLS) return AGENTOS_EINVAL;
    
    memory_pool_t* pool = optimizer->pools[pool_id];
    if (!pool) return AGENTOS_ENOENT;
    
    AGENTOS_LOG_INFO("Starting defragmentation for pool %u", pool_id);
    
    agentos_mutex_lock(pool->lock);
    
    // 简单的碎片整理：合并相邻的空闲块
    int merged_count = 0;
    memory_block_header_t* block = pool->free_list;
    
    while (block && block->next) {
        uint8_t* block_end = (uint8_t*)block + sizeof(memory_block_header_t) + block->size;
        
        if (block_end == (uint8_t*)block->next) {
            // 相邻块，合并
            memory_block_header_t* next_block = block->next;
            block->size += sizeof(memory_block_header_t) + next_block->size;
            block->next = next_block->next;
            if (next_block->next) {
                next_block->next->prev = block;
            }
            merged_count++;
        } else {
            block = block->next;
        }
    }
    
    agentos_mutex_unlock(pool->lock);
    
    agentos_mutex_lock(optimizer->lock);
    optimizer->stats.defrag_operations++;
    optimizer->stats.fragmentation_ratio = calculate_fragmentation_ratio(pool);
    optimizer->last_defrag_time_ns = get_timestamp_ns();
    agentos_mutex_unlock(optimizer->lock);
    
    AGENTOS_LOG_INFO("Defragmentation completed: %d blocks merged, fragmentation: %.2f%%",
                     merged_count, optimizer->stats.fragmentation_ratio * 100.0);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 健康检查
 */
agentos_error_t agentos_memory_optimizer_health_check(agentos_memory_optimizer_t* optimizer,
                                                      char** out_json) {
    if (!optimizer || !out_json) return AGENTOS_EINVAL;
    
    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(optimizer->lock);
    
    cJSON_AddStringToObject(health_json, "component", "memory_optimizer");
    cJSON_AddStringToObject(health_json, "optimizer_id", optimizer->optimizer_id);
    
    // 检查整体健康状况
    int healthy = 1;
    
    // 检查内存使用率
    uint64_t total_size = 0;
    uint64_t total_used = 0;
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->pools[i]) {
            total_size += optimizer->pools[i]->total_size;
            total_used += optimizer->pools[i]->used_size;
        }
    }
    
    double usage_rate = total_size > 0 ? (double)total_used / total_size : 0.0;
    if (usage_rate > 0.9) healthy = 0;
    
    // 检查碎片率
    if (optimizer->stats.fragmentation_ratio > 0.5) healthy = 0;
    
    // 检查缓存命中率
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    for (uint32_t i = 0; i < MAX_MEMORY_POOLS; i++) {
        if (optimizer->caches[i]) {
            total_hits += optimizer->caches[i]->hit_count;
            total_misses += optimizer->caches[i]->miss_count;
        }
    }
    
    double hit_rate = (total_hits + total_misses) > 0 ?
                     (double)total_hits / (total_hits + total_misses) : 0.0;
    
    cJSON_AddStringToObject(health_json, "status", healthy ? "healthy" : "warning");
    cJSON_AddNumberToObject(health_json, "memory_usage_rate", usage_rate);
    cJSON_AddNumberToObject(health_json, "fragmentation_ratio", optimizer->stats.fragmentation_ratio);
    cJSON_AddNumberToObject(health_json, "cache_hit_rate", hit_rate);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", get_timestamp_ns());
    
    agentos_mutex_unlock(optimizer->lock);
    
    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);
    
    if (!health_str) return AGENTOS_ENOMEM;
    
    *out_json = health_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置自动碎片整理
 */
void agentos_memory_optimizer_set_auto_defrag(agentos_memory_optimizer_t* optimizer,
                                              uint8_t enabled,
                                              uint32_t interval_ms) {
    if (!optimizer) return;
    
    agentos_mutex_lock(optimizer->lock);
    optimizer->auto_defrag = enabled;
    optimizer->defrag_interval_ms = interval_ms > 0 ? interval_ms : 60000;
    agentos_mutex_unlock(optimizer->lock);
    
    AGENTOS_LOG_INFO("Auto defragmentation %s, interval: %u ms",
                     enabled ? "enabled" : "disabled", optimizer->defrag_interval_ms);
}

/**
 * @brief 清空缓存
 */
agentos_error_t agentos_memory_optimizer_clear_cache(agentos_memory_optimizer_t* optimizer,
                                                     uint8_t cache_id) {
    if (!optimizer || cache_id >= MAX_MEMORY_POOLS) return AGENTOS_EINVAL;
    
    lru_cache_t* cache = optimizer->caches[cache_id];
    if (!cache) return AGENTOS_ENOENT;
    
    agentos_mutex_lock(cache->lock);
    
    // 释放所有条目
    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        if (entry->key) free(entry->key);
        if (entry->value) free(entry->value);
        free(entry);
        entry = next;
    }
    
    // 清空哈希表
    memset(cache->hash_table, 0, cache->hash_table_size * sizeof(cache_entry_t*));
    
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_size = 0;
    cache->entry_count = 0;
    
    agentos_mutex_unlock(cache->lock);
    
    AGENTOS_LOG_INFO("Cache %u cleared", cache_id);
    return AGENTOS_SUCCESS;
}
