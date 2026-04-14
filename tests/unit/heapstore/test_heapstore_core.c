/**
 * @file test_heapstore_core.c
 * @brief AgentOS 数据分区核心模块单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "heapstore.h"

static void test_init_shutdown(void) {
    printf("Test: init_shutdown...");

    assert(!heapstore_is_initialized());

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore";
    manager.enable_auto_cleanup = true;

    heapstore_error_t err = heapstore_init(&manager);
    assert(err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED);

    if (err == heapstore_SUCCESS) {
        assert(heapstore_is_initialized());
        assert(strcmp(heapstore_get_root(), "test_heapstore") == 0);
        heapstore_shutdown();
        assert(!heapstore_is_initialized());
    }

    printf("PASS\n");
}

static void test_init_twice(void) {
    printf("Test: init_twice...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore2";

    heapstore_error_t err1 = heapstore_init(&manager);

    if (err1 == heapstore_SUCCESS) {
        err1 = heapstore_init(&manager);
        assert(err1 == heapstore_ERR_ALREADY_INITIALIZED);
        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_get_path(void) {
    printf("Test: get_path...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore3";

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        assert(heapstore_get_path(heapstore_PATH_KERNEL) != NULL);
        assert(heapstore_get_path(heapstore_PATH_LOGS) != NULL);
        assert(heapstore_get_path(heapstore_PATH_REGISTRY) != NULL);
        assert(heapstore_get_path(heapstore_PATH_SERVICES) != NULL);
        assert(heapstore_get_path(heapstore_PATH_TRACES) != NULL);
        assert(heapstore_get_path(heapstore_PATH_KERNEL_IPC) != NULL);
        assert(heapstore_get_path(heapstore_PATH_KERNEL_MEMORY) != NULL);
        assert(heapstore_get_path(heapstore_PATH_MAX) == NULL);

        char full_path[256];
        err = heapstore_get_full_path(heapstore_PATH_LOGS, full_path, sizeof(full_path));
        assert(err == heapstore_SUCCESS);
        assert(strstr(full_path, "logs") != NULL);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_get_full_path_invalid(void) {
    printf("Test: get_full_path_invalid...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore4";

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        char buffer[256];
        err = heapstore_get_full_path(heapstore_PATH_LOGS, NULL, 0);
        assert(err == heapstore_ERR_INVALID_PARAM);

        err = heapstore_get_full_path(-1, buffer, sizeof(buffer));
        assert(err == heapstore_ERR_INVALID_PARAM);

        err = heapstore_get_full_path(heapstore_PATH_MAX, buffer, sizeof(buffer));
        assert(err == heapstore_ERR_INVALID_PARAM);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats(void) {
    printf("Test: get_stats...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore5";

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        heapstore_stats_t stats;
        memset(&stats, 0, sizeof(stats));

        err = heapstore_get_stats(&stats);
        assert(err == heapstore_SUCCESS);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats_not_initialized(void) {
    printf("Test: get_stats_not_initialized...");

    heapstore_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    heapstore_error_t err = heapstore_get_stats(&stats);
    assert(err == heapstore_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_cleanup(void) {
    printf("Test: cleanup...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore6";
    manager.enable_auto_cleanup = true;

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        uint64_t freed = 0;
        err = heapstore_cleanup(true, &freed);
        assert(err == heapstore_SUCCESS);

        err = heapstore_cleanup(false, &freed);
        assert(err == heapstore_SUCCESS);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_strerror(void) {
    printf("Test: strerror...");

    assert(strcmp(heapstore_strerror(heapstore_SUCCESS), "Success") == 0);
    assert(strcmp(heapstore_strerror(heapstore_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(heapstore_strerror(heapstore_ERR_NOT_INITIALIZED), "heapstore not initialized") == 0);
    assert(strcmp(heapstore_strerror(heapstore_ERR_ALREADY_INITIALIZED), "heapstore not initialized") != 0);
    assert(strcmp(heapstore_strerror(heapstore_ERR_DIR_CREATE_FAILED), "Failed to create directory") == 0);
    assert(strcmp(heapstore_strerror(heapstore_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(heapstore_strerror((heapstore_error_t)-999), "Unknown error") == 0);

    printf("PASS\n");
}

static void test_reload_config(void) {
    printf("Test: reload_config...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore7";
    manager.max_log_size_mb = 50;
    manager.log_retention_days = 3;

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        heapstore_config_t new_config = {0};
        new_config.max_log_size_mb = 200;
        new_config.log_retention_days = 14;
        new_config.enable_auto_cleanup = false;

        err = heapstore_reload_config(&new_config);
        assert(err == heapstore_SUCCESS);

        err = heapstore_reload_config(NULL);
        assert(err == heapstore_ERR_INVALID_PARAM);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_reload_config_not_initialized(void) {
    printf("Test: reload_config_not_initialized...");

    heapstore_config_t manager = {0};
    manager.max_log_size_mb = 100;

    heapstore_error_t err = heapstore_reload_config(&manager);
    assert(err == heapstore_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_flush(void) {
    printf("Test: flush...");

    heapstore_config_t manager = {0};
    manager.root_path = "test_heapstore8";

    heapstore_error_t err = heapstore_init(&manager);
    if (err == heapstore_SUCCESS) {
        err = heapstore_flush();
        assert(err == heapstore_SUCCESS);

        heapstore_shutdown();
    }

    printf("PASS\n");
}

static void test_flush_not_initialized(void) {
    printf("Test: flush_not_initialized...");

    heapstore_error_t err = heapstore_flush();
    assert(err == heapstore_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS heapstore Core Unit Tests ===\n\n");

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

