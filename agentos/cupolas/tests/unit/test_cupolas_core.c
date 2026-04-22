/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_core.c - cupolas Module Unit Tests
 */

/**
 * @file test_cupolas_core.c
 * @brief cupolas Module Unit Tests
 * @author Spharx AgentOS Team
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/cupolas.h"
#include "../src/platform/platform.h"
#include "../src/permission/permission_cache.h"
#include "../src/permission/permission.h"
#include "../src/audit/audit.h"
#include "../src/sanitizer/sanitizer.h"
#include "../src/workbench/workbench.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

/* ============================================================================
 * 平台抽象层测试
 * ============================================================================ */

static void test_platform_mutex(void) {
    cupolas_mutex_t mutex;
    
    assert(cupolas_mutex_init(&mutex) == CUPOLAS_OK);
    assert(cupolas_mutex_lock(&mutex) == CUPOLAS_OK);
    assert(cupolas_mutex_unlock(&mutex) == CUPOLAS_OK);
    assert(cupolas_mutex_destroy(&mutex) == CUPOLAS_OK);
    
    TEST_PASS("platform_mutex");
}

static void test_platform_rwlock(void) {
    cupolas_rwlock_t rwlock;
    
    assert(cupolas_rwlock_init(&rwlock) == CUPOLAS_OK);
    assert(cupolas_rwlock_rdlock(&rwlock) == CUPOLAS_OK);
    assert(cupolas_rwlock_unlock(&rwlock) == CUPOLAS_OK);
    assert(cupolas_rwlock_wrlock(&rwlock) == CUPOLAS_OK);
    assert(cupolas_rwlock_unlock(&rwlock) == CUPOLAS_OK);
    assert(cupolas_rwlock_destroy(&rwlock) == CUPOLAS_OK);
    
    TEST_PASS("platform_rwlock");
}

static void test_platform_time(void) {
    cupolas_timestamp_t ts;
    
    assert(cupolas_time_now(&ts) == cupolas_OK);
    assert(ts.sec > 0);
    
    uint64_t ms = cupolas_time_ms();
    assert(ms > 0);
    
    TEST_PASS("platform_time");
}

static void test_platform_atomic(void) {
    cupolas_atomic32_t val32 = 0;
    cupolas_atomic64_t val64 = 0;
    
    assert(cupolas_atomic_load32(&val32) == 0);
    cupolas_atomic_store32(&val32, 42);
    assert(cupolas_atomic_load32(&val32) == 42);
    assert(cupolas_atomic_inc32(&val32) == 42);
    assert(cupolas_atomic_load32(&val32) == 43);
    assert(cupolas_atomic_dec32(&val32) == 43);
    assert(cupolas_atomic_load32(&val32) == 42);
    assert(cupolas_atomic_cas32(&val32, 42, 100) == true);
    assert(cupolas_atomic_load32(&val32) == 100);
    
    cupolas_atomic_store64(&val64, 1000000);
    assert(cupolas_atomic_load64(&val64) == 1000000);
    assert(cupolas_atomic_add64(&val64, 500000) == 1000000);
    assert(cupolas_atomic_load64(&val64) == 1500000);
    
    TEST_PASS("platform_atomic");
}

static void test_platform_string(void) {
    char* dup = cupolas_strdup("hello");
    assert(dup != NULL);
    assert(strcmp(dup, "hello") == 0);
    cupolas_mem_free(dup);
    
    assert(cupolas_strcasecmp("Hello", "HELLO") == 0);
    assert(cupolas_strcasecmp("Hello", "World") != 0);
    
    TEST_PASS("platform_string");
}

/* ============================================================================
 * 权限模块测试
 * ============================================================================ */

static void test_permission_engine(void) {
    permission_engine_t* engine = permission_engine_create(NULL);
    assert(engine != NULL);
    
    assert(permission_engine_add_rule(engine, "agent1", "read", "/data/*", 1, 100) == cupolas_OK);
    assert(permission_engine_add_rule(engine, "agent1", "write", "/data/*", 0, 100) == cupolas_OK);
    assert(permission_engine_add_rule(engine, "*", "read", "/public/*", 1, 50) == cupolas_OK);
    
    assert(permission_engine_check(engine, "agent1", "read", "/data/file.txt", NULL) == 1);
    assert(permission_engine_check(engine, "agent1", "write", "/data/file.txt", NULL) == 0);
    assert(permission_engine_check(engine, "agent2", "read", "/public/info.txt", NULL) == 1);
    
    permission_engine_destroy(engine);
    
    TEST_PASS("permission_engine");
}

static void test_permission_cache(void) {
    cache_manager_t* cache = cache_manager_create(100, 60000);
    assert(cache != NULL);
    
    assert(cache_manager_get(cache, "agent1", "read", "/data", NULL) == -1);
    
    cache_manager_put(cache, "agent1", "read", "/data", NULL, 1);
    assert(cache_manager_get(cache, "agent1", "read", "/data", NULL) == 1);
    
    cache_manager_clear(cache);
    assert(cache_manager_get(cache, "agent1", "read", "/data", NULL) == -1);
    
    cache_manager_destroy(cache);
    
    TEST_PASS("permission_cache");
}

