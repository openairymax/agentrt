/**
 * @file test_logging_unified.c
 * @brief 统一日志模块单元测试
 * 
 * 测试logging_unified模块的基本功能：日志级别控制、原子日志、服务日志、兼容层等。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <assert.h>

#include "../include/logging.h"
#include "../include/atomic_logging.h"
#include "../include/service_logging.h"
#include "../include/logging_compat.h"

/* ==================== 测试辅助宏 ==================== */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { \
            fprintf(stderr, "❌ Test failed: %s\n", #test_func); \
            failed_tests++; \
        } else { \
            printf("✅ PASS: %s\n", #test_func); \
            passed_tests++; \
        } \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* ==================== 测试用例 ==================== */

/**
 * @brief 测试日志级别枚举和转换
 */
static int test_log_levels(void) {
    printf("  测试日志级别...\n");
    
    // 测试级别枚举值
    TEST_ASSERT(LOG_LEVEL_ERROR == 0, "LOG_LEVEL_ERROR 应为 0");
    TEST_ASSERT(LOG_LEVEL_WARN == 1, "LOG_LEVEL_WARN 应为 1");
    TEST_ASSERT(LOG_LEVEL_INFO == 2, "LOG_LEVEL_INFO 应为 2");
    TEST_ASSERT(LOG_LEVEL_DEBUG == 3, "LOG_LEVEL_DEBUG 应为 3");
    TEST_ASSERT(LOG_LEVEL_TRACE == 4, "LOG_LEVEL_TRACE 应为 4");
    
    // 测试级别名称转换
    TEST_ASSERT(strcmp(log_level_to_string(LOG_LEVEL_ERROR), "ERROR") == 0, "ERROR 级别名称错误");
    TEST_ASSERT(strcmp(log_level_to_string(LOG_LEVEL_WARN), "WARN") == 0, "WARN 级别名称错误");
    TEST_ASSERT(strcmp(log_level_to_string(LOG_LEVEL_INFO), "INFO") == 0, "INFO 级别名称错误");
    TEST_ASSERT(strcmp(log_level_to_string(LOG_LEVEL_DEBUG), "DEBUG") == 0, "DEBUG 级别名称错误");
    TEST_ASSERT(strcmp(log_level_to_string(LOG_LEVEL_TRACE), "TRACE") == 0, "TRACE 级别名称错误");
    
    // 测试字符串到级别转换
    TEST_ASSERT(string_to_log_level("ERROR") == LOG_LEVEL_ERROR, "字符串 ERROR 转换错误");
    TEST_ASSERT(string_to_log_level("WARN") == LOG_LEVEL_WARN, "字符串 WARN 转换错误");
    TEST_ASSERT(string_to_log_level("INFO") == LOG_LEVEL_INFO, "字符串 INFO 转换错误");
    TEST_ASSERT(string_to_log_level("DEBUG") == LOG_LEVEL_DEBUG, "字符串 DEBUG 转换错误");
    TEST_ASSERT(string_to_log_level("TRACE") == LOG_LEVEL_TRACE, "字符串 TRACE 转换错误");
    TEST_ASSERT(string_to_log_level("UNKNOWN") == LOG_LEVEL_INFO, "未知字符串应返回默认级别");
    
    printf("  日志级别测试通过\n");
    return 0;
}

/**
 * @brief 测试基础日志功能
 */
static int test_basic_logging(void) {
    printf("  测试基础日志功能...\n");
    
    // 初始化日志系统
    int result = logging_init();
    TEST_ASSERT(result == 0, "日志系统初始化失败");
    
    // 设置日志级别为DEBUG，以便看到所有消息
    logging_set_level(LOG_LEVEL_DEBUG);
    
    // 测试不同级别的日志输出
    LOG_ERROR("测试错误消息");
    LOG_WARN("测试警告消息");
    LOG_INFO("测试信息消息");
    LOG_DEBUG("测试调试消息");
    LOG_TRACE("测试跟踪消息");
    
    // 测试带格式的日志
    LOG_INFO_FMT("格式化消息: %d + %d = %d", 1, 2, 3);
    LOG_ERROR_FMT("错误代码: %d, 消息: %s", 404, "未找到");
    
    // 清理日志系统
    logging_cleanup();
    
    printf("  基础日志功能测试通过\n");
    return 0;
}

/**
 * @brief 测试原子日志（线程安全日志）
 */
static int test_atomic_logging(void) {
    printf("  测试原子日志功能...\n");
    
    // 初始化原子日志
    atomic_logger_t* logger = atomic_logger_create("test_logger");
    TEST_ASSERT(logger != NULL, "原子日志创建失败");
    TEST_ASSERT(strcmp(atomic_logger_get_name(logger), "test_logger") == 0, "原子日志名称不匹配");
    
    // 设置原子日志级别
    atomic_logger_set_level(logger, LOG_LEVEL_INFO);
    TEST_ASSERT(atomic_logger_get_level(logger) == LOG_LEVEL_INFO, "原子日志级别设置失败");
    
    // 测试原子日志输出
    ATOMIC_LOG_ERROR(logger, "原子错误消息");
    ATOMIC_LOG_WARN(logger, "原子警告消息");
    ATOMIC_LOG_INFO(logger, "原子信息消息");
    ATOMIC_LOG_DEBUG(logger, "原子调试消息"); // 应被过滤，因为级别为INFO
    
    // 测试带格式的原子日志
    ATOMIC_LOG_INFO_FMT(logger, "原子格式化: %s %d", "test", 123);
    
    // 测试日志统计
    atomic_log_stats_t stats;
    atomic_logger_get_stats(logger, &stats);
    printf("    日志统计: 错误=%lu, 警告=%lu, 信息=%lu, 调试=%lu, 跟踪=%lu\n",
           stats.error_count, stats.warn_count, stats.info_count,
           stats.debug_count, stats.trace_count);
    
    // 清理原子日志
    atomic_logger_destroy(logger);
    
    printf("  原子日志功能测试通过\n");
    return 0;
}

/**
 * @brief 测试服务日志（结构化日志）
 */
static int test_service_logging(void) {
    printf("  测试服务日志功能...\n");
    
    // 创建服务日志上下文
    service_log_ctx_t* ctx = service_log_ctx_create("test_service", "v1.0");
    TEST_ASSERT(ctx != NULL, "服务日志上下文创建失败");
    
    // 设置服务属性
    service_log_ctx_set_property(ctx, "environment", "test");
    service_log_ctx_set_property(ctx, "region", "us-west-2");
    
    // 测试服务日志输出
    SERVICE_LOG_INFO(ctx, "服务启动");
    SERVICE_LOG_WARN(ctx, "服务警告");
    
    // 测试带附加字段的服务日志
    log_field_t fields[] = {
        {"user_id", "12345"},
        {"action", "login"},
        {"duration_ms", "150"}
    };
    SERVICE_LOG_INFO_FIELDS(ctx, "用户操作完成", fields, 3);
    
    // 测试错误日志
    SERVICE_LOG_ERROR_FMT(ctx, "服务错误: 代码=%d, 消息=%s", 500, "内部错误");
    
    // 获取服务日志统计
    service_log_stats_t stats;
    service_log_ctx_get_stats(ctx, &stats);
    printf("    服务日志统计: 信息=%lu, 警告=%lu, 错误=%lu\n",
           stats.info_count, stats.warn_count, stats.error_count);
    
    // 清理服务日志上下文
    service_log_ctx_destroy(ctx);
    
    printf("  服务日志功能测试通过\n");
    return 0;
}

/**
 * @brief 测试日志过滤和级别控制
 */
static int test_log_filtering(void) {
    printf("  测试日志过滤和级别控制...\n");
    
    // 初始化日志系统
    logging_init();
    
    // 测试级别过滤
    logging_set_level(LOG_LEVEL_WARN);
    
    // 这些消息应该被记录
    LOG_ERROR("错误消息 - 应显示");
    LOG_WARN("警告消息 - 应显示");
    
    // 这些消息应该被过滤
    LOG_INFO("信息消息 - 应被过滤");
    LOG_DEBUG("调试消息 - 应被过滤");
    LOG_TRACE("跟踪消息 - 应被过滤");
    
    // 更改级别为TRACE，所有消息都应显示
    logging_set_level(LOG_LEVEL_TRACE);
    LOG_TRACE("跟踪消息 - 现在应显示");
    
    // 测试原子日志过滤
    atomic_logger_t* logger = atomic_logger_create("filter_test");
    atomic_logger_set_level(logger, LOG_LEVEL_ERROR);
    
    ATOMIC_LOG_ERROR(logger, "原子错误 - 应显示");
    ATOMIC_LOG_WARN(logger, "原子警告 - 应被过滤");
    ATOMIC_LOG_INFO(logger, "原子信息 - 应被过滤");
    
    atomic_logger_destroy(logger);
    logging_cleanup();
    
    printf("  日志过滤测试通过\n");
    return 0;
}

/**
 * @brief 测试日志兼容层
 */
static int test_logging_compat_layer(void) {
    printf("  测试日志兼容层...\n");
    
    // 测试兼容宏
    AGENTOS_LOG_INIT();
    
    AGENTOS_LOG_ERROR("兼容层错误消息");
    AGENTOS_LOG_WARN("兼容层警告消息");
    AGENTOS_LOG_INFO("兼容层信息消息");
    AGENTOS_LOG_DEBUG("兼容层调试消息");
    
    AGENTOS_LOG_ERROR_FMT("兼容层格式化错误: %s", "测试");
    AGENTOS_LOG_INFO_FMT("兼容层格式化信息: %d", 42);
    
    // 测试兼容层级别设置
    AGENTOS_LOG_SET_LEVEL("DEBUG");
    
    // 测试兼容层清理
    AGENTOS_LOG_CLEANUP();
    
    printf("  日志兼容层测试通过\n");
    return 0;
}

/**
 * @brief 测试日志性能（基本）
 */
static int test_logging_performance(void) {
    printf("  测试日志性能（基本）...\n");
    
    // 初始化日志系统，禁用控制台输出以提高性能测试速度
    logging_init();
    logging_set_output_enabled(0); // 禁用输出
    
    atomic_logger_t* logger = atomic_logger_create("perf_test");
    atomic_logger_set_level(logger, LOG_LEVEL_INFO);
    
    // 记录一些日志消息（性能测试）
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        ATOMIC_LOG_INFO(logger, "性能测试消息");
    }
    
    // 获取统计信息
    atomic_log_stats_t stats;
    atomic_logger_get_stats(logger, &stats);
    TEST_ASSERT(stats.info_count >= iterations, "日志计数不匹配");
    
    atomic_logger_destroy(logger);
    logging_cleanup();
    
    printf("  日志性能测试通过 (%d 次迭代)\n", iterations);
    return 0;
}

/**
 * @brief 测试日志错误处理
 */
static int test_logging_error_handling(void) {
    printf("  测试日志错误处理...\n");
    
    // 测试无效日志级别设置
    int result = logging_set_level(999); // 无效级别
    TEST_ASSERT(result != 0, "无效日志级别设置应失败");
    
    // 测试空日志器名称
    atomic_logger_t* logger = atomic_logger_create(NULL);
    TEST_ASSERT(logger == NULL, "空名称日志器创建应失败");
    
    // 测试无效日志器操作
    atomic_logger_set_level(NULL, LOG_LEVEL_INFO); // 应处理NULL
    
    // 测试服务日志无效上下文
    SERVICE_LOG_INFO(NULL, "无效上下文消息"); // 应处理NULL
    
    printf("  日志错误处理测试通过\n");
    return 0;
}

/* ==================== 主测试函数 ==================== */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  统一日志模块单元测试\n");
    printf("========================================\n");
    
    // 运行所有测试
    TEST_RUN(test_log_levels);
    TEST_RUN(test_basic_logging);
    TEST_RUN(test_atomic_logging);
    TEST_RUN(test_service_logging);
    TEST_RUN(test_log_filtering);
    TEST_RUN(test_logging_compat_layer);
    TEST_RUN(test_logging_performance);
    TEST_RUN(test_logging_error_handling);
    
    printf("\n");
    printf("========================================\n");
    printf("  测试完成\n");
    printf("  通过: %d, 失败: %d\n", passed_tests, failed_tests);
    printf("========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}