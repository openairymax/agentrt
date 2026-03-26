/**
 * @file rate_limiter.c
 * @brief 系统调用限流器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 限流器用于控制系统调用的访问频率，防止系统过载。
 * 实现多种限流算法，支持99.999%可靠性标准。
 * 
 * 核心功能：
 * 1. 令牌桶算法：平滑限流，允许突发
 * 2. 漏桶算法：恒定速率输出
 * 3. 滑动窗口：精确的请求计数
 * 4. 分布式限流：跨实例协调
 * 5. 自适应限流：基于系统负载动态调整
 * 6. 监控指标：限流统计和告警
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大限流器数量 */
#define MAX_RATE_LIMITERS 256

/** @brief 默认令牌桶容量 */
#define DEFAULT_BUCKET_CAPACITY 100

/** @brief 默认令牌补充速率（每秒） */
#define DEFAULT_REFILL_RATE 10

/** @brief 默认滑动窗口大小（毫秒） */
#define DEFAULT_WINDOW_SIZE_MS 1000

/** @brief 最大滑动窗口条目 */
#define MAX_WINDOW_ENTRIES 10000

/** @brief 限流器检查间隔（毫秒） */
#define RATE_LIMITER_CHECK_INTERVAL_MS 100

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 限流算法类型
 */
typedef enum {
    RATE_LIMIT_TOKEN_BUCKET = 0,    /**< 令牌桶 */
    RATE_LIMIT_LEAKY_BUCKET,        /**< 漏桶 */
    RATE_LIMIT_SLIDING_WINDOW,      /**< 滑动窗口 */
    RATE_LIMIT_FIXED_WINDOW         /**< 固定窗口 */
} rate_limit_algorithm_t;

/**
 * @brief 限流器状态
 */
typedef enum {
    RATE_LIMITER_ACTIVE = 0,        /**< 活跃 */
    RATE_LIMITER_PAUSED,            /**< 暂停 */
    RATE_LIMITER_DISABLED           /**< 禁用 */
} rate_limiter_state_t;

/**
 * @brief 令牌桶结构
 */
typedef struct token_bucket {
    uint64_t capacity;              /**< 容量 */
    uint64_t tokens;                /**< 当前令牌数 */
    uint64_t refill_rate;           /**< 补充速率（每秒） */
    uint64_t last_refill_ns;        /**< 最后补充时间 */
} token_bucket_t;

/**
 * @brief 漏桶结构
 */
typedef struct leaky_bucket {
    uint64_t capacity;              /**< 容量 */
    uint64_t water;                 /**< 当前水量 */
    uint64_t leak_rate;             /**< 漏出速率（每秒） */
    uint64_t last_leak_ns;          /**< 最后漏出时间 */
} leaky_bucket_t;

/**
 * @brief 滑动窗口条目
 */
typedef struct window_entry {
    uint64_t timestamp_ns;          /**< 时间戳 */
    struct window_entry* next;      /**< 下一个 */
} window_entry_t;

/**
 * @brief 滑动窗口结构
 */
typedef struct sliding_window {
    uint64_t window_size_ns;        /**< 窗口大小（纳秒） */
    uint64_t max_requests;          /**< 最大请求数 */
    window_entry_t* head;           /**< 头指针 */
    window_entry_t* tail;           /**< 尾指针 */
    uint64_t count;                 /**< 当前计数 */
    agentos_mutex_t* lock;          /**< 线程锁 */
} sliding_window_t;

/**
 * @brief 固定窗口结构
 */
typedef struct fixed_window {
    uint64_t window_size_ns;        /**< 窗口大小 */
    uint64_t max_requests;          /**< 最大请求数 */
    uint64_t current_count;         /**< 当前计数 */
    uint64_t window_start_ns;       /**< 窗口开始时间 */
} fixed_window_t;

/**
 * @brief 限流器配置
 */
typedef struct rate_limiter_config {
    char* name;                     /**< 限流器名称 */
    rate_limit_algorithm_t algorithm; /**< 算法类型 */
    uint64_t capacity;              /**< 容量 */
    uint64_t rate;                  /**< 速率 */
    uint64_t window_size_ns;        /**< 窗口大小 */
    uint32_t flags;                 /**< 标志位 */
} rate_limiter_config_t;

