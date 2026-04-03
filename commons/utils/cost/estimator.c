/**
 * @file estimator.c
 * @brief 成本预估器实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现LLM调用成本预估功能�?
 * - 基于模型配置的成本计�?
 * - 支持自定义费率配�?
 * - 提供成本分析和报告接�?
 */

#include "cost.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include "../../platform/include/platform.h"
#else
    #include <pthread.h>
#endif

/* Unified base library compatibility layer */
#include <memory_compat.h>
#include <string_compat.h>

#ifdef _WIN32
    /* Windows: 使用 agentos_mutex_t 替代 pthread_mutex_t */
    #define pthread_mutex_t   agentos_mutex_t
    #define pthread_mutex_init(m, a)  agentos_mutex_init(m)
    #define pthread_mutex_destroy(m)  agentos_mutex_destroy(m)
    #define pthread_mutex_lock(m)     agentos_mutex_lock(m)
    #define pthread_mutex_unlock(m)   agentos_mutex_unlock(m)
#endif

#define MAX_MODEL_NAME 64
#define MAX_CONFIG_ENTRIES 16

/**
 * @brief 模型成本配置
 */
typedef struct {
    char model_name[MAX_MODEL_NAME];     /**< 模型名称 */
    double input_cost_per_1k;         /**< 输入成本（美�?1K Token�?*/
    double output_cost_per_1k;         /**< 输出成本（美�?1K Token�?*/
    int max_input_tokens;             /**< 最大输入Token */
    int max_output_tokens;            /**< 最大输出Token */
} model_cost_config_t;

/**
 * @brief 成本预估器内部结�?
 */
struct agentos_cost_estimator {
    model_cost_config_t configs[MAX_CONFIG_ENTRIES];  /**< 模型配置数组 */
    int config_count;                                 /**< 配置数量 */
    pthread_mutex_t mutex;                           /**< 互斥�?*/
    double total_cost;                               /**< 累计成本 */
    size_t total_input_tokens;                       /**< 累计输入Token */
    size_t total_output_tokens;                      /**< 累计输出Token */
    uint64_t request_count;                         /**< 请求计数 */
};

/**
 * @brief 默认模型配置
 */
static const model_cost_config_t default_configs[] = {
    {"gpt-4o", 0.005, 0.015, 128000, 16384},
    {"gpt-4-turbo", 0.01, 0.03, 128000, 4096},
    {"gpt-4", 0.03, 0.06, 8192, 4096},
    {"gpt-3.5-turbo", 0.0005, 0.0015, 16385, 4096},
    {"claude-3-opus", 0.015, 0.075, 200000, 4096},
    {"claude-3-sonnet", 0.003, 0.015, 200000, 4096},
    {"claude-3-haiku", 0.00025, 0.00125, 200000, 4096},
    {"deepseek-chat", 0.00014, 0.00028, 163840, 16384},
    {"deepseek-coder", 0.00014, 0.00028, 163840, 16384},
    {"", 0.001, 0.002, 4096, 4096}
};

/**
 * @brief 查找模型配置
 */
static const model_cost_config_t* find_model_config(agentos_cost_estimator_t* estimator, 
                                                   const char* model_name) {
    if (!estimator || !model_name) {
        return NULL;
    }
    
    for (int i = 0; i < estimator->config_count; i++) {
        if (strcasecmp(estimator->configs[i].model_name, model_name) == 0) {
            return &estimator->configs[i];
        }
    }
    
    for (size_t i = 0; i < sizeof(default_configs) / sizeof(default_configs[0]); i++) {
        if (strcasecmp(default_configs[i].model_name, model_name) == 0) {
            return &default_configs[i];
        }
    }
    
    return &default_configs[sizeof(default_configs) / sizeof(default_configs[0]) - 1];
}

/**
 * @brief 规范化模型名�?
 */
static void normalize_model_name(const char* input, char* output, size_t output_size) {
    if (!input || !output) {
        return;
    }
    
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 1; i++) {
        if (!isspace((unsigned char)input[i])) {
            output[j++] = tolower((unsigned char)input[i]);
        }
    }
    output[j] = '\0';
}

