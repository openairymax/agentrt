// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file circuit_breaker.c
 * @brief 熔断器与自愈框架实现
 *
 * 实现三态熔断器模式，支持自动故障检测、渐进恢复和故障转移。
 *
 * @see circuit_breaker.h
 */

#include "circuit_breaker.h"

#include "daemon_defaults.h"
#include "daemon_errors.h"
#include "error.h"
#include "memory_compat.h"
#include "platform.h"
#include "safe_string_utils.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */

typedef struct {
    cb_event_callback_t callback;
    void *user_data;
} cb_callback_entry_t;

#define CB_MAX_CALLBACKS 8

typedef struct circuit_breaker_s {
    char name[CB_MAX_NAME_LEN];
    cb_config_t config;
    cb_failover_config_t failover_config;
    cb_state_t state;
    cb_stats_t stats;
    uint64_t state_changed_at;
    uint32_t half_open_calls;
    uint64_t window_start;
    uint32_t window_failures;
    uint32_t window_calls;
    bool destroying;
    struct cb_manager_s *manager;
    agentrt_mutex_t mutex;
} cb_internal_t;

typedef struct cb_manager_s {
    cb_internal_t *breakers[CB_MAX_BREAKERS];
    uint32_t breaker_count;
    cb_callback_entry_t callbacks[CB_MAX_CALLBACKS];
    uint32_t callback_count;
    agentrt_mutex_t mutex;
} cb_manager_internal_t;

/* ==================== 辅助函数 ==================== */

static void notify_event(cb_manager_internal_t *mgr, const cb_event_t *event)
{
    if (!mgr)
        return;
    for (uint32_t i = 0; i < mgr->callback_count; i++) {
        if (mgr->callbacks[i].callback) {
            mgr->callbacks[i].callback(event, mgr->callbacks[i].user_data);
        }
    }
}

static void transition_state(cb_internal_t *cb, cb_manager_internal_t *mgr, cb_state_t new_state)
{
    cb_state_t old_state = cb->state;
    if (old_state == new_state)
        return;

    cb->state = new_state;
    cb->state_changed_at = agentrt_platform_get_time_ms();
    cb->stats.state_transitions++;
    cb->stats.last_state_change_time = cb->state_changed_at;

    if (new_state == CB_STATE_HALF_OPEN) {
        cb->half_open_calls = 0;
        cb->stats.consecutive_successes = 0;
    }

    if (new_state == CB_STATE_CLOSED) {
        cb->stats.consecutive_failures = 0;
        cb->stats.consecutive_successes = 0;
        cb->window_failures = 0;
        cb->window_calls = 0;
        cb->window_start = agentrt_platform_get_time_ms();
    }

    cb_event_t event;
    __builtin_memset(&event, 0, sizeof(event));
    event.type = CB_EVENT_STATE_CHANGE;
    safe_strcpy(event.breaker_name, cb->name, CB_MAX_NAME_LEN);
    event.old_state = old_state;
    event.new_state = new_state;
    event.timestamp = cb->state_changed_at;

    const char *state_names[] = {"CLOSED", "OPEN", "HALF_OPEN"};
    char msg[128];
    snprintf(msg, sizeof(msg), "State: %s -> %s", state_names[old_state], state_names[new_state]);
    event.message = msg;

    LOG_INFO("Circuit breaker '%s': %s", cb->name, msg);
    notify_event(mgr, &event);
}

static void check_window_reset(cb_internal_t *cb)
{
    uint64_t now = agentrt_platform_get_time_ms();
    if (cb->config.window_size_ms > 0 && (now - cb->window_start) >= cb->config.window_size_ms) {
        cb->window_start = now;
        cb->window_failures = 0;
        cb->window_calls = 0;
    }
}

static bool should_trip(cb_internal_t *cb)
{
    if (cb->stats.consecutive_failures >= cb->config.failure_threshold)
        return true;

    if (cb->config.failure_rate_threshold > 0 && cb->window_calls > 0) {
        double rate = (double)cb->window_failures * 100.0 / cb->window_calls;
        if (rate >= cb->config.failure_rate_threshold)
            return true;
    }

    if (cb->config.enable_slow_call_detection && cb->stats.total_calls > 0 &&
        cb->config.slow_call_rate_threshold > 0) {
        if (cb->stats.slow_call_rate >= cb->config.slow_call_rate_threshold)
            return true;
    }

    return false;
}

