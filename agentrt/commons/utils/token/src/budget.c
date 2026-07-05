/**
 * @file budget.c
 * @brief Token预算管理实现（跨平台?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现Token预算管理功能?
 * - 支持输入/输出Token分离统计
 * - 提供预算重置和查询接?
 * - 线程安全的预算操?
 */

#include "error.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

#ifdef _WIN32
#else
#include <unistd.h>
#endif



/**
 * @brief 跨平台互斥锁类型
 */
#ifdef _WIN32
typedef agentrt_mutex_t budget_mutex_t;
#else
typedef agentrt_mutex_t budget_mutex_t;
#endif

/**
 * @brief 初始化互斥锁
 */
static int budget_mutex_init(budget_mutex_t *mutex)
{
#ifdef _WIN32
    agentrt_mutex_init(mutex);
    return 0;
#else
    return agentrt_mutex_init(mutex);
#endif
}

/**
 * @brief 销毁互斥锁
 */
static void budget_mutex_destroy(budget_mutex_t *mutex)
{
#ifdef _WIN32
    agentrt_mutex_destroy(mutex);
#else
    agentrt_mutex_destroy(mutex);
#endif
}

/**
 * @brief 加锁
 */
static void budget_mutex_lock(budget_mutex_t *mutex)
{
#ifdef _WIN32
    agentrt_mutex_lock(mutex);
#else
    agentrt_mutex_lock(mutex);
#endif
}

/**
 * @brief 解锁
 */
static void budget_mutex_unlock(budget_mutex_t *mutex)
{
#ifdef _WIN32
    agentrt_mutex_unlock(mutex);
#else
    agentrt_mutex_unlock(mutex);
#endif
}

/**
 * @brief Token预算内部结构
 */
struct agentrt_token_budget {
    size_t max_tokens;           /**< 最大Token配额 */
    atomic_size_t used_tokens;   /**< 已使用Token?*/
    atomic_size_t input_tokens;  /**< 输入Token?*/
    atomic_size_t output_tokens; /**< 输出Token?*/
    atomic_uint request_count;   /**< 请求计数 */
    atomic_uint denied_count;    /**< 拒绝计数 */
    budget_mutex_t mutex;        /**< 互斥?*/
    time_t reset_time;           /**< 重置时间 */
    size_t window_seconds;       /**< 时间窗口（秒?*/
};

/**
 * @brief 检查预算是否充?
 */
static int check_budget_available(agentrt_token_budget_t *budget, size_t input, size_t output)
{
    if (!budget) {
        return AGENTRT_EINVAL;
    }

    size_t total = atomic_load(&budget->used_tokens);
    size_t requested = input + output;

    if (total + requested > budget->max_tokens) {
        atomic_fetch_add(&budget->denied_count, 1);
        return AGENTRT_EINVAL;
    }

    return 0;
}

agentrt_token_budget_t *agentrt_token_budget_create(size_t max_tokens)
{
    if (max_tokens == 0) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    agentrt_token_budget_t *budget =
        (agentrt_token_budget_t *)AGENTRT_MALLOC(sizeof(agentrt_token_budget_t));
    if (!budget) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    AGENTRT_MEMSET(budget, 0, sizeof(agentrt_token_budget_t));

    budget->max_tokens = max_tokens;
    atomic_init(&budget->used_tokens, 0);
    atomic_init(&budget->input_tokens, 0);
    atomic_init(&budget->output_tokens, 0);
    atomic_init(&budget->request_count, 0);
    atomic_init(&budget->denied_count, 0);

    if (budget_mutex_init(&budget->mutex) != 0) {
        AGENTRT_FREE(budget);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    budget->reset_time = 0;
    budget->window_seconds = 0;

    return budget;
}

void agentrt_token_budget_destroy(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return;
    }

    budget_mutex_destroy(&budget->mutex);
    AGENTRT_FREE(budget);
}

int agentrt_token_budget_add(agentrt_token_budget_t *budget, size_t input_tokens,
                             size_t output_tokens)
{
    if (!budget) {
        return AGENTRT_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    if (check_budget_available(budget, input_tokens, output_tokens) != 0) {
        budget_mutex_unlock(&budget->mutex);
        return AGENTRT_EINVAL;
    }

    atomic_fetch_add(&budget->used_tokens, input_tokens + output_tokens);
    atomic_fetch_add(&budget->input_tokens, input_tokens);
    atomic_fetch_add(&budget->output_tokens, output_tokens);
    atomic_fetch_add(&budget->request_count, 1);

    budget_mutex_unlock(&budget->mutex);

    return 0;
}

size_t agentrt_token_budget_remaining(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    size_t used = atomic_load(&budget->used_tokens);

    if (used >= budget->max_tokens) {
        return 0;
    }

    return budget->max_tokens - used;
}

void agentrt_token_budget_reset(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return;
    }

    budget_mutex_lock(&budget->mutex);

    atomic_store(&budget->used_tokens, 0);
    atomic_store(&budget->input_tokens, 0);
    atomic_store(&budget->output_tokens, 0);

    budget_mutex_unlock(&budget->mutex);
}

size_t agentrt_token_budget_used(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->used_tokens);
}

size_t agentrt_token_budget_input(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->input_tokens);
}

size_t agentrt_token_budget_output(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->output_tokens);
}

uint32_t agentrt_token_budget_requests(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->request_count);
}

uint32_t agentrt_token_budget_denied(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return 0;
    }

    return atomic_load(&budget->denied_count);
}

int agentrt_token_budget_set_window(agentrt_token_budget_t *budget, size_t window_seconds)
{
    if (!budget) {
        return AGENTRT_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    budget->window_seconds = window_seconds;
    budget->reset_time = time(NULL) + window_seconds;

    budget_mutex_unlock(&budget->mutex);

    return 0;
}

int agentrt_token_budget_check_window(agentrt_token_budget_t *budget)
{
    if (!budget) {
        return AGENTRT_EINVAL;
    }

    budget_mutex_lock(&budget->mutex);

    time_t now = time(NULL);

    if (budget->reset_time > 0 && now >= budget->reset_time) {
        atomic_store(&budget->used_tokens, 0);
        atomic_store(&budget->input_tokens, 0);
        atomic_store(&budget->output_tokens, 0);

        budget->reset_time = now + budget->window_seconds;
    }

    budget_mutex_unlock(&budget->mutex);

    return 0;
}