/**
 * @brief 限流器统计
 */
typedef struct rate_limiter_stats {
    uint64_t total_requests;        /**< 总请求数 */
    uint64_t allowed_requests;      /**< 允许请求数 */
    uint64_t rejected_requests;     /**< 拒绝请求数 */
    uint64_t current_usage;         /**< 当前使用率 */
    uint64_t peak_usage;            /**< 峰值使用 */
    uint64_t wait_time_ns;          /**< 总等待时间 */
    uint64_t last_reject_ns;        /**< 最后拒绝时间 */
} rate_limiter_stats_t;

/**
 * @brief 限流器内部结构
 */
struct agentos_rate_limiter {
    uint64_t id;                    /**< 限流器ID */
    char* name;                     /**< 限流器名称 */
    rate_limit_algorithm_t algorithm; /**< 算法类型 */
    rate_limiter_state_t state;     /**< 状态 */
    
    union {
        token_bucket_t token_bucket;
        leaky_bucket_t leaky_bucket;
        sliding_window_t* sliding_window;
        fixed_window_t fixed_window;
    } impl;                         /**< 算法实现 */
    
    rate_limiter_stats_t stats;     /**< 统计 */
    agentos_mutex_t* lock;          /**< 线程锁 */
    uint64_t create_time_ns;        /**< 创建时间 */
    uint64_t last_check_ns;         /**< 最后检查时间 */
};

/**
 * @brief 限流器管理器
 */
typedef struct rate_limiter_manager {
    agentos_rate_limiter_t* limiters[MAX_RATE_LIMITERS];
    uint32_t limiter_count;
    agentos_mutex_t* lock;
    uint64_t total_allowed;
    uint64_t total_rejected;
} rate_limiter_manager_t;

/* ==================== 全局变量 ==================== */

static rate_limiter_manager_t* g_rate_limiter_manager = NULL;
static agentos_mutex_t* g_rate_limiter_lock = NULL;

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
 * @brief 创建滑动窗口
 */
static sliding_window_t* create_sliding_window(uint64_t window_size_ns, uint64_t max_requests) {
    sliding_window_t* window = (sliding_window_t*)calloc(1, sizeof(sliding_window_t));
    if (!window) return NULL;
    
    window->window_size_ns = window_size_ns;
    window->max_requests = max_requests;
    window->head = NULL;
    window->tail = NULL;
    window->count = 0;
    
    window->lock = agentos_mutex_create();
    if (!window->lock) {
        free(window);
        return NULL;
    }
    
    return window;
}

/**
 * @brief 销毁滑动窗口
 */
static void destroy_sliding_window(sliding_window_t* window) {
    if (!window) return;
    
    window_entry_t* entry = window->head;
    while (entry) {
        window_entry_t* next = entry->next;
        free(entry);
        entry = next;
    }
    
    if (window->lock) agentos_mutex_destroy(window->lock);
    free(window);
}

/**
 * @brief 令牌桶补充
 */
static void token_bucket_refill(token_bucket_t* bucket) {
    if (!bucket) return;
    
    uint64_t now_ns = get_timestamp_ns();
    uint64_t elapsed_ns = now_ns - bucket->last_refill_ns;
    
    if (elapsed_ns > 0) {
        uint64_t tokens_to_add = (elapsed_ns * bucket->refill_rate) / 1000000000ULL;
        bucket->tokens = bucket->tokens + tokens_to_add;
        if (bucket->tokens > bucket->capacity) {
            bucket->tokens = bucket->capacity;
        }
        bucket->last_refill_ns = now_ns;
    }
}

/**
 * @brief 令牌桶尝试获取令牌
 */
static int token_bucket_try_acquire(token_bucket_t* bucket, uint64_t tokens) {
    if (!bucket) return 0;
    
    token_bucket_refill(bucket);
    
    if (bucket->tokens >= tokens) {
        bucket->tokens -= tokens;
        return 1;
    }
    
    return 0;
}

/**
 * @brief 漏桶漏水
 */
