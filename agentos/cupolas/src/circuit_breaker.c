/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * circuit_breaker.c - Circuit Breaker Pattern Implementation
 */

#include "circuit_breaker.h"
#include "../utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_FAILURE_THRESHOLD 5
#define DEFAULT_SUCCESS_THRESHOLD 3
#define DEFAULT_TIMEOUT_MS 60000
#define DEFAULT_HALF_OPEN_MAX_CALLS 3
#define DEFAULT_FAILURE_RATE_THRESHOLD 0.5

static const char* state_strings[] = {
    "CLOSED",
    "OPEN",
    "HALF_OPEN"
};

static const char* event_strings[] = {
    "SUCCESS",
    "FAILURE",
    "TIMEOUT",
    "REJECTED"
};

const char* circuit_state_to_string(circuit_state_t state) {
    if (state >= 0 && state <= 2) {
        return state_strings[state];
    }
    return "UNKNOWN";
}

const char* circuit_event_to_string(circuit_event_t event) {
    if (event >= 0 && event <= 3) {
        return event_strings[event];
    }
    return "UNKNOWN";
}

struct circuit_breaker {
    circuit_breaker_config_t config;
    circuit_state_t state;
    uint32_t consecutive_failures;
    uint32_t consecutive_successes;
    uint64_t last_failure_time_ms;
    uint64_t state_change_time_ms;
    uint32_t half_open_calls;
    uint64_t total_calls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t rejected_calls;
    uint64_t timed_out_calls;
    uint64_t state_changes;
    cupolas_atomic64_t current_failure_rate;
    cupolas_mutex_t lock;
    circuit_state_change_callback_t callback;
    void* callback_user_data;
};

static void default_config(circuit_breaker_config_t* config) {
    if (!config) return;
    config->failure_threshold = DEFAULT_FAILURE_THRESHOLD;
    config->success_threshold = DEFAULT_SUCCESS_THRESHOLD;
    config->timeout_ms = DEFAULT_TIMEOUT_MS;
    config->half_open_max_calls = DEFAULT_HALF_OPEN_MAX_CALLS;
    config->failure_rate_threshold = DEFAULT_FAILURE_RATE_THRESHOLD;
}

static void update_failure_rate(circuit_breaker_t* breaker) {
    uint64_t total = breaker->successful_calls + breaker->failed_calls;
    if (total > 0) {
        double rate = (double)breaker->failed_calls / (double)total;
        cupolas_atomic_store64(&breaker->current_failure_rate, (int64_t)(rate * 1000000));
    }
}

static bool should_open_circuit(circuit_breaker_t* breaker) {
    if (breaker->consecutive_failures >= breaker->config.failure_threshold) {
        return true;
    }
    
    uint64_t total = breaker->successful_calls + breaker->failed_calls;
    if (total >= 10) {
        double rate = (double)breaker->failed_calls / (double)total;
        if (rate >= breaker->config.failure_rate_threshold) {
            return true;
        }
    }
    
    return false;
}

circuit_breaker_t* circuit_breaker_create(const circuit_breaker_config_t* config) {
    circuit_breaker_t* breaker = (circuit_breaker_t*)cupolas_mem_alloc(sizeof(circuit_breaker_t));
    if (!breaker) return NULL;
    
    memset(breaker, 0, sizeof(circuit_breaker_t));
    
    if (config) {
        memcpy(&breaker->config, config, sizeof(circuit_breaker_config_t));
    } else {
        default_config(&breaker->config);
    }
    
    breaker->state = CIRCUIT_STATE_CLOSED;
    breaker->state_change_time_ms = cupolas_time_ms();
    
    if (cupolas_mutex_init(&breaker->lock) != cupolas_OK) {
        cupolas_mem_free(breaker);
        return NULL;
    }
    
    return breaker;
}

void circuit_breaker_destroy(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_destroy(&breaker->lock);
    cupolas_mem_free(breaker);
}