/* ==================== 公共API实现 ==================== */

AGENTRT_API cb_config_t cb_create_default_config(void)
{
    cb_config_t config;
    __builtin_memset(&config, 0, sizeof(cb_config_t));
    config.failure_threshold = CB_DEFAULT_FAILURE_THRESHOLD;
    config.success_threshold = CB_DEFAULT_SUCCESS_THRESHOLD;
    config.timeout_ms = CB_DEFAULT_TIMEOUT_MS;
    config.half_open_max_calls = CB_DEFAULT_HALF_OPEN_MAX;
    config.window_size_ms = AGENTRT_CB_WINDOW_SIZE_MS;
    config.slow_call_duration_ms = AGENTRT_CB_SLOW_CALL_MS;
    config.slow_call_rate_threshold = AGENTRT_CB_SLOW_CALL_RATE_PCT;
    config.failure_rate_threshold = AGENTRT_CB_FAILURE_RATE_PCT;
    config.enable_slow_call_detection = true;
    config.enable_auto_failover = false;
    return config;
}

AGENTRT_API cb_failover_config_t cb_create_default_failover_config(void)
{
    cb_failover_config_t config;
    __builtin_memset(&config, 0, sizeof(cb_failover_config_t));
    config.strategy = CB_FAILOVER_RETRY;
    config.max_retries = AGENTRT_DEFAULT_MAX_RETRIES;
    config.retry_delay_ms = AGENTRT_DEFAULT_RETRY_DELAY_MS;
    config.retry_backoff_factor = AGENTRT_DEFAULT_BACKOFF_FACTOR;
    return config;
}

