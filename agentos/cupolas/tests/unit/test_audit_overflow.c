/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_audit_overflow.c - Audit Overflow Handler Unit Tests
 */

#include "audit_overflow.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void test_overflow_handler_create_destroy(void) {
    printf("Test: overflow_handler_create_destroy... ");
    
    overflow_handler_t* handler = overflow_handler_create("/tmp/cupolas_test", 1, 100);
    assert(handler != NULL);
    
    overflow_handler_destroy(handler);
    printf("PASS\n");
}

static void test_overflow_level_check(void) {
    printf("Test: overflow_level_check... ");
    
    assert(overflow_handler_check_level(0, 100) == OVERFLOW_LEVEL_NORMAL);
    assert(overflow_handler_check_level(79, 100) == OVERFLOW_LEVEL_NORMAL);
    
    assert(overflow_handler_check_level(80, 100) == OVERFLOW_LEVEL_WARNING);
    assert(overflow_handler_check_level(94, 100) == OVERFLOW_LEVEL_WARNING);
    
    assert(overflow_handler_check_level(95, 100) == OVERFLOW_LEVEL_CRITICAL);
    assert(overflow_handler_check_level(99, 100) == OVERFLOW_LEVEL_CRITICAL);
    
    assert(overflow_handler_check_level(100, 100) == OVERFLOW_LEVEL_SPILLING);
    assert(overflow_handler_check_level(150, 100) == OVERFLOW_LEVEL_SPILLING);
    
    assert(overflow_handler_check_level(0, 0) == OVERFLOW_LEVEL_SPILLING);
    
    printf("PASS\n");
}

static void test_overflow_write_and_stats(void) {
    printf("Test: overflow_write_and_stats... ");
    
    overflow_handler_t* handler = overflow_handler_create("/tmp/cupolas_test", 10, 0);
    assert(handler != NULL);
    
    audit_entry_t* entry = audit_entry_create(
        AUDIT_EVENT_PERMISSION,
        "test_agent",
        "test_action",
        "test_resource",
        "test_detail",
        1
    );
    assert(entry != NULL);
    
    int result = overflow_handler_write(handler, entry);
    assert(result == 0);
    
    audit_entry_destroy(entry);
    
    overflow_stats_t stats;
    overflow_handler_get_stats(handler, &stats);
    assert(stats.total_events_received >= 1);
    assert(stats.events_written_to_disk >= 1);
    assert(stats.disk_write_errors == 0);
    
    overflow_handler_flush(handler);
    
    overflow_handler_reset_stats(handler);
    overflow_handler_get_stats(handler, &stats);
    assert(stats.total_events_received == 0);
    
    overflow_handler_destroy(handler);
    printf("PASS\n");
}

static void test_overflow_null_handling(void) {
    printf("Test: overflow_null_handling... ");
    
    assert(overflow_handler_create(NULL, 0, 0) != NULL);
    
    audit_entry_t* entry = audit_entry_create(AUDIT_EVENT_SYSTEM, NULL, NULL, NULL, NULL, 0);
    if (entry) {
        int result = overflow_handler_write(NULL, entry);
        assert(result == -1);
        audit_entry_destroy(entry);
    }
    
    overflow_handler_flush(NULL);
    
    overflow_stats_t stats;
    overflow_handler_get_stats(NULL, &stats);
    assert(stats.total_events_received == 0);
    
    overflow_handler_reset_stats(NULL);
    
    overflow_handler_destroy(NULL);
    
    printf("PASS\n");
}

static void test_queue_ex_create_destroy(void) {
    printf("Test: queue_ex_create_destroy... ");
    
    audit_queue_ex_t* queue = audit_queue_ex_create(100, "/tmp/cupolas_test", 5);
    assert(queue != NULL);
    
    assert(audit_queue_ex_size(queue) == 0);
    
    audit_queue_ex_destroy(queue);
    printf("PASS\n");
}