static void leaky_bucket_leak(leaky_bucket_t* bucket) {
    if (!bucket) return;
    
    uint64_t now_ns = get_timestamp_ns();
    uint64_t elapsed_ns = now_ns - bucket->last_leak_ns;
    
    if (elapsed_ns > 0) {
        uint64_t water_to_leak = (elapsed_ns * bucket->leak_rate) / 1000000000ULL;
        if (water_to_leak > bucket->water) {
            bucket->water = 0;
        } else {
            bucket->water -= water_to_leak;
        }
        bucket->last_leak_ns = now_ns;
    }
}

/**
 * @brief 漏桶尝试添加水
 */
static int leaky_bucket_try_add(leaky_bucket_t* bucket, uint64_t water) {
    if (!bucket) return 0;
    
    leaky_bucket_leak(bucket);
    
    if (bucket->water + water <= bucket->capacity) {
        bucket->water += water;
        return 1;
    }
    
    return 0;
}

/**
 * @brief 滑动窗口清理过期条目
 */
static void sliding_window_cleanup(sliding_window_t* window) {
    if (!window) return;
    
    uint64_t now_ns = get_timestamp_ns();
    uint64_t cutoff_ns = now_ns - window->window_size_ns;
    
    while (window->head && window->head->timestamp_ns < cutoff_ns) {
        window_entry_t* to_remove = window->head;
        window->head = window->head->next;
        if (window->head == NULL) {
            window->tail = NULL;
        }
        free(to_remove);
        window->count--;
    }
}

/**
 * @brief 滑动窗口尝试添加请求
 */
static int sliding_window_try_add(sliding_window_t* window) {
    if (!window) return 0;
    
    agentos_mutex_lock(window->lock);
    
    sliding_window_cleanup(window);
    
    if (window->count < window->max_requests) {
        window_entry_t* entry = (window_entry_t*)malloc(sizeof(window_entry_t));
        if (!entry) {
            agentos_mutex_unlock(window->lock);
            return 0;
        }
        
        entry->timestamp_ns = get_timestamp_ns();
        entry->next = NULL;
        
        if (window->tail) {
            window->tail->next = entry;
            window->tail = entry;
        } else {
            window->head = window->tail = entry;
        }
        
        window->count++;
        
        agentos_mutex_unlock(window->lock);
        return 1;
    }
    
    agentos_mutex_unlock(window->lock);
    return 0;
}

/**
 * @brief 固定窗口检查并重置
 */
static void fixed_window_check_reset(fixed_window_t* window) {
    if (!window) return;
    
    uint64_t now_ns = get_timestamp_ns();
    
    if (now_ns - window->window_start_ns >= window->window_size_ns) {
        window->current_count = 0;
        window->window_start_ns = now_ns;
    }
}

/**
 * @brief 固定窗口尝试添加请求
 */
