/**
 * @file circuit_breaker.c
 * @brief 熔断器模式实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 熔断器模式用于防止级联故障，当系统调用失败率达到阈值时自动熔断，
 * 快速失败以保护系统稳定性。支持99.999%可靠性标准。
 * 
 * 核心功能：
 * 1. 状态机：关闭、打开、半开三种状态
 * 2. 失败计数：滑动窗口内的失败统计
 * 3. 自动恢复：半开状态下的探测恢复
 * 4. 降级策略：熔断时的降级处理
 * 5. 监控指标：熔断器状态和统计
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

/** @brief 最大熔断器数量 */
#define MAX_CIRCUIT_BREAKERS 128

/** @brief 默认失败阈值 */
#define DEFAULT_FAILURE_THRESHOLD 5

/** @brief 默认成功阈值（半开状态） */
#define DEFAULT_SUCCESS_THRESHOLD 3

/** @brief 默认超时时间（毫秒） */
#define DEFAULT_TIMEOUT_MS 30000

/** @brief 默认滑动窗口大小 */
#define DEFAULT_WINDOW_SIZE 100

/** @brief 默认半开探测数量 */
#define DEFAULT_HALF_OPEN_REQUESTS 3

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 熔断器状态枚举
 */
typedef enum {
    CB_STATE_CLOSED = 0,    /**< 关闭状态（正常） */
    CB_STATE_OPEN,          /**< 打开状态（熔断） */
    CB_STATE_HALF_OPEN      /**< 半开状态（探测） */
} circuit_breaker_state_t;

/**
 * @brief 滑动窗口计数器
 */
typedef struct sliding_window {
    uint64_t* timestamps;    /**< 时间戳数组 */
    int* results;            /**< 结果数组（0成功，1失败） */
    size_t capacity;         /**< 容量 */
    size_t count;            /**< 当前数量 */
    size_t head;             /**< 头指针 */
    size_t tail;             /**< 尾指针 */
    agentos_mutex_t* lock;   /**< 线程锁 */
} sliding_window_t;

/**
 * @brief 熔断器配置
 */
typedef struct cb_config {
    char* name;                       /**< 熔断器名称 */
    uint32_t failure_threshold;       /**< 失败阈值 */
    uint32_t success_threshold;       /**< 成功阈值 */
    uint32_t timeout_ms;              /**< 超时时间 */
    uint32_t window_size;             /**< 滑动窗口大小 */
    uint32_t half_open_requests;      /**< 半开探测数量 */
    uint32_t flags;                   /**< 标志位 */
} cb_config_t;

/**
 * @brief 熔断器统计
 */
typedef struct cb_stats {
    uint64_t total_calls;             /**< 总调用次数 */
    uint64_t success_calls;           /**< 成功次数 */
    uint64_t failed_calls;            /**< 失败次数 */
    uint64_t rejected_calls;          /**< 拒绝次数 */
    uint64_t timeout_calls;           /**< 超时次数 */
    uint64_t state_changes;           /**< 状态变更次数 */
    uint64_t last_failure_time_ns;    /**< 最后失败时间 */
    uint64_t last_state_change_ns;    /**< 最后状态变更时间 */
} cb_stats_t;

/**
 * @brief 熔断器内部结构
 */
struct agentos_circuit_breaker {
    uint64_t id;                      /**< 熔断器ID */
    char* name;                       /**< 熔断器名称 */
    circuit_breaker_state_t state;    /**< 当前状态 */
    cb_config_t config;               /**< 配置 */
    cb_stats_t stats;                 /**< 统计 */
    sliding_window_t* window;         /**< 滑动窗口 */
    uint64_t open_time_ns;            /**< 打开时间 */
    uint32_t half_open_success;       /**< 半开成功计数 */
    uint32_t half_open_failure;       /**< 半开失败计数 */
    agentos_mutex_t* lock;            /**< 线程锁 */
    void* fallback_data;              /**< 降级数据 */
    agentos_error_t (*fallback_fn)(void*); /**< 降级函数 */
};

/**
 * @brief 熔断器管理器
 */
typedef struct cb_manager {
    agentos_circuit_breaker_t* breakers[MAX_CIRCUIT_BREAKERS];
    uint32_t breaker_count;
    agentos_mutex_t* lock;
    uint64_t total_rejected;
    uint64_t total_state_changes;
} cb_manager_t;

