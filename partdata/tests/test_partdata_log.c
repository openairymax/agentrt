/**
 * @file test_partdata_log.c
 * @brief AgentOS 数据分区日志管理单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "partdata.h"
#include "partdata_log.h"

static void test_log_init_shutdown(void) {
    printf("Test: log_init_shutdown...");

    partdata_error_t err = partdata_log_init();
    assert(err == PARTDATA_SUCCESS || err == PARTDATA_ERR_ALREADY_INITIALIZED);

    if (err == PARTDATA_SUCCESS) {
        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_write(void) {
    printf("Test: log_write...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        partdata_log_set_level(PARTDATA_LOG_DEBUG);

        PARTDATA_LOG_ERROR("test_service", "trace_001", "Test error message");
        PARTDATA_LOG_WARN("test_service", "trace_001", "Test warning message");
        PARTDATA_LOG_INFO("test_service", "trace_001", "Test info message");
        PARTDATA_LOG_DEBUG("test_service", "trace_001", "Test debug message");

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_level_filter(void) {
    printf("Test: log_level_filter...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        partdata_log_set_level(PARTDATA_LOG_ERROR);

        PARTDATA_LOG_ERROR("test_service", NULL, "This should appear");
        PARTDATA_LOG_WARN("test_service", NULL, "This should NOT appear");
        PARTDATA_LOG_INFO("test_service", NULL, "This should NOT appear");
        PARTDATA_LOG_DEBUG("test_service", NULL, "This should NOT appear");

        partdata_log_set_level(PARTDATA_LOG_DEBUG);
        PARTDATA_LOG_DEBUG("test_service", NULL, "This should appear");

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_set_level(void) {
    printf("Test: log_get_set_level...");

    partdata_log_set_level(PARTDATA_LOG_ERROR);
    assert(partdata_log_get_level() == PARTDATA_LOG_ERROR);

    partdata_log_set_level(PARTDATA_LOG_WARN);
    assert(partdata_log_get_level() == PARTDATA_LOG_WARN);

    partdata_log_set_level(PARTDATA_LOG_INFO);
    assert(partdata_log_get_level() == PARTDATA_LOG_INFO);

    partdata_log_set_level(PARTDATA_LOG_DEBUG);
    assert(partdata_log_get_level() == PARTDATA_LOG_DEBUG);

    printf("PASS\n");
}

static void test_log_get_service_path(void) {
    printf("Test: log_get_service_path...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        char path[512];

        err = partdata_log_get_service_path(NULL, path, sizeof(path));
        assert(err == PARTDATA_SUCCESS);
        assert(strstr(path, "agentos.log") != NULL);

        err = partdata_log_get_service_path("test_service", path, sizeof(path));
        assert(err == PARTDATA_SUCCESS);
        assert(strstr(path, "test_service.log") != NULL);

        err = partdata_log_get_service_path(NULL, NULL, 0);
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_rotate(void) {
    printf("Test: log_rotate...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        PARTDATA_LOG_INFO("test_service", NULL, "Message before rotation");

        err = partdata_log_rotate();
        assert(err == PARTDATA_SUCCESS);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_cleanup(void) {
    printf("Test: log_cleanup...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        uint64_t freed = 0;

        err = partdata_log_cleanup(7, &freed);
        assert(err == PARTDATA_SUCCESS);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_file_info(void) {
    printf("Test: log_get_file_info...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        partdata_log_file_info_t info;
        memset(&info, 0, sizeof(info));

        err = partdata_log_get_file_info(NULL, &info);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_log_get_file_info("test_service", &info);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_log_get_file_info(NULL, NULL);
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_stats(void) {
    printf("Test: log_get_stats...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        uint32_t total_files = 0;
        uint64_t total_size = 0;
        time_t oldest = 0;

        err = partdata_log_get_stats(&total_files, &total_size, &oldest);
        assert(err == PARTDATA_SUCCESS);

        err = partdata_log_get_stats(NULL, NULL, NULL);
        assert(err == PARTDATA_ERR_INVALID_PARAM);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_multiple_services(void) {
    printf("Test: log_multiple_services...");

    partdata_error_t err = partdata_log_init();
    if (err == PARTDATA_SUCCESS) {
        partdata_log_set_level(PARTDATA_LOG_DEBUG);

        PARTDATA_LOG_INFO("service_a", "trace_a", "Message from service A");
        PARTDATA_LOG_INFO("service_b", "trace_b", "Message from service B");
        PARTDATA_LOG_INFO("service_c", "trace_c", "Message from service C");

        char path[512];
        err = partdata_log_get_service_path("service_a", path, sizeof(path));
        assert(err == PARTDATA_SUCCESS);

        err = partdata_log_get_service_path("service_b", path, sizeof(path));
        assert(err == PARTDATA_SUCCESS);

        err = partdata_log_get_service_path("service_c", path, sizeof(path));
        assert(err == PARTDATA_SUCCESS);

        partdata_log_shutdown();
    }

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS Partdata Log Unit Tests ===\n\n");

    test_log_init_shutdown();
    test_log_write();
    test_log_level_filter();
    test_log_get_set_level();
    test_log_get_service_path();
    test_log_rotate();
    test_log_cleanup();
    test_log_get_file_info();
    test_log_get_stats();
    test_log_multiple_services();

    printf("\n=== All Log Tests Passed ===\n");
    return 0;
}
