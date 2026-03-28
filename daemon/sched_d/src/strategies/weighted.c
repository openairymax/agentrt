/**
 * @file weighted.c
 * @brief 加权调度策略实现（线程安全版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 改进�?
 * 1. 线程安全的权重数据访�?
 * 2. 使用线程安全的随机数生成
 * 3. 内存安全
 * 4. 完善的错误处�?
 */

#include "strategy_interface.h"
#include "../../../bases/utils/strategy/include/strategy_common.h"
#include "platform.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ==================== 配置常量 ==================== */

#define MAX_AGENTS 256
#define MIN_WEIGHT 0.01
#define MAX_WEIGHT 100.0
#define DEFAULT_WEIGHT 1.0
#define WEIGHT_DECAY 0.95
#define WEIGHT_BOOST 1.05
#define LEARNING_RATE 0.1

/* ==================== Agent 权重数据 ==================== */

typedef struct {
    char* agent_id;
    double weight;
    double success_rate;
    uint64_t total_tasks;
    uint64_t successful_tasks;
    double avg_latency;
    uint64_t last_used;
} agent_weight_t;

/* ==================== 加权策略数据 ==================== */

typedef struct {
    agent_weight_t agents[MAX_AGENTS];
    size_t agent_count;
    agentos_mutex_t lock;
    int initialized;
    double total_weight;
} weighted_data_t;

/* ==================== 内部函数 ==================== */

/**
 * @brief 查找 Agent 索引
 */
static int find_agent_index(weighted_data_t* data, const char* agent_id) {
    for (size_t i = 0; i < data->agent_count; i++) {
        if (strcmp(data->agents[i].agent_id, agent_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 归一化权�?
 */
static void normalize_weights(weighted_data_t* data) {
    double sum = 0.0;
    
    for (size_t i = 0; i < data->agent_count; i++) {
        sum += data->agents[i].weight;
    }
    
    if (sum > 0) {
        data->total_weight = sum;
        for (size_t i = 0; i < data->agent_count; i++) {
            /* 保持原始权重，仅更新 total_weight */
        }
    }
}

/**
 * @brief 根据权重选择 Agent（轮盘赌算法�?
 */
static int select_by_weight(weighted_data_t* data) {
    if (data->agent_count == 0) {
        return -1;
    }
    
    /* 使用线程安全的随机数 */
    double r = agentos_random_float() * data->total_weight;
    double cumulative = 0.0;
    
    for (size_t i = 0; i < data->agent_count; i++) {
        cumulative += data->agents[i].weight;
        if (r <= cumulative) {
            return (int)i;
        }
    }
    
    /* 返回最后一�?*/
    return (int)(data->agent_count - 1);
}

/**
 * @brief 更新 Agent 权重
 */
static void update_agent_weight(agent_weight_t* agent, int success, double latency) {
    agent->total_tasks++;
    
    if (success) {
        agent->successful_tasks++;
        
        /* 更新成功�?*/
        agent->success_rate = (double)agent->successful_tasks / (double)agent->total_tasks;
        
        /* 更新平均延迟 */
        if (agent->avg_latency == 0) {
            agent->avg_latency = latency;
        } else {
            agent->avg_latency = agent->avg_latency * 0.9 + latency * 0.1;
        }
        
        /* 提升权重 */
        agent->weight *= WEIGHT_BOOST;
    } else {
        /* 降低权重 */
        agent->weight *= WEIGHT_DECAY;
    }
    
    /* 限制权重范围 */
    if (agent->weight < MIN_WEIGHT) {
        agent->weight = MIN_WEIGHT;
    }
    if (agent->weight > MAX_WEIGHT) {
        agent->weight = MAX_WEIGHT;
    }
}

/* ==================== 策略接口实现 ==================== */

/**
 * @brief 创建加权策略
 */
static int weighted_create(sched_strategy_t* strategy, const sched_strategy_config_t* manager) {
    (void)manager;
    
    weighted_data_t* data = (weighted_data_t*)calloc(1, sizeof(weighted_data_t));
    if (!data) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to allocate weighted data");
    }
    
    if (agentos_mutex_init(&data->lock) != 0) {
        free(data);
        AGENTOS_ERROR(AGENTOS_ERR_UNKNOWN, "Failed to initialize mutex");
    }
    
    data->initialized = 1;
    data->total_weight = 0.0;
    data->agent_count = 0;
    
    strategy->data = data;
    return AGENTOS_OK;
}

/**
 * @brief 销毁加权策�?
 */
static void weighted_destroy(sched_strategy_t* strategy) {
    if (!strategy || !strategy->data) return;
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    /* 释放 Agent ID */
    for (size_t i = 0; i < data->agent_count; i++) {
        free(data->agents[i].agent_id);
    }
    
    agentos_mutex_unlock(&data->lock);
    agentos_mutex_destroy(&data->lock);
    free(data);
    strategy->data = NULL;
}

/**
 * @brief 注册 Agent
 */
static int weighted_register(sched_strategy_t* strategy, const sched_agent_info_t* agent) {
    if (!strategy || !strategy->data || !agent) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    /* 检查是否已存在 */
    int idx = find_agent_index(data, agent->agent_id);
    if (idx >= 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_ALREADY_EXISTS, "Agent already registered");
    }
    
    /* 检查容�?*/
    if (data->agent_count >= MAX_AGENTS) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "Too many agents");
    }
    
    /* 添加�?Agent */
    idx = (int)data->agent_count;
    data->agents[idx].agent_id = strdup(agent->agent_id);
    if (!data->agents[idx].agent_id) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to duplicate agent ID");
    }
    
    /* 初始化权�?*/
    data->agents[idx].weight = agent->initial_weight > 0 ? agent->initial_weight : DEFAULT_WEIGHT;
    data->agents[idx].success_rate = 1.0;
    data->agents[idx].total_tasks = 0;
    data->agents[idx].successful_tasks = 0;
    data->agents[idx].avg_latency = 0.0;
    data->agents[idx].last_used = 0;
    
    data->agent_count++;
    normalize_weights(data);
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/**
 * @brief 注销 Agent
 */
