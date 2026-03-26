/**
 * @file test_logger.c
 * @brief 日志模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "svc_logger.h"

static void test_logger_level_conversion(void) {
    printf("  test_logger_level_conversion...\n");

    assert(strcmp(svc_logger_level_to_string(LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(svc_logger_level_to_string(LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(svc_logger_level_to_string(LOG_LEVEL_WARN), "WARN") == 0);
    assert(strcmp(svc_logger_level_to_string(LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(svc_logger_level_to_string(LOG_LEVEL_FATAL), "FATAL") == 0);

    assert(svc_logger_string_to_level("DEBUG") == LOG_LEVEL_DEBUG);
    assert(svc_logger_string_to_level("INFO") == LOG_LEVEL_INFO);
    assert(svc_logger_string_to_level("WARN") == LOG_LEVEL_WARN);
    assert(svc_logger_string_to_level("ERROR") == LOG_LEVEL_ERROR);
    assert(svc_logger_string_to_level("FATAL") == LOG_LEVEL_FATAL);
    assert(svc_logger_string_to_level("UNKNOWN") == -1);

    printf("    PASSED\n");
}

static void test_logger_init_shutdown(void) {
    printf("  test_logger_init_shutdown...\n");

    logger_config_t config = {
        .min_level = LOG_LEVEL_DEBUG,
        .targets = LOG_TARGET_CONSOLE,
        .log_dir = NULL,
        .log_prefix = "test_agentos",
        .max_file_size = 1024 * 1024,
        .max_backup_files = 3,
        .async_mode = 0,
        .use_colors = 0
    };

    int ret = svc_logger_init(&config);
    assert(ret == 0);

    svc_logger_set_level(LOG_LEVEL_DEBUG);
    assert(svc_logger_get_level() == LOG_LEVEL_DEBUG);

    svc_logger_shutdown();

    printf("    PASSED\n");
}

static void test_logger_stats(void) {
    printf("  test_logger_stats...\n");

    logger_config_t config = {
        .min_level = LOG_LEVEL_DEBUG,
        .targets = LOG_TARGET_CONSOLE,
        .log_dir = NULL,
        .log_prefix = "test_agentos",
        .max_file_size = 1024 * 1024,
        .max_backup_files = 3,
        .async_mode = 0,
        .use_colors = 0
    };

    svc_logger_init(&config);
    svc_logger_reset_stats();

    logger_stats_t stats;
    svc_logger_get_stats(&stats);

    assert(stats.total_messages >= 0);
    svc_logger_shutdown();

    printf("    PASSED\n");
}

static void test_logger_trace_id(void) {
    printf("  test_logger_trace_id...\n");

    logger_config_t config = {
        .min_level = LOG_LEVEL_DEBUG,
        .targets = LOG_TARGET_CONSOLE,
        .log_dir = NULL,
        .log_prefix = "test_agentos",
        .max_file_size = 1024 * 1024,
        .max_backup_files = 3,
        .async_mode = 0,
        .use_colors = 0
    };

    svc_logger_init(&config);

    char* trace_id = svc_logger_gen_trace_id();
    assert(trace_id != NULL);
    assert(strlen(trace_id) > 0);

    svc_logger_set_trace_id(trace_id);
    const char* current_trace = svc_logger_get_trace_id();
    assert(current_trace != NULL);

    free(trace_id);
    svc_logger_shutdown();

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Logger Module Unit Tests\n");
    printf("=========================================\n");

    test_logger_level_conversion();
    test_logger_init_shutdown();
    test_logger_stats();
    test_logger_trace_id();

    printf("\n✅ All logger module tests PASSED\n");
    return 0;
}