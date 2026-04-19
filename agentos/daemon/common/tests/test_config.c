/**
 * @file test_config.c
 * @brief 配置管理器单元测试 (TeamC)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 对齐: cm_* 全局配置API (config_manager.h)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_manager.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (cond) { pass_count++; } else { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

static void test_config_init_shutdown(void) {
    printf("  test_config_init_shutdown...\n");

    cm_config_t cfg = cm_create_default_config();
    TEST_ASSERT(cfg.watch_interval_ms > 0, "default config valid");

    int ret = cm_init(&cfg);
    TEST_ASSERT(ret == 0, "cm_init success");

    cm_shutdown();

    printf("    PASSED\n");
}

static void test_config_set_get(void) {
    printf("  test_config_set_get...\n");

    cm_init(NULL);

    int ret = cm_set("test.port", "8080", "unit_test");
    TEST_ASSERT(ret == 0, "cm_set string");

    int64_t port = cm_get_int("test.port", -1);
    TEST_ASSERT(port == 8080, "cm_get_int value");

    ret = cm_set("test.host", "localhost", "unit_test");
    TEST_ASSERT(ret == 0, "cm_set host");

    const char* host = cm_get("test.host", NULL);
    TEST_ASSERT(host != NULL, "cm_get non-null");
    TEST_ASSERT(strcmp(host, "localhost") == 0, "cm_get value match");

    ret = cm_set("test.timeout", "3.14", "unit_test");
    TEST_ASSERT(ret == 0, "cm_set double");

    double timeout = cm_get_double("test.timeout", 0.0);
    TEST_ASSERT(timeout > 3.13 && timeout < 3.15, "cm_get_double value");

    bool debug_val = cm_get_bool("test.debug", false);
    TEST_ASSERT(debug_val == false, "cm_get_bool default");

    cm_shutdown();

    printf("    PASSED\n");
}

static void test_config_namespaced(void) {
    printf("  test_config_namespaced...\n");

    cm_init(NULL);

    int ret = cm_set_namespaced("daemon", "port", "9000", "test");
    TEST_ASSERT(ret == 0, "cm_set_namespaced");

    int64_t port = cm_get_int("daemon.port", -1);
    TEST_ASSERT(port == 9000, "namespaced get_int");

    const char* val = cm_get("daemon.port", NULL);
    TEST_ASSERT(val != NULL && strcmp(val, "9000") == 0, "namespaced get");

    uint32_t count = cm_entry_count();
    TEST_ASSERT(count >= 2, "entry count >= 2");

    cm_shutdown();

    printf("    PASSED\n");
}

static void test_config_environment(void) {
    printf("  test_config_environment...\n");

    cm_init(NULL);

    const char* env = cm_get_environment();
    TEST_ASSERT(env != NULL, "environment not null");

    int ret = cm_set_environment("test");
    TEST_ASSERT(ret == 0, "set environment");

    env = cm_get_environment();
    TEST_ASSERT(strcmp(env, "test") == 0, "environment value");

    cm_shutdown();

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Config Manager Unit Tests (TeamC)\n");
    printf("=========================================\n\n");

    test_config_init_shutdown();
    test_config_set_get();
    test_config_namespaced();
    test_config_environment();

    printf("\n-----------------------------------------\n");
    printf("  Results: %d/%d tests passed\n", pass_count, test_count);
    if (pass_count == test_count) {
        printf("  ✅ All tests PASSED\n");
        return 0;
    } else {
        printf("  ❌ %d test(s) FAILED\n", test_count - pass_count);
        return 1;
    }
}
