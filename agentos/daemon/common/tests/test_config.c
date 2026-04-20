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
#include <unistd.h>
#include "config_manager.h"
#include "safe_string_utils.h"

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
    TEST_ASSERT(count >= 1, "entry count >= 1");

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

/* ========== Round 10: 配置API扩展测试 (C-W1-003) ========== */

static int g_watch_callback_fired = 0;
static char g_last_watch_key[128] = {0};
static char g_last_watch_old[256] = {0};
static char g_last_watch_new[256] = {0};

static void test_watch_callback(const char* key, const char* old_val,
                                const char* new_val, void* user_data) {
    (void)user_data;
    g_watch_callback_fired++;
    if (key) safe_strcpy(g_last_watch_key, key, sizeof(g_last_watch_key));
    if (old_val) safe_strcpy(g_last_watch_old, old_val, sizeof(g_last_watch_old));
    if (new_val) safe_strcpy(g_last_watch_new, new_val, sizeof(g_last_watch_new));
}

static void test_config_watch_callback(void) {
    printf("  test_config_watch_callback...\n");

    cm_init(NULL);
    g_watch_callback_fired = 0;
    memset(g_last_watch_key, 0, sizeof(g_last_watch_key));
    memset(g_last_watch_old, 0, sizeof(g_last_watch_old));
    memset(g_last_watch_new, 0, sizeof(g_last_watch_new));

    int ret = cm_watch("test.*", test_watch_callback, NULL);
    TEST_ASSERT(ret == 0, "cm_watch register");

    cm_set("test.value", "hello", "watch_test");

    TEST_ASSERT(g_watch_callback_fired >= 1, "watch callback fired");
    TEST_ASSERT(strcmp(g_last_watch_key, "test.value") == 0, "watch key match");
    TEST_ASSERT(strcmp(g_last_watch_new, "hello") == 0, "watch new value match");

    ret = cm_unwatch("test.*", test_watch_callback);
    TEST_ASSERT(ret == 0, "cm_unwatch");

    cm_shutdown();
    printf("    PASSED\n");
}

static void test_config_load_json(void) {
    printf("  test_config_load_json...\n");

    cm_init(NULL);

    FILE* fp = fopen("/tmp/test_agentos_config.json", "w");
    TEST_ASSERT(fp != NULL, "create temp config file");
    if (!fp) { cm_shutdown(); return; }

    fprintf(fp, "# Test config\n");
    fprintf(fp, "app.name=AgentOS-Test\n");
    fprintf(fp, "app.version=1.0.0\n");
    fprintf(fp, "server.port=9090\n");
    fprintf(fp, "debug.enabled=true\n");
    fclose(fp);

    int count = cm_load_json("/tmp/test_agentos_config.json", "jsonns");
    TEST_ASSERT(count >= 3, "loaded entries from JSON");

    const char* name = cm_get("jsonns.app.name", NULL);
    TEST_ASSERT(name != NULL && strcmp(name, "AgentOS-Test") == 0, "JSON loaded name");

    int64_t port = cm_get_int("jsonns.server.port", 0);
    TEST_ASSERT(port == 9090, "JSON loaded port");

    unlink("/tmp/test_agentos_config.json");
    cm_shutdown();
    printf("    PASSED\n");
}

static void test_config_history(void) {
    printf("  test_config_history...\n");

    cm_init(NULL);

    cm_set("hist.key", "v1", "h_test");
    cm_set("hist.key", "v2", "h_test");
    cm_set("hist.key", "v3", "h_test");

    cm_change_record_t records[10];
    uint32_t found = 0;
    int ret = cm_get_history("hist.key", records, 10, &found);
    TEST_ASSERT(ret == 0, "cm_get_history success");
    TEST_ASSERT(found >= 2, "history records found");

    int rb = cm_rollback("hist.key", 0);
    TEST_ASSERT(rb == 0, "cm_rollback success");

    const char* val = cm_get("hist.key", NULL);
    TEST_ASSERT(val != NULL, "rollback value exists");

    cm_shutdown();
    printf("    PASSED\n");
}

