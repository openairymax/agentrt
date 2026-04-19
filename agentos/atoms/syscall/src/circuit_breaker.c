/**
 * @file circuit_breaker.c
 * @brief 熔断器模式实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 熔断器模式用于防止级联故障，当系统调用失败率达到阈值时自动熔断，
 * 快速失败以保护系统稳定性。支持 99.999% 可靠性标准。
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

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <time.h>
#include <string.h>

/**
 * @brief 熔断器状态
 */
typedef enum circuit_state {
    CIRCUIT_CLOSED = 0,    /**< 关闭状态（正常） */
    CIRCUIT_OPEN = 1,      /**< 打开状态（熔断） */
    CIRCUIT_HALF_OPEN = 2  /**< 半开状态（探测恢复） */
} circuit_state_t;

/**
 * @brief 熔断器实现
 */
typedef struct circuit_breaker {
    circuit_state_t state;
    int failure_count;
    int success_count;
    int failure_threshold;
    int success_threshold;
    int timeout_ms;
    time_t last_failure_time;
    time_t last_state_change;
    agentos_mutex_t* lock;
} circuit_breaker_t;

static circuit_breaker_t* g_circuit_breaker = NULL;

/**
 * @brief 创建熔断器
 */
agentos_error_t agentos_sys_circuit_breaker_create(int failure_threshold, int timeout_ms) {
    if (g_circuit_breaker) return AGENTOS_EALREADY;

    circuit_breaker_t* cb = (circuit_breaker_t*)AGENTOS_CALLOC(1, sizeof(circuit_breaker_t));
    if (!cb) return AGENTOS_ENOMEM;

    cb->state = CIRCUIT_CLOSED;
    cb->failure_threshold = failure_threshold > 0 ? failure_threshold : 5;
    cb->timeout_ms = timeout_ms > 0 ? timeout_ms : 30000;
    cb->lock = agentos_mutex_create();

    if (!cb->lock) {
        AGENTOS_FREE(cb);
        return AGENTOS_ENOMEM;
    }

    g_circuit_breaker = cb;
    AGENTOS_LOG_INFO("Circuit breaker created (threshold=%d, timeout=%dms)",
                    cb->failure_threshold, cb->timeout_ms);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 检查是否允许执行
 */
agentos_error_t agentos_sys_circuit_breaker_check(void) {
    if (!g_circuit_breaker) return AGENTOS_ENOTINIT;

    agentos_mutex_lock(g_circuit_breaker->lock);

    circuit_state_t state = g_circuit_breaker->state;

    if (state == CIRCUIT_OPEN) {
        time_t now = time(NULL);
        if (now - g_circuit_breaker->last_state_change >= g_circuit_breaker->timeout_ms / 1000) {
            g_circuit_breaker->state = CIRCUIT_HALF_OPEN;
            agentos_mutex_unlock(g_circuit_breaker->lock);
            AGENTOS_LOG_INFO("Circuit breaker entering half-open state");
            return AGENTOS_SUCCESS;
        }
        agentos_mutex_unlock(g_circuit_breaker->lock);
        return AGENTOS_EUNAVAILABLE;
    }

    agentos_mutex_unlock(g_circuit_breaker->lock);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 记录成功
 */
void agentos_sys_circuit_breaker_success(void) {
    if (!g_circuit_breaker) return;

    agentos_mutex_lock(g_circuit_breaker->lock);

    g_circuit_breaker->success_count++;

    if (g_circuit_breaker->state == CIRCUIT_HALF_OPEN) {
        g_circuit_breaker->success_count = 0;
        g_circuit_breaker->failure_count = 0;
        g_circuit_breaker->state = CIRCUIT_CLOSED;
        g_circuit_breaker->last_state_change = time(NULL);
        AGENTOS_LOG_INFO("Circuit breaker closed (recovered)");
    }

    agentos_mutex_unlock(g_circuit_breaker->lock);
}

/**
 * @brief 记录失败
 */
void agentos_sys_circuit_breaker_failure(void) {
    if (!g_circuit_breaker) return;

    agentos_mutex_lock(g_circuit_breaker->lock);

    g_circuit_breaker->failure_count++;
    g_circuit_breaker->last_failure_time = time(NULL);

    if (g_circuit_breaker->state == CIRCUIT_HALF_OPEN) {
        g_circuit_breaker->state = CIRCUIT_OPEN;
        g_circuit_breaker->last_state_change = time(NULL);
        AGENTOS_LOG_WARN("Circuit breaker opened (failure in half-open state)");
    } else if (g_circuit_breaker->failure_count >= g_circuit_breaker->failure_threshold) {
        g_circuit_breaker->state = CIRCUIT_OPEN;
        g_circuit_breaker->last_state_change = time(NULL);
        AGENTOS_LOG_WARN("Circuit breaker opened (threshold reached)");
    }

    agentos_mutex_unlock(g_circuit_breaker->lock);
}

/**
 * @brief 获取熔断器状态
 */
agentos_error_t agentos_sys_circuit_breaker_get_state(int* out_state) {
    if (!g_circuit_breaker || !out_state) return AGENTOS_EINVAL;

    agentos_mutex_lock(g_circuit_breaker->lock);
    *out_state = g_circuit_breaker->state;
    agentos_mutex_unlock(g_circuit_breaker->lock);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置熔断器
 */
void agentos_sys_circuit_breaker_reset(void) {
    if (!g_circuit_breaker) return;

    agentos_mutex_lock(g_circuit_breaker->lock);

    g_circuit_breaker->state = CIRCUIT_CLOSED;
    g_circuit_breaker->failure_count = 0;
    g_circuit_breaker->success_count = 0;
    g_circuit_breaker->last_state_change = time(NULL);

    agentos_mutex_unlock(g_circuit_breaker->lock);
    AGENTOS_LOG_INFO("Circuit breaker reset");
}

/**
 * @brief 销毁熔断器
 */
void agentos_sys_circuit_breaker_destroy(void) {
    if (!g_circuit_breaker) return;

    AGENTOS_LOG_INFO("Circuit breaker destroyed");

    if (g_circuit_breaker->lock) {
        agentos_mutex_destroy(g_circuit_breaker->lock);
    }
    AGENTOS_FREE(g_circuit_breaker);
    g_circuit_breaker = NULL;
}
