// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file guard_core.c
 * @brief SafetyGuard Core Implementation
 */

#include "guard_core.h"
#include "utils/cupolas_utils.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// 内部数据结构
// ============================================================================

// 守卫管理器私有数据
typedef struct guard_manager_private {
    guard_manager_config_t config;      // 管理器配置
    guard_t** guards;                   // 守卫指针数组
    size_t guard_count;                 // 当前守卫数量
    size_t guard_capacity;              // 守卫数组容量
    cupolas_mutex_t lock;               // 互斥锁
    cupolas_cond_t cond;                // 条件变量（用于异步）
    uint64_t next_guard_id;             // 下一个守卫ID
    guard_stats_t stats;                // 管理器统计信息
    void* result_cache;                 // 结果缓存（可选）
    bool initialized;                   // 是否已初始化
} guard_manager_private_t;

// 异步请求上下文
typedef struct async_request {
    uint64_t request_id;                // 请求ID
    guard_manager_private_t* manager;   // 管理器
    guard_context_t context;            // 检测上下文（复制）
    guard_result_t* results;            // 结果数组
    size_t max_results;                 // 最大结果数
    size_t actual_results;              // 实际结果数
    void (*callback)(uint64_t, const guard_result_t*, size_t, void*); // 回调函数
    void* user_data;                    // 用户数据
    struct async_request* next;         // 下一个请求（链表）
} async_request_t;

// ============================================================================
// 静态辅助函数
// ============================================================================

// 生成唯一守卫ID
static guard_id_t generate_guard_id(guard_manager_private_t* manager) {
    cupolas_mutex_lock(&manager->lock);
    guard_id_t id = ++manager->next_guard_id;
    cupolas_mutex_unlock(&manager->lock);
    return id;
}

// 查找守卫索引（按ID）
static size_t find_guard_index_by_id(guard_manager_private_t* manager, guard_id_t guard_id) {
    for (size_t i = 0; i < manager->guard_count; i++) {
        if (manager->guards[i] && manager->guards[i]->id == guard_id) {
            return i;
        }
    }
    return SIZE_MAX;
}