AGENTRT_API cb_manager_t cb_manager_create(void)
{
    cb_manager_internal_t *mgr =
        (cb_manager_internal_t *)AGENTRT_CALLOC(1, sizeof(cb_manager_internal_t));
    if (!mgr) {
        SVC_LOG_ERROR("cb_manager_create: memory allocation failed");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_error_t err = agentrt_mutex_init(&mgr->mutex);
    if (err != AGENTRT_SUCCESS) {
        SVC_LOG_ERROR("cb_manager_create: mutex init failed err=%d", err);
        AGENTRT_FREE(mgr);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    LOG_INFO("Circuit breaker manager created");
    return (cb_manager_t)mgr;
}

AGENTRT_API void cb_manager_destroy(cb_manager_t manager)
{
    if (!manager)
        return;

    cb_manager_internal_t *mgr = (cb_manager_internal_t *)manager;

    agentrt_mutex_lock(&mgr->mutex);
    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (mgr->breakers[i]) {
            mgr->breakers[i]->destroying = true;
            agentrt_mutex_destroy(&mgr->breakers[i]->mutex);
            AGENTRT_FREE(mgr->breakers[i]);
        }
    }
    mgr->breaker_count = 0;
    agentrt_mutex_unlock(&mgr->mutex);

    agentrt_mutex_destroy(&mgr->mutex);
    AGENTRT_FREE(mgr);

    LOG_INFO("Circuit breaker manager destroyed");
}

AGENTRT_API circuit_breaker_t cb_create(cb_manager_t manager, const char *name,
                                        const cb_config_t *config)
{
    if (!manager || !name) {
        SVC_LOG_ERROR("cb_create: null parameter manager=%p name=%p", (void *)manager, (void *)name);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    cb_manager_internal_t *mgr = (cb_manager_internal_t *)manager;

    agentrt_mutex_lock(&mgr->mutex);

    if (mgr->breaker_count >= CB_MAX_BREAKERS) {
        agentrt_mutex_unlock(&mgr->mutex);
        SVC_LOG_ERROR("cb_create: max circuit breakers reached count=%u max=%u", mgr->breaker_count, CB_MAX_BREAKERS);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (strcmp(mgr->breakers[i]->name, name) == 0) {
            agentrt_mutex_unlock(&mgr->mutex);
            return (circuit_breaker_t)mgr->breakers[i];
        }
    }

    cb_internal_t *cb = (cb_internal_t *)AGENTRT_CALLOC(1, sizeof(cb_internal_t));
    if (!cb) {
        agentrt_mutex_unlock(&mgr->mutex);
        SVC_LOG_ERROR("cb_create: memory allocation failed for breaker '%s'", name);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    safe_strcpy(cb->name, name, CB_MAX_NAME_LEN);

    if (config) {
        __builtin_memcpy(&cb->config, config, sizeof(cb_config_t));
    } else {
        cb->config = cb_create_default_config();
    }

    cb->failover_config = cb_create_default_failover_config();
    cb->state = CB_STATE_CLOSED;
    cb->state_changed_at = agentrt_platform_get_time_ms();
    cb->window_start = agentrt_platform_get_time_ms();
    cb->manager = mgr;

    agentrt_error_t err = agentrt_mutex_init(&cb->mutex);
    if (err != AGENTRT_SUCCESS) {
        SVC_LOG_ERROR("cb_create: mutex init failed for breaker '%s' err=%d", name, err);
        AGENTRT_FREE(cb);
        agentrt_mutex_unlock(&mgr->mutex);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    mgr->breakers[mgr->breaker_count] = cb;
    mgr->breaker_count++;

    agentrt_mutex_unlock(&mgr->mutex);

    LOG_INFO("Circuit breaker '%s' created (failure_threshold=%u, timeout=%ums)", name,
             cb->config.failure_threshold, cb->config.timeout_ms);
    return (circuit_breaker_t)cb;
}

AGENTRT_API void cb_destroy(circuit_breaker_t breaker)
{
    if (!breaker)
        return;

    cb_internal_t *cb = (cb_internal_t *)breaker;

    /* 从管理器数组中移除，避免悬空指针 */
    if (cb->manager) {
        cb_manager_internal_t *mgr = cb->manager;

        agentrt_mutex_lock(&mgr->mutex);
        for (uint32_t i = 0; i < mgr->breaker_count; i++) {
            if (mgr->breakers[i] == cb) {
                /* 将后续元素前移填补空位 */
                for (uint32_t j = i; j < mgr->breaker_count - 1; j++) {
                    mgr->breakers[j] = mgr->breakers[j + 1];
                }
                mgr->breaker_count--;
                mgr->breakers[mgr->breaker_count] = NULL;
                break;
            }
        }
        agentrt_mutex_unlock(&mgr->mutex);
    }

    agentrt_mutex_lock(&cb->mutex);
    cb->destroying = true;
    agentrt_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s' destroyed", cb->name);

    agentrt_mutex_destroy(&cb->mutex);
    AGENTRT_FREE(cb);
}

AGENTRT_API bool cb_allow_request(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_allow_request: null breaker parameter");
        return false;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_allow_request: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return false;
    }

    switch (cb->state) {
    case CB_STATE_CLOSED:
        agentrt_mutex_unlock(&cb->mutex);
        return true;

    case CB_STATE_OPEN: {
        uint64_t now = agentrt_platform_get_time_ms();
        if (now - cb->state_changed_at >= cb->config.timeout_ms) {
            transition_state(cb, cb->manager, CB_STATE_HALF_OPEN);
            agentrt_mutex_unlock(&cb->mutex);
            return true;
        }
        cb->stats.rejected_calls++;
        agentrt_mutex_unlock(&cb->mutex);
        return false;
    }

    case CB_STATE_HALF_OPEN:
        if (cb->half_open_calls < cb->config.half_open_max_calls) {
            cb->half_open_calls++;
            agentrt_mutex_unlock(&cb->mutex);
            return true;
        }
        cb->stats.rejected_calls++;
        agentrt_mutex_unlock(&cb->mutex);
        return false;

    default:
        agentrt_mutex_unlock(&cb->mutex);
        return false;
    }
}

AGENTRT_API void cb_record_success(circuit_breaker_t breaker, uint32_t duration_ms)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_record_success: null breaker parameter");
        return;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_record_success: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }

    cb->stats.total_calls++;
    cb->stats.successful_calls++;
    cb->stats.last_success_time = agentrt_platform_get_time_ms();
    cb->stats.consecutive_failures = 0;
    cb->stats.consecutive_successes++;

    cb->window_calls++;
    check_window_reset(cb);

    if (cb->config.enable_slow_call_detection && duration_ms > cb->config.slow_call_duration_ms) {
        cb->stats.slow_calls++;
        if (cb->stats.total_calls > 0) {
            cb->stats.slow_call_rate = (double)cb->stats.slow_calls * 100.0 / cb->stats.total_calls;
        }
    }

    if (cb->state == CB_STATE_HALF_OPEN) {
        if (cb->stats.consecutive_successes >= cb->config.success_threshold) {
            transition_state(cb, cb->manager, CB_STATE_CLOSED);
        }
    }

    agentrt_mutex_unlock(&cb->mutex);
}

AGENTRT_API void cb_record_failure(circuit_breaker_t breaker, int32_t error_code)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_record_failure: null breaker parameter");
        return;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_record_failure: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }

    cb->stats.total_calls++;
    cb->stats.failed_calls++;
    cb->stats.last_failure_time = agentrt_platform_get_time_ms();
    cb->stats.consecutive_failures++;
    cb->stats.consecutive_successes = 0;

    cb->window_calls++;
    cb->window_failures++;
    check_window_reset(cb);

    if (cb->window_calls > 0) {
        cb->stats.failure_rate = (double)cb->window_failures * 100.0 / cb->window_calls;
    }

    if (cb->state == CB_STATE_CLOSED) {
        if (should_trip(cb)) {
            SVC_LOG_WARN("cb_record_failure: breaker '%s' threshold exceeded, tripping to OPEN (consecutive_failures=%u, failure_rate=%.1f%%)",
                         cb->name, cb->stats.consecutive_failures, cb->stats.failure_rate);
            transition_state(cb, cb->manager, CB_STATE_OPEN);
        }
    } else if (cb->state == CB_STATE_HALF_OPEN) {
        SVC_LOG_WARN("cb_record_failure: breaker '%s' failed in HALF_OPEN, reopening (error_code=%d)", cb->name, error_code);
        transition_state(cb, cb->manager, CB_STATE_OPEN);
    }

    agentrt_mutex_unlock(&cb->mutex);

    LOG_DEBUG("Circuit breaker '%s': failure recorded (error=%d, consecutive=%u)", cb->name,
              error_code, cb->stats.consecutive_failures);
}