void circuit_breaker_set_callback(circuit_breaker_t* breaker,
                                 circuit_state_change_callback_t callback,
                                 void* user_data) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    breaker->callback = callback;
    breaker->callback_user_data = user_data;
    cupolas_mutex_unlock(&breaker->lock);
}

static void transition_state(circuit_breaker_t* breaker, circuit_state_t new_state) {
    if (!breaker || breaker->state == new_state) return;
    
    circuit_state_t old_state = breaker->state;
    breaker->state = new_state;
    breaker->state_change_time_ms = cupolas_time_ms();
    breaker->state_changes++;
    
    if (old_state == CIRCUIT_STATE_OPEN) {
        breaker->consecutive_failures = 0;
    } else if (old_state == CIRCUIT_STATE_HALF_OPEN) {
        breaker->half_open_calls = 0;
        breaker->consecutive_successes = 0;
    }
    
    circuit_state_change_callback_t cb = breaker->callback;
    void* cb_data = breaker->callback_user_data;
    
    cupolas_mutex_unlock(&breaker->lock);
    
    if (cb) {
        cb(old_state, new_state, cb_data);
    }
}

void circuit_breaker_record_success(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    breaker->total_calls++;
    breaker->successful_calls++;
    breaker->consecutive_failures = 0;
    
    if (breaker->state == CIRCUIT_STATE_HALF_OPEN) {
        breaker->consecutive_successes++;
        breaker->half_open_calls++;
        
        if (breaker->consecutive_successes >= breaker->config.success_threshold) {
            transition_state(breaker, CIRCUIT_STATE_CLOSED);
        }
    }
    
    update_failure_rate(breaker);
    
    cupolas_mutex_unlock(&breaker->lock);
}

void circuit_breaker_record_failure(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    breaker->total_calls++;
    breaker->failed_calls++;
    breaker->consecutive_failures++;
    breaker->last_failure_time_ms = cupolas_time_ms();
    
    if (breaker->state == CIRCUIT_STATE_HALF_OPEN) {
        breaker->half_open_calls++;
        transition_state(breaker, CIRCUIT_STATE_OPEN);
    } else if (breaker->state == CIRCUIT_STATE_CLOSED && should_open_circuit(breaker)) {
        transition_state(breaker, CIRCUIT_STATE_OPEN);
    }
    
    update_failure_rate(breaker);
    
    cupolas_mutex_unlock(&breaker->lock);
}

void circuit_breaker_record_timeout(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    breaker->total_calls++;
    breaker->timed_out_calls++;
    breaker->consecutive_failures++;
    breaker->last_failure_time_ms = cupolas_time_ms();
    
    if (breaker->state == CIRCUIT_STATE_HALF_OPEN) {
        transition_state(breaker, CIRCUIT_STATE_OPEN);
    } else if (breaker->state == CIRCUIT_STATE_CLOSED && should_open_circuit(breaker)) {
        transition_state(breaker, CIRCUIT_STATE_OPEN);
    }
    
    update_failure_rate(breaker);
    
    cupolas_mutex_unlock(&breaker->lock);
}

circuit_state_t circuit_breaker_get_state(circuit_breaker_t* breaker) {
    if (!breaker) return CIRCUIT_STATE_CLOSED;
    
    cupolas_mutex_lock(&breaker->lock);
    
    if (breaker->state == CIRCUIT_STATE_OPEN) {
        uint64_t now = cupolas_time_ms();
        if (now - breaker->state_change_time_ms >= breaker->config.timeout_ms) {
            transition_state(breaker, CIRCUIT_STATE_HALF_OPEN);
        }
    }
    
    circuit_state_t state = breaker->state;
    cupolas_mutex_unlock(&breaker->lock);
    
    return state;
}