/* ==================== 全局变量 ==================== */

static cb_manager_t* g_cb_manager = NULL;
static agentos_mutex_t* g_cb_lock = NULL;

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
static sliding_window_t* sliding_window_create(size_t capacity) {
    sliding_window_t* window = (sliding_window_t*)calloc(1, sizeof(sliding_window_t));
    if (!window) return NULL;
    
    window->timestamps = (uint64_t*)calloc(capacity, sizeof(uint64_t));
    window->results = (int*)calloc(capacity, sizeof(int));
    if (!window->timestamps || !window->results) {
        if (window->timestamps) free(window->timestamps);
        if (window->results) free(window->results);
        free(window);
        return NULL;
    }
    
    window->capacity = capacity;
    window->count = 0;
    window->head = 0;
    window->tail = 0;
    window->lock = agentos_mutex_create();
    
    if (!window->lock) {
        free(window->timestamps);
        free(window->results);
        free(window);
        return NULL;
    }
    
    return window;
}

/**
 * @brief 销毁滑动窗口
 */
static void sliding_window_destroy(sliding_window_t* window) {
    if (!window) return;
    if (window->timestamps) free(window->timestamps);
    if (window->results) free(window->results);
    if (window->lock) agentos_mutex_destroy(window->lock);
    free(window);
}

/**
 * @brief 添加记录到滑动窗口
 */
static void sliding_window_push(sliding_window_t* window, uint64_t timestamp, int result) {
    if (!window) return;
    
    agentos_mutex_lock(window->lock);
    
    if (window->count < window->capacity) {
        window->timestamps[window->tail] = timestamp;
        window->results[window->tail] = result;
        window->tail = (window->tail + 1) % window->capacity;
        window->count++;
    } else {
        window->timestamps[window->tail] = timestamp;
        window->results[window->tail] = result;
        window->tail = (window->tail + 1) % window->capacity;
        window->head = (window->head + 1) % window->capacity;
    }
    
    agentos_mutex_unlock(window->lock);
}

/**
 * @brief 统计窗口内的失败数量
 */
static size_t sliding_window_count_failures(sliding_window_t* window, uint64_t since_ns) {
    if (!window) return 0;
    
    agentos_mutex_lock(window->lock);
    
    size_t failures = 0;
    size_t idx = window->head;
    
    for (size_t i = 0; i < window->count; i++) {
        if (window->timestamps[idx] >= since_ns && window->results[idx] == 1) {
            failures++;
        }
        idx = (idx + 1) % window->capacity;
    }
    
    agentos_mutex_unlock(window->lock);
    
    return failures;
}

/**
 * @brief 统计窗口内的成功数量
 */
static size_t sliding_window_count_successes(sliding_window_t* window, uint64_t since_ns) {
    if (!window) return 0;
    
    agentos_mutex_lock(window->lock);
    
    size_t successes = 0;
    size_t idx = window->head;
    
    for (size_t i = 0; i < window->count; i++) {
        if (window->timestamps[idx] >= since_ns && window->results[idx] == 0) {
            successes++;
        }
        idx = (idx + 1) % window->capacity;
    }
    
    agentos_mutex_unlock(window->lock);
    
    return successes;
}

/**
 * @brief 状态转换
 */