AGENTRT_API void cb_record_timeout(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_record_timeout: null breaker parameter");
        return;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_record_timeout: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }

    cb->stats.total_calls++;
    cb->stats.timeout_calls++;
    cb->stats.failed_calls++;
    cb->stats.last_failure_time = agentrt_platform_get_time_ms();
    cb->stats.consecutive_failures++;
    cb->stats.consecutive_successes = 0;

    cb->window_calls++;
    cb->window_failures++;
    check_window_reset(cb);

    if (cb->window_calls > 0) {
        cb->stats.failure_rate = (double)cb->window_failures * 100.0 / cb->window_calls;
    }

    if (cb->state == CB_STATE_CLOSED) {
        if (should_trip(cb)) {
            SVC_LOG_WARN("cb_record_timeout: breaker '%s' threshold exceeded after timeout, tripping to OPEN (consecutive_failures=%u)",
                         cb->name, cb->stats.consecutive_failures);
            transition_state(cb, cb->manager, CB_STATE_OPEN);
        }
    } else if (cb->state == CB_STATE_HALF_OPEN) {
        SVC_LOG_WARN("cb_record_timeout: breaker '%s' timed out in HALF_OPEN, reopening", cb->name);
        transition_state(cb, cb->manager, CB_STATE_OPEN);
    }

    agentrt_mutex_unlock(&cb->mutex);

    LOG_DEBUG("Circuit breaker '%s': timeout recorded", cb->name);
}

/* ==================== 状态查询 ==================== */