static void test_queue_ex_push_pop(void) {
    printf("Test: queue_ex_push_pop... ");
    
    audit_queue_ex_t* queue = audit_queue_ex_create(10, "/tmp/cupolas_test", 5);
    assert(queue != NULL);
    
    audit_entry_t* entry = audit_entry_create(
        AUDIT_EVENT_PERMISSION,
        "agent1",
        "action1",
        "resource1",
        "detail1",
        1
    );
    assert(entry != NULL);
    
    int result = audit_queue_ex_push(queue, entry);
    assert(result == 0);
    
    assert(audit_queue_ex_size(queue) == 1);
    
    audit_entry_t* popped;
    result = audit_queue_ex_pop(queue, &popped);
    assert(result == 0);
    assert(popped != NULL);
    assert(strcmp(popped->agent_id, "agent1") == 0);
    
    audit_entry_destroy(popped);
    
    assert(audit_queue_ex_size(queue) == 0);
    
    audit_queue_ex_destroy(queue);
    printf("PASS\n");
}

static void test_queue_ex_overflow_callback_called(int call_count) {
    static int callback_invoked = 0;
    
    static void test_callback(overflow_level_t level, size_t queue_size, size_t max_size, void* user_data) {
        (void)queue_size; (void)max_size; (void)user_data;
        callback_invoked++;
        assert(level >= OVERFLOW_LEVEL_WARNING);
    }
    
    printf("Test: queue_ex_overflow_callback... ");
    
    audit_queue_ex_t* queue = audit_queue_ex_create(3, "/tmp/cupolas_test", 5);
    assert(queue != NULL);
    
    callback_invoked = 0;
    int cb_result = audit_queue_ex_set_overflow_callback(queue, test_callback, NULL);
    assert(cb_result == 0);
    
    for (int i = 0; i < 4; i++) {
        char agent[32], action[32];
        snprintf(agent, sizeof(agent), "agent%d", i);
        snprintf(action, sizeof(action), "action%d", i);
        
        audit_entry_t* entry = audit_entry_create(
            AUDIT_EVENT_PERMISSION,
            agent,
            action,
            "resource",
            "detail",
            1
        );
        
        if (entry) {
            audit_queue_ex_push_with_callback(queue, entry, test_callback, NULL);
        }
    }
    
    assert(callback_invoked > 0 || audit_queue_ex_get_overflow_level(queue) >= OVERFLOW_LEVEL_WARNING);
    
    audit_queue_ex_destroy(queue);
    printf("PASS\n");
}

static void test_queue_ex_stats(void) {
    printf("Test: queue_ex_stats... ");
    
    audit_queue_ex_t* queue = audit_queue_ex_create(10, "/tmp/cupolas_test", 5);
    assert(queue != NULL);
    
    uint64_t pushed, popped, spilled, dropped;
    audit_queue_ex_get_stats(queue, &pushed, &popped, &spilled, &dropped);
    assert(pushed == 0);
    assert(popped == 0);
    assert(spilled == 0);
    assert(dropped == 0);
    
    for (int i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "entry_%d", i);
        
        audit_entry_t* entry = audit_entry_create(
            AUDIT_EVENT_SANITIZER,
            buf,
            "action",
            "resource",
            "detail",
            1
        );
        
        if (entry) {
            audit_queue_ex_push(queue, entry);
        }
    }
    
    audit_queue_ex_get_stats(queue, &pushed, &popped, &spilled, &dropped);
    assert(pushed >= 5);
    
    audit_queue_ex_shutdown(queue, false);
    audit_queue_ex_destroy(queue);
    printf("PASS\n");
}

int main(void) {
    printf("=== Audit Overflow Tests ===\n");
    
    test_overflow_handler_create_destroy();
    test_overflow_level_check();
    test_overflow_write_and_stats();
    test_overflow_null_handling();
    test_queue_ex_create_destroy();
    test_queue_ex_push_pop();
    test_queue_ex_overflow_callback_called(0);
    test_queue_ex_stats();
    
    printf("=== All tests PASSED ===\n");
    return 0;
}
