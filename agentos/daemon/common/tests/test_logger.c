/**
 * @file test_logger.c
 * @brief 日志模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_logger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_logger_level_conversion(void)
{
    SVC_LOG_INFO("  test_logger_level_conversion...");

    assert(strcmp(agentos_log_level_to_string((agentos_log_level_t)LOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(agentos_log_level_to_string((agentos_log_level_t)LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(agentos_log_level_to_string((agentos_log_level_t)LOG_LEVEL_WARN), "WARN") == 0);
    assert(strcmp(agentos_log_level_to_string((agentos_log_level_t)LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(agentos_log_level_to_string((agentos_log_level_t)LOG_LEVEL_FATAL), "FATAL") == 0);

    assert(agentos_log_level_from_string("DEBUG") == (agentos_log_level_t)LOG_LEVEL_DEBUG);
    assert(agentos_log_level_from_string("INFO") == (agentos_log_level_t)LOG_LEVEL_INFO);
    assert(agentos_log_level_from_string("WARN") == (agentos_log_level_t)LOG_LEVEL_WARN);
    assert(agentos_log_level_from_string("ERROR") == (agentos_log_level_t)LOG_LEVEL_ERROR);
    assert(agentos_log_level_from_string("FATAL") == (agentos_log_level_t)LOG_LEVEL_FATAL);

    SVC_LOG_INFO("    PASSED");
}

static void test_logger_init_shutdown(void)
{
    SVC_LOG_INFO("  test_logger_init_shutdown...");

    agentos_logger_config_t config = {.name = "test_agentos",
                                      .level = (int)LOG_LEVEL_DEBUG,
                                      .targets = NULL,
                                      .target_count = 0,
                                      .include_source = true,
                                      .include_trace = true,
                                      .json_format = false};

    int ret = agentos_log_init(&config);
    assert(ret == 0);

    agentos_log_set_level((agentos_log_level_t)LOG_LEVEL_DEBUG);

    agentos_log_shutdown();

    SVC_LOG_INFO("    PASSED");
}

static void test_logger_trace_context(void)
{
    SVC_LOG_INFO("  test_logger_trace_context...");

    agentos_trace_context_t ctx;
    agentos_trace_new(&ctx);

    assert(ctx.trace_id[0] != '\0');
    assert(strlen(ctx.trace_id) > 0);

    agentos_trace_set_current(&ctx);

    const char *current_trace = ctx.trace_id;
    assert(current_trace != NULL);

    agentos_trace_set_session_id("test-session-123");
    const char *session_id = agentos_trace_get_session_id();
    assert(strcmp(session_id, "test-session-123") == 0);

    SVC_LOG_INFO("    PASSED");
}

static void test_logger_macros(void)
{
    SVC_LOG_INFO("  test_logger_macros...");

    agentos_logger_config_t config = {.name = "test_agentos",
                                      .level = (int)LOG_LEVEL_DEBUG,
                                      .targets = NULL,
                                      .target_count = 0,
                                      .include_source = true,
                                      .include_trace = true,
                                      .json_format = false};

    agentos_log_init(&config);

    /* 测试日志宏 */
    LOG_DEBUG("Test debug message: %d", 42);
    LOG_INFO("Test info message");
    LOG_WARN("Test warn message");
    LOG_ERROR("Test error message");

    /* 测试带追踪上下文的日志 */
    agentos_trace_context_t ctx;
    agentos_trace_new(&ctx);
    LOG_INFO_T(&ctx, "Test message with trace context");

    agentos_log_shutdown();

    SVC_LOG_INFO("    PASSED");
}

int main(void)
{
    SVC_LOG_INFO("=========================================");
    SVC_LOG_INFO("  Logger Module Unit Tests");
    SVC_LOG_INFO("=========================================");

    test_logger_level_conversion();
    test_logger_init_shutdown();
    test_logger_trace_context();
    test_logger_macros();

    SVC_LOG_INFO("All logger module tests PASSED");
    return 0;
}
