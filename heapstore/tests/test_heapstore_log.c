/**
 * @file test_heapstore_log.c
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

#include "heapstore.h"
#include "heapstore_log.h"

static void test_log_init_shutdown(void) {
    printf("Test: log_init_shutdown...");

    heapstore_error_t err = heapstore_log_init();
    assert(err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED);

    if (err == heapstore_SUCCESS) {
        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_write(void) {
    printf("Test: log_write...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        heapstore_log_set_level(heapstore_LOG_DEBUG);

        heapstore_LOG_ERROR("test_service", "trace_001", "Test error message");
        heapstore_LOG_WARN("test_service", "trace_001", "Test warning message");
        heapstore_LOG_INFO("test_service", "trace_001", "Test info message");
        heapstore_LOG_DEBUG("test_service", "trace_001", "Test debug message");

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_level_filter(void) {
    printf("Test: log_level_filter...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        heapstore_log_set_level(heapstore_LOG_ERROR);

        heapstore_LOG_ERROR("test_service", NULL, "This should appear");
        heapstore_LOG_WARN("test_service", NULL, "This should NOT appear");
        heapstore_LOG_INFO("test_service", NULL, "This should NOT appear");
        heapstore_LOG_DEBUG("test_service", NULL, "This should NOT appear");

        heapstore_log_set_level(heapstore_LOG_DEBUG);
        heapstore_LOG_DEBUG("test_service", NULL, "This should appear");

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_set_level(void) {
    printf("Test: log_get_set_level...");

    heapstore_log_set_level(heapstore_LOG_ERROR);
    assert(heapstore_log_get_level() == heapstore_LOG_ERROR);

    heapstore_log_set_level(heapstore_LOG_WARN);
    assert(heapstore_log_get_level() == heapstore_LOG_WARN);

    heapstore_log_set_level(heapstore_LOG_INFO);
    assert(heapstore_log_get_level() == heapstore_LOG_INFO);

    heapstore_log_set_level(heapstore_LOG_DEBUG);
    assert(heapstore_log_get_level() == heapstore_LOG_DEBUG);

    printf("PASS\n");
}

static void test_log_get_service_path(void) {
    printf("Test: log_get_service_path...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        char path[512];

        err = heapstore_log_get_service_path(NULL, path, sizeof(path));
        assert(err == heapstore_SUCCESS);
        assert(strstr(path, "agentos.log") != NULL);

        err = heapstore_log_get_service_path("test_service", path, sizeof(path));
        assert(err == heapstore_SUCCESS);
        assert(strstr(path, "test_service.log") != NULL);

        err = heapstore_log_get_service_path(NULL, NULL, 0);
        assert(err == heapstore_ERR_INVALID_PARAM);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_rotate(void) {
    printf("Test: log_rotate...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        heapstore_LOG_INFO("test_service", NULL, "Message before rotation");

        err = heapstore_log_rotate();
        assert(err == heapstore_SUCCESS);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_cleanup(void) {
    printf("Test: log_cleanup...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        uint64_t freed = 0;

        err = heapstore_log_cleanup(7, &freed);
        assert(err == heapstore_SUCCESS);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_file_info(void) {
    printf("Test: log_get_file_info...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        heapstore_log_file_info_t info;
        memset(&info, 0, sizeof(info));

        err = heapstore_log_get_file_info(NULL, &info);
        assert(err == heapstore_SUCCESS);

        err = heapstore_log_get_file_info("test_service", &info);
        assert(err == heapstore_SUCCESS);

        err = heapstore_log_get_file_info(NULL, NULL);
        assert(err == heapstore_ERR_INVALID_PARAM);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_get_stats(void) {
    printf("Test: log_get_stats...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        uint32_t total_files = 0;
        uint64_t total_size = 0;
        time_t oldest = 0;

        err = heapstore_log_get_stats(&total_files, &total_size, &oldest);
        assert(err == heapstore_SUCCESS);

        err = heapstore_log_get_stats(NULL, NULL, NULL);
        assert(err == heapstore_ERR_INVALID_PARAM);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

static void test_log_multiple_services(void) {
    printf("Test: log_multiple_services...");

    heapstore_error_t err = heapstore_log_init();
    if (err == heapstore_SUCCESS) {
        heapstore_log_set_level(heapstore_LOG_DEBUG);

        heapstore_LOG_INFO("service_a", "trace_a", "Message from service A");
        heapstore_LOG_INFO("service_b", "trace_b", "Message from service B");
        heapstore_LOG_INFO("service_c", "trace_c", "Message from service C");

        char path[512];
        err = heapstore_log_get_service_path("service_a", path, sizeof(path));
        assert(err == heapstore_SUCCESS);

        err = heapstore_log_get_service_path("service_b", path, sizeof(path));
        assert(err == heapstore_SUCCESS);

        err = heapstore_log_get_service_path("service_c", path, sizeof(path));
        assert(err == heapstore_SUCCESS);

        heapstore_log_shutdown();
    }

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS heapstore Log Unit Tests ===\n\n");

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

