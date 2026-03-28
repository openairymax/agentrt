/**
 * @file test_lodges_log.c
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

#include "lodges.h"
#include "lodges_log.h"

static void test_log_init_shutdown(void) {
    printf("Test: log_init_shutdown...");

    lodges_error_t err = lodges_log_init();
    assert(err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED);

    if (err == lodges_SUCCESS) {
        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_write(void) {
    printf("Test: log_write...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        lodges_log_set_level(lodges_LOG_DEBUG);

        lodges_LOG_ERROR("test_service", "trace_001", "Test error message");
        lodges_LOG_WARN("test_service", "trace_001", "Test warning message");
        lodges_LOG_INFO("test_service", "trace_001", "Test info message");
        lodges_LOG_DEBUG("test_service", "trace_001", "Test debug message");

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_level_filter(void) {
    printf("Test: log_level_filter...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        lodges_log_set_level(lodges_LOG_ERROR);

        lodges_LOG_ERROR("test_service", NULL, "This should appear");
        lodges_LOG_WARN("test_service", NULL, "This should NOT appear");
        lodges_LOG_INFO("test_service", NULL, "This should NOT appear");
        lodges_LOG_DEBUG("test_service", NULL, "This should NOT appear");

        lodges_log_set_level(lodges_LOG_DEBUG);
        lodges_LOG_DEBUG("test_service", NULL, "This should appear");

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_set_level(void) {
    printf("Test: log_get_set_level...");

    lodges_log_set_level(lodges_LOG_ERROR);
    assert(lodges_log_get_level() == lodges_LOG_ERROR);

    lodges_log_set_level(lodges_LOG_WARN);
    assert(lodges_log_get_level() == lodges_LOG_WARN);

    lodges_log_set_level(lodges_LOG_INFO);
    assert(lodges_log_get_level() == lodges_LOG_INFO);

    lodges_log_set_level(lodges_LOG_DEBUG);
    assert(lodges_log_get_level() == lodges_LOG_DEBUG);

    printf("PASS\n");
}

static void test_log_get_service_path(void) {
    printf("Test: log_get_service_path...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        char path[512];

        err = lodges_log_get_service_path(NULL, path, sizeof(path));
        assert(err == lodges_SUCCESS);
        assert(strstr(path, "agentos.log") != NULL);

        err = lodges_log_get_service_path("test_service", path, sizeof(path));
        assert(err == lodges_SUCCESS);
        assert(strstr(path, "test_service.log") != NULL);

        err = lodges_log_get_service_path(NULL, NULL, 0);
        assert(err == lodges_ERR_INVALID_PARAM);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_rotate(void) {
    printf("Test: log_rotate...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        lodges_LOG_INFO("test_service", NULL, "Message before rotation");

        err = lodges_log_rotate();
        assert(err == lodges_SUCCESS);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_cleanup(void) {
    printf("Test: log_cleanup...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        uint64_t freed = 0;

        err = lodges_log_cleanup(7, &freed);
        assert(err == lodges_SUCCESS);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_file_info(void) {
    printf("Test: log_get_file_info...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        lodges_log_file_info_t info;
        memset(&info, 0, sizeof(info));

        err = lodges_log_get_file_info(NULL, &info);
        assert(err == lodges_SUCCESS);

        err = lodges_log_get_file_info("test_service", &info);
        assert(err == lodges_SUCCESS);

        err = lodges_log_get_file_info(NULL, NULL);
        assert(err == lodges_ERR_INVALID_PARAM);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_stats(void) {
    printf("Test: log_get_stats...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        uint32_t total_files = 0;
        uint64_t total_size = 0;
        time_t oldest = 0;

        err = lodges_log_get_stats(&total_files, &total_size, &oldest);
        assert(err == lodges_SUCCESS);

        err = lodges_log_get_stats(NULL, NULL, NULL);
        assert(err == lodges_ERR_INVALID_PARAM);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_multiple_services(void) {
    printf("Test: log_multiple_services...");

    lodges_error_t err = lodges_log_init();
    if (err == lodges_SUCCESS) {
        lodges_log_set_level(lodges_LOG_DEBUG);

        lodges_LOG_INFO("service_a", "trace_a", "Message from service A");
        lodges_LOG_INFO("service_b", "trace_b", "Message from service B");
        lodges_LOG_INFO("service_c", "trace_c", "Message from service C");

        char path[512];
        err = lodges_log_get_service_path("service_a", path, sizeof(path));
        assert(err == lodges_SUCCESS);

        err = lodges_log_get_service_path("service_b", path, sizeof(path));
        assert(err == lodges_SUCCESS);

        err = lodges_log_get_service_path("service_c", path, sizeof(path));
        assert(err == lodges_SUCCESS);

        lodges_log_shutdown();
    }

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS lodges Log Unit Tests ===\n\n");

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

