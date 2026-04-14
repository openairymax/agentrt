/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_circuit_breaker.c - Circuit Breaker Unit Tests
 */

#include "circuit_breaker.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int success_func(void* arg) {
    (void)arg;
    return 0;
}

static int failure_func(void* arg) {
    (void)arg;
    return -1;
}

static void test_breaker_create_destroy(void) {
    printf("Test: breaker_create_destroy... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_CLOSED);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_success_record(void) {
    printf("Test: breaker_success_record... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    circuit_breaker_record_success(breaker);
    circuit_breaker_record_success(breaker);
    
    circuit_breaker_stats_t stats;
    circuit_breaker_get_stats(breaker, &stats);
    assert(stats.successful_calls == 2);
    assert(stats.current_state == CIRCUIT_STATE_CLOSED);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_failure_transition(void) {
    printf("Test: breaker_failure_transition... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    circuit_breaker_record_failure(breaker);
    circuit_breaker_record_failure(breaker);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_CLOSED);
    
    circuit_breaker_record_failure(breaker);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_OPEN);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_half_open_recovery(void) {
    printf("Test: breaker_half_open_recovery... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 2,
        .success_threshold = 2,
        .timeout_ms = 100,
        .half_open_max_calls = 3,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    circuit_breaker_record_failure(breaker);
    circuit_breaker_record_failure(breaker);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_OPEN);
    
    cupolas_sleep_ms(150);
    
    circuit_state_t state = circuit_breaker_get_state(breaker);
    assert(state == CIRCUIT_STATE_HALF_OPEN);
    
    circuit_breaker_record_success(breaker);
    circuit_breaker_record_success(breaker);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_CLOSED);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_is_available(void) {
    printf("Test: breaker_is_available... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 2,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    assert(circuit_breaker_is_available(breaker) == true);
    
    circuit_breaker_record_failure(breaker);
    circuit_breaker_record_failure(breaker);
    assert(circuit_breaker_is_available(breaker) == false);
    
    circuit_breaker_reset(breaker);
    assert(circuit_breaker_is_available(breaker) == true);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_call_success(void) {
    printf("Test: breaker_call_success... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    int result = circuit_breaker_call(breaker, success_func, NULL, 100);
    assert(result == 0);
    
    circuit_breaker_stats_t stats;
    circuit_breaker_get_stats(breaker, &stats);
    assert(stats.successful_calls == 1);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_call_failure(void) {
    printf("Test: breaker_call_failure... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    int result = circuit_breaker_call(breaker, failure_func, NULL, 100);
    assert(result == -1);
    
    circuit_breaker_stats_t stats;
    circuit_breaker_get_stats(breaker, &stats);
    assert(stats.failed_calls == 1);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_reject_when_open(void) {
    printf("Test: breaker_reject_when_open... ");
    
    circuit_breaker_config_t config = {
        .failure_threshold = 1,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2,
        .failure_rate_threshold = 0.5
    };
    
    circuit_breaker_t* breaker = circuit_breaker_create(&config);
    assert(breaker != NULL);
    
    circuit_breaker_record_failure(breaker);
    assert(circuit_breaker_get_state(breaker) == CIRCUIT_STATE_OPEN);
    
    int result = circuit_breaker_call(breaker, success_func, NULL, 100);
    assert(result == -2);
    
    circuit_breaker_stats_t stats;
    circuit_breaker_get_stats(breaker, &stats);
    assert(stats.rejected_calls == 1);
    
    circuit_breaker_destroy(breaker);
    printf("PASS\n");
}

static void test_breaker_state_strings(void) {
    printf("Test: breaker_state_strings... ");
    
    assert(strcmp(circuit_state_to_string(CIRCUIT_STATE_CLOSED), "CLOSED") == 0);
    assert(strcmp(circuit_state_to_string(CIRCUIT_STATE_OPEN), "OPEN") == 0);
    assert(strcmp(circuit_state_to_string(CIRCUIT_STATE_HALF_OPEN), "HALF_OPEN") == 0);
    
    assert(strcmp(circuit_event_to_string(CIRCUIT_EVENT_SUCCESS), "SUCCESS") == 0);
    assert(strcmp(circuit_event_to_string(CIRCUIT_EVENT_FAILURE), "FAILURE") == 0);
    assert(strcmp(circuit_event_to_string(CIRCUIT_EVENT_TIMEOUT), "TIMEOUT") == 0);
    assert(strcmp(circuit_event_to_string(CIRCUIT_EVENT_REJECTED), "REJECTED") == 0);
    
    printf("PASS\n");
}

static void test_breaker_registry(void) {
    printf("Test: breaker_registry... ");
    
    circuit_breaker_registry_t* registry = circuit_breaker_registry_create();
    assert(registry != NULL);
    
    circuit_breaker_config_t config = {0};
    circuit_breaker_t* breaker1 = circuit_breaker_create(&config);
    circuit_breaker_t* breaker2 = circuit_breaker_create(&config);
    
    assert(circuit_breaker_registry_register(registry, "service1", breaker1) == 0);
    assert(circuit_breaker_registry_register(registry, "service2", breaker2) == 0);
    
    assert(circuit_breaker_registry_get(registry, "service1") == breaker1);
    assert(circuit_breaker_registry_get(registry, "service2") == breaker2);
    assert(circuit_breaker_registry_get(registry, "nonexistent") == NULL);
    
    circuit_breaker_registry_reset_all(registry);
    
    circuit_breaker_registry_destroy(registry);
    printf("PASS\n");
}

int main(void) {
    printf("=== Circuit Breaker Tests ===\n");
    
    test_breaker_create_destroy();
    test_breaker_success_record();
    test_breaker_failure_transition();
    test_breaker_half_open_recovery();
    test_breaker_is_available();
    test_breaker_call_success();
    test_breaker_call_failure();
    test_breaker_reject_when_open();
    test_breaker_state_strings();
    test_breaker_registry();
    
    printf("=== All tests PASSED ===\n");
    return 0;
}
