/**
 * @file budget.c
 * @brief Token预算管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现Token预算管理功能：
 * - 支持输入/输出Token分离统计
 * - 提供预算重置和查询接口
 * - 线程安全的预算操作
 */

#include "token.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

/**
 * @brief Token预算内部结构
 */
struct agentos_token_budget {
    size_t max_tokens;                    /**< 最大Token配额 */
    atomic_size_t used_tokens;          /**< 已使用Token数 */
    atomic_size_t input_tokens;          /**< 输入Token数 */
    atomic_size_t output_tokens;        /**< 输出Token数 */
    atomic_uint request_count;          /**< 请求计数 */
    atomic_uint denied_count;            /**< 拒绝计数 */
    pthread_mutex_t mutex;               /**< 互斥锁 */
    time_t reset_time;                  /**< 重置时间 */
    size_t window_seconds;              /**< 时间窗口（秒） */
};

/**
 * @brief 检查预算是否充足
 */
static int check_budget_available(agentos_token_budget_t* budget, size_t input, size_t output) {
    if (!budget) {
        return -1;
    }
    
    size_t total = atomic_load(&budget->used_tokens);
    size_t requested = input + output;
    
    if (total + requested > budget->max_tokens) {
        atomic_fetch_add(&budget->denied_count, 1);
        return -1;
    }
    
    return 0;
}

agentos_token_budget_t* agentos_token_budget_create(size_t max_tokens) {
    if (max_tokens == 0) {
        return NULL;
    }
    
    agentos_token_budget_t* budget = (agentos_token_budget_t*)malloc(
        sizeof(agentos_token_budget_t));
    if (!budget) {
        return NULL;
    }
    
    memset(budget, 0, sizeof(agentos_token_budget_t));
    
    budget->max_tokens = max_tokens;
    atomic_init(&budget->used_tokens, 0);
    atomic_init(&budget->input_tokens, 0);
    atomic_init(&budget->output_tokens, 0);
    atomic_init(&budget->request_count, 0);
    atomic_init(&budget->denied_count, 0);
    
    if (pthread_mutex_init(&budget->mutex, NULL) != 0) {
        free(budget);
        return NULL;
    }
    
    budget->reset_time = 0;
    budget->window_seconds = 0;
    
    return budget;
}

void agentos_token_budget_destroy(agentos_token_budget_t* budget) {
    if (!budget) {
        return;
    }
    
    pthread_mutex_destroy(&budget->mutex);
    free(budget);
}

int agentos_token_budget_add(agentos_token_budget_t* budget, size_t input_tokens, size_t output_tokens) {
    if (!budget) {
        return -1;
    }
    
    pthread_mutex_lock(&budget->mutex);
    
    if (check_budget_available(budget, input_tokens, output_tokens) != 0) {
        pthread_mutex_unlock(&budget->mutex);
        return -1;
    }
    
    atomic_fetch_add(&budget->used_tokens, input_tokens + output_tokens);
    atomic_fetch_add(&budget->input_tokens, input_tokens);
    atomic_fetch_add(&budget->output_tokens, output_tokens);
    atomic_fetch_add(&budget->request_count, 1);
    
    pthread_mutex_unlock(&budget->mutex);
    
    return 0;
}

size_t agentos_token_budget_remaining(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    size_t used = atomic_load(&budget->used_tokens);
    
    if (used >= budget->max_tokens) {
        return 0;
    }
    
    return budget->max_tokens - used;
}

void agentos_token_budget_reset(agentos_token_budget_t* budget) {
    if (!budget) {
        return;
    }
    
    pthread_mutex_lock(&budget->mutex);
    
    atomic_store(&budget->used_tokens, 0);
    atomic_store(&budget->input_tokens, 0);
    atomic_store(&budget->output_tokens, 0);
    
    pthread_mutex_unlock(&budget->mutex);
}

size_t agentos_token_budget_used(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    return atomic_load(&budget->used_tokens);
}

size_t agentos_token_budget_input(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    return atomic_load(&budget->input_tokens);
}

size_t agentos_token_budget_output(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    return atomic_load(&budget->output_tokens);
}

uint32_t agentos_token_budget_requests(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    return atomic_load(&budget->request_count);
}

uint32_t agentos_token_budget_denied(agentos_token_budget_t* budget) {
    if (!budget) {
        return 0;
    }
    
    return atomic_load(&budget->denied_count);
}

int agentos_token_budget_set_window(agentos_token_budget_t* budget, size_t window_seconds) {
    if (!budget) {
        return -1;
    }
    
    pthread_mutex_lock(&budget->mutex);
    
    budget->window_seconds = window_seconds;
    budget->reset_time = time(NULL) + window_seconds;
    
    pthread_mutex_unlock(&budget->mutex);
    
    return 0;
}

int agentos_token_budget_check_window(agentos_token_budget_t* budget) {
    if (!budget) {
        return -1;
    }
    
    pthread_mutex_lock(&budget->mutex);
    
    time_t now = time(NULL);
    
    if (budget->reset_time > 0 && now >= budget->reset_time) {
        atomic_store(&budget->used_tokens, 0);
        atomic_store(&budget->input_tokens, 0);
        atomic_store(&budget->output_tokens, 0);
        
        budget->reset_time = now + budget->window_seconds;
    }
    
    pthread_mutex_unlock(&budget->mutex);
    
    return 0;
}
