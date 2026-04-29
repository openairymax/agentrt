/**
 * @file ml_based.c
 * @brief ML-Based Dispatching Strategy Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * Multi-dimensional scoring dispatching strategy that evaluates candidates
 * using cost, success_rate, trust_score, and priority with configurable
 * feature weights. Supports online weight adaptation based on task outcomes.
 */

#include "cognition.h"
#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>

#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <math.h>
#include <time.h>

#define ML_FEATURE_COUNT 4
#define ML_MAX_HISTORY 256

typedef struct ml_dispatch_data {
    float feature_weights[ML_FEATURE_COUNT];
    float learning_rate;
    void* registry_ctx;
    agent_registry_get_agents_func get_agents_func;
    float reward_history[ML_MAX_HISTORY];
    size_t history_count;
    size_t history_pos;
    uint64_t last_dispatch_time_ns;
    char last_selected_agent_id[128];
} ml_dispatch_data_t;

enum ml_feature_index {
    ML_FEATURE_COST = 0,
    ML_FEATURE_SUCCESS_RATE = 1,
    ML_FEATURE_TRUST = 2,
    ML_FEATURE_PRIORITY = 3
};

static float normalize_cost(float cost) {
    if (cost <= 0.0f) return 1.0f;
    return 1.0f / (1.0f + cost);
}

static float compute_agent_score(
    ml_dispatch_data_t* data,
    const agent_info_t* info)
{
    if (!info) return 0.0f;

    float features[ML_FEATURE_COUNT];
    features[ML_FEATURE_COST] = normalize_cost(info->cost_estimate);
    features[ML_FEATURE_SUCCESS_RATE] = info->success_rate;
    features[ML_FEATURE_TRUST] = info->trust_score;
    features[ML_FEATURE_PRIORITY] = info->priority > 0 ?
        (float)info->priority / 10.0f : 0.1f;

    for (size_t i = 0; i < ML_FEATURE_COUNT; i++) {
        if (features[i] < 0.0f) features[i] = 0.0f;
        if (features[i] > 1.0f) features[i] = 1.0f;
    }

    float score = 0.0f;
    for (size_t i = 0; i < ML_FEATURE_COUNT; i++) {
        score += data->feature_weights[i] * features[i];
    }

    return score;
}

static void ml_update_weights(ml_dispatch_data_t* data, float reward) {
    float lr = data->learning_rate;
    for (size_t i = 0; i < ML_FEATURE_COUNT; i++) {
        data->feature_weights[i] += lr * reward * data->feature_weights[i] * 0.01f;
        if (data->feature_weights[i] < 0.01f) data->feature_weights[i] = 0.01f;
        if (data->feature_weights[i] > 10.0f) data->feature_weights[i] = 10.0f;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < ML_FEATURE_COUNT; i++) {
        sum += data->feature_weights[i];
    }
    if (sum > 0.0f) {
        for (size_t i = 0; i < ML_FEATURE_COUNT; i++) {
            data->feature_weights[i] /= sum;
        }
    }
}

static float ml_average_reward(ml_dispatch_data_t* data) {
    if (data->history_count == 0) return 0.5f;
    float sum = 0.0f;
    size_t count = data->history_count < ML_MAX_HISTORY ?
        data->history_count : ML_MAX_HISTORY;
    for (size_t i = 0; i < count; i++) {
        sum += data->reward_history[i];
    }
    return sum / (float)count;
}

