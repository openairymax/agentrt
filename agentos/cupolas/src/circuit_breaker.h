/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * circuit_breaker.h - Circuit Breaker Pattern Implementation
 *
 * Design principles:
 * - Failure fast: Open state rejects requests immediately
 * - Self-healing: Automatic recovery after timeout
 * - State transparency: Clear state transitions with callbacks
 * - Configurable thresholds: Adjustable based on service characteristics
 */

#ifndef CUPOLAS_CIRCUIT_BREAKER_H
#define CUPOLAS_CIRCUIT_BREAKER_H

#include "platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum circuit_state {
    CIRCUIT_STATE_CLOSED = 0,
    CIRCUIT_STATE_OPEN = 1,
    CIRCUIT_STATE_HALF_OPEN = 2
} circuit_state_t;

typedef enum circuit_event {
    CIRCUIT_EVENT_SUCCESS = 0,
    CIRCUIT_EVENT_FAILURE = 1,
    CIRCUIT_EVENT_TIMEOUT = 2,
    CIRCUIT_EVENT_REJECTED = 3
} circuit_event_t;

typedef struct circuit_breaker_config {
    uint32_t failure_threshold;
    uint32_t success_threshold;
    uint32_t timeout_ms;
    uint32_t half_open_max_calls;
    double failure_rate_threshold;
} circuit_breaker_config_t;

typedef void (*circuit_state_change_callback_t)(circuit_state_t old_state,
                                               circuit_state_t new_state,
                                               void* user_data);

typedef struct circuit_breaker circuit_breaker_t;

circuit_breaker_t* circuit_breaker_create(const circuit_breaker_config_t* config);

void circuit_breaker_destroy(circuit_breaker_t* breaker);

int circuit_breaker_call(circuit_breaker_t* breaker,
                        int (*func)(void* arg),
                        void* arg,
                        uint32_t timeout_ms);

bool circuit_breaker_is_available(circuit_breaker_t* breaker);

circuit_state_t circuit_breaker_get_state(circuit_breaker_t* breaker);

void circuit_breaker_record_success(circuit_breaker_t* breaker);

void circuit_breaker_record_failure(circuit_breaker_t* breaker);

void circuit_breaker_record_timeout(circuit_breaker_t* breaker);

void circuit_breaker_reset(circuit_breaker_t* breaker);

void circuit_breaker_set_callback(circuit_breaker_t* breaker,
                                 circuit_state_change_callback_t callback,
                                 void* user_data);

typedef struct circuit_breaker_stats {
    uint64_t total_calls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t rejected_calls;
    uint64_t timed_out_calls;
    uint64_t state_changes;
    circuit_state_t current_state;
    uint64_t last_state_change_time_ms;
    double current_failure_rate;
} circuit_breaker_stats_t;

void circuit_breaker_get_stats(circuit_breaker_t* breaker, circuit_breaker_stats_t* stats);

void circuit_breaker_reset_stats(circuit_breaker_t* breaker);

const char* circuit_state_to_string(circuit_state_t state);

const char* circuit_event_to_string(circuit_event_t event);

typedef struct circuit_breaker_registry circuit_breaker_registry_t;

circuit_breaker_registry_t* circuit_breaker_registry_create(void);

void circuit_breaker_registry_destroy(circuit_breaker_registry_t* registry);

int circuit_breaker_registry_register(circuit_breaker_registry_t* registry,
                                      const char* name,
                                      circuit_breaker_t* breaker);

circuit_breaker_t* circuit_breaker_registry_get(circuit_breaker_registry_t* registry,
                                                const char* name);

void circuit_breaker_registry_remove(circuit_breaker_registry_t* registry,
                                     const char* name);

void circuit_breaker_registry_reset_all(circuit_breaker_registry_t* registry);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_CIRCUIT_BREAKER_H */
