// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file circuit_breaker.c
 * @brief 熔断器与自愈框架实现
 *
 * 实现三态熔断器模式，支持自动故障检测、渐进恢复和故障转移。
 *
 * @see circuit_breaker.h
 */

#include "circuit_breaker.h"
#include "svc_logger.h"
#include "platform.h"
#include "error.h"
#include "daemon_errors.h"
#include "safe_string_utils.h"

#include "include/memory_compat.h"

#include <stdlib.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */

typedef struct {
    cb_event_callback_t callback;
    void* user_data;
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
    agentos_platform_mutex_t mutex;
} cb_internal_t;

typedef struct cb_manager_s {
    cb_internal_t* breakers[CB_MAX_BREAKERS];
    uint32_t breaker_count;
    cb_callback_entry_t callbacks[CB_MAX_CALLBACKS];
    uint32_t callback_count;
    agentos_platform_mutex_t mutex;
} cb_manager_internal_t;

/* ==================== 辅助函数 ==================== */

static void notify_event(
    cb_manager_internal_t* mgr,
    const cb_event_t* event
) {
    if (!mgr) return;
    for (uint32_t i = 0; i < mgr->callback_count; i++) {
        if (mgr->callbacks[i].callback) {
            mgr->callbacks[i].callback(event, mgr->callbacks[i].user_data);
        }
    }
}

static void transition_state(
    cb_internal_t* cb,
    cb_manager_internal_t* mgr,
    cb_state_t new_state
) {
    cb_state_t old_state = cb->state;
    if (old_state == new_state) return;

    cb->state = new_state;
    cb->state_changed_at = agentos_platform_get_time_ms();
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
        cb->window_start = agentos_platform_get_time_ms();
    }

    cb_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = CB_EVENT_STATE_CHANGE;
    safe_strcpy(event.breaker_name, cb->name, CB_MAX_NAME_LEN);
    event.old_state = old_state;
    event.new_state = new_state;
    event.timestamp = cb->state_changed_at;

    const char* state_names[] = {"CLOSED", "OPEN", "HALF_OPEN"};
    char msg[128];
    snprintf(msg, sizeof(msg), "State: %s -> %s",
             state_names[old_state], state_names[new_state]);
    event.message = msg;

    LOG_INFO("Circuit breaker '%s': %s", cb->name, msg);
    notify_event(mgr, &event);
}

static void check_window_reset(cb_internal_t* cb) {
    uint64_t now = agentos_platform_get_time_ms();
    if (cb->config.window_size_ms > 0 &&
        (now - cb->window_start) >= cb->config.window_size_ms) {
        cb->window_start = now;
        cb->window_failures = 0;
        cb->window_calls = 0;
    }
}

static bool should_trip(cb_internal_t* cb) {
    if (cb->stats.consecutive_failures >= cb->config.failure_threshold)
        return true;

    if (cb->config.failure_rate_threshold > 0 && cb->window_calls > 0) {
        double rate = (double)cb->window_failures * 100.0 / cb->window_calls;
        if (rate >= cb->config.failure_rate_threshold)
            return true;
    }

    if (cb->config.enable_slow_call_detection &&
        cb->stats.total_calls > 0 &&
        cb->config.slow_call_rate_threshold > 0) {
        if (cb->stats.slow_call_rate >= cb->config.slow_call_rate_threshold)
            return true;
    }

    return false;
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API cb_config_t cb_create_default_config(void) {
    cb_config_t config;
    memset(&config, 0, sizeof(cb_config_t));
    config.failure_threshold = CB_DEFAULT_FAILURE_THRESHOLD;
    config.success_threshold = CB_DEFAULT_SUCCESS_THRESHOLD;
    config.timeout_ms = CB_DEFAULT_TIMEOUT_MS;
    config.half_open_max_calls = CB_DEFAULT_HALF_OPEN_MAX;
    config.window_size_ms = 60000;
    config.slow_call_duration_ms = 5000;
    config.slow_call_rate_threshold = 50;
    config.failure_rate_threshold = 50;
    config.enable_slow_call_detection = true;
    config.enable_auto_failover = false;
    return config;
}

AGENTOS_API cb_failover_config_t cb_create_default_failover_config(void) {
    cb_failover_config_t config;
    memset(&config, 0, sizeof(cb_failover_config_t));
    config.strategy = CB_FAILOVER_RETRY;
    config.max_retries = 3;
    config.retry_delay_ms = 100;
    config.retry_backoff_factor = 2;
    return config;
}

AGENTOS_API cb_manager_t cb_manager_create(void) {
    cb_manager_internal_t* mgr =
        (cb_manager_internal_t*)AGENTOS_CALLOC(1, sizeof(cb_manager_internal_t));
    if (!mgr) return NULL;

    agentos_error_t err = agentos_platform_mutex_init(&mgr->mutex);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(mgr);
        return NULL;
    }

    LOG_INFO("Circuit breaker manager created");
    return (cb_manager_t)mgr;
}