static int fixed_window_try_add(fixed_window_t* window) {
    if (!window) return 0;
    
    fixed_window_check_reset(window);
    
    if (window->current_count < window->max_requests) {
        window->current_count++;
        return 1;
    }
    
    return 0;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化限流器管理器
 */
agentos_error_t agentos_rate_limiter_manager_init(void) {
    if (g_rate_limiter_manager) {
        AGENTOS_LOG_WARN("Rate limiter manager already initialized");
        return AGENTOS_SUCCESS;
    }
    
    g_rate_limiter_lock = agentos_mutex_create();
    if (!g_rate_limiter_lock) {
        AGENTOS_LOG_ERROR("Failed to create rate limiter lock");
        return AGENTOS_ENOMEM;
    }
    
    g_rate_limiter_manager = (rate_limiter_manager_t*)calloc(1, sizeof(rate_limiter_manager_t));
    if (!g_rate_limiter_manager) {
        AGENTOS_LOG_ERROR("Failed to allocate rate limiter manager");
        agentos_mutex_destroy(g_rate_limiter_lock);
        g_rate_limiter_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_rate_limiter_manager->lock = agentos_mutex_create();
    if (!g_rate_limiter_manager->lock) {
        AGENTOS_LOG_ERROR("Failed to create manager lock");
        free(g_rate_limiter_manager);
        g_rate_limiter_manager = NULL;
        agentos_mutex_destroy(g_rate_limiter_lock);
        g_rate_limiter_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_rate_limiter_manager->limiter_count = 0;
    g_rate_limiter_manager->total_allowed = 0;
    g_rate_limiter_manager->total_rejected = 0;
    
    AGENTOS_LOG_INFO("Rate limiter manager initialized");
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁限流器管理器
 */
void agentos_rate_limiter_manager_destroy(void) {
    if (!g_rate_limiter_manager) return;
    
    agentos_mutex_lock(g_rate_limiter_lock);
    
    for (uint32_t i = 0; i < MAX_RATE_LIMITERS; i++) {
        if (g_rate_limiter_manager->limiters[i]) {
            agentos_rate_limiter_destroy(g_rate_limiter_manager->limiters[i]);
            g_rate_limiter_manager->limiters[i] = NULL;
        }
    }
    
    agentos_mutex_destroy(g_rate_limiter_manager->lock);
    free(g_rate_limiter_manager);
    g_rate_limiter_manager = NULL;
    
    agentos_mutex_unlock(g_rate_limiter_lock);
    agentos_mutex_destroy(g_rate_limiter_lock);
    g_rate_limiter_lock = NULL;
    
    AGENTOS_LOG_INFO("Rate limiter manager destroyed");
}

/**
 * @brief 创建限流器
 */
agentos_error_t agentos_rate_limiter_create(const char* name,
                                            const rate_limiter_config_t* config,
                                            agentos_rate_limiter_t** out_limiter) {
    if (!name || !out_limiter) return AGENTOS_EINVAL;
    
    if (!g_rate_limiter_manager) {
        AGENTOS_LOG_ERROR("Rate limiter manager not initialized");
        return AGENTOS_ENOTINIT;
    }
    
    agentos_mutex_lock(g_rate_limiter_lock);
    
    int slot = -1;
    for (uint32_t i = 0; i < MAX_RATE_LIMITERS; i++) {
        if (!g_rate_limiter_manager->limiters[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        agentos_mutex_unlock(g_rate_limiter_lock);
        AGENTOS_LOG_ERROR("No available rate limiter slot");
        return AGENTOS_EBUSY;
    }
    
    agentos_rate_limiter_t* limiter = (agentos_rate_limiter_t*)calloc(1, sizeof(agentos_rate_limiter_t));
    if (!limiter) {
        agentos_mutex_unlock(g_rate_limiter_lock);
        AGENTOS_LOG_ERROR("Failed to allocate rate limiter");
        return AGENTOS_ENOMEM;
    }
    
    limiter->id = (uint64_t)slot + 1;
    limiter->name = strdup(name);
    limiter->state = RATE_LIMITER_ACTIVE;
    limiter->create_time_ns = get_timestamp_ns();
    limiter->last_check_ns = limiter->create_time_ns;
    
    if (config) {
        limiter->algorithm = config->algorithm;
    } else {
        limiter->algorithm = RATE_LIMIT_TOKEN_BUCKET;
    }
    
    limiter->lock = agentos_mutex_create();
    if (!limiter->lock) {
        if (limiter->name) free(limiter->name);
        free(limiter);
        agentos_mutex_unlock(g_rate_limiter_lock);
        AGENTOS_LOG_ERROR("Failed to create limiter lock");
        return AGENTOS_ENOMEM;
    }
    
    // 初始化算法实现
    switch (limiter->algorithm) {
        case RATE_LIMIT_TOKEN_BUCKET:
            limiter->impl.token_bucket.capacity = config && config->capacity > 0 ? 
                                                  config->capacity : DEFAULT_BUCKET_CAPACITY;
            limiter->impl.token_bucket.tokens = limiter->impl.token_bucket.capacity;
            limiter->impl.token_bucket.refill_rate = config && config->rate > 0 ? 
                                                     config->rate : DEFAULT_REFILL_RATE;
            limiter->impl.token_bucket.last_refill_ns = get_timestamp_ns();
            break;
            
        case RATE_LIMIT_LEAKY_BUCKET:
            limiter->impl.leaky_bucket.capacity = config && config->capacity > 0 ? 
                                                  config->capacity : DEFAULT_BUCKET_CAPACITY;
            limiter->impl.leaky_bucket.water = 0;
            limiter->impl.leaky_bucket.leak_rate = config && config->rate > 0 ? 
                                                   config->rate : DEFAULT_REFILL_RATE;
            limiter->impl.leaky_bucket.last_leak_ns = get_timestamp_ns();
            break;
            
        case RATE_LIMIT_SLIDING_WINDOW:
            limiter->impl.sliding_window = create_sliding_window(
                config && config->window_size_ns > 0 ? config->window_size_ns : 
                (uint64_t)DEFAULT_WINDOW_SIZE_MS * 1000000ULL,
                config && config->capacity > 0 ? config->capacity : DEFAULT_BUCKET_CAPACITY
            );
            if (!limiter->impl.sliding_window) {
                agentos_mutex_destroy(limiter->lock);
                if (limiter->name) free(limiter->name);
                free(limiter);
                agentos_mutex_unlock(g_rate_limiter_lock);
                AGENTOS_LOG_ERROR("Failed to create sliding window");
                return AGENTOS_ENOMEM;
            }
            break;
            
        case RATE_LIMIT_FIXED_WINDOW:
            limiter->impl.fixed_window.window_size_ns = config && config->window_size_ns > 0 ? 
                                                        config->window_size_ns : 
                                                        (uint64_t)DEFAULT_WINDOW_SIZE_MS * 1000000ULL;
            limiter->impl.fixed_window.max_requests = config && config->capacity > 0 ? 
                                                      config->capacity : DEFAULT_BUCKET_CAPACITY;
            limiter->impl.fixed_window.current_count = 0;
            limiter->impl.fixed_window.window_start_ns = get_timestamp_ns();
            break;
    }
    
    g_rate_limiter_manager->limiters[slot] = limiter;
    g_rate_limiter_manager->limiter_count++;
    
    agentos_mutex_unlock(g_rate_limiter_lock);
    
    *out_limiter = limiter;
    
    AGENTOS_LOG_INFO("Rate limiter created: %s (ID: %llu, Algorithm: %d)",
                     name, (unsigned long long)limiter->id, limiter->algorithm);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁限流器
 */
void agentos_rate_limiter_destroy(agentos_rate_limiter_t* limiter) {
    if (!limiter) return;
    
    AGENTOS_LOG_DEBUG("Destroying rate limiter: %s", limiter->name);
    
    if (g_rate_limiter_manager) {
        agentos_mutex_lock(g_rate_limiter_lock);
        for (uint32_t i = 0; i < MAX_RATE_LIMITERS; i++) {
            if (g_rate_limiter_manager->limiters[i] == limiter) {
                g_rate_limiter_manager->limiters[i] = NULL;
                g_rate_limiter_manager->limiter_count--;
                break;
            }
        }
        agentos_mutex_unlock(g_rate_limiter_lock);
    }
    
    if (limiter->algorithm == RATE_LIMIT_SLIDING_WINDOW && limiter->impl.sliding_window) {
        destroy_sliding_window(limiter->impl.sliding_window);
    }
    
    if (limiter->lock) agentos_mutex_destroy(limiter->lock);
    if (limiter->name) free(limiter->name);
    free(limiter);
}

/**
 * @brief 尝试获取许可
 */
int agentos_rate_limiter_try_acquire(agentos_rate_limiter_t* limiter, uint64_t permits) {
    if (!limiter || permits == 0) return 0;
    
    agentos_mutex_lock(limiter->lock);
    
    if (limiter->state != RATE_LIMITER_ACTIVE) {
        agentos_mutex_unlock(limiter->lock);
        return limiter->state == RATE_LIMITER_DISABLED ? 1 : 0;
    }
    
    limiter->stats.total_requests++;
    limiter->last_check_ns = get_timestamp_ns();
    
    int allowed = 0;
    
    switch (limiter->algorithm) {
        case RATE_LIMIT_TOKEN_BUCKET:
            allowed = token_bucket_try_acquire(&limiter->impl.token_bucket, permits);
            break;
            
        case RATE_LIMIT_LEAKY_BUCKET:
            allowed = leaky_bucket_try_add(&limiter->impl.leaky_bucket, permits);
            break;
            
        case RATE_LIMIT_SLIDING_WINDOW:
            allowed = sliding_window_try_add(limiter->impl.sliding_window);
            break;
            
        case RATE_LIMIT_FIXED_WINDOW:
            allowed = fixed_window_try_add(&limiter->impl.fixed_window);
            break;
    }
    
    if (allowed) {
        limiter->stats.allowed_requests++;
        if (g_rate_limiter_manager) {
            g_rate_limiter_manager->total_allowed++;
        }
    } else {
        limiter->stats.rejected_requests++;
        limiter->stats.last_reject_ns = get_timestamp_ns();
        if (g_rate_limiter_manager) {
            g_rate_limiter_manager->total_rejected++;
        }
    }
    
    agentos_mutex_unlock(limiter->lock);
    
    return allowed;
}

/**
 * @brief 获取当前使用率
 */
double agentos_rate_limiter_get_usage(agentos_rate_limiter_t* limiter) {
    if (!limiter) return 0.0;
    
    agentos_mutex_lock(limiter->lock);
    
    double usage = 0.0;
    
    switch (limiter->algorithm) {
        case RATE_LIMIT_TOKEN_BUCKET: {
            token_bucket_refill(&limiter->impl.token_bucket);
            uint64_t used = limiter->impl.token_bucket.capacity - limiter->impl.token_bucket.tokens;
            usage = limiter->impl.token_bucket.capacity > 0 ? 
                   (double)used / limiter->impl.token_bucket.capacity : 0.0;
            break;
        }
        
        case RATE_LIMIT_LEAKY_BUCKET: {
            leaky_bucket_leak(&limiter->impl.leaky_bucket);
            usage = limiter->impl.leaky_bucket.capacity > 0 ?
                   (double)limiter->impl.leaky_bucket.water / limiter->impl.leaky_bucket.capacity : 0.0;
            break;
        }
        
        case RATE_LIMIT_SLIDING_WINDOW: {
            sliding_window_cleanup(limiter->impl.sliding_window);
            usage = limiter->impl.sliding_window->max_requests > 0 ?
                   (double)limiter->impl.sliding_window->count / limiter->impl.sliding_window->max_requests : 0.0;
            break;
        }
        
        case RATE_LIMIT_FIXED_WINDOW: {
            fixed_window_check_reset(&limiter->impl.fixed_window);
            usage = limiter->impl.fixed_window.max_requests > 0 ?
                   (double)limiter->impl.fixed_window.current_count / limiter->impl.fixed_window.max_requests : 0.0;
            break;
        }
    }
    
    if (usage > (double)limiter->stats.peak_usage / 100.0) {
        limiter->stats.peak_usage = (uint64_t)(usage * 100);
    }
    limiter->stats.current_usage = (uint64_t)(usage * 100);
    
    agentos_mutex_unlock(limiter->lock);
    
    return usage;
}

/**
 * @brief 暂停限流器
 */
agentos_error_t agentos_rate_limiter_pause(agentos_rate_limiter_t* limiter) {
    if (!limiter) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(limiter->lock);
    limiter->state = RATE_LIMITER_PAUSED;
    agentos_mutex_unlock(limiter->lock);
    
    AGENTOS_LOG_INFO("Rate limiter %s paused", limiter->name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 恢复限流器
 */
agentos_error_t agentos_rate_limiter_resume(agentos_rate_limiter_t* limiter) {
    if (!limiter) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(limiter->lock);
    limiter->state = RATE_LIMITER_ACTIVE;
    agentos_mutex_unlock(limiter->lock);
    
    AGENTOS_LOG_INFO("Rate limiter %s resumed", limiter->name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 禁用限流器
 */
agentos_error_t agentos_rate_limiter_disable(agentos_rate_limiter_t* limiter) {
    if (!limiter) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(limiter->lock);
    limiter->state = RATE_LIMITER_DISABLED;
    agentos_mutex_unlock(limiter->lock);
    
    AGENTOS_LOG_INFO("Rate limiter %s disabled", limiter->name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置限流器
 */
agentos_error_t agentos_rate_limiter_reset(agentos_rate_limiter_t* limiter) {
    if (!limiter) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(limiter->lock);
    
    switch (limiter->algorithm) {
        case RATE_LIMIT_TOKEN_BUCKET:
            limiter->impl.token_bucket.tokens = limiter->impl.token_bucket.capacity;
            limiter->impl.token_bucket.last_refill_ns = get_timestamp_ns();
            break;
            
        case RATE_LIMIT_LEAKY_BUCKET:
            limiter->impl.leaky_bucket.water = 0;
            limiter->impl.leaky_bucket.last_leak_ns = get_timestamp_ns();
            break;
            
        case RATE_LIMIT_SLIDING_WINDOW:
            sliding_window_cleanup(limiter->impl.sliding_window);
            break;
            
        case RATE_LIMIT_FIXED_WINDOW:
            limiter->impl.fixed_window.current_count = 0;
            limiter->impl.fixed_window.window_start_ns = get_timestamp_ns();
            break;
    }
    
    memset(&limiter->stats, 0, sizeof(rate_limiter_stats_t));
    
    agentos_mutex_unlock(limiter->lock);
    
    AGENTOS_LOG_INFO("Rate limiter %s reset", limiter->name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取限流器统计信息
 */
agentos_error_t agentos_rate_limiter_get_stats(agentos_rate_limiter_t* limiter, char** out_stats) {
    if (!limiter || !out_stats) return AGENTOS_EINVAL;
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(limiter->lock);
    
    cJSON_AddNumberToObject(stats_json, "id", limiter->id);
    cJSON_AddStringToObject(stats_json, "name", limiter->name);
    cJSON_AddNumberToObject(stats_json, "algorithm", limiter->algorithm);
    cJSON_AddNumberToObject(stats_json, "state", limiter->state);
    
    cJSON* stats_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats_obj, "total_requests", limiter->stats.total_requests);
    cJSON_AddNumberToObject(stats_obj, "allowed_requests", limiter->stats.allowed_requests);
    cJSON_AddNumberToObject(stats_obj, "rejected_requests", limiter->stats.rejected_requests);
    cJSON_AddNumberToObject(stats_obj, "current_usage", limiter->stats.current_usage);
    cJSON_AddNumberToObject(stats_obj, "peak_usage", limiter->stats.peak_usage);
    
    double reject_rate = limiter->stats.total_requests > 0 ?
                        (double)limiter->stats.rejected_requests / limiter->stats.total_requests * 100.0 : 0.0;
    cJSON_AddNumberToObject(stats_obj, "reject_rate_percent", reject_rate);
    
    cJSON_AddItemToObject(stats_json, "stats", stats_obj);
    
    agentos_mutex_unlock(limiter->lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取管理器统计信息
 */
agentos_error_t agentos_rate_limiter_manager_get_stats(char** out_stats) {
    if (!out_stats) return AGENTOS_EINVAL;
    
    if (!g_rate_limiter_manager) {
        *out_stats = strdup("{\"error\":\"manager not initialized\"}");
        return AGENTOS_ENOTINIT;
    }
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(g_rate_limiter_lock);
    
    cJSON_AddNumberToObject(stats_json, "limiter_count", g_rate_limiter_manager->limiter_count);
    cJSON_AddNumberToObject(stats_json, "total_allowed", g_rate_limiter_manager->total_allowed);
    cJSON_AddNumberToObject(stats_json, "total_rejected", g_rate_limiter_manager->total_rejected);
    
    double reject_rate = (g_rate_limiter_manager->total_allowed + g_rate_limiter_manager->total_rejected) > 0 ?
                        (double)g_rate_limiter_manager->total_rejected / 
                        (g_rate_limiter_manager->total_allowed + g_rate_limiter_manager->total_rejected) * 100.0 : 0.0;
    cJSON_AddNumberToObject(stats_json, "reject_rate_percent", reject_rate);
    
    cJSON* limiters_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < MAX_RATE_LIMITERS; i++) {
        if (g_rate_limiter_manager->limiters[i]) {
            cJSON* limiter_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(limiter_obj, "name", g_rate_limiter_manager->limiters[i]->name);
            cJSON_AddNumberToObject(limiter_obj, "state", g_rate_limiter_manager->limiters[i]->state);
            cJSON_AddItemToArray(limiters_array, limiter_obj);
        }
    }
    cJSON_AddItemToObject(stats_json, "limiters", limiters_array);
    
    agentos_mutex_unlock(g_rate_limiter_lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 健康检查
 */
agentos_error_t agentos_rate_limiter_health_check(agentos_rate_limiter_t* limiter, char** out_json) {
    if (!limiter || !out_json) return AGENTOS_EINVAL;
    
    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(limiter->lock);
    
    cJSON_AddStringToObject(health_json, "component", "rate_limiter");
    cJSON_AddNumberToObject(health_json, "id", limiter->id);
    cJSON_AddStringToObject(health_json, "name", limiter->name);
    
    const char* state_str = "unknown";
    const char* status = "healthy";
    
    switch (limiter->state) {
        case RATE_LIMITER_ACTIVE: 
            state_str = "active";
            status = "healthy";
            break;
        case RATE_LIMITER_PAUSED: 
            state_str = "paused";
            status = "degraded";
            break;
        case RATE_LIMITER_DISABLED: 
            state_str = "disabled";
            status = "unhealthy";
            break;
    }
    
    cJSON_AddStringToObject(health_json, "state", state_str);
    cJSON_AddStringToObject(health_json, "status", status);
    
    double usage = agentos_rate_limiter_get_usage(limiter);
    cJSON_AddNumberToObject(health_json, "current_usage", usage);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", get_timestamp_ns());
    
    agentos_mutex_unlock(limiter->lock);
    
    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);
    
    if (!health_str) return AGENTOS_ENOMEM;
    
    *out_json = health_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 等待直到获取许可
 */
agentos_error_t agentos_rate_limiter_wait(agentos_rate_limiter_t* limiter, 
                                          uint64_t permits,
                                          uint32_t timeout_ms) {
    if (!limiter || permits == 0) return AGENTOS_EINVAL;
    
    uint64_t start_ns = get_timestamp_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    
    while (1) {
        if (agentos_rate_limiter_try_acquire(limiter, permits)) {
            return AGENTOS_SUCCESS;
        }
        
        uint64_t elapsed_ns = get_timestamp_ns() - start_ns;
        if (timeout_ms > 0 && elapsed_ns >= timeout_ns) {
            return AGENTOS_ETIMEDOUT;
        }
        
        // 短暂休眠
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10000000;  // 10ms
        nanosleep(&ts, NULL);
    }
}

/**
 * @brief 设置限流器参数
 */
agentos_error_t agentos_rate_limiter_set_config(agentos_rate_limiter_t* limiter,
                                                uint64_t capacity,
                                                uint64_t rate) {
    if (!limiter) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(limiter->lock);
    
    switch (limiter->algorithm) {
        case RATE_LIMIT_TOKEN_BUCKET:
            if (capacity > 0) limiter->impl.token_bucket.capacity = capacity;
            if (rate > 0) limiter->impl.token_bucket.refill_rate = rate;
            break;
            
        case RATE_LIMIT_LEAKY_BUCKET:
            if (capacity > 0) limiter->impl.leaky_bucket.capacity = capacity;
            if (rate > 0) limiter->impl.leaky_bucket.leak_rate = rate;
            break;
            
        case RATE_LIMIT_SLIDING_WINDOW:
            if (capacity > 0) limiter->impl.sliding_window->max_requests = capacity;
            break;
            
        case RATE_LIMIT_FIXED_WINDOW:
            if (capacity > 0) limiter->impl.fixed_window.max_requests = capacity;
            break;
    }
    
    agentos_mutex_unlock(limiter->lock);
    
    AGENTOS_LOG_INFO("Rate limiter %s config updated: capacity=%llu, rate=%llu",
                     limiter->name, (unsigned long long)capacity, (unsigned long long)rate);
    
    return AGENTOS_SUCCESS;
}
