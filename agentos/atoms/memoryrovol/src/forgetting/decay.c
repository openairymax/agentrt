/**
 * @file decay.c
 * @brief 遗忘衰减计算（基于艾宾浩斯曲线，支持自适应学习）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer1_raw.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "include/memory_compat.h"
#include "string_compat.h"

#define ADAPTIVE_SAMPLE_SIZE 50      /* 自适应学习所需的最小样本数 */
#define DEFAULT_LEARNING_RATE 0.05f   /* 默认学习率 */
#define MIN_LAMBDA 0.001f             /* 最小lambda值 */
#define MAX_LAMBDA 1.0f               /* 最大lambda值 */

/**
 * @brief 自适应学习历史记录
 */
typedef struct adaptive_record {
    char record_id[64];               /* 记录ID */
    float predicted_weight;           /* 预测权重 */
    float actual_access_rate;         /* 实际访问率（0-1） */
    uint64_t timestamp;              /* 记录时间戳 */
} adaptive_record_t;

/**
 * @brief 自适应学习状态
 */
typedef struct adaptive_state {
    adaptive_record_t samples[ADAPTIVE_SAMPLE_SIZE];  /* 样本缓冲区 */
    size_t sample_count;                 /* 当前样本数 */
    size_t sample_index;                 /* 写入索引（环形） */
    float learning_rate;                 /* 学习率 */
    int enabled;                         /* 是否启用自适应 */
    uint64_t total_adjustments;          /* 总调整次数 */
    float avg_error;                     /* 平均预测误差 */
    float lambda_history[100];           /* lambda变化历史 */
    size_t history_count;                /* 历史记录数 */
} adaptive_state_t;

static void init_adaptive_state(adaptive_state_t* state);

#define ADAPTIVE(engine) ((adaptive_state_t*)(engine)->adaptive)

agentos_error_t agentos_forgetting_create(
    const agentos_forgetting_config_t* manager,
    agentos_layer1_raw_t* layer1,
    agentos_layer2_feature_t* layer2,
    agentos_forgetting_engine_t** out_engine) {

    if (!layer1 || !layer2 || !out_engine) return AGENTOS_EINVAL;

    agentos_forgetting_engine_t* eng = (agentos_forgetting_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_forgetting_engine_t));
    if (!eng) {
        AGENTOS_LOG_ERROR("Failed to allocate forgetting engine");
        return AGENTOS_ENOMEM;
    }

    if (manager) {
        eng->manager = *manager;
    } else {
        // 默认配置
        eng->manager.strategy = AGENTOS_FORGET_EBBINGHAUS;
        eng->manager.lambda = 0.01;
        eng->manager.threshold = 0.3;
        eng->manager.min_access = 1;
        eng->manager.check_interval_sec = 3600; // 1小时
        eng->manager.archive_path = "/var/agentos/archive";
    }

    eng->layer1 = layer1;
    eng->layer2 = layer2;
    eng->lock = agentos_mutex_create();
    if (!eng->lock) {
        AGENTOS_FREE(eng);
        return AGENTOS_ENOMEM;
    }

    eng->adaptive = AGENTOS_CALLOC(1, sizeof(adaptive_state_t));
    if (!eng->adaptive) {
        agentos_mutex_destroy(eng->lock);
        AGENTOS_FREE(eng);
        return AGENTOS_ENOMEM;
    }
    init_adaptive_state(ADAPTIVE(eng));

    *out_engine = eng;
    return AGENTOS_SUCCESS;
}

void agentos_forgetting_destroy(agentos_forgetting_engine_t* engine) {
    if (!engine) return;
    agentos_forgetting_stop_auto(engine);
    if (engine->adaptive) AGENTOS_FREE(engine->adaptive);
    if (engine->lock) agentos_mutex_destroy(engine->lock);
    AGENTOS_FREE(engine);
}

/**
 * 艾宾浩斯遗忘曲线：R = exp(-λ * t)  (t 单位为秒)
 */
static double ebbinghaus_weight(double lambda, uint64_t last_access, uint64_t now) {
    double age_sec = (now - last_access) / 1e9; /* 纳秒转秒 */
    return exp(-lambda * age_sec);
}

/**
 * 基于访问次数的遗忘：权重 = min(1.0, access_count / min_access)
 */
static double access_weight(uint32_t access_count, uint32_t min_access) {
    if (min_access == 0) return 0.0;
    return (access_count >= min_access) ? 1.0 : (double)access_count / (double)min_access;
}

/**
 * 初始化自适应学习状态
 */
static void init_adaptive_state(adaptive_state_t* state) {
    if (!state) return;
    
    memset(state, 0, sizeof(adaptive_state_t));
    state->learning_rate = DEFAULT_LEARNING_RATE;
    state->enabled = 0; /* 默认禁用 */
}