AGENTOS_API void cb_manager_destroy(cb_manager_t manager) {
    if (!manager) return;

    cb_manager_internal_t* mgr = (cb_manager_internal_t*)manager;

    agentos_platform_mutex_lock(&mgr->mutex);
    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (mgr->breakers[i]) {
            agentos_platform_mutex_destroy(&mgr->breakers[i]->mutex);
            AGENTOS_FREE(mgr->breakers[i]);
        }
    }
    mgr->breaker_count = 0;
    agentos_platform_mutex_unlock(&mgr->mutex);

    agentos_platform_mutex_destroy(&mgr->mutex);
    AGENTOS_FREE(mgr);

    LOG_INFO("Circuit breaker manager destroyed");
}

AGENTOS_API circuit_breaker_t cb_create(
    cb_manager_t manager,
    const char* name,
    const cb_config_t* config
) {
    if (!manager || !name) return NULL;

    cb_manager_internal_t* mgr = (cb_manager_internal_t*)manager;

    agentos_platform_mutex_lock(&mgr->mutex);

    if (mgr->breaker_count >= CB_MAX_BREAKERS) {
        agentos_platform_mutex_unlock(&mgr->mutex);
        LOG_ERROR("Max circuit breakers reached");
        return NULL;
    }

    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (strcmp(mgr->breakers[i]->name, name) == 0) {
            agentos_platform_mutex_unlock(&mgr->mutex);
            return (circuit_breaker_t)mgr->breakers[i];
        }
    }

    cb_internal_t* cb = (cb_internal_t*)AGENTOS_CALLOC(1, sizeof(cb_internal_t));
    if (!cb) {
        agentos_platform_mutex_unlock(&mgr->mutex);
        return NULL;
    }

    safe_strcpy(cb->name, name, CB_MAX_NAME_LEN);

    if (config) {
        memcpy(&cb->config, config, sizeof(cb_config_t));
    } else {
        cb->config = cb_create_default_config();
    }

    cb->failover_config = cb_create_default_failover_config();
    cb->state = CB_STATE_CLOSED;
    cb->state_changed_at = agentos_platform_get_time_ms();
    cb->window_start = agentos_platform_get_time_ms();

    agentos_error_t err = agentos_platform_mutex_init(&cb->mutex);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(cb);
        agentos_platform_mutex_unlock(&mgr->mutex);
        return NULL;
    }

    mgr->breakers[mgr->breaker_count] = cb;
    mgr->breaker_count++;

    agentos_platform_mutex_unlock(&mgr->mutex);

    LOG_INFO("Circuit breaker '%s' created (failure_threshold=%u, timeout=%ums)",
             name, cb->config.failure_threshold, cb->config.timeout_ms);
    return (circuit_breaker_t)cb;
}

AGENTOS_API void cb_destroy(circuit_breaker_t breaker) {
    if (!breaker) return;

    cb_internal_t* cb = (cb_internal_t*)breaker;
    LOG_INFO("Circuit breaker '%s' destroyed", cb->name);

    agentos_platform_mutex_destroy(&cb->mutex);
    AGENTOS_FREE(cb);
}

AGENTOS_API bool cb_allow_request(circuit_breaker_t breaker) {
    if (!breaker) return false;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    switch (cb->state) {
        case CB_STATE_CLOSED:
            agentos_platform_mutex_unlock(&cb->mutex);
            return true;

        case CB_STATE_OPEN: {
            uint64_t now = agentos_platform_get_time_ms();
            if (now - cb->state_changed_at >= cb->config.timeout_ms) {
                cb_manager_internal_t* mgr = NULL;
                transition_state(cb, mgr, CB_STATE_HALF_OPEN);
                agentos_platform_mutex_unlock(&cb->mutex);
                return true;
            }
            cb->stats.rejected_calls++;
            agentos_platform_mutex_unlock(&cb->mutex);
            return false;
        }

        case CB_STATE_HALF_OPEN:
            if (cb->half_open_calls < cb->config.half_open_max_calls) {
                cb->half_open_calls++;
                agentos_platform_mutex_unlock(&cb->mutex);
                return true;
            }
            cb->stats.rejected_calls++;
            agentos_platform_mutex_unlock(&cb->mutex);
            return false;

        default:
            agentos_platform_mutex_unlock(&cb->mutex);
            return false;
    }
}