static void cb_change_state(agentos_circuit_breaker_t* cb, circuit_breaker_state_t new_state) {
    if (!cb) return;
    
    circuit_breaker_state_t old_state = cb->state;
    cb->state = new_state;
    cb->stats.state_changes++;
    cb->stats.last_state_change_ns = get_timestamp_ns();
    
    if (new_state == CB_STATE_OPEN) {
        cb->open_time_ns = get_timestamp_ns();
    } else if (new_state == CB_STATE_HALF_OPEN) {
        cb->half_open_success = 0;
        cb->half_open_failure = 0;
    }
    
    AGENTOS_LOG_INFO("Circuit breaker '%s' state changed: %d -> %d", 
                     cb->name, old_state, new_state);
    
    if (g_cb_manager) {
        g_cb_manager->total_state_changes++;
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化熔断器管理器
 */
agentos_error_t agentos_circuit_breaker_manager_init(void) {
    if (g_cb_manager) {
        AGENTOS_LOG_WARN("Circuit breaker manager already initialized");
        return AGENTOS_SUCCESS;
    }
    
    g_cb_lock = agentos_mutex_create();
    if (!g_cb_lock) {
        AGENTOS_LOG_ERROR("Failed to create circuit breaker lock");
        return AGENTOS_ENOMEM;
    }
    
    g_cb_manager = (cb_manager_t*)calloc(1, sizeof(cb_manager_t));
    if (!g_cb_manager) {
        AGENTOS_LOG_ERROR("Failed to allocate circuit breaker manager");
        agentos_mutex_destroy(g_cb_lock);
        g_cb_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_cb_manager->lock = agentos_mutex_create();
    if (!g_cb_manager->lock) {
        AGENTOS_LOG_ERROR("Failed to create manager lock");
        free(g_cb_manager);
        g_cb_manager = NULL;
        agentos_mutex_destroy(g_cb_lock);
        g_cb_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_cb_manager->breaker_count = 0;
    g_cb_manager->total_rejected = 0;
    g_cb_manager->total_state_changes = 0;
    
    AGENTOS_LOG_INFO("Circuit breaker manager initialized");
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁熔断器管理器
 */
void agentos_circuit_breaker_manager_destroy(void) {
    if (!g_cb_manager) return;
    
    agentos_mutex_lock(g_cb_lock);
    
    for (uint32_t i = 0; i < MAX_CIRCUIT_BREAKERS; i++) {
        if (g_cb_manager->breakers[i]) {
            agentos_circuit_breaker_destroy(g_cb_manager->breakers[i]);
            g_cb_manager->breakers[i] = NULL;
        }
    }
    
    agentos_mutex_destroy(g_cb_manager->lock);
    free(g_cb_manager);
    g_cb_manager = NULL;
    
    agentos_mutex_unlock(g_cb_lock);
    agentos_mutex_destroy(g_cb_lock);
    g_cb_lock = NULL;
    
    AGENTOS_LOG_INFO("Circuit breaker manager destroyed");
}

/**
 * @brief 创建熔断器
 */
agentos_error_t agentos_circuit_breaker_create(const char* name,
                                               const cb_config_t* config,
                                               agentos_circuit_breaker_t** out_cb) {
    if (!name || !out_cb) return AGENTOS_EINVAL;
    
    if (!g_cb_manager) {
        AGENTOS_LOG_ERROR("Circuit breaker manager not initialized");
        return AGENTOS_ENOTINIT;
    }
    
    agentos_mutex_lock(g_cb_lock);
    
    int slot = -1;
    for (uint32_t i = 0; i < MAX_CIRCUIT_BREAKERS; i++) {
        if (!g_cb_manager->breakers[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        agentos_mutex_unlock(g_cb_lock);
        AGENTOS_LOG_ERROR("No available circuit breaker slot");
        return AGENTOS_EBUSY;
    }
    
    agentos_circuit_breaker_t* cb = (agentos_circuit_breaker_t*)calloc(1, sizeof(agentos_circuit_breaker_t));
    if (!cb) {
        agentos_mutex_unlock(g_cb_lock);
        AGENTOS_LOG_ERROR("Failed to allocate circuit breaker");
        return AGENTOS_ENOMEM;
    }
    
    cb->id = (uint64_t)slot + 1;
    cb->name = strdup(name);
    cb->state = CB_STATE_CLOSED;
    
    if (config) {
        cb->config.name = strdup(config->name ? config->name : name);
        cb->config.failure_threshold = config->failure_threshold > 0 ? 
                                       config->failure_threshold : DEFAULT_FAILURE_THRESHOLD;
        cb->config.success_threshold = config->success_threshold > 0 ?
                                       config->success_threshold : DEFAULT_SUCCESS_THRESHOLD;
        cb->config.timeout_ms = config->timeout_ms > 0 ?
                               config->timeout_ms : DEFAULT_TIMEOUT_MS;
        cb->config.window_size = config->window_size > 0 ?
                                config->window_size : DEFAULT_WINDOW_SIZE;
        cb->config.half_open_requests = config->half_open_requests > 0 ?
                                       config->half_open_requests : DEFAULT_HALF_OPEN_REQUESTS;
        cb->config.flags = config->flags;
    } else {
        cb->config.name = strdup(name);
        cb->config.failure_threshold = DEFAULT_FAILURE_THRESHOLD;
        cb->config.success_threshold = DEFAULT_SUCCESS_THRESHOLD;
        cb->config.timeout_ms = DEFAULT_TIMEOUT_MS;
        cb->config.window_size = DEFAULT_WINDOW_SIZE;
        cb->config.half_open_requests = DEFAULT_HALF_OPEN_REQUESTS;
        cb->config.flags = 0;
    }
    
    cb->lock = agentos_mutex_create();
    if (!cb->lock) {
        if (cb->name) free(cb->name);
        if (cb->config.name) free(cb->config.name);
        free(cb);
        agentos_mutex_unlock(g_cb_lock);
        AGENTOS_LOG_ERROR("Failed to create circuit breaker lock");
        return AGENTOS_ENOMEM;
    }
    
    cb->window = sliding_window_create(cb->config.window_size);
    if (!cb->window) {
        agentos_mutex_destroy(cb->lock);
        if (cb->name) free(cb->name);
        if (cb->config.name) free(cb->config.name);
        free(cb);
        agentos_mutex_unlock(g_cb_lock);
        AGENTOS_LOG_ERROR("Failed to create sliding window");
        return AGENTOS_ENOMEM;
    }
    
    cb->open_time_ns = 0;
    cb->half_open_success = 0;
    cb->half_open_failure = 0;
    cb->fallback_data = NULL;
    cb->fallback_fn = NULL;
    
    g_cb_manager->breakers[slot] = cb;
    g_cb_manager->breaker_count++;
    
    agentos_mutex_unlock(g_cb_lock);
    
    *out_cb = cb;
    
    AGENTOS_LOG_INFO("Circuit breaker created: %s (ID: %llu)", 
                     name, (unsigned long long)cb->id);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁熔断器
 */
void agentos_circuit_breaker_destroy(agentos_circuit_breaker_t* cb) {
    if (!cb) return;
    
    AGENTOS_LOG_DEBUG("Destroying circuit breaker: %s", cb->name);
    
    if (g_cb_manager) {
        agentos_mutex_lock(g_cb_lock);
        for (uint32_t i = 0; i < MAX_CIRCUIT_BREAKERS; i++) {
            if (g_cb_manager->breakers[i] == cb) {
                g_cb_manager->breakers[i] = NULL;
                g_cb_manager->breaker_count--;
                break;
            }
        }
        agentos_mutex_unlock(g_cb_lock);
    }
    
    if (cb->name) free(cb->name);
    if (cb->config.name) free(cb->config.name);
    if (cb->window) sliding_window_destroy(cb->window);
    if (cb->lock) agentos_mutex_destroy(cb->lock);
    
    free(cb);
}

/**
 * @brief 检查熔断器是否允许调用
 */
int agentos_circuit_breaker_allow(agentos_circuit_breaker_t* cb) {
    if (!cb) return 0;
    
    agentos_mutex_lock(cb->lock);
    
    uint64_t now_ns = get_timestamp_ns();
    
    switch (cb->state) {
        case CB_STATE_CLOSED:
            agentos_mutex_unlock(cb->lock);
            return 1;
            
        case CB_STATE_OPEN:
            if (now_ns - cb->open_time_ns >= (uint64_t)cb->config.timeout_ms * 1000000ULL) {
                cb_change_state(cb, CB_STATE_HALF_OPEN);
                agentos_mutex_unlock(cb->lock);
                return 1;
            }
            cb->stats.rejected_calls++;
            if (g_cb_manager) g_cb_manager->total_rejected++;
            agentos_mutex_unlock(cb->lock);
            return 0;
            
        case CB_STATE_HALF_OPEN:
            if (cb->half_open_success + cb->half_open_failure < cb->config.half_open_requests) {
                agentos_mutex_unlock(cb->lock);
                return 1;
            }
            cb->stats.rejected_calls++;
            if (g_cb_manager) g_cb_manager->total_rejected++;
            agentos_mutex_unlock(cb->lock);
            return 0;
            
        default:
            agentos_mutex_unlock(cb->lock);
            return 0;
    }
}

/**
 * @brief 记录成功
 */
void agentos_circuit_breaker_success(agentos_circuit_breaker_t* cb) {
    if (!cb) return;
    
    agentos_mutex_lock(cb->lock);
    
    uint64_t now_ns = get_timestamp_ns();
    
    sliding_window_push(cb->window, now_ns, 0);
    cb->stats.total_calls++;
    cb->stats.success_calls++;
    
    if (cb->state == CB_STATE_HALF_OPEN) {
        cb->half_open_success++;
        if (cb->half_open_success >= cb->config.success_threshold) {
            cb_change_state(cb, CB_STATE_CLOSED);
        }
    }
    
    agentos_mutex_unlock(cb->lock);
}

/**
 * @brief 记录失败
 */
void agentos_circuit_breaker_failure(agentos_circuit_breaker_t* cb) {
    if (!cb) return;
    
    agentos_mutex_lock(cb->lock);
    
    uint64_t now_ns = get_timestamp_ns();
    
    sliding_window_push(cb->window, now_ns, 1);
    cb->stats.total_calls++;
    cb->stats.failed_calls++;
    cb->stats.last_failure_time_ns = now_ns;
    
    if (cb->state == CB_STATE_HALF_OPEN) {
        cb->half_open_failure++;
        if (cb->half_open_failure >= cb->config.failure_threshold) {
            cb_change_state(cb, CB_STATE_OPEN);
        }
    } else if (cb->state == CB_STATE_CLOSED) {
        size_t failures = sliding_window_count_failures(cb->window, now_ns - 60000000000ULL);
        if (failures >= cb->config.failure_threshold) {
            cb_change_state(cb, CB_STATE_OPEN);
        }
    }
    
    agentos_mutex_unlock(cb->lock);
}

/**
 * @brief 强制打开熔断器
 */
agentos_error_t agentos_circuit_breaker_force_open(agentos_circuit_breaker_t* cb) {
    if (!cb) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cb->lock);
    cb_change_state(cb, CB_STATE_OPEN);
    agentos_mutex_unlock(cb->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 强制关闭熔断器
 */
agentos_error_t agentos_circuit_breaker_force_close(agentos_circuit_breaker_t* cb) {
    if (!cb) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cb->lock);
    cb_change_state(cb, CB_STATE_CLOSED);
    agentos_mutex_unlock(cb->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取熔断器状态
 */
circuit_breaker_state_t agentos_circuit_breaker_state(agentos_circuit_breaker_t* cb) {
    if (!cb) return CB_STATE_OPEN;
    return cb->state;
}

/**
 * @brief 获取熔断器统计信息
 */
agentos_error_t agentos_circuit_breaker_stats(agentos_circuit_breaker_t* cb, char** out_stats) {
    if (!cb || !out_stats) return AGENTOS_EINVAL;
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(cb->lock);
    
    cJSON_AddNumberToObject(stats_json, "id", cb->id);
    cJSON_AddStringToObject(stats_json, "name", cb->name);
    
    const char* state_str = "unknown";
    switch (cb->state) {
        case CB_STATE_CLOSED: state_str = "closed"; break;
        case CB_STATE_OPEN: state_str = "open"; break;
        case CB_STATE_HALF_OPEN: state_str = "half_open"; break;
    }
    cJSON_AddStringToObject(stats_json, "state", state_str);
    
    cJSON* stats_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats_obj, "total_calls", cb->stats.total_calls);
    cJSON_AddNumberToObject(stats_obj, "success_calls", cb->stats.success_calls);
    cJSON_AddNumberToObject(stats_obj, "failed_calls", cb->stats.failed_calls);
    cJSON_AddNumberToObject(stats_obj, "rejected_calls", cb->stats.rejected_calls);
    cJSON_AddNumberToObject(stats_obj, "timeout_calls", cb->stats.timeout_calls);
    cJSON_AddNumberToObject(stats_obj, "state_changes", cb->stats.state_changes);
    cJSON_AddItemToObject(stats_json, "stats", stats_obj);
    
    cJSON* config_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(config_obj, "failure_threshold", cb->config.failure_threshold);
    cJSON_AddNumberToObject(config_obj, "success_threshold", cb->config.success_threshold);
    cJSON_AddNumberToObject(config_obj, "timeout_ms", cb->config.timeout_ms);
    cJSON_AddNumberToObject(config_obj, "window_size", cb->config.window_size);
    cJSON_AddItemToObject(stats_json, "config", config_obj);
    
    agentos_mutex_unlock(cb->lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 设置降级函数
 */
agentos_error_t agentos_circuit_breaker_set_fallback(agentos_circuit_breaker_t* cb,
                                                     agentos_error_t (*fallback_fn)(void*),
                                                     void* data) {
    if (!cb) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cb->lock);
    cb->fallback_fn = fallback_fn;
    cb->fallback_data = data;
    agentos_mutex_unlock(cb->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行降级
 */
agentos_error_t agentos_circuit_breaker_fallback(agentos_circuit_breaker_t* cb) {
    if (!cb) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(cb->lock);
    
    if (cb->fallback_fn) {
        agentos_error_t result = cb->fallback_fn(cb->fallback_data);
        agentos_mutex_unlock(cb->lock);
        return result;
    }
    
    agentos_mutex_unlock(cb->lock);
    return AGENTOS_EUNAVAILABLE;
}

/**
 * @brief 获取管理器统计信息
 */
agentos_error_t agentos_circuit_breaker_manager_stats(char** out_stats) {
    if (!out_stats) return AGENTOS_EINVAL;
    
    if (!g_cb_manager) {
        *out_stats = strdup("{\"error\":\"manager not initialized\"}");
        return AGENTOS_ENOTINIT;
    }
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(g_cb_lock);
    
    cJSON_AddNumberToObject(stats_json, "breaker_count", g_cb_manager->breaker_count);
    cJSON_AddNumberToObject(stats_json, "total_rejected", g_cb_manager->total_rejected);
    cJSON_AddNumberToObject(stats_json, "total_state_changes", g_cb_manager->total_state_changes);
    
    cJSON* breakers_array = cJSON_CreateArray();
    for (uint32_t i = 0; i < MAX_CIRCUIT_BREAKERS; i++) {
        if (g_cb_manager->breakers[i]) {
            cJSON* breaker_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(breaker_obj, "name", g_cb_manager->breakers[i]->name);
            
            const char* state_str = "unknown";
            switch (g_cb_manager->breakers[i]->state) {
                case CB_STATE_CLOSED: state_str = "closed"; break;
                case CB_STATE_OPEN: state_str = "open"; break;
                case CB_STATE_HALF_OPEN: state_str = "half_open"; break;
            }
            cJSON_AddStringToObject(breaker_obj, "state", state_str);
            cJSON_AddItemToArray(breakers_array, breaker_obj);
        }
    }
    cJSON_AddItemToObject(stats_json, "breakers", breakers_array);
    
    agentos_mutex_unlock(g_cb_lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置熔断器统计
 */
void agentos_circuit_breaker_reset_stats(agentos_circuit_breaker_t* cb) {
    if (!cb) return;
    
    agentos_mutex_lock(cb->lock);
    
    memset(&cb->stats, 0, sizeof(cb_stats_t));
    
    agentos_mutex_unlock(cb->lock);
    
    AGENTOS_LOG_DEBUG("Circuit breaker %s stats reset", cb->name);
}

/**
 * @brief 健康检查
 */
agentos_error_t agentos_circuit_breaker_health_check(agentos_circuit_breaker_t* cb, char** out_json) {
    if (!cb || !out_json) return AGENTOS_EINVAL;
    
    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(cb->lock);
    
    cJSON_AddStringToObject(health_json, "component", "circuit_breaker");
    cJSON_AddNumberToObject(health_json, "id", cb->id);
    cJSON_AddStringToObject(health_json, "name", cb->name);
    
    const char* state_str = "unknown";
    const char* status = "healthy";
    
    switch (cb->state) {
        case CB_STATE_CLOSED: 
            state_str = "closed"; 
            status = "healthy";
            break;
        case CB_STATE_OPEN: 
            state_str = "open"; 
            status = "unhealthy";
            break;
        case CB_STATE_HALF_OPEN: 
            state_str = "half_open"; 
            status = "degraded";
            break;
    }
    
    cJSON_AddStringToObject(health_json, "state", state_str);
    cJSON_AddStringToObject(health_json, "status", status);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", get_timestamp_ns());
    
    double failure_rate = cb->stats.total_calls > 0 ?
                         (double)cb->stats.failed_calls / cb->stats.total_calls * 100.0 : 0.0;
    cJSON_AddNumberToObject(health_json, "failure_rate_percent", failure_rate);
    
    agentos_mutex_unlock(cb->lock);
    
    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);
    
    if (!health_str) return AGENTOS_ENOMEM;
    
    *out_json = health_str;
    return AGENTOS_SUCCESS;
}