AGENTRT_API cb_state_t cb_get_state(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_get_state: null breaker parameter");
        return CB_STATE_OPEN;
    }
    cb_internal_t *cb = (cb_internal_t *)breaker;
    return cb->state;
}

AGENTRT_API const char *cb_get_name(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_get_name: null breaker parameter");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    cb_internal_t *cb = (cb_internal_t *)breaker;
    return cb->name;
}

AGENTRT_API agentrt_error_t cb_get_stats(circuit_breaker_t breaker, cb_stats_t *stats)
{
    if (!breaker || !stats) {
        SVC_LOG_ERROR("cb_get_stats: null parameter breaker=%p stats=%p", (void *)breaker, (void *)stats);
        return AGENTRT_EINVAL;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_get_stats: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return AGENTRT_EINVAL;
    }
    __builtin_memcpy(stats, &cb->stats, sizeof(cb_stats_t));
    agentrt_mutex_unlock(&cb->mutex);

    return AGENTRT_SUCCESS;
}

AGENTRT_API void cb_reset(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_reset: null breaker parameter");
        return;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_reset: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }

    cb_state_t old = cb->state;
    cb->state = CB_STATE_CLOSED;
    cb->state_changed_at = agentrt_platform_get_time_ms();
    cb->stats.consecutive_failures = 0;
    cb->stats.consecutive_successes = 0;
    cb->window_failures = 0;
    cb->window_calls = 0;
    cb->window_start = agentrt_platform_get_time_ms();
    cb->half_open_calls = 0;

    if (old != CB_STATE_CLOSED) {
        cb->stats.state_transitions++;
        cb->stats.last_state_change_time = cb->state_changed_at;
    }

    agentrt_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s' reset to CLOSED", cb->name);
}

AGENTRT_API void cb_force_open(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_force_open: null breaker parameter");
        return;
    }
    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_force_open: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }
    transition_state(cb, cb->manager, CB_STATE_OPEN);
    agentrt_mutex_unlock(&cb->mutex);
}

AGENTRT_API void cb_force_close(circuit_breaker_t breaker)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_force_close: null breaker parameter");
        return;
    }
    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_force_close: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return;
    }
    transition_state(cb, cb->manager, CB_STATE_CLOSED);
    agentrt_mutex_unlock(&cb->mutex);
}

/* ==================== 故障转移 ==================== */

AGENTRT_API agentrt_error_t cb_set_failover_config(circuit_breaker_t breaker,
                                                   const cb_failover_config_t *config)
{
    if (!breaker || !config) {
        SVC_LOG_ERROR("cb_set_failover_config: null parameter breaker=%p config=%p", (void *)breaker, (void *)config);
        return AGENTRT_EINVAL;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_set_failover_config: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return AGENTRT_EINVAL;
    }
    __builtin_memcpy(&cb->failover_config, config, sizeof(cb_failover_config_t));
    agentrt_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s': failover config updated (strategy=%d)", cb->name,
             config->strategy);
    return AGENTRT_SUCCESS;
}