static int weighted_unregister(sched_strategy_t* strategy, const char* agent_id) {
    if (!strategy || !strategy->data || !agent_id) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    int idx = find_agent_index(data, agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    /* 释放 Agent ID */
    free(data->agents[idx].agent_id);
    
    /* 移动后面的元�?*/
    for (size_t i = (size_t)idx; i < data->agent_count - 1; i++) {
        data->agents[i] = data->agents[i + 1];
    }
    
    data->agent_count--;
    normalize_weights(data);
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/**
 * @brief 选择 Agent
 */
static int weighted_select(sched_strategy_t* strategy,
                           const sched_task_t* task,
                           sched_result_t* result) {
    if (!strategy || !strategy->data || !task || !result) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    if (data->agent_count == 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_SCHED_NO_AGENT, "No agents available");
    }
    
    /* 根据权重选择 */
    int idx = select_by_weight(data);
    if (idx < 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_SCHED_NO_AGENT, "Failed to select agent");
    }
    
    /* 填充结果 */
    result->agent_id = strdup(data->agents[idx].agent_id);
    result->score = data->agents[idx].weight / data->total_weight;
    result->reason = "Selected by weighted random";
    
    /* 更新最后使用时�?*/
    data->agents[idx].last_used = agentos_time_ms();
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/**
 * @brief 反馈执行结果
 */
static int weighted_feedback(sched_strategy_t* strategy,
                             const char* agent_id,
                             const sched_feedback_t* feedback) {
    if (!strategy || !strategy->data || !agent_id || !feedback) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    int idx = find_agent_index(data, agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    /* 更新权重 */
    update_agent_weight(&data->agents[idx], 
                        feedback->success ? 1 : 0, 
                        (double)feedback->latency_ms);
    
    normalize_weights(data);
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/**
 * @brief 获取 Agent 统计信息
 */
static int weighted_get_stats(sched_strategy_t* strategy,
                              const char* agent_id,
                              sched_agent_stats_t* stats) {
    if (!strategy || !strategy->data || !agent_id || !stats) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    int idx = find_agent_index(data, agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&data->lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    stats->weight = data->agents[idx].weight;
    stats->success_rate = data->agents[idx].success_rate;
    stats->total_tasks = data->agents[idx].total_tasks;
    stats->successful_tasks = data->agents[idx].successful_tasks;
    stats->avg_latency_ms = data->agents[idx].avg_latency;
    stats->last_used = data->agents[idx].last_used;
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/**
 * @brief 重置策略
 */
static int weighted_reset(sched_strategy_t* strategy) {
    if (!strategy || !strategy->data) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    weighted_data_t* data = (weighted_data_t*)strategy->data;
    
    agentos_mutex_lock(&data->lock);
    
    /* 重置所�?Agent 权重 */
    for (size_t i = 0; i < data->agent_count; i++) {
        data->agents[i].weight = DEFAULT_WEIGHT;
        data->agents[i].success_rate = 1.0;
        data->agents[i].total_tasks = 0;
        data->agents[i].successful_tasks = 0;
        data->agents[i].avg_latency = 0.0;
        data->agents[i].last_used = 0;
    }
    
    normalize_weights(data);
    
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

/* ==================== 策略注册 ==================== */

sched_strategy_vtable_t g_weighted_strategy_vtable = {
    .name = "weighted",
    .description = "Weighted random scheduling strategy",
    .create = weighted_create,
    .destroy = weighted_destroy,
    .register_agent = weighted_register,
    .unregister_agent = weighted_unregister,
    .select = weighted_select,
    .feedback = weighted_feedback,
    .get_stats = weighted_get_stats,
    .reset = weighted_reset,
};

/**
 * @brief 获取加权策略虚表
 */
sched_strategy_vtable_t* weighted_strategy_get_vtable(void) {
    return &g_weighted_strategy_vtable;
}