/**
 * 记录一个样本用于自适应学习
 * @param state 自适应状态
 * @param record_id 记录ID
 * @param predicted 预测的遗忘权重
 * @param actual 实际的访问情况（1=被访问，0=未被访问）
 */
static void record_adaptive_sample(adaptive_state_t* state,
                                   const char* record_id,
                                   float predicted,
                                   int actual_accessed) {
    if (!state || !state->enabled || !record_id) return;
    
    adaptive_record_t* record = &state->samples[state->sample_index];
    
    /* 记录样本数据 */
    strncpy(record->record_id, record_id, sizeof(record->record_id) - 1);
    record->record_id[sizeof(record->record_id) - 1] = '\0';
    record->predicted_weight = predicted;
    record->actual_access_rate = actual_accessed ? 1.0f : 0.0f;
    record->timestamp = (uint64_t)time(NULL);
    
    /* 更新索引（环形缓冲） */
    state->sample_index = (state->sample_index + 1) % ADAPTIVE_SAMPLE_SIZE;
    if (state->sample_count < ADAPTIVE_SAMPLE_SIZE) {
        state->sample_count++;
    }
}

/**
 * 基于历史样本调整lambda参数
 * @param engine 遗忘引擎实例
 * @return 调整后的lambda值
 */
static float adapt_lambda(agentos_forgetting_engine_t* engine) {
    adaptive_state_t* state = ADAPTIVE(engine);
    
    /* 检查是否有足够的数据 */
    if (state->sample_count < ADAPTIVE_SAMPLE_SIZE / 2) {
        return engine->manager.lambda; /* 数据不足，不调整 */
    }
    
    /* 计算预测误差 */
    float total_error = 0.0f;
    size_t valid_samples = 0;
    
    for (size_t i = 0; i < state->sample_count; i++) {
        adaptive_record_t* record = &state->samples[i];
        
        /* 计算误差：预测值与实际值的差异 */
        float error = fabsf(record->predicted_weight - record->actual_access_rate);
        total_error += error;
        valid_samples++;
    }
    
    if (valid_samples == 0) return engine->manager.lambda;
    
    /* 更新平均误差 */
    state->avg_error = total_error / (float)valid_samples;
    
    /* 简单的自适应规则：
     * - 如果平均误差 > 0.3，说明遗忘太快或太慢，需要调整lambda
     * - 如果实际访问率普遍高于预测，说明遗忘太慢，增加lambda
     * - 如果实际访问率普遍低于预测，说明遗忘太快，减少lambda
     */
    float adjustment = 0.0f;
    float actual_avg = 0.0f;
    float predicted_avg = 0.0f;
    
    for (size_t i = 0; i < state->sample_count; i++) {
        actual_avg += state->samples[i].actual_access_rate;
        predicted_avg += state->samples[i].predicted_weight;
    }
    
    if (state->sample_count > 0) {
        actual_avg /= (float)state->sample_count;
        predicted_avg /= (float)state->sample_count;
    }
    
    /* 根据差异计算调整量 */
    float diff = actual_avg - predicted_avg;
    adjustment = diff * state->learning_rate;
    
    /* 应用调整 */
    float new_lambda = engine->manager.lambda + adjustment;
    
    /* 限制lambda范围 */
    if (new_lambda < MIN_LAMBDA) new_lambda = MIN_LAMBDA;
    if (new_lambda > MAX_LAMBDA) new_lambda = MAX_LAMBDA;
    
    /* 记录lambda变化历史 */
    if (state->history_count < 100) {
        state->lambda_history[state->history_count] = new_lambda;
        state->history_count++;
    } else {
        /* 移动历史记录 */
        memmove(state->lambda_history, state->lambda_history + 1,
                99 * sizeof(float));
        state->lambda_history[99] = new_lambda;
    }
    
    state->total_adjustments++;
    
    return new_lambda;
}