agentos_cost_estimator_t* agentos_cost_estimator_create(const char* config_path) {
    (void)config_path;
    
    agentos_cost_estimator_t* estimator = (agentos_cost_estimator_t*)AGENTOS_MALLOC(
        sizeof(agentos_cost_estimator_t));
    if (!estimator) {
        return NULL;
    }
    
    memset(estimator, 0, sizeof(agentos_cost_estimator_t));
    
    if (pthread_mutex_init(&estimator->mutex, NULL) != 0) {
        AGENTOS_FREE(estimator);
        return NULL;
    }
    
    estimator->config_count = 0;
    estimator->total_cost = 0.0;
    estimator->total_input_tokens = 0;
    estimator->total_output_tokens = 0;
    estimator->request_count = 0;
    
    for (size_t i = 0; i < sizeof(default_configs) / sizeof(default_configs[0]); i++) {
        if (estimator->config_count < MAX_CONFIG_ENTRIES) {
            strncpy(estimator->configs[estimator->config_count].model_name,
                   default_configs[i].model_name,
                   MAX_MODEL_NAME - 1);
            estimator->configs[estimator->config_count].input_cost_per_1k = default_configs[i].input_cost_per_1k;
            estimator->configs[estimator->config_count].output_cost_per_1k = default_configs[i].output_cost_per_1k;
            estimator->configs[estimator->config_count].max_input_tokens = default_configs[i].max_input_tokens;
            estimator->configs[estimator->config_count].max_output_tokens = default_configs[i].max_output_tokens;
            estimator->config_count++;
        }
    }
    
    return estimator;
}

void agentos_cost_estimator_destroy(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return;
    }
    
    pthread_mutex_destroy(&estimator->mutex);
    AGENTOS_FREE(estimator);
}

double agentos_cost_estimator_estimate(agentos_cost_estimator_t* estimator, 
                                      const char* model_name, 
                                      size_t input_tokens, 
                                      size_t output_tokens) {
    if (!estimator || !model_name) {
        return -1.0;
    }
    
    char normalized[MAX_MODEL_NAME];
    normalize_model_name(model_name, normalized, sizeof(normalized));
    
    const model_cost_config_t* manager = find_model_config(estimator, normalized);
    if (!manager) {
        return -1.0;
    }
    
    double input_cost = (input_tokens / 1000.0) * manager->input_cost_per_1k;
    double output_cost = (output_tokens / 1000.0) * manager->output_cost_per_1k;
    double total_cost = input_cost + output_cost;
    
    pthread_mutex_lock(&estimator->mutex);
    
    estimator->total_cost += total_cost;
    estimator->total_input_tokens += input_tokens;
    estimator->total_output_tokens += output_tokens;
    estimator->request_count++;
    
    pthread_mutex_unlock(&estimator->mutex);
    
    return total_cost;
}

double agentos_cost_estimator_get_total(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return 0.0;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    double total = estimator->total_cost;
    pthread_mutex_unlock(&estimator->mutex);
    
    return total;
}

size_t agentos_cost_estimator_get_input_tokens(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return 0;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    size_t tokens = estimator->total_input_tokens;
    pthread_mutex_unlock(&estimator->mutex);
    
    return tokens;
}

size_t agentos_cost_estimator_get_output_tokens(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return 0;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    size_t tokens = estimator->total_output_tokens;
    pthread_mutex_unlock(&estimator->mutex);
    
    return tokens;
}

uint64_t agentos_cost_estimator_get_request_count(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return 0;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    uint64_t count = estimator->request_count;
    pthread_mutex_unlock(&estimator->mutex);
    
    return count;
}

void agentos_cost_estimator_reset(agentos_cost_estimator_t* estimator) {
    if (!estimator) {
        return;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    
    estimator->total_cost = 0.0;
    estimator->total_input_tokens = 0;
    estimator->total_output_tokens = 0;
    estimator->request_count = 0;
    
    pthread_mutex_unlock(&estimator->mutex);
}

int agentos_cost_estimator_add_model(agentos_cost_estimator_t* estimator,
                                    const char* model_name,
                                    double input_cost_per_1k,
                                    double output_cost_per_1k) {
    if (!estimator || !model_name) {
        return -1;
    }
    
    if (estimator->config_count >= MAX_CONFIG_ENTRIES) {
        return -1;
    }
    
    pthread_mutex_lock(&estimator->mutex);
    
    strncpy(estimator->configs[estimator->config_count].model_name,
           model_name,
           MAX_MODEL_NAME - 1);
    estimator->configs[estimator->config_count].input_cost_per_1k = input_cost_per_1k;
    estimator->configs[estimator->config_count].output_cost_per_1k = output_cost_per_1k;
    estimator->configs[estimator->config_count].max_input_tokens = 4096;
    estimator->configs[estimator->config_count].max_output_tokens = 4096;
    estimator->config_count++;
    
    pthread_mutex_unlock(&estimator->mutex);
    
    return 0;
}