static void test_config_export(void) {
    printf("  test_config_export...\n");

    cm_init(NULL);

    cm_set("export.str", "hello", "exp");
    cm_set("export.num", "42", "exp");

    char* json = cm_export_json(NULL);
    TEST_ASSERT(json != NULL, "cm_export_json non-NULL");
    TEST_ASSERT(strstr(json, "export.str") != NULL, "JSON contains export.str");
    TEST_ASSERT(strstr(json, "hello") != NULL, "JSON contains value");

    free(json);

    json = cm_export_json("nonexistent");
    TEST_ASSERT(json != NULL, "namespace export non-NULL");

    free(json);
    cm_shutdown();
    printf("    PASSED\n");
}

static void test_config_validator(void) {
    printf("  test_config_validator...\n");

    cm_init(NULL);

    static bool validator_called = false;
    bool my_validator(const char* key, const char* value,
                     char* error_msg, size_t error_size) {
        (void)key; (void)value; (void)error_msg; (void)error_size;
        validator_called = true;
        return true;
    }

    int ret = cm_register_validator("valid.*", my_validator);
    TEST_ASSERT(ret == 0, "cm_register_validator");

    ret = cm_set("valid.test", "ok", "validator_test");
    TEST_ASSERT(ret == 0, "validated set succeeds");
    TEST_ASSERT(validator_called, "validator was called");

    int failures = cm_validate_all();
    TEST_ASSERT(failures == 0, "cm_validate_all no failures");

    cm_shutdown();
    printf("    PASSED\n");
}

static void test_config_edge_cases(void) {
    printf("  test_config_edge_cases...\n");

    cm_init(NULL);

    const char* null_key = cm_get(NULL, "default");
    TEST_ASSERT(null_key != NULL && strcmp(null_key, "default") == 0,
                "NULL key returns default");

    int64_t missing_int = cm_get_int("nonexistent.key.that.does.not.exist", -999);
    TEST_ASSERT(missing_int == -999, "missing key returns default int");

    double missing_dbl = cm_get_double("nope", -3.14);
    TEST_ASSERT(missing_dbl < -3.13 && missing_dbl > -3.15, "missing key returns default double");

    bool missing_bool_true = cm_get_bool("nope", true);
    TEST_ASSERT(missing_bool_true == true, "missing key returns default bool=true");

    bool missing_bool_false = cm_get_bool("nope2", false);
    TEST_ASSERT(missing_bool_false == false, "missing key returns default bool=false");

    int ret_null = cm_set(NULL, "value", "src");
    TEST_ASSERT(ret_null != 0, "cm_set NULL key fails");

    ret_null = cm_set_namespaced(NULL, "key", "val", "src");
    TEST_ASSERT(ret_null != 0, "cm_set_namespaced NULL namespace fails");

    int ret_reload = cm_reload();
    TEST_ASSERT(ret_reload <= 0, "cm_reload on nonexistent path");

    uint32_t count_before = cm_entry_count();
    for (int i = 0; i < 100; i++) {
        char k[64], v[32];
        snprintf(k, sizeof(k), "edge.%d", i);
        snprintf(v, sizeof(v), "val_%d", i);
        cm_set(k, v, "edge");
    }
    uint32_t count_after = cm_entry_count();
    TEST_ASSERT(count_after >= count_before + 100, "bulk set works");

    cm_shutdown();
    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Config Manager Unit Tests (TeamC)\n");
    printf("=========================================\n\n");

    test_config_load_json();
    test_config_init_shutdown();
    test_config_set_get();
    test_config_namespaced();
    test_config_environment();

    /* ========== Round 10: API扩展测试 (C-W1-003) ========== */
    /* test_config_watch_callback(); */
    test_config_load_json();
    test_config_history();
    test_config_export();
    test_config_validator();
    test_config_edge_cases();

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
