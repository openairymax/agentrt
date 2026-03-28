/**
 * @file test_lodges_core.c
 * @brief AgentOS 数据分区核心模块单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lodges.h"

static void test_init_shutdown(void) {
    printf("Test: init_shutdown...");

    assert(!lodges_is_initialized());

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges";
    manager.enable_auto_cleanup = true;

    lodges_error_t err = lodges_init(&manager);
    assert(err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED);

    if (err == lodges_SUCCESS) {
        assert(lodges_is_initialized());
        assert(strcmp(lodges_get_root(), "test_lodges") == 0);
        lodges_shutdown();
        assert(!lodges_is_initialized());
    }

    printf("PASS\n");
}

static void test_init_twice(void) {
    printf("Test: init_twice...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges2";

    lodges_error_t err1 = lodges_init(&manager);

    if (err1 == lodges_SUCCESS) {
        err1 = lodges_init(&manager);
        assert(err1 == lodges_ERR_ALREADY_INITIALIZED);
        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_get_path(void) {
    printf("Test: get_path...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges3";

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        assert(lodges_get_path(lodges_PATH_KERNEL) != NULL);
        assert(lodges_get_path(lodges_PATH_LOGS) != NULL);
        assert(lodges_get_path(lodges_PATH_REGISTRY) != NULL);
        assert(lodges_get_path(lodges_PATH_SERVICES) != NULL);
        assert(lodges_get_path(lodges_PATH_TRACES) != NULL);
        assert(lodges_get_path(lodges_PATH_KERNEL_IPC) != NULL);
        assert(lodges_get_path(lodges_PATH_KERNEL_MEMORY) != NULL);
        assert(lodges_get_path(lodges_PATH_MAX) == NULL);

        char full_path[256];
        err = lodges_get_full_path(lodges_PATH_LOGS, full_path, sizeof(full_path));
        assert(err == lodges_SUCCESS);
        assert(strstr(full_path, "logs") != NULL);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_get_full_path_invalid(void) {
    printf("Test: get_full_path_invalid...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges4";

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        char buffer[256];
        err = lodges_get_full_path(lodges_PATH_LOGS, NULL, 0);
        assert(err == lodges_ERR_INVALID_PARAM);

        err = lodges_get_full_path(-1, buffer, sizeof(buffer));
        assert(err == lodges_ERR_INVALID_PARAM);

        err = lodges_get_full_path(lodges_PATH_MAX, buffer, sizeof(buffer));
        assert(err == lodges_ERR_INVALID_PARAM);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats(void) {
    printf("Test: get_stats...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges5";

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        lodges_stats_t stats;
        memset(&stats, 0, sizeof(stats));

        err = lodges_get_stats(&stats);
        assert(err == lodges_SUCCESS);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats_not_initialized(void) {
    printf("Test: get_stats_not_initialized...");

    lodges_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    lodges_error_t err = lodges_get_stats(&stats);
    assert(err == lodges_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_cleanup(void) {
    printf("Test: cleanup...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges6";
    manager.enable_auto_cleanup = true;

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        uint64_t freed = 0;
        err = lodges_cleanup(true, &freed);
        assert(err == lodges_SUCCESS);

        err = lodges_cleanup(false, &freed);
        assert(err == lodges_SUCCESS);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_strerror(void) {
    printf("Test: strerror...");

    assert(strcmp(lodges_strerror(lodges_SUCCESS), "Success") == 0);
    assert(strcmp(lodges_strerror(lodges_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(lodges_strerror(lodges_ERR_NOT_INITIALIZED), "lodges not initialized") == 0);
    assert(strcmp(lodges_strerror(lodges_ERR_ALREADY_INITIALIZED), "lodges not initialized") != 0);
    assert(strcmp(lodges_strerror(lodges_ERR_DIR_CREATE_FAILED), "Failed to create directory") == 0);
    assert(strcmp(lodges_strerror(lodges_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(lodges_strerror((lodges_error_t)-999), "Unknown error") == 0);

    printf("PASS\n");
}

static void test_reload_config(void) {
    printf("Test: reload_config...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges7";
    manager.max_log_size_mb = 50;
    manager.log_retention_days = 3;

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        lodges_config_t new_config = {0};
        new_config.max_log_size_mb = 200;
        new_config.log_retention_days = 14;
        new_config.enable_auto_cleanup = false;

        err = lodges_reload_config(&new_config);
        assert(err == lodges_SUCCESS);

        err = lodges_reload_config(NULL);
        assert(err == lodges_ERR_INVALID_PARAM);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_reload_config_not_initialized(void) {
    printf("Test: reload_config_not_initialized...");

    lodges_config_t manager = {0};
    manager.max_log_size_mb = 100;

    lodges_error_t err = lodges_reload_config(&manager);
    assert(err == lodges_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_flush(void) {
    printf("Test: flush...");

    lodges_config_t manager = {0};
    manager.root_path = "test_lodges8";

    lodges_error_t err = lodges_init(&manager);
    if (err == lodges_SUCCESS) {
        err = lodges_flush();
        assert(err == lodges_SUCCESS);

        lodges_shutdown();
    }

    printf("PASS\n");
}

static void test_flush_not_initialized(void) {
    printf("Test: flush_not_initialized...");

    lodges_error_t err = lodges_flush();
    assert(err == lodges_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS lodges Core Unit Tests ===\n\n");

    test_init_shutdown();
    test_init_twice();
    test_get_path();
    test_get_full_path_invalid();
    test_get_stats();
    test_get_stats_not_initialized();
    test_cleanup();
    test_strerror();
    test_reload_config();
    test_reload_config_not_initialized();
    test_flush();
    test_flush_not_initialized();

    printf("\n=== All Tests Passed ===\n");
    return 0;
}

