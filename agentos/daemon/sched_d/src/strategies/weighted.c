/**
 * @file weighted.c
 * @brief 加权调度策略实现（基于实际API定义）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy_interface.h"
#include "scheduler_service.h"
#include "platform.h"
#include "daemon_errors.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef AGENTOS_ERR_SCHED_NO_AGENT
#define AGENTOS_ERR_SCHED_NO_AGENT  (-2001)
#endif

#define MAX_AGENTS 256
#define DEFAULT_WEIGHT 1.0
#define WEIGHT_DECAY 0.95
#define WEIGHT_BOOST 1.05
#define MIN_WEIGHT 0.01
#define MAX_WEIGHT 100.0

typedef struct {
    char* agent_id;
    double weight;
    double success_rate;
    uint64_t total_tasks;
    uint64_t successful_tasks;
    double avg_latency_ms;
} agent_weight_t;

typedef struct {
    agent_weight_t agents[MAX_AGENTS];
    size_t agent_count;
    agentos_mutex_t lock;
    double total_weight;
} weighted_data_t;

static int find_agent_index(weighted_data_t* data, const char* agent_id) {
    for (size_t i = 0; i < data->agent_count; i++) {
        if (data->agents[i].agent_id && strcmp(data->agents[i].agent_id, agent_id) == 0)
            return (int)i;
    }
    return -1;
}

static void normalize_weights(weighted_data_t* data) {
    double sum = 0.0;
    for (size_t i = 0; i < data->agent_count; i++)
        sum += data->agents[i].weight;
    data->total_weight = sum > 0 ? sum : 1.0;
}

static int select_by_weight(weighted_data_t* data) {
    if (data->agent_count == 0) return -1;
    double r = ((double)agentos_random_float()) * data->total_weight;
    double cumulative = 0.0;
    for (size_t i = 0; i < data->agent_count; i++) {
        cumulative += data->agents[i].weight;
        if (r <= cumulative) return (int)i;
    }
    return (int)(data->agent_count - 1);
}

/* ==================== strategy_interface_t 实现 ==================== */

static int weighted_create(const sched_config_t* manager, void** out_data) {
    (void)manager;
    weighted_data_t* data = (weighted_data_t*)calloc(1, sizeof(weighted_data_t));
    if (!data) return AGENTOS_ERR_OUT_OF_MEMORY;
    if (agentos_mutex_init(&data->lock) != 0) { free(data); return AGENTOS_ERR_UNKNOWN; }
    data->total_weight = 0.0;
    *out_data = data;
    return AGENTOS_OK;
}

static int weighted_destroy(void* raw_data) {
    if (!raw_data) return AGENTOS_ERR_INVALID_PARAM;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    for (size_t i = 0; i < data->agent_count; i++) free(data->agents[i].agent_id);
    agentos_mutex_unlock(&data->lock);
    agentos_mutex_destroy(&data->lock);
    free(data);
    return AGENTOS_OK;
}

static int weighted_register_agent(void* raw_data, const agent_info_t* agent) {
    if (!raw_data || !agent || !agent->agent_id) return AGENTOS_ERR_INVALID_PARAM;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    if (find_agent_index(data, agent->agent_id) >= 0) {
        agentos_mutex_unlock(&data->lock);
        return AGENTOS_ERR_ALREADY_EXISTS;
    }
    if (data->agent_count >= MAX_AGENTS) {
        agentos_mutex_unlock(&data->lock);
        return AGENTOS_ERR_OVERFLOW;
    }
    size_t idx = data->agent_count;
    data->agents[idx].agent_id = strdup(agent->agent_id);
    if (!data->agents[idx].agent_id) {
        agentos_mutex_unlock(&data->lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    data->agents[idx].weight = agent->weight > 0 ? agent->weight : DEFAULT_WEIGHT;
    data->agents[idx].success_rate = agent->success_rate > 0 ? agent->success_rate : 1.0;
    data->agents[idx].total_tasks = 0;
    data->agents[idx].successful_tasks = 0;
    data->agents[idx].avg_latency_ms = 0.0;
    data->agent_count++;
    normalize_weights(data);
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

static int weighted_unregister_agent(void* raw_data, const char* agent_id) {
    if (!raw_data || !agent_id) return AGENTOS_ERR_INVALID_PARAM;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    int idx = find_agent_index(data, agent_id);
    if (idx < 0) { agentos_mutex_unlock(&data->lock); return AGENTOS_ERR_NOT_FOUND; }
    free(data->agents[idx].agent_id);
    for (size_t i = (size_t)idx; i < data->agent_count - 1; i++)
        data->agents[i] = data->agents[i + 1];
    memset(&data->agents[--data->agent_count], 0, sizeof(agent_weight_t));
    normalize_weights(data);
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

static int weighted_update_agent_status(void* raw_data, const agent_info_t* agent_info) {
    if (!raw_data || !agent_info) return AGENTOS_ERR_INVALID_PARAM;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    int idx = find_agent_index(data, agent_info->agent_id);
    if (idx >= 0) {
        data->agents[idx].success_rate = agent_info->success_rate;
        data->agents[idx].avg_latency_ms = (double)agent_info->avg_response_time_ms;
    }
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

static int weighted_schedule(void* raw_data, const task_info_t* task_info, sched_result_t** out_result) {
    (void)task_info;
    if (!raw_data || !out_result) return AGENTOS_ERR_INVALID_PARAM;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    if (data->agent_count == 0) { agentos_mutex_unlock(&data->lock); return AGENTOS_ERR_SCHED_NO_AGENT; }
    int idx = select_by_weight(data);
    if (idx < 0) { agentos_mutex_unlock(&data->lock); return AGENTOS_ERR_SCHED_NO_AGENT; }
    sched_result_t* result = (sched_result_t*)calloc(1, sizeof(sched_result_t));
    if (!result) { agentos_mutex_unlock(&data->lock); return AGENTOS_ERR_OUT_OF_MEMORY; }
    result->selected_agent_id = strdup(data->agents[idx].agent_id);
    result->confidence = (float)(data->agents[idx].weight / data->total_weight);
    result->estimated_time_ms = (uint32_t)data->agents[idx].avg_latency_ms;
    *out_result = result;
    agentos_mutex_unlock(&data->lock);
    return AGENTOS_OK;
}

static const char* weighted_get_name(void) { return "weighted"; }

static size_t weighted_get_available_agent_count(void* raw_data) {
    if (!raw_data) return 0;
    weighted_data_t* data = (weighted_data_t*)raw_data;
    agentos_mutex_lock(&data->lock);
    size_t c = data->agent_count;
    agentos_mutex_unlock(&data->lock);
    return c;
}

static size_t weighted_get_total_agent_count(void* raw_data) {
    return weighted_get_available_agent_count(raw_data);
}

static const strategy_interface_t g_weighted_strategy = {
    .create = weighted_create,
    .destroy = weighted_destroy,
    .register_agent = weighted_register_agent,
    .unregister_agent = weighted_unregister_agent,
    .update_agent_status = weighted_update_agent_status,
    .schedule = weighted_schedule,
    .get_name = weighted_get_name,
    .get_available_agent_count = weighted_get_available_agent_count,
    .get_total_agent_count = weighted_get_total_agent_count,
};

const strategy_interface_t* get_weighted_strategy(void) {
    return &g_weighted_strategy;
}