AGENTOS_API void cb_record_success(circuit_breaker_t breaker, uint32_t duration_ms) {
    if (!breaker) return;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    cb->stats.total_calls++;
    cb->stats.successful_calls++;
    cb->stats.last_success_time = agentos_platform_get_time_ms();
    cb->stats.consecutive_failures = 0;
    cb->stats.consecutive_successes++;

    cb->window_calls++;
    check_window_reset(cb);

    if (cb->config.enable_slow_call_detection &&
        duration_ms > cb->config.slow_call_duration_ms) {
        cb->stats.slow_calls++;
        if (cb->stats.total_calls > 0) {
            cb->stats.slow_call_rate =
                (double)cb->stats.slow_calls * 100.0 / cb->stats.total_calls;
        }
    }

    if (cb->state == CB_STATE_HALF_OPEN) {
        if (cb->stats.consecutive_successes >= cb->config.success_threshold) {
            transition_state(cb, NULL, CB_STATE_CLOSED);
        }
    }

    agentos_platform_mutex_unlock(&cb->mutex);
}

AGENTOS_API void cb_record_failure(circuit_breaker_t breaker, int32_t error_code) {
    if (!breaker) return;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    cb->stats.total_calls++;
    cb->stats.failed_calls++;
    cb->stats.last_failure_time = agentos_platform_get_time_ms();
    cb->stats.consecutive_failures++;
    cb->stats.consecutive_successes = 0;

    cb->window_calls++;
    cb->window_failures++;
    check_window_reset(cb);

    if (cb->window_calls > 0) {
        cb->stats.failure_rate =
            (double)cb->window_failures * 100.0 / cb->window_calls;
    }

    if (cb->state == CB_STATE_CLOSED) {
        if (should_trip(cb)) {
            transition_state(cb, NULL, CB_STATE_OPEN);
        }
    } else if (cb->state == CB_STATE_HALF_OPEN) {
        transition_state(cb, NULL, CB_STATE_OPEN);
    }

    agentos_platform_mutex_unlock(&cb->mutex);

    LOG_DEBUG("Circuit breaker '%s': failure recorded (error=%d, consecutive=%u)",
              cb->name, error_code, cb->stats.consecutive_failures);
}

AGENTOS_API void cb_record_timeout(circuit_breaker_t breaker) {
    if (!breaker) return;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    cb->stats.total_calls++;
    cb->stats.timeout_calls++;
    cb->stats.failed_calls++;
    cb->stats.last_failure_time = agentos_platform_get_time_ms();
    cb->stats.consecutive_failures++;
    cb->stats.consecutive_successes = 0;

    cb->window_calls++;
    cb->window_failures++;
    check_window_reset(cb);

    if (cb->window_calls > 0) {
        cb->stats.failure_rate =
            (double)cb->window_failures * 100.0 / cb->window_calls;
    }

    if (cb->state == CB_STATE_CLOSED) {
        if (should_trip(cb)) {
            transition_state(cb, NULL, CB_STATE_OPEN);
        }
    } else if (cb->state == CB_STATE_HALF_OPEN) {
        transition_state(cb, NULL, CB_STATE_OPEN);
    }

    agentos_platform_mutex_unlock(&cb->mutex);

    LOG_DEBUG("Circuit breaker '%s': timeout recorded", cb->name);
}

/* ==================== 状态查询 ==================== */

AGENTOS_API cb_state_t cb_get_state(circuit_breaker_t breaker) {
    if (!breaker) return CB_STATE_OPEN;
    cb_internal_t* cb = (cb_internal_t*)breaker;
    return cb->state;
}

AGENTOS_API const char* cb_get_name(circuit_breaker_t breaker) {
    if (!breaker) return NULL;
    cb_internal_t* cb = (cb_internal_t*)breaker;
    return cb->name;
}

AGENTOS_API agentos_error_t cb_get_stats(circuit_breaker_t breaker, cb_stats_t* stats) {
    if (!breaker || !stats) return AGENTOS_EINVAL;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);
    memcpy(stats, &cb->stats, sizeof(cb_stats_t));
    agentos_platform_mutex_unlock(&cb->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API void cb_reset(circuit_breaker_t breaker) {
    if (!breaker) return;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    cb_state_t old = cb->state;
    cb->state = CB_STATE_CLOSED;
    cb->state_changed_at = agentos_platform_get_time_ms();
    cb->stats.consecutive_failures = 0;
    cb->stats.consecutive_successes = 0;
    cb->window_failures = 0;
    cb->window_calls = 0;
    cb->window_start = agentos_platform_get_time_ms();
    cb->half_open_calls = 0;

    if (old != CB_STATE_CLOSED) {
        cb->stats.state_transitions++;
        cb->stats.last_state_change_time = cb->state_changed_at;
    }

    agentos_platform_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s' reset to CLOSED", cb->name);
}

AGENTOS_API void cb_force_open(circuit_breaker_t breaker) {
    if (!breaker) return;
    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);
    transition_state(cb, NULL, CB_STATE_OPEN);
    agentos_platform_mutex_unlock(&cb->mutex);
}

