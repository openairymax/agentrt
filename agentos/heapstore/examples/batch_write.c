/**
 * @file batch_write.c
 * @brief heapstore 批量写入示例
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 本示例展示如何使用 heapstore 的批量写入功能，
 *       包括批量日志、追踪、注册表等操作。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "heapstore.h"

/**
 * @brief 主函数：批量写入示例
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("heapstore Batch Write Example\n");
    printf("========================================\n\n");

    /* 1. 初始化 */
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
    if (err != heapstore_SUCCESS) {
        printf("ERROR: init failed with %d\n", err);
        return 1;
    }
    printf("  SUCCESS: initialized\n\n");

    /* 2. 创建批量上下文 */
    printf("Step 2: Create Batch Context\n");
    heapstore_batch_context_t* ctx = heapstore_batch_begin(1024);
    if (!ctx) {
        printf("ERROR: batch_begin failed\n");
        heapstore_shutdown();
        return 1;
    }
    printf("  SUCCESS: batch context created with capacity %zu\n\n", ctx->capacity);

    /* 3. 批量添加日志 */
    printf("Step 3: Add Logs to Batch\n");
    for (int i = 0; i < 10; i++) {
        char message[128];
        snprintf(message, sizeof(message), "Batch log message #%d at %ld", i, (long)time(NULL));

        err = heapstore_batch_add_log(
            ctx,
            HEAPSTORE_LOG_INFO,
            "batch_example",
            NULL,
            message
        );

        if (err != heapstore_SUCCESS) {
            printf("  WARNING: failed to add log #%d: error %d\n", i, err);
        }
    }
    printf("  Added 10 logs to batch (count=%zu)\n\n", ctx->count);

    /* 4. 批量添加追踪数据 */
    printf("Step 4: Add Trace Spans to Batch\n");
    for (int i = 0; i < 5; i++) {
        heapstore_trace_entry_t entry = {0};

        char trace_id[32], span_id[32];
        snprintf(trace_id, sizeof(trace_id), "trace-batch-%03d", i);
        snprintf(span_id, sizeof(span_id), "span-batch-%03d", i);

        strncpy(entry.trace_id, trace_id, sizeof(entry.trace_id) - 1);
        strncpy(entry.span_id, span_id, sizeof(entry.span_id) - 1);
        strncpy(entry.parent_span_id, "parent-000", sizeof(entry.parent_span_id) - 1);
        strncpy(entry.name, "batch_operation", sizeof(entry.name) - 1);
        strncpy(entry.kind, "internal", sizeof(entry.kind) - 1);

        entry.start_time_us = (uint64_t)(time(NULL) - 100) * 1000000ULL;
        entry.end_time_us = (uint64_t)time(NULL) * 1000000ULL;
        entry.status = 0;

        err = heapstore_batch_add_span(ctx, &entry);
        if (err != heapstore_SUCCESS) {
            printf("  WARNING: failed to add span #%d: error %d\n", i, err);
        }
    }
    printf("  Added 5 spans to batch (count=%zu)\n\n", ctx->count);

    /* 5. 批量添加会话记录 */
    printf("Step 5: Add Session Records to Batch\n");
    for (int i = 0; i < 3; i++) {
        heapstore_session_record_t session = {0};

        char session_id[32];
        snprintf(session_id, sizeof(session_id), "session-batch-%03d", i);

        strncpy(session.session_id, session_id, sizeof(session.session_id) - 1);
        strncpy(session.user_id, "user-001", sizeof(session.user_id) - 1);
        strncpy(session.status, "active", sizeof(session.status) - 1);
        session.created_ns = (uint64_t)(time(NULL) - 3600) * 1000000000ULL;
        session.last_active_ns = (uint64_t)time(NULL) * 1000000000ULL;

        err = heapstore_batch_add_session(ctx, &session);
        if (err != heapstore_SUCCESS) {
            printf("  WARNING: failed to add session #%d: error %d\n", i, err);
        }
    }
    printf("  Added 3 sessions to batch (count=%zu)\n\n", ctx->count);

    /* 6. 提交批量写入 */
    printf("Step 6: Commit Batch\n");
    printf("  Total items before commit: %zu\n", ctx->count);

    err = heapstore_batch_commit(ctx);
    if (err != heapstore_SUCCESS) {
        printf("  WARNING: batch commit completed with error %d\n", err);
    } else {
        printf("  SUCCESS: batch commit completed\n");
    }
    printf("  Total items after commit: %zu\n\n", ctx->count);

    /* 7. 清理 */
    printf("Step 7: Cleanup\n");
    heapstore_batch_destroy(ctx);
    heapstore_cleanup();
    printf("  SUCCESS: cleanup completed\n\n");

    printf("========================================\n");
    printf("Batch Write Example Completed!\n");
    printf("========================================\n");

    return 0;
}
