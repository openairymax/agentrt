/**
 * @file ratelimit.c
 * @brief 请求速率限制器实现
 * 
 * 使用滑动窗口算法实现请求速率限制，
 * 支持突发流量处理。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ratelimit.h"
#include "logger.h"
#include "platform.h"
#include "utils/hash.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* 最大客户端条目数 */
#define MAX_CLIENTS 4096

/* 客户端条目结构 */
typedef struct client_entry {
    char                client_id[64];    /**< 客户端标识 */
    atomic_uint         request_count;   /**< 当前窗口请求数 */
    uint64_t            window_start_ns; /**< 窗口起始时间 */
    struct client_entry* next;           /**< 哈希表链表指针 */
} client_entry_t;

/* 速率限制器结构 */
struct ratelimiter {
    ratelimit_config_t   manager;           /**< 配置 */
    client_entry_t**     buckets;          /**< 哈希桶数组 */
    size_t               bucket_count;     /**< 桶数量 */
    
    platform_mutex_t     lock;             /**< 全局锁 */
    atomic_size_t        client_count;     /**< 当前客户端数 */
    
    atomic_uint_fast64_t total_allowed;    /**< 总允许请求数 */
    atomic_uint_fast64_t total_denied;     /**< 总拒绝请求数 */
};

/* ========== 哈希函数（使用公共模块） ========== */

/**
 * @brief 客户端 ID 哈希函数
 */
static size_t hash_client_id(const char* id, size_t bucket_count) {
    return hash_string_mod(id, bucket_count);
}

/* ========== 公共 API 实现 ========== */

ratelimiter_t* ratelimiter_create(const ratelimit_config_t* manager) {
    ratelimiter_t* limiter = (ratelimiter_t*)calloc(1, sizeof(ratelimiter_t));
    if (!limiter) return NULL;
    
    /* 设置配置 */
    if (manager) {
        limiter->manager = *manager;
    } else {
        /* 默认配置 */
        limiter->manager.requests_per_second = 100;
        limiter->manager.burst_size = 20;
        limiter->manager.window_ms = 1000;
        limiter->manager.enabled = true;
    }
    
    /* 计算桶数量 */
    limiter->bucket_count = MAX_CLIENTS / 4;
    if (limiter->bucket_count < 16) limiter->bucket_count = 16;
    if (limiter->bucket_count > 1024) limiter->bucket_count = 1024;
    
    /* 分配桶数组 */
    limiter->buckets = (client_entry_t**)calloc(limiter->bucket_count, sizeof(client_entry_t*));
    if (!limiter->buckets) {
        free(limiter);
        return NULL;
    }
    
    /* 初始化同步原语 */
    if (platform_mutex_init(&limiter->lock) != 0) {
        free(limiter->buckets);
        free(limiter);
        return NULL;
    }
    
    atomic_init(&limiter->client_count, 0);
    atomic_init(&limiter->total_allowed, 0);
    atomic_init(&limiter->total_denied, 0);
    
    AGENTOS_LOG_INFO("Rate limiter created: %u req/s, burst=%u, window=%ums",
        limiter->manager.requests_per_second,
        limiter->manager.burst_size,
        limiter->manager.window_ms);
    
    return limiter;
}

ratelimiter_t* ratelimiter_create_simple(uint32_t max_requests, uint32_t window_seconds) {
    ratelimit_config_t manager = {
        .requests_per_second = max_requests / (window_seconds > 0 ? window_seconds : 1),
        .burst_size = max_requests / 10,
        .window_ms = window_seconds * 1000,
        .enabled = true
    };
    return ratelimiter_create(&manager);
}

