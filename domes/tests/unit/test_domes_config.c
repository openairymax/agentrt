/**
 * @file test_domes_config.c
 * @brief domes 配置热重载单元测试
 * @author Spharx
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/domes_config.h"
#include "../../src/platform/platform.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("[FAIL] %s: %s\n", name, msg); return 1; } while(0)

/* ============================================================================
 * 配置热重载基础功能测试
 * ============================================================================ */

static int test_config_create_destroy(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_create_destroy", "Failed to create config");

    const char* dir = domes_config_get_config_dir(cfg);
    if (!dir) TEST_FAIL("config_create_destroy", "Failed to get config dir");

    domes_config_destroy(cfg);

    cfg = domes_config_create("/tmp/test_config");
    if (!cfg) TEST_FAIL("config_create_destroy", "Failed to create config with dir");
    domes_config_destroy(cfg);

    TEST_PASS("config_create_destroy");
    return 0;
}

static int test_config_status(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_status", "Failed to create config");

    config_status_t status = domes_config_get_status(cfg, CONFIG_TYPE_PERMISSION_RULES);
    if (status != CONFIG_STATUS_OK) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_status", "Initial status should be OK");
    }

    status = domes_config_get_status(cfg, CONFIG_TYPE_ALL);
    if (status != CONFIG_STATUS_OK) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_status", "ALL status should be OK");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_status");
    return 0;
}

static int test_config_version(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_version", "Failed to create config");

    config_version_t version;
    if (domes_config_get_version(cfg, CONFIG_TYPE_PERMISSION_RULES, &version) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_version", "Failed to get version");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_version");
    return 0;
}

static int test_config_reload(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_reload", "Failed to create config");

    if (domes_config_reload(cfg, CONFIG_TYPE_PERMISSION_RULES) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_reload", "Failed to reload config");
    }

    if (domes_config_reload(cfg, CONFIG_TYPE_ALL) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_reload", "Failed to reload all configs");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_reload");
    return 0;
}

static int test_config_validate(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_validate", "Failed to create config");

    config_validation_result_t result;
    if (domes_config_validate(cfg, CONFIG_TYPE_PERMISSION_RULES, &result) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_validate", "Failed to validate config");
    }

    if (!result.valid) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_validate", "Config should be valid by default");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_validate");
    return 0;
}

static int test_config_apply_rollback(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_apply_rollback", "Failed to create config");

    if (domes_config_apply(cfg, CONFIG_TYPE_PERMISSION_RULES) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_apply_rollback", "Failed to apply config");
    }

    if (domes_config_rollback(cfg, CONFIG_TYPE_PERMISSION_RULES) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_apply_rollback", "Failed to rollback config");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_apply_rollback");
    return 0;
}

static int test_config_watch(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_watch", "Failed to create config");

    int watcher_id = domes_config_watch(cfg, CONFIG_TYPE_PERMISSION_RULES, NULL, NULL);
    if (watcher_id < 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_watch", "Failed to register watcher");
    }

    if (domes_config_unwatch(cfg, watcher_id) != 0) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_watch", "Failed to unregister watcher");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_watch");
    return 0;
}

static int test_config_export(void) {
    domes_config_t* cfg = domes_config_create(NULL);
    if (!cfg) TEST_FAIL("config_export", "Failed to create config");

    char json_buf[4096];
    char yaml_buf[4096];

    size_t json_len = domes_config_export_json(cfg, CONFIG_TYPE_ALL, json_buf, sizeof(json_buf));
    size_t yaml_len = domes_config_export_yaml(cfg, CONFIG_TYPE_ALL, yaml_buf, sizeof(yaml_buf));

    if (json_len == 0) TEST_FAIL("config_export", "Failed to export JSON");
    if (yaml_len == 0) TEST_FAIL("config_export", "Failed to export YAML");

    if (strstr(json_buf, "{\"configs\":[") == NULL) {
        domes_config_destroy(cfg);
        TEST_FAIL("config_export", "Invalid JSON format");
    }

    domes_config_destroy(cfg);
    TEST_PASS("config_export");
    return 0;
}

static int test_config_string_conversions(void) {
    const char* type_str = domes_config_type_string(CONFIG_TYPE_PERMISSION_RULES);
    if (strcmp(type_str, "permission_rules") != 0) {
        TEST_FAIL("config_string_conversions", "Invalid type string");
    }

    const char* status_str = domes_config_status_string(CONFIG_STATUS_APPLIED);
    if (strcmp(status_str, "applied") != 0) {
        TEST_FAIL("config_string_conversions", "Invalid status string");
    }

    TEST_PASS("config_string_conversions");
    return 0;
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("domes Config Hot-Reload Unit Tests\n");
    printf("========================================\n\n");

    printf("--- Config Basic Tests ---\n");
    if (test_config_create_destroy() != 0) return 1;
    if (test_config_status() != 0) return 1;
    if (test_config_version() != 0) return 1;
    if (test_config_reload() != 0) return 1;
    if (test_config_validate() != 0) return 1;
    if (test_config_apply_rollback() != 0) return 1;
    if (test_config_watch() != 0) return 1;
    if (test_config_export() != 0) return 1;
    if (test_config_string_conversions() != 0) return 1;

    printf("\n========================================\n");
    printf("All config tests passed!\n");
    printf("========================================\n");

    return 0;
}