/* ============================================================================
 * 审计模块测试
 * ============================================================================ */

static void test_audit_queue(void) {
    audit_queue_t* queue = audit_queue_create(100);
    assert(queue != NULL);
    
    audit_entry_t* entry1 = audit_entry_create(AUDIT_EVENT_PERMISSION, "agent1", "read", "/data", NULL, 1);
    assert(entry1 != NULL);
    
    assert(audit_queue_push(queue, entry1) == cupolas_OK);
    assert(audit_queue_size(queue) == 1);
    
    audit_entry_t* entry2 = NULL;
    assert(audit_queue_try_pop(queue, &entry2) == cupolas_OK);
    assert(entry2 != NULL);
    assert(strcmp(entry2->agent_id, "agent1") == 0);
    audit_entry_destroy(entry2);
    
    audit_queue_destroy(queue);
    
    TEST_PASS("audit_queue");
}

/* ============================================================================
 * 净化器模块测试
 * ============================================================================ */

static void test_sanitizer(void) {
    sanitizer_t* san = sanitizer_create(NULL);
    assert(san != NULL);
    
    char output[256];
    sanitize_context_t ctx;
    sanitizer_default_context(&ctx);
    
    assert(sanitizer_sanitize(san, "hello world", output, sizeof(output), &ctx) == SANITIZE_OK);
    assert(strcmp(output, "hello world") == 0);
    
    ctx.level = SANITIZE_LEVEL_STRICT;
    assert(sanitizer_sanitize(san, "<script>alert(1)</script>", output, sizeof(output), &ctx) == SANITIZE_REJECTED);
    
    ctx.level = SANITIZE_LEVEL_NORMAL;
    assert(sanitizer_sanitize(san, "<script>", output, sizeof(output), &ctx) == SANITIZE_MODIFIED);
    
    sanitizer_destroy(san);
    
    TEST_PASS("sanitizer");
}

static void test_sanitizer_escape(void) {
    char output[256];
    
    sanitizer_escape_html("<script>", output, sizeof(output));
    assert(strcmp(output, "&lt;script&gt;") == 0);
    
    sanitizer_escape_sql("test'value", output, sizeof(output));
    assert(strstr(output, "''") != NULL);
    
    sanitizer_escape_shell("hello world", output, sizeof(output));
    assert(strstr(output, "\\x20") != NULL);
    
    TEST_PASS("sanitizer_escape");
}

/* ============================================================================
 * 工位模块测试
 * ============================================================================ */

static void test_workbench_config(void) {
    workbench_config_t manager;
    workbench_default_config(&manager);
    
    assert(manager.timeout_ms > 0);
    assert(manager.max_output_size > 0);
    assert(manager.redirect_stdout == true);
    
    TEST_PASS("workbench_config");
}

static void test_workbench_create_destroy(void) {
    workbench_t* wb = workbench_create(NULL);
    assert(wb != NULL);
    assert(workbench_get_state(wb) == WORKBENCH_STATE_IDLE);
    workbench_destroy(wb);
    
    TEST_PASS("workbench_create_destroy");
}

/* ============================================================================
 * 核心模块测试
 * ============================================================================ */

static void test_cupolas_init_cleanup(void) {
    assert(cupolas_init(NULL) == cupolas_OK);
    assert(strcmp(cupolas_version(), "1.0.0") == 0);
    cupolas_cleanup();
    
    TEST_PASS("cupolas_init_cleanup");
}

static void test_cupolas_permission(void) {
    assert(cupolas_init(NULL) == cupolas_OK);
    
    assert(cupolas_add_permission_rule("test_agent", "read", "/test/*", 1, 100) == cupolas_OK);
    assert(cupolas_check_permission("test_agent", "read", "/test/file.txt", NULL) == 1);
    
    cupolas_cleanup();
    
    TEST_PASS("cupolas_permission");
}

static void test_cupolas_sanitize(void) {
    assert(cupolas_init(NULL) == cupolas_OK);
    
    char output[256];
    assert(cupolas_sanitize_input("hello", output, sizeof(output)) == cupolas_OK);
    assert(strcmp(output, "hello") == 0);
    
    cupolas_cleanup();
    
    TEST_PASS("cupolas_sanitize");
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("cupolas Module Unit Tests\n");
    printf("========================================\n\n");
    
    printf("--- Platform Tests ---\n");
    test_platform_mutex();
    test_platform_rwlock();
    test_platform_time();
    test_platform_atomic();
    test_platform_string();
    
    printf("\n--- Permission Tests ---\n");
    test_permission_engine();
    test_permission_cache();
    
    printf("\n--- Audit Tests ---\n");
    test_audit_queue();
    
    printf("\n--- Sanitizer Tests ---\n");
    test_sanitizer();
    test_sanitizer_escape();
    
    printf("\n--- Workbench Tests ---\n");
    test_workbench_config();
    test_workbench_create_destroy();
    
    printf("\n--- Core Tests ---\n");
    test_cupolas_init_cleanup();
    test_cupolas_permission();
    test_cupolas_sanitize();
    
    printf("\n========================================\n");
    printf("All tests passed!\n");
    printf("========================================\n");
    
    return 0;
}