AGENTOS_API void cb_force_close(circuit_breaker_t breaker) {
    if (!breaker) return;
    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);
    transition_state(cb, NULL, CB_STATE_CLOSED);
    agentos_platform_mutex_unlock(&cb->mutex);
}

/* ==================== 故障转移 ==================== */

AGENTOS_API agentos_error_t cb_set_failover_config(
    circuit_breaker_t breaker,
    const cb_failover_config_t* config
) {
    if (!breaker || !config) return AGENTOS_EINVAL;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);
    memcpy(&cb->failover_config, config, sizeof(cb_failover_config_t));
    agentos_platform_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s': failover config updated (strategy=%d)",
             cb->name, config->strategy);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t cb_execute_failover(
    circuit_breaker_t breaker,
    int32_t original_error,
    char* fallback_result,
    size_t result_size
) {
    if (!breaker) return AGENTOS_EINVAL;

    cb_internal_t* cb = (cb_internal_t*)breaker;

    agentos_platform_mutex_lock(&cb->mutex);

    cb_failover_config_t* fc = &cb->failover_config;
    agentos_error_t err = AGENTOS_EFAIL;

    switch (fc->strategy) {
        case CB_FAILOVER_RETRY:
            snprintf(fallback_result, result_size,
                     "{\"failover\":\"retry\",\"service\":\"%s\",\"retries\":%u,\"delay_ms\":%u}",
                     cb->name, fc->max_retries, fc->retry_delay_ms);
            err = AGENTOS_SUCCESS;
            break;

        case CB_FAILOVER_FALLBACK:
            snprintf(fallback_result, result_size,
                     "{\"failover\":\"fallback\",\"service\":\"%s\",\"fallback\":\"%s\"}",
                     cb->name, fc->fallback_service);
            err = AGENTOS_SUCCESS;
            break;

        case CB_FAILOVER_REDIRECT:
            snprintf(fallback_result, result_size,
                     "{\"failover\":\"redirect\",\"service\":\"%s\",\"target\":\"%s\"}",
                     cb->name, fc->fallback_service);
            err = AGENTOS_SUCCESS;
            break;

        case CB_FAILOVER_CACHE:
            snprintf(fallback_result, result_size,
                     "{\"failover\":\"cache\",\"service\":\"%s\",\"error\":%d}",
                     cb->name, original_error);
            err = AGENTOS_SUCCESS;
            break;

        default:
            snprintf(fallback_result, result_size,
                     "{\"failover\":\"none\",\"service\":\"%s\",\"error\":%d}",
                     cb->name, original_error);
            break;
    }

    agentos_platform_mutex_unlock(&cb->mutex);

    LOG_INFO("Circuit breaker '%s': failover executed (strategy=%d, error=%d)",
             cb->name, fc->strategy, original_error);
    return err;
}

/* ==================== 事件与回调 ==================== */

AGENTOS_API agentos_error_t cb_register_event_callback(
    cb_manager_t manager,
    cb_event_callback_t callback,
    void* user_data
) {
    if (!manager || !callback) return AGENTOS_EINVAL;

    cb_manager_internal_t* mgr = (cb_manager_internal_t*)manager;

    agentos_platform_mutex_lock(&mgr->mutex);

    if (mgr->callback_count >= CB_MAX_CALLBACKS) {
        agentos_platform_mutex_unlock(&mgr->mutex);
        return AGENTOS_ENOMEM;
    }

    mgr->callbacks[mgr->callback_count].callback = callback;
    mgr->callbacks[mgr->callback_count].user_data = user_data;
    mgr->callback_count++;

    agentos_platform_mutex_unlock(&mgr->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API circuit_breaker_t cb_find(cb_manager_t manager, const char* name) {
    if (!manager || !name) return NULL;

    cb_manager_internal_t* mgr = (cb_manager_internal_t*)manager;

    agentos_platform_mutex_lock(&mgr->mutex);

    for (uint32_t i = 0; i < mgr->breaker_count; i++) {
        if (strcmp(mgr->breakers[i]->name, name) == 0) {
            agentos_platform_mutex_unlock(&mgr->mutex);
            return (circuit_breaker_t)mgr->breakers[i];
        }
    }

    agentos_platform_mutex_unlock(&mgr->mutex);
    return NULL;
}

AGENTOS_API uint32_t cb_count(cb_manager_t manager) {
    if (!manager) return 0;
    cb_manager_internal_t* mgr = (cb_manager_internal_t*)manager;
    return mgr->breaker_count;
}

AGENTOS_API const char* cb_state_to_string(cb_state_t state) {
    static const char* state_strings[] = {
        "CLOSED",
        "OPEN",
        "HALF_OPEN"
    };

    if (state < 0 || state > CB_STATE_HALF_OPEN) return "UNKNOWN";
    return state_strings[state];
}