void ratelimiter_destroy(ratelimiter_t* limiter) {
    if (!limiter) return;
    
    /* 释放所有条目 */
    platform_mutex_lock(&limiter->lock);
    
    for (size_t i = 0; i < limiter->bucket_count; i++) {
        client_entry_t* entry = limiter->buckets[i];
        while (entry) {
            client_entry_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
    platform_mutex_unlock(&limiter->lock);
    
    platform_mutex_destroy(&limiter->lock);
    free(limiter->buckets);
    free(limiter);
    
    AGENTOS_LOG_INFO("Rate limiter destroyed");
}

ratelimit_result_t ratelimiter_check(ratelimiter_t* limiter, const char* client_id) {
    if (!limiter || !client_id) {
        return RATELIMIT_ALLOWED;
    }
    
    if (!limiter->manager.enabled) {
        return RATELIMIT_DISABLED;
    }
    
    uint64_t now_ns = agentos_time_monotonic_ns();
    uint64_t window_ns = (uint64_t)limiter->manager.window_ms * 1000000ULL;
    
    size_t bucket = hash_client_id(client_id, limiter->bucket_count);
    
    platform_mutex_lock(&limiter->lock);
    
    /* 查找或创建条目 */
    client_entry_t* entry = limiter->buckets[bucket];
    client_entry_t** pp = &limiter->buckets[bucket];
    
    while (entry && strcmp(entry->client_id, client_id) != 0) {
        pp = &entry->next;
        entry = entry->next;
    }
    
    if (!entry) {
        /* 创建新条目 */
        if (atomic_load(&limiter->client_count) >= MAX_CLIENTS) {
            platform_mutex_unlock(&limiter->lock);
            atomic_fetch_add(&limiter->total_denied, 1);
            return RATELIMIT_DENIED;
        }
        
        entry = (client_entry_t*)calloc(1, sizeof(client_entry_t));
        if (!entry) {
            platform_mutex_unlock(&limiter->lock);
            atomic_fetch_add(&limiter->total_denied, 1);
            return RATELIMIT_DENIED;
        }
        
        strncpy(entry->client_id, client_id, sizeof(entry->client_id) - 1);
        entry->window_start_ns = now_ns;
        atomic_init(&entry->request_count, 0);
        
        *pp = entry;
        atomic_fetch_add(&limiter->client_count, 1);
    }
    
    /* 检查是否需要重置窗口 */
    if ((now_ns - entry->window_start_ns) > window_ns) {
        atomic_store(&entry->request_count, 0);
        entry->window_start_ns = now_ns;
    }
    
    /* 检查限制 */
    uint32_t count = atomic_load(&entry->request_count);
    uint32_t limit = limiter->manager.requests_per_second + limiter->manager.burst_size;
    
    if (count >= limit) {
        platform_mutex_unlock(&limiter->lock);
        atomic_fetch_add(&limiter->total_denied, 1);
        return RATELIMIT_DENIED;
    }
    
    /* 增加计数 */
    atomic_fetch_add(&entry->request_count, 1);
    
    platform_mutex_unlock(&limiter->lock);
    
    atomic_fetch_add(&limiter->total_allowed, 1);
    return RATELIMIT_ALLOWED;
}

agentos_error_t ratelimiter_reset(ratelimiter_t* limiter, const char* client_id) {
    if (!limiter || !client_id) return AGENTOS_EINVAL;
    
    size_t bucket = hash_client_id(client_id, limiter->bucket_count);
    
    platform_mutex_lock(&limiter->lock);
    
    client_entry_t* entry = limiter->buckets[bucket];
    while (entry && strcmp(entry->client_id, client_id) != 0) {
        entry = entry->next;
    }
    
    if (entry) {
        atomic_store(&entry->request_count, 0);
        entry->window_start_ns = agentos_time_monotonic_ns();
    }
    
    platform_mutex_unlock(&limiter->lock);
    
    return AGENTOS_SUCCESS;
}

uint32_t ratelimiter_get_count(ratelimiter_t* limiter, const char* client_id) {
    if (!limiter || !client_id) return 0;
    
    size_t bucket = hash_client_id(client_id, limiter->bucket_count);
    
    platform_mutex_lock(&limiter->lock);
    
    client_entry_t* entry = limiter->buckets[bucket];
    while (entry && strcmp(entry->client_id, client_id) != 0) {
        entry = entry->next;
    }
    
    uint32_t count = entry ? atomic_load(&entry->request_count) : 0;
    
    platform_mutex_unlock(&limiter->lock);
    
    return count;
}

size_t ratelimiter_cleanup(ratelimiter_t* limiter) {
    if (!limiter) return 0;
    
    uint64_t now_ns = agentos_time_monotonic_ns();
    uint64_t window_ns = (uint64_t)limiter->manager.window_ms * 1000000ULL;
    size_t cleaned = 0;
    
    platform_mutex_lock(&limiter->lock);
    
    for (size_t i = 0; i < limiter->bucket_count; i++) {
        client_entry_t** pp = &limiter->buckets[i];
        
        while (*pp) {
            client_entry_t* entry = *pp;
            
            /* 清理过期条目 */
            if ((now_ns - entry->window_start_ns) > window_ns * 2) {
                *pp = entry->next;
                free(entry);
                atomic_fetch_sub(&limiter->client_count, 1);
                cleaned++;
            } else {
                pp = &entry->next;
            }
        }
    }
    
    platform_mutex_unlock(&limiter->lock);
    
    if (cleaned > 0) {
        AGENTOS_LOG_DEBUG("Rate limiter cleaned %zu expired entries", cleaned);
    }
    
    return cleaned;
}

agentos_error_t ratelimiter_get_stats(
    ratelimiter_t* limiter,
    char** out_json) {
    
    if (!limiter || !out_json) return AGENTOS_EINVAL;
    
    char* json = NULL;
    int len = asprintf(&json,
        "{"
        "\"enabled\":%s,"
        "\"requests_per_second\":%u,"
        "\"burst_size\":%u,"
        "\"window_ms\":%u,"
        "\"clients\":%zu,"
        "\"total_allowed\":%llu,"
        "\"total_denied\":%llu"
        "}",
        limiter->manager.enabled ? "true" : "false",
        limiter->manager.requests_per_second,
        limiter->manager.burst_size,
        limiter->manager.window_ms,
        atomic_load(&limiter->client_count),
        (unsigned long long)atomic_load(&limiter->total_allowed),
        (unsigned long long)atomic_load(&limiter->total_denied));
    
    if (len < 0 || !json) {
        return AGENTOS_ENOMEM;
    }
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}