AGENTRT_API agentrt_error_t cb_execute_failover(circuit_breaker_t breaker, int32_t original_error,
                                                char *fallback_result, size_t result_size)
{
    if (!breaker) {
        SVC_LOG_ERROR("cb_execute_failover: null breaker parameter");
        return AGENTRT_EINVAL;
    }

    cb_internal_t *cb = (cb_internal_t *)breaker;

    agentrt_mutex_lock(&cb->mutex);
    if (cb->destroying) {
        SVC_LOG_WARN("cb_execute_failover: breaker is being destroyed");
        agentrt_mutex_unlock(&cb->mutex);
        return AGENTRT_EINVAL;
    }

    cb_failover_config_t *fc = &cb->failover_config;
    agentrt_error_t err = DAEMON_EFAIL;

    switch (fc->strategy) {
    case CB_FAILOVER_RETRY:
        snprintf(fallback_result, result_size,
                 "{\"failover\":\"retry\",\"service\":\"%s\",\"retries\":%u,\"delay_ms\":%u}",
                 cb->name, fc->max_retries, fc->retry_delay_ms);
        err = AGENTRT_SUCCESS;
        break;

    case CB_FAILOVER_FALLBACK:
        snprintf(fallback_result, result_size,
                 "{\"failover\":\"fallback\",\"service\":\"%s\",\"fallback\":\"%s\"}", cb->name,
                 fc->fallback_service);
        err = AGENTRT_SUCCESS;
        break;

    case CB_FAILOVER_REDIRECT:
        snprintf(fallback_result, result_size,
                 "{\"failover\":\"redirect\",\"service\":\"%s\",\"target\":\"%s\"}", cb->name,
                 fc->fallback_service);
        err = AGENTRT_SUCCESS;
        break;

    case CB_FAILOVER_CACHE:
        snprintf(fallback_result, result_size,
                 "{\"failover\":\"cache\",\"service\":\"%s\",\"error\":%d}", cb->name,
                 original_error);
        err = AGENTRT_SUCCESS;
        break;

    default:
        SVC_LOG_WARN("cb_execute_failover: unknown failover strategy=%d for breaker '%s'", fc->strategy, cb->name);
        snprintf(fallback_result, result_size,
                 "{\"failover\":\"none\",\"service\":\"%s\",\"error\":%d}", cb->name,
                 original_error);
        break;
    }

    agentrt_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s': failover executed (strategy=%d, error=%d)", cb->name,
             fc->strategy, original_error);
    return err;
}

/* ==================== 事件与回调 ==================== */

AGENTRT_API agentrt_error_t cb_register_event_callback(cb_manager_t manager,
                                                       cb_event_callback_t callback,
                                                       void *user_data)
{
    if (!manager || !callback) {
        SVC_LOG_ERROR("cb_register_event_callback: null parameter manager=%p callback=%p", (void *)manager, (void *)(uintptr_t)callback);
        return AGENTRT_EINVAL;
    }

    cb_manager_internal_t *mgr = (cb_manager_internal_t *)manager;

    agentrt_mutex_lock(&mgr->mutex);

    if (mgr->callback_count >= CB_MAX_CALLBACKS) {
        agentrt_mutex_unlock(&mgr->mutex);
        SVC_LOG_ERROR("cb_register_event_callback: max callbacks reached count=%u max=%u", mgr->callback_count, CB_MAX_CALLBACKS);
        return AGENTRT_ENOMEM;
    }

    mgr->callbacks[mgr->callback_count].callback = callback;
    mgr->callbacks[mgr->callback_count].user_data = user_data;
    mgr->callback_count++;

    agentrt_mutex_unlock(&mgr->mutex);

    return AGENTRT_SUCCESS;
}

AGENTRT_API circuit_breaker_t cb_find(cb_manager_t manager, const char *name)
{
    if (!manager || !name) {
        SVC_LOG_ERROR("cb_find: null parameter manager=%p name=%p", (void *)manager, (void *)name);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    cb_manager_internal_t *mgr = (cb_manager_internal_t *)manager;

    agentrt_mutex_lock(&mgr->mutex);

    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (strcmp(mgr->breakers[i]->name, name) == 0) {
            agentrt_mutex_unlock(&mgr->mutex);
            return (circuit_breaker_t)mgr->breakers[i];
        }
    }

    agentrt_mutex_unlock(&mgr->mutex);
    SVC_LOG_WARN("cb_find: breaker '%s' not found", name);
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}

AGENTRT_API uint32_t cb_count(cb_manager_t manager)
{
    if (!manager) {
        SVC_LOG_ERROR("cb_count: null manager parameter");
        return 0;
    }
    cb_manager_internal_t *mgr = (cb_manager_internal_t *)manager;
    return mgr->breaker_count;
}

AGENTRT_API const char *cb_state_to_string(cb_state_t state)
{
    static const char *state_strings[] = {"CLOSED", "OPEN", "HALF_OPEN"};

    if (state < 0 || state > CB_STATE_HALF_OPEN) {
        SVC_LOG_WARN("cb_state_to_string: invalid state=%d", state);
        return "UNKNOWN";
    }
    return state_strings[state];
}