agentos_error_t agentos_forgetting_get_weight(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    float* out_weight) {

    if (!engine || !record_id || !out_weight) return AGENTOS_EINVAL;

    agentos_raw_metadata_t* meta = NULL;
    agentos_error_t err = agentos_raw_metadata_db_query(NULL, record_id, &meta);
    if (err != AGENTOS_SUCCESS) return err;

    uint64_t now = agentos_time_monotonic_ns();
    double weight = 1.0;

    float effective_lambda = engine->manager.lambda;
    
    if (ADAPTIVE(engine)->enabled && ADAPTIVE(engine)->sample_count >= ADAPTIVE_SAMPLE_SIZE / 2) {
        effective_lambda = adapt_lambda(engine);
        if (fabs(effective_lambda - engine->manager.lambda) > 0.0001) {
            engine->manager.lambda = effective_lambda;
        }
    }

    uint64_t last_access = meta->modified_ns > 0 ? meta->modified_ns : meta->created_ns;

    switch (engine->manager.strategy) {
        case AGENTOS_FORGET_EBBINGHAUS:
            weight = ebbinghaus_weight(effective_lambda, last_access, now);
            break;
        case AGENTOS_FORGET_LINEAR:
            {
                double age_sec = (now - last_access) / 1e9;
                weight = 1.0 - effective_lambda * age_sec;
                if (weight < 0) weight = 0.0;
            }
            break;
        case AGENTOS_FORGET_ACCESS_BASED:
            weight = access_weight(meta->access_count, engine->manager.min_access);
            break;
        default:
            weight = 1.0;
            break;
    }

    *out_weight = (float)weight;
    
    if (ADAPTIVE(engine)->enabled) {
        int accessed = (weight > engine->manager.threshold) ? 1 : 0;
        record_adaptive_sample(ADAPTIVE(engine), record_id, (float)weight, accessed);
    }
    
    agentos_raw_metadata_free(meta);
    return AGENTOS_SUCCESS;
}

/* 自动裁剪线程函数（在 prune.c 中实现） */
static void* auto_worker(void* arg) {
    agentos_forgetting_engine_t* eng = (agentos_forgetting_engine_t*)arg;
    while (eng->auto_running) {
        agentos_task_sleep(eng->manager.check_interval_sec * 1000);
        agentos_forgetting_prune(eng, NULL);
    }
    return NULL;
}

agentos_error_t agentos_forgetting_start_auto(agentos_forgetting_engine_t* engine) {
    if (!engine) return AGENTOS_EINVAL;
    if (engine->auto_running) return AGENTOS_SUCCESS;
    engine->auto_running = 1;
    if (agentos_thread_create(&engine->auto_thread, auto_worker, engine) != 0) {
        engine->auto_running = 0;
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

void agentos_forgetting_stop_auto(agentos_forgetting_engine_t* engine) {
    if (!engine || !engine->auto_running) return;
    engine->auto_running = 0;
    agentos_thread_join(engine->auto_thread, NULL);
}

/**
 * @brief 启用或禁用自适应学习功能
 * @param engine 遗忘引擎实例
 * @param enable 是否启用（1=启用，0=禁用）
 * @param learning_rate 学习率（0.0-1.0），仅在启用时有效
 * @return 错误码
 */
agentos_error_t agentos_forgetting_enable_adaptive(
    agentos_forgetting_engine_t* engine,
    int enable,
    float learning_rate) {
    
    if (!engine) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(engine->lock);
    
    ADAPTIVE(engine)->enabled = enable;
    
    if (enable) {
        /* 验证并设置学习率 */
        if (learning_rate < 0.001f || learning_rate > 1.0f) {
            learning_rate = DEFAULT_LEARNING_RATE;  /* 使用默认值 */
        }
        ADAPTIVE(engine)->learning_rate = learning_rate;
        
        /* 如果之前未初始化，现在初始化 */
        if (ADAPTIVE(engine)->sample_count == 0 && 
            ADAPTIVE(engine)->sample_index == 0) {
            init_adaptive_state(ADAPTIVE(engine));
            ADAPTIVE(engine)->enabled = 1;
            ADAPTIVE(engine)->learning_rate = learning_rate;
        }
    }
    
    agentos_mutex_unlock(engine->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取自适应学习状态信息
 * @param engine 遗忘引擎实例
 * @param[out] sample_count 当前样本数量
 * @param[out] total_adjustments 总调整次数
 * @param[out] avg_error 平均预测误差
 * @param[out] current_lambda 当前的lambda值
 * @return 错误码
 */
agentos_error_t agentos_forgetting_get_adaptive_stats(
    agentos_forgetting_engine_t* engine,
    size_t* sample_count,
    uint64_t* total_adjustments,
    float* avg_error,
    float* current_lambda) {
    
    if (!engine) return AGENTOS_EINVAL;
    
    if (sample_count) *sample_count = ADAPTIVE(engine)->sample_count;
    if (total_adjustments) *total_adjustments = ADAPTIVE(engine)->total_adjustments;
    if (avg_error) *avg_error = ADAPTIVE(engine)->avg_error;
    if (current_lambda) *current_lambda = engine->manager.lambda;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置自适应学习状态
 * @param engine 遗忘引擎实例
 * @return 错误码
 */
agentos_error_t agentos_forgetting_reset_adaptive(agentos_forgetting_engine_t* engine) {
    if (!engine) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(engine->lock);
    init_adaptive_state(ADAPTIVE(engine));
    /* 恢复默认lambda值 */
    engine->manager.lambda = 0.01f;  /* 默认值 */
    agentos_mutex_unlock(engine->lock);
    
    return AGENTOS_SUCCESS;
}
