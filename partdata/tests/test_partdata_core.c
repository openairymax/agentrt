/**
 * @file test_partdata_core.c
 * @brief AgentOS 数据分区核心模块单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "partdata.h"

static void test_init_shutdown(void) {
    printf("Test: init_shutdown...");

    assert(!partdata_is_initialized());

    partdata_config_t config = {0};
    config.root_path = "test_partdata";
    config.enable_auto_cleanup = true;

    partdata_error_t err = partdata_init(&config);
    assert(err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED);

    if (err == PARTDATA_SUCCESS) {
        assert(partdata_is_initialized());
        assert(strcmp(partdata_get_root(), "test_partdata") == 0);
        partdata_shutdown();
        assert(!partdata_is_initialized());
    }

    printf("PASS\n");
}

static void test_init_twice(void) {
    printf("Test: init_twice...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata2";

    partdata_error_t err1 = partdata_init(&config);

    if (err1 == PARTDATA_SUCCESS) {
        err1 = partdata_init(&config);
        assert(err1 == PARTDATA_ERR_ALREADY_INITIALIZED);
        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_get_path(void) {
    printf("Test: get_path...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata3";

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        assert(partdata_get_path(PARTDATA_PATH_KERNEL) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_LOGS) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_REGISTRY) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_SERVICES) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_TRACES) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_KERNEL_IPC) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_KERNEL_MEMORY) != NULL);
        assert(partdata_get_path(PARTDATA_PATH_MAX) == NULL);

        char full_path[256];
        err = partdata_get_full_path(PARTDATA_PATH_LOGS, full_path, sizeof(full_path));
        assert(err == PARTDATA_SUCCESS);
        assert(strstr(full_path, "logs") != NULL);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_get_full_path_invalid(void) {
    printf("Test: get_full_path_invalid...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata4";

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        char buffer[256];
        err = partdata_get_full_path(PARTDATA_PATH_LOGS, NULL, 0);
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        err = partdata_get_full_path(-1, buffer, sizeof(buffer));
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        err = partdata_get_full_path(PARTDATA_PATH_MAX, buffer, sizeof(buffer));
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats(void) {
    printf("Test: get_stats...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata5";

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        partdata_stats_t stats;
        memset(&stats, 0, sizeof(stats));

        err = partdata_get_stats(&stats);
        assert(err == PARTDATA_SUCCESS);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_get_stats_not_initialized(void) {
    printf("Test: get_stats_not_initialized...");

    partdata_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    partdata_error_t err = partdata_get_stats(&stats);
    assert(err == PARTDATA_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_cleanup(void) {
    printf("Test: cleanup...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata6";
    config.enable_auto_cleanup = true;

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        uint64_t freed = 0;
        err = partdata_cleanup(true, &freed);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_cleanup(false, &freed);
        assert(err == PARTDATA_SUCCESS);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_strerror(void) {
    printf("Test: strerror...");

    assert(strcmp(partdata_strerror(PARTDATA_SUCCESS), "Success") == 0);
    assert(strcmp(partdata_strerror(PARTDATA_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(partdata_strerror(PARTDATA_ERR_NOT_INITIALIZED), "Partdata not initialized") == 0);
    assert(strcmp(partdata_strerror(PARTDATA_ERR_ALREADY_INITIALIZED), "Partdata not initialized") != 0);
    assert(strcmp(partdata_strerror(PARTDATA_ERR_DIR_CREATE_FAILED), "Failed to create directory") == 0);
    assert(strcmp(partdata_strerror(PARTDATA_ERR_OUT_OF_MEMORY), "Out of memory") == 0);
    assert(strcmp(partdata_strerror((partdata_error_t)-999), "Unknown error") == 0);

    printf("PASS\n");
}

static void test_reload_config(void) {
    printf("Test: reload_config...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata7";
    config.max_log_size_mb = 50;
    config.log_retention_days = 3;

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        partdata_config_t new_config = {0};
        new_config.max_log_size_mb = 200;
        new_config.log_retention_days = 14;
        new_config.enable_auto_cleanup = false;

        err = partdata_reload_config(&new_config);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_reload_config(NULL);
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_reload_config_not_initialized(void) {
    printf("Test: reload_config_not_initialized...");

    partdata_config_t config = {0};
    config.max_log_size_mb = 100;

    partdata_error_t err = partdata_reload_config(&config);
    assert(err == PARTDATA_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

static void test_flush(void) {
    printf("Test: flush...");

    partdata_config_t config = {0};
    config.root_path = "test_partdata8";

    partdata_error_t err = partdata_init(&config);
    if (err == PARTDATA_SUCCESS) {
        err = partdata_flush();
        assert(err == PARTDATA_SUCCESS);

        partdata_shutdown();
    }

    printf("PASS\n");
}

static void test_flush_not_initialized(void) {
    printf("Test: flush_not_initialized...");

    partdata_error_t err = partdata_flush();
    assert(err == PARTDATA_ERR_NOT_INITIALIZED);

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS Partdata Core Unit Tests ===\n\n");

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