bool circuit_breaker_is_available(circuit_breaker_t* breaker) {
    circuit_state_t state = circuit_breaker_get_state(breaker);
    
    if (state == CIRCUIT_STATE_CLOSED) {
        return true;
    } else if (state == CIRCUIT_STATE_HALF_OPEN) {
        cupolas_mutex_lock(&breaker->lock);
        bool available = breaker->half_open_calls < breaker->config.half_open_max_calls;
        cupolas_mutex_unlock(&breaker->lock);
        return available;
    }
    
    return false;
}

int circuit_breaker_call(circuit_breaker_t* breaker,
                        int (*func)(void* arg),
                        void* arg,
                        uint32_t timeout_ms) {
    if (!breaker || !func) return -1;
    
    circuit_state_t state = circuit_breaker_get_state(breaker);
    
    if (state == CIRCUIT_STATE_OPEN) {
        breaker->rejected_calls++;
        return -2;
    }
    
    if (state == CIRCUIT_STATE_HALF_OPEN) {
        cupolas_mutex_lock(&breaker->lock);
        if (breaker->half_open_calls >= breaker->config.half_open_max_calls) {
            cupolas_mutex_unlock(&breaker->lock);
            breaker->rejected_calls++;
            return -2;
        }
        breaker->half_open_calls++;
        cupolas_mutex_unlock(&breaker->lock);
    }
    
    uint64_t start_time = cupolas_time_ms();
    uint32_t effective_timeout = timeout_ms > 0 ? timeout_ms : breaker->config.timeout_ms;
    
    int result = -1;
    bool completed = false;
    
    while (!completed && (cupolas_time_ms() - start_time < effective_timeout)) {
        result = func(arg);
        completed = true;
    }
    
    if (!completed) {
        circuit_breaker_record_timeout(breaker);
        return -3;
    }
    
    if (result == 0) {
        circuit_breaker_record_success(breaker);
    } else {
        circuit_breaker_record_failure(breaker);
    }
    
    return result;
}

void circuit_breaker_reset(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    breaker->state = CIRCUIT_STATE_CLOSED;
    breaker->consecutive_failures = 0;
    breaker->consecutive_successes = 0;
    breaker->half_open_calls = 0;
    breaker->state_change_time_ms = cupolas_time_ms();
    breaker->total_calls = 0;
    breaker->successful_calls = 0;
    breaker->failed_calls = 0;
    breaker->rejected_calls = 0;
    breaker->timed_out_calls = 0;
    
    cupolas_atomic_store64(&breaker->current_failure_rate, 0);
    
    cupolas_mutex_unlock(&breaker->lock);
}

void circuit_breaker_get_stats(circuit_breaker_t* breaker, circuit_breaker_stats_t* stats) {
    if (!breaker || !stats) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    stats->total_calls = breaker->total_calls;
    stats->successful_calls = breaker->successful_calls;
    stats->failed_calls = breaker->failed_calls;
    stats->rejected_calls = breaker->rejected_calls;
    stats->timed_out_calls = breaker->timed_out_calls;
    stats->state_changes = breaker->state_changes;
    stats->current_state = breaker->state;
    stats->last_state_change_time_ms = breaker->state_change_time_ms;
    stats->current_failure_rate = (double)cupolas_atomic_load64(&breaker->current_failure_rate) / 1000000.0;
    
    cupolas_mutex_unlock(&breaker->lock);
}

void circuit_breaker_reset_stats(circuit_breaker_t* breaker) {
    if (!breaker) return;
    
    cupolas_mutex_lock(&breaker->lock);
    
    breaker->total_calls = 0;
    breaker->successful_calls = 0;
    breaker->failed_calls = 0;
    breaker->rejected_calls = 0;
    breaker->timed_out_calls = 0;
    breaker->consecutive_failures = 0;
    breaker->consecutive_successes = 0;
    
    cupolas_atomic_store64(&breaker->current_failure_rate, 0);
    
    cupolas_mutex_unlock(&breaker->lock);
}

struct circuit_breaker_registry_entry {
    char name[128];
    circuit_breaker_t* breaker;
    struct circuit_breaker_registry_entry* next;
};

