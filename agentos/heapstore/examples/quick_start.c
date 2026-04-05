/**
 * @file quick_start.c
 * @brief heapstore 快速入门示例
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 本示例展示 heapstore 的基本使用方法，
 *       适合首次使用 heapstore 的开发者快速上手。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heapstore.h"
#include "heapstore_token.h"

/**
 * @brief 错误处理辅助函数
 */
static void check_error(heapstore_error_t err, const char* context) {
    if (err != heapstore_SUCCESS) {
        printf("ERROR: %s failed with error code %d\n", context, err);
        exit(1);
    }
    printf("SUCCESS: %s\n", context);
}

/**
 * @brief 打印内存池统计信息
 */
static void print_memory_stats(void) {
    heapstore_memory_stats_t stats;
    heapstore_error_t err = heapstore_memory_get_stats(&stats);
    if (err == heapstore_SUCCESS) {
        printf("  - Active pools: %zu\n", stats.active_pools);
        printf("  - Total allocations: %zu\n", stats.total_allocations);
        printf("  - Peak concurrent: %zu\n", stats.peak_concurrent_allocations);
    }
}

/**
 * @brief 主函数：快速入门示例
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("heapstore Quick Start Example\n");
    printf("========================================\n\n");

    /* 1. 初始化 heapstore */
    printf("Step 1: Initialize heapstore\n");
    heapstore_config_t config = {
        .root_path = "./heapstore_data",
        .max_log_size_mb = 100,
        .log_retention_days = 7,
        .trace_retention_days = 3,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true,
        .enable_trace_export = true,
        .db_vacuum_interval_days = 7,
        .circuit_breaker_threshold = 5,
        .circuit_breaker_timeout_sec = 30
    };

    heapstore_error_t err = heapstore_init(&config);
    check_error(err, "heapstore_init");
    printf("\n");

    /* 2. 初始化 Token 计数器 */
    printf("Step 2: Initialize Token Counter\n");
    err = heapstore_token_init();
    check_error(err, "heapstore_token_init");
    printf("\n");

    /* 3. 记录 Token 使用 */
    printf("Step 3: Record Token Usage\n");
    err = heapstore_token_record(HEAPSTORE_TOKEN_TYPE_PROMPT, 150, HEAPSTORE_TOKEN_OP_WRITE);
    check_error(err, "record prompt tokens");
    err = heapstore_token_record(HEAPSTORE_TOKEN_TYPE_COMPLETION, 80, HEAPSTORE_TOKEN_OP_WRITE);
    check_error(err, "record completion tokens");
    printf("\n");

    /* 4. 获取 Token 统计 */
    printf("Step 4: Get Token Statistics\n");
    heapstore_token_stats_t token_stats;
    err = heapstore_token_get_stats(&token_stats);
    if (err == heapstore_SUCCESS) {
        printf("  - Total Prompt Tokens: %lu\n", (unsigned long)token_stats.total_prompt_tokens);
        printf("  - Total Completion Tokens: %lu\n", (unsigned long)token_stats.total_completion_tokens);
        printf("  - Write Operations: %lu\n", (unsigned long)token_stats.total_write_operations);
        printf("  - Cache Saved Tokens: %lu\n", (unsigned long)token_stats.tokens_saved_by_cache);
    }
    printf("\n");

    /* 5. 写入日志 */
    printf("Step 5: Write Log\n");
    err = heapstore_log_write(
        HEAPSTORE_LOG_INFO,
        "quick_start_example",
        NULL,
        NULL, 0,
        "Hello from heapstore quick start!"
    );
    check_error(err, "heapstore_log_write");
    printf("\n");

    /* 6. 写入追踪数据 */
    printf("Step 6: Write Trace Span\n");
    heapstore_span_t span = {0};
    strncpy(span.trace_id, "trace-001", sizeof(span.trace_id) - 1);
    strncpy(span.span_id, "span-001", sizeof(span.span_id) - 1);
    strncpy(span.name, "quick_start_example", sizeof(span.name) - 1);
    strncpy(span.kind, "internal", sizeof(span.kind) - 1);
    span.start_time_ns = 1000000000ULL;
    span.end_time_ns = 1001000000ULL;
    strncpy(span.status, "OK", sizeof(span.status) - 1);

    err = heapstore_trace_write_span(&span);
    check_error(err, "heapstore_trace_write_span");
    printf("\n");

    /* 7. 获取内存池统计 */
    printf("Step 7: Get Memory Pool Statistics\n");
    print_memory_stats();
    printf("\n");

    /* 8. 获取堆存储统计 */
    printf("Step 8: Get Heapstore Statistics\n");
    heapstore_stats_t stats;
    err = heapstore_get_stats(&stats);
    if (err == heapstore_SUCCESS) {
        printf("  - Total Operations: %lu\n", (unsigned long)stats.total_operations);
        printf("  - Failed Operations: %lu\n", (unsigned long)stats.failed_operations);
        printf("  - Success Rate: %.2f%%\n", stats.success_rate);
    }
    printf("\n");

    /* 9. 健康检查 */
    printf("Step 9: Health Check\n");
    heapstore_health_status_t health;
    err = heapstore_health_check(&health);
    if (err == heapstore_SUCCESS) {
        printf("  - Overall Status: %s\n", health.is_healthy ? "HEALTHY" : "UNHEALTHY");
        printf("  - Subsystems: %d/%d healthy\n", health.healthy_subsystems, health.total_subsystems);
    }
    printf("\n");

    /* 10. 清理资源 */
    printf("Step 10: Cleanup\n");
    heapstore_token_shutdown();
    heapstore_cleanup();
    printf("SUCCESS: cleanup completed\n\n");

    printf("========================================\n");
    printf("Quick Start Example Completed Successfully!\n");
    printf("========================================\n");

    return 0;
}