static agentos_error_t ml_dispatch(
    const agentos_task_node_t* task,
    const void** candidates,
    size_t count,
    void* context,
    char** out_agent_id) {
    if (!context || !out_agent_id) return AGENTOS_EINVAL;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)context;

    if (data->registry_ctx && data->get_agents_func) {
        const char* role_filter = NULL;
        if (task && task->task_node_agent_role) {
            role_filter = task->task_node_agent_role;
        }

        agent_info_t** agents = NULL;
        size_t agent_count = 0;
        agentos_error_t err = data->get_agents_func(
            data->registry_ctx, role_filter, &agents, &agent_count);

        if (err == AGENTOS_SUCCESS && agent_count > 0 && agents) {
            float best_score = -1.0f;
            const char* best_id = NULL;

            for (size_t i = 0; i < agent_count; i++) {
                if (!agents[i]) continue;
                float score = compute_agent_score(data, agents[i]);
                if (score > best_score) {
                    best_score = score;
                    best_id = agents[i]->agent_id;
                }
            }

            if (best_id) {
                *out_agent_id = AGENTOS_STRDUP(best_id);
                snprintf(data->last_selected_agent_id,
                         sizeof(data->last_selected_agent_id), "%s", best_id);
                data->last_dispatch_time_ns = (uint64_t)time(NULL) * 1000000000ULL;
                return AGENTOS_SUCCESS;
            }
        }
    }

    if (candidates && count > 0) {
        float best_score = -1.0f;
        const char* best_id = NULL;

        for (size_t i = 0; i < count; i++) {
            const agent_info_t* info = (const agent_info_t*)candidates[i];
            if (!info) continue;
            float score = compute_agent_score(data, info);
            if (score > best_score) {
                best_score = score;
                best_id = info->agent_id;
            }
        }

        if (best_id) {
            *out_agent_id = AGENTOS_STRDUP(best_id);
            snprintf(data->last_selected_agent_id,
                     sizeof(data->last_selected_agent_id), "%s", best_id);
            data->last_dispatch_time_ns = (uint64_t)time(NULL) * 1000000000ULL;
            return AGENTOS_SUCCESS;
        }
    }

    *out_agent_id = AGENTOS_STRDUP("default");
    return AGENTOS_SUCCESS;
}

static void ml_destroy(agentos_dispatching_strategy_t* strategy) {
    if (!strategy) return;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)strategy->data;
    if (data) {
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

agentos_dispatching_strategy_t* agentos_dispatching_ml_create(
    const char* model_path,
    void* registry_ctx,
    agent_registry_get_agents_func get_agents_func) {
    (void)model_path;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)AGENTOS_CALLOC(1, sizeof(ml_dispatch_data_t));
    if (!data) return NULL;

    data->feature_weights[ML_FEATURE_COST] = 0.2f;
    data->feature_weights[ML_FEATURE_SUCCESS_RATE] = 0.35f;
    data->feature_weights[ML_FEATURE_TRUST] = 0.25f;
    data->feature_weights[ML_FEATURE_PRIORITY] = 0.2f;
    data->learning_rate = 0.1f;
    data->registry_ctx = registry_ctx;
    data->get_agents_func = get_agents_func;
    data->history_count = 0;
    data->history_pos = 0;
    data->last_dispatch_time_ns = 0;
    memset(data->last_selected_agent_id, 0, sizeof(data->last_selected_agent_id));

    agentos_dispatching_strategy_t* strategy = (agentos_dispatching_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_dispatching_strategy_t));
    if (!strategy) {
        AGENTOS_FREE(data);
        return NULL;
    }

    strategy->dispatch = ml_dispatch;
    strategy->destroy = ml_destroy;
    strategy->data = data;

    return strategy;
}

agentos_error_t agentos_dispatching_ml_report_outcome(
    agentos_dispatching_strategy_t* strategy,
    float reward) {
    if (!strategy || !strategy->data) return AGENTOS_EINVAL;

    ml_dispatch_data_t* data = (ml_dispatch_data_t*)strategy->data;
    if (reward < 0.0f) reward = 0.0f;
    if (reward > 1.0f) reward = 1.0f;

    data->reward_history[data->history_pos] = reward;
    data->history_pos = (data->history_pos + 1) % ML_MAX_HISTORY;
    if (data->history_count < ML_MAX_HISTORY) {
        data->history_count++;
    }

    ml_update_weights(data, reward - ml_average_reward(data));

    return AGENTOS_SUCCESS;
}

float agentos_dispatching_ml_get_avg_reward(
    const agentos_dispatching_strategy_t* strategy) {
    if (!strategy || !strategy->data) return 0.0f;
    ml_dispatch_data_t* data = (ml_dispatch_data_t*)strategy->data;
    return ml_average_reward(data);
}