struct circuit_breaker_registry {
    struct circuit_breaker_registry_entry* entries;
    cupolas_mutex_t lock;
};

circuit_breaker_registry_t* circuit_breaker_registry_create(void) {
    circuit_breaker_registry_t* registry = 
        (circuit_breaker_registry_t*)cupolas_mem_alloc(sizeof(circuit_breaker_registry_t));
    if (!registry) return NULL;
    
    memset(registry, 0, sizeof(circuit_breaker_registry_t));
    
    if (cupolas_mutex_init(&registry->lock) != cupolas_OK) {
        cupolas_mem_free(registry);
        return NULL;
    }
    
    return registry;
}

void circuit_breaker_registry_destroy(circuit_breaker_registry_t* registry) {
    if (!registry) return;
    
    cupolas_mutex_lock(&registry->lock);
    
    struct circuit_breaker_registry_entry* entry = registry->entries;
    while (entry) {
        struct circuit_breaker_registry_entry* next = entry->next;
        circuit_breaker_destroy(entry->breaker);
        cupolas_mem_free(entry);
        entry = next;
    }
    
    cupolas_mutex_unlock(&registry->lock);
    
    cupolas_mutex_destroy(&registry->lock);
    cupolas_mem_free(registry);
}

int circuit_breaker_registry_register(circuit_breaker_registry_t* registry,
                                      const char* name,
                                      circuit_breaker_t* breaker) {
    if (!registry || !name || !breaker) return -1;
    
    cupolas_mutex_lock(&registry->lock);
    
    struct circuit_breaker_registry_entry* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            cupolas_mutex_unlock(&registry->lock);
            return -2;
        }
        entry = entry->next;
    }
    
    struct circuit_breaker_registry_entry* new_entry =
        (struct circuit_breaker_registry_entry*)cupolas_mem_alloc(
            sizeof(struct circuit_breaker_registry_entry));
    if (!new_entry) {
        cupolas_mutex_unlock(&registry->lock);
        return -1;
    }
    
    snprintf(new_entry->name, sizeof(new_entry->name), "%s", name);
    new_entry->breaker = breaker;
    new_entry->next = registry->entries;
    registry->entries = new_entry;
    
    cupolas_mutex_unlock(&registry->lock);
    
    return 0;
}

circuit_breaker_t* circuit_breaker_registry_get(circuit_breaker_registry_t* registry,
                                                const char* name) {
    if (!registry || !name) return NULL;
    
    cupolas_mutex_lock(&registry->lock);
    
    struct circuit_breaker_registry_entry* entry = registry->entries;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            circuit_breaker_t* breaker = entry->breaker;
            cupolas_mutex_unlock(&registry->lock);
            return breaker;
        }
        entry = entry->next;
    }
    
    cupolas_mutex_unlock(&registry->lock);
    
    return NULL;
}

void circuit_breaker_registry_remove(circuit_breaker_registry_t* registry,
                                     const char* name) {
    if (!registry || !name) return;
    
    cupolas_mutex_lock(&registry->lock);
    
    struct circuit_breaker_registry_entry** prev = &registry->entries;
    struct circuit_breaker_registry_entry* entry = registry->entries;
    
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            *prev = entry->next;
            circuit_breaker_destroy(entry->breaker);
            cupolas_mem_free(entry);
            break;
        }
        prev = &entry->next;
        entry = entry->next;
    }
    
    cupolas_mutex_unlock(&registry->lock);
}

void circuit_breaker_registry_reset_all(circuit_breaker_registry_t* registry) {
    if (!registry) return;
    
    cupolas_mutex_lock(&registry->lock);
    
    struct circuit_breaker_registry_entry* entry = registry->entries;
    while (entry) {
        circuit_breaker_reset(entry->breaker);
        entry = entry->next;
    }
    
    cupolas_mutex_unlock(&registry->lock);
}