// 查找守卫索引（按名称）
static size_t find_guard_index_by_name(guard_manager_private_t* manager, const char* name) {
    if (!name) return SIZE_MAX;
    
    for (size_t i = 0; i < manager->guard_count; i++) {
        if (manager->guards[i] && strcmp(manager->guards[i]->name, name) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

// 比较守卫优先级（用于排序）
static int compare_guard_priority(const void* a, const void* b) {
    const guard_t* guard_a = *(const guard_t**)a;
    const guard_t* guard_b = *(const guard_t**)b;
    
    // 按优先级降序排列（优先级高的先检测）
    if (guard_a->priority > guard_b->priority) return -1;
    if (guard_a->priority < guard_b->priority) return 1;
    return 0;
}

// 排序守卫数组（按优先级）
static void sort_guards_by_priority(guard_manager_private_t* manager) {
    if (manager->guard_count < 2) return;
    
    qsort(manager->guards, manager->guard_count, sizeof(guard_t*), compare_guard_priority);
}

// 复制检测上下文
static guard_context_t* copy_guard_context(const guard_context_t* src) {
    if (!src) return NULL;
    
    guard_context_t* dst = (guard_context_t*)calloc(1, sizeof(guard_context_t));
    if (!dst) return NULL;
    
    // 复制字符串字段（如果非NULL）
    if (src->operation) dst->operation = strdup(src->operation);
    if (src->resource) dst->resource = strdup(src->resource);
    if (src->agent_id) dst->agent_id = strdup(src->agent_id);
    if (src->session_id) dst->session_id = strdup(src->session_id);
    
    // 复制输入数据
    if (src->input_data && src->input_size > 0) {
        dst->input_data = malloc(src->input_size);
        if (dst->input_data) {
            memcpy(dst->input_data, src->input_data, src->input_size);
            dst->input_size = src->input_size;
        }
    }
    
    // 注意：context_data不复制，由调用者管理
    dst->context_data = src->context_data;
    dst->timestamp = src->timestamp;
    
    return dst;
}

// 释放检测上下文
static void free_guard_context(guard_context_t* context) {
    if (!context) return;
    
    if (context->operation) free((void*)context->operation);
    if (context->resource) free((void*)context->resource);
    if (context->agent_id) free((void*)context->agent_id);
    if (context->session_id) free((void*)context->session_id);
    if (context->input_data) free(context->input_data);
    
    free(context);
}

// ============================================================================
// 守卫管理器API实现
// ============================================================================

guard_manager_t* guard_manager_create(const guard_manager_config_t* config) {
    if (!config) return NULL;
    
    // 分配管理器内存
    guard_manager_private_t* manager = (guard_manager_private_t*)calloc(1, sizeof(guard_manager_private_t));
    if (!manager) return NULL;
    
    // 复制配置
    manager->config = *config;
    
    // 设置默认值
    if (manager->config.max_guards == 0) {
        manager->config.max_guards = 32;
    }
    if (manager->config.default_timeout_ms == 0) {
        manager->config.default_timeout_ms = 5000; // 5秒
    }
    
    // 分配守卫数组
    manager->guard_capacity = manager->config.max_guards;
    manager->guards = (guard_t**)calloc(manager->guard_capacity, sizeof(guard_t*));
    if (!manager->guards) {
        free(manager);
        return NULL;
    }
    
    // 初始化锁和条件变量
    if (cupolas_mutex_init(&manager->lock) != CUPOLAS_OK) {
        free(manager->guards);
        free(manager);
        return NULL;
    }
    
    if (cupolas_cond_init(&manager->cond) != CUPOLAS_OK) {
        cupolas_mutex_destroy(&manager->lock);
        free(manager->guards);
        free(manager);
        return NULL;
    }
    
    // 初始化统计信息
    memset(&manager->stats, 0, sizeof(manager->stats));
    
    manager->next_guard_id = 1000; // 起始ID
    manager->initialized = true;
    
    return (guard_manager_t*)manager;
}

void guard_manager_destroy(guard_manager_t* manager) {
    if (!manager) return;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    // 销毁所有守卫
    for (size_t i = 0; i < priv->guard_count; i++) {
        if (priv->guards[i]) {
            guard_destroy(priv->guards[i]);
        }
    }
    
    // 销毁锁和条件变量
    cupolas_mutex_destroy(&priv->lock);
    cupolas_cond_destroy(&priv->cond);
    
    // 释放内存
    free(priv->guards);
    free(priv);
}

int guard_manager_register_guard(guard_manager_t* manager, guard_t* guard) {
    if (!manager || !guard) return cupolas_ERROR_INVALID_ARG;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    
    // 检查容量
    if (priv->guard_count >= priv->guard_capacity) {
        cupolas_mutex_unlock(&priv->lock);
        return cupolas_ERROR_NO_MEMORY;
    }
    
    // 检查名称是否重复
    if (find_guard_index_by_name(priv, guard->name) != SIZE_MAX) {
        cupolas_mutex_unlock(&priv->lock);
        return cupolas_ERROR_BUSY;
    }
    
    // 分配ID
    guard->id = generate_guard_id(priv);
    
    // 添加到数组
    priv->guards[priv->guard_count++] = guard;
    
    // 按优先级排序
    if (priv->config.enable_priority_scheduling) {
        sort_guards_by_priority(priv);
    }
    
    cupolas_mutex_unlock(&priv->lock);
    
    return CUPOLAS_OK;
}

int guard_manager_unregister_guard(guard_manager_t* manager, guard_id_t guard_id) {
    if (!manager || guard_id == 0) return cupolas_ERROR_INVALID_ARG;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    
    size_t index = find_guard_index_by_id(priv, guard_id);
    if (index == SIZE_MAX) {
        cupolas_mutex_unlock(&priv->lock);
        return cupolas_ERROR_NOT_FOUND;
    }
    
    // 移除守卫
    guard_t* guard = priv->guards[index];
    
    // 移动后续元素
    for (size_t i = index; i < priv->guard_count - 1; i++) {
        priv->guards[i] = priv->guards[i + 1];
    }
    priv->guard_count--;
    
    cupolas_mutex_unlock(&priv->lock);
    
    // 销毁守卫
    guard_destroy(guard);
    
    return CUPOLAS_OK;
}

guard_t* guard_manager_find_guard_by_name(guard_manager_t* manager, const char* name) {
    if (!manager || !name) return NULL;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    size_t index = find_guard_index_by_name(priv, name);
    guard_t* guard = (index != SIZE_MAX) ? priv->guards[index] : NULL;
    cupolas_mutex_unlock(&priv->lock);
    
    return guard;
}

guard_t* guard_manager_find_guard_by_id(guard_manager_t* manager, guard_id_t id) {
    if (!manager || id == 0) return NULL;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    size_t index = find_guard_index_by_id(priv, id);
    guard_t* guard = (index != SIZE_MAX) ? priv->guards[index] : NULL;
    cupolas_mutex_unlock(&priv->lock);
    
    return guard;
}

int guard_manager_check_sync(
    guard_manager_t* manager,
    const guard_context_t* context,
    guard_result_t* results,
    size_t max_results,
    size_t* actual_results)
{
    if (!manager || !context || !results || !actual_results) {
        return cupolas_ERROR_INVALID_ARG;
    }
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    
    // 更新统计信息
    priv->stats.total_checks++;
    
    // 执行每个守卫的检测
    size_t result_count = 0;
    uint64_t start_time = cupolas_get_timestamp_ns();
    
    for (size_t i = 0; i < priv->guard_count && result_count < max_results; i++) {
        guard_t* guard = priv->guards[i];
        
        // 只检测启用的守卫
        if (!guard || guard->state != GUARD_STATE_ENABLED) {
            continue;
        }
        
        // 执行检测
        guard_result_t result;
        memset(&result, 0, sizeof(result));
        
        int check_result = guard_check(guard, context, &result);
        if (check_result == CUPOLAS_OK) {
            if (result_count < max_results) {
                results[result_count] = result;
                result_count++;
                
                if (result.risk_level == RISK_LEVEL_SAFE) {
                    priv->stats.safe_checks++;
                } else {
                    priv->stats.risky_checks++;
                    
                    if (result.recommended_action == GUARD_ACTION_BLOCK ||
                        result.recommended_action == GUARD_ACTION_ISOLATE ||
                        result.recommended_action == GUARD_ACTION_TERMINATE) {
                        priv->stats.blocked_operations++;
                    } else if (result.recommended_action == GUARD_ACTION_WARN) {
                        priv->stats.warning_operations++;
                    }
                }
            }
        } else {
            priv->stats.error_checks++;
        }
        
        // TODO: 处理超时
    }
    
    uint64_t end_time = cupolas_get_timestamp_ns();
    uint64_t detection_time = end_time - start_time;
    
    // 更新检测时间统计
    priv->stats.total_detection_time += detection_time;
    if (detection_time > priv->stats.max_detection_time) {
        priv->stats.max_detection_time = detection_time;
    }
    
    *actual_results = result_count;
    
    cupolas_mutex_unlock(&priv->lock);
    
    return CUPOLAS_OK;
}

uint64_t guard_manager_check_async(
    guard_manager_t* manager,
    const guard_context_t* context,
    void (*callback)(uint64_t request_id, const guard_result_t* results, 
                     size_t count, void* user_data),
    void* user_data)
{
    // 简化实现：调用同步版本
    // 实际实现应创建异步请求并放入队列，由工作线程处理
    
    static uint64_t next_request_id = 1;
    uint64_t request_id = __atomic_fetch_add(&next_request_id, 1, __ATOMIC_RELAXED);
    
    // 分配结果数组
    const size_t MAX_RESULTS = 16;
    guard_result_t* results = (guard_result_t*)calloc(MAX_RESULTS, sizeof(guard_result_t));
    if (!results) return 0;
    
    size_t actual_results = 0;
    
    // 执行同步检测
    int result = guard_manager_check_sync(manager, context, results, MAX_RESULTS, &actual_results);
    
    // 调用回调
    if (callback) {
        callback(request_id, results, actual_results, user_data);
    }
    
    // 清理
    free(results);
    
    return (result == CUPOLAS_OK) ? request_id : 0;
}

int guard_manager_get_stats(guard_manager_t* manager, guard_stats_t* stats) {
    if (!manager || !stats) return cupolas_ERROR_INVALID_ARG;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    *stats = priv->stats;
    cupolas_mutex_unlock(&priv->lock);
    
    return CUPOLAS_OK;
}

int guard_manager_reset_stats(guard_manager_t* manager) {
    if (!manager) return cupolas_ERROR_INVALID_ARG;
    
    guard_manager_private_t* priv = (guard_manager_private_t*)manager;
    
    cupolas_mutex_lock(&priv->lock);
    memset(&priv->stats, 0, sizeof(priv->stats));
    cupolas_mutex_unlock(&priv->lock);
    
    return CUPOLAS_OK;
}

// ============================================================================
// 守卫实例管理API实现
// ============================================================================

guard_t* guard_create(
    const char* name,
    const char* description,
    guard_type_t type,
    const guard_ops_t* ops)
{
    if (!name || !ops) return NULL;
    
    // 分配守卫内存
    guard_t* guard = (guard_t*)calloc(1, sizeof(guard_t));
    if (!guard) return NULL;
    
    // 复制名称和描述
    strncpy(guard->name, name, GUARD_NAME_MAX_LEN - 1);
    guard->name[GUARD_NAME_MAX_LEN - 1] = '\0';
    if (description) {
        strncpy(guard->description, description, GUARD_DESC_MAX_LEN - 1);
        guard->description[GUARD_DESC_MAX_LEN - 1] = '\0';
    }
    
    // 设置基本属性
    guard->type = type;
    guard->state = GUARD_STATE_DISABLED;
    guard->priority = GUARD_PRIORITY_NORMAL;
    
    // 复制操作函数表
    guard->ops = (guard_ops_t*)malloc(sizeof(guard_ops_t));
    if (!guard->ops) {
        free(guard);
        return NULL;
    }
    memcpy(guard->ops, ops, sizeof(guard_ops_t));
    
    // 初始化统计信息
    memset(&guard->stats, 0, sizeof(guard->stats));
    
    guard->created_time = cupolas_get_timestamp_ns();
    
    return guard;
}

void guard_destroy(guard_t* guard) {
    if (!guard) return;
    
    // 调用清理函数
    if (guard->ops && guard->ops->cleanup) {
        guard->ops->cleanup(guard->priv_data);
    }
    
    // 清理操作函数表
    if (guard->ops) free(guard->ops);
    
    // 清理私有数据
    if (guard->priv_data) free(guard->priv_data);
    
    // 清理配置中的自定义数据
    if (guard->config.custom_config) free(guard->config.custom_config);
    
    free(guard);
}

int guard_init(guard_t* guard, const guard_config_t* config) {
    if (!guard || !config || !guard->ops) return cupolas_ERROR_INVALID_ARG;
    
    // 复制配置
    guard->config = *config;
    
    // 复制自定义配置数据
    if (config->custom_config && config->custom_config_size > 0) {
        guard->config.custom_config = malloc(config->custom_config_size);
        if (!guard->config.custom_config) return cupolas_ERROR_NO_MEMORY;
        memcpy(guard->config.custom_config, config->custom_config, config->custom_config_size);
    }
    
    // 调用守卫初始化函数
    if (guard->ops->init) {
        int result = guard->ops->init(guard->priv_data, config);
        if (result != CUPOLAS_OK) return result;
    }
    
    guard->state = GUARD_STATE_ENABLED;
    
    return CUPOLAS_OK;
}

int guard_check(guard_t* guard, const guard_context_t* context, guard_result_t* result) {
    if (!guard || !context || !result || !guard->ops) return cupolas_ERROR_INVALID_ARG;
    
    // 检查守卫状态
    if (guard->state != GUARD_STATE_ENABLED && guard->state != GUARD_STATE_ACTIVE) {
        return cupolas_ERROR_BUSY;
    }
    
    // 更新最后使用时间
    guard->last_used_time = cupolas_get_timestamp_ns();
    
    // 调用检测函数
    if (!guard->ops->check) return cupolas_ERROR_NOT_SUPPORTED;
    
    uint64_t start_time = cupolas_get_timestamp_ns();
    int check_result = guard->ops->check(guard->priv_data, context, result);
    uint64_t end_time = cupolas_get_timestamp_ns();
    
    // 更新统计信息
    guard->stats.total_checks++;
    guard->stats.total_detection_time += (end_time - start_time);
    
    if (check_result == CUPOLAS_OK) {
        // 更新检测时间
        result->detection_time = end_time - start_time;
        
        // 更新风险统计
        if (result->risk_level == RISK_LEVEL_SAFE) {
            guard->stats.safe_checks++;
        } else {
            guard->stats.risky_checks++;
        }
    }
    
    return check_result;
}

int guard_enable(guard_t* guard) {
    if (!guard) return cupolas_ERROR_INVALID_ARG;
    
    guard->state = GUARD_STATE_ENABLED;
    return CUPOLAS_OK;
}

int guard_disable(guard_t* guard) {
    if (!guard) return cupolas_ERROR_INVALID_ARG;
    
    guard->state = GUARD_STATE_DISABLED;
    return CUPOLAS_OK;
}

int guard_update_rules(guard_t* guard, const guard_rule_t* rules, size_t count) {
    if (!guard || !rules || count == 0) return cupolas_ERROR_INVALID_ARG;
    
    if (!guard->ops || !guard->ops->update_rules) {
        return cupolas_ERROR_NOT_SUPPORTED;
    }
    
    return guard->ops->update_rules(guard->priv_data, rules, count);
}

int guard_get_stats(guard_t* guard, guard_stats_t* stats) {
    if (!guard || !stats) return cupolas_ERROR_INVALID_ARG;
    
    *stats = guard->stats;
    return CUPOLAS_OK;
}

int guard_reset_stats(guard_t* guard) {
    if (!guard) return cupolas_ERROR_INVALID_ARG;
    
    memset(&guard->stats, 0, sizeof(guard->stats));
    return CUPOLAS_OK;
}