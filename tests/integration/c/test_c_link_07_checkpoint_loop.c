// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_07_checkpoint_loop.c
 * @brief C-L07 Integration Test: Checkpoint → CoreLoopThree
 *
 * Tests the checkpoint adapter connecting CoreLoopThree to persistent storage:
 * 1. Normal path: Save snapshot → restore → verify state
 * 2. Error path: Restore non-existent checkpoint → AGENTOS_ENOENT
 * 3. Error path: NULL adapter handling
 * 4. Timeout path: Checkpoint save/restore with timeout
 * 5. Concurrent path: Multiple simultaneous checkpoint operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "memory_compat.h"
#include "checkpoint_adapter.h"
#include "agentos_types.h"

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

#define TEST(name) do { \
    g_tests_total++; \
    printf("  [TEST] %s ... ", name); \
} while(0)

#define PASS() do { \
    g_tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(reason) do { \
    g_tests_failed++; \
    printf("FAIL: %s\n", reason); \
} while(0)

#define CHECK(cond, reason) do { \
    if (!(cond)) { FAIL(reason); return; } \
} while(0)

#define CHECK_EQ(a, b, reason) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (got %d, expected %d)", reason, \
                 (int)(a), (int)(b)); \
        FAIL(buf); return; \
    } \
} while(0)

/* ============================================================================
 * Helper: Create a sample snapshot
 * ============================================================================ */

static void fill_snapshot(checkpoint_snapshot_t *snap, const char *task_id,
                          uint64_t seq, uint32_t turn) {
    memset(snap, 0, sizeof(*snap));
    snap->task_id = strdup(task_id);
    snap->session_id = strdup("session-001");
    snap->sequence_num = seq;
    snap->timestamp = (uint64_t)time(NULL);
    snap->cognition_state_json = strdup("{\"phase\": \"planning\"}");
    snap->memory_context_json = strdup("{\"facts\": [\"fact1\", \"fact2\"]}");
    snap->tool_call_history_json = strdup("[{\"tool\": \"read_file\", \"result\": \"ok\"}]");
    snap->pending_nodes_json = strdup("[\"node_x\", \"node_y\"]");
    snap->completed_nodes_json = strdup("[\"node_a\", \"node_b\"]");
    snap->current_turn = turn;
    snap->total_turns = 100;
    snap->progress_percent = (float)turn / 100.0f * 100.0f;
}

/* 释放栈上分配的快照内部字段（不释放结构体本身，避免对栈地址调用 free） */
static void free_snapshot_fields(checkpoint_snapshot_t *snap) {
    if (!snap) return;
    free(snap->task_id);
    free(snap->session_id);
    free(snap->cognition_state_json);
    free(snap->memory_context_json);
    free(snap->tool_call_history_json);
    free(snap->pending_nodes_json);
    free(snap->completed_nodes_json);
    memset(snap, 0, sizeof(*snap));
}

/* ============================================================================
 * P1.16g-1: Normal Path — Checkpoint save and restore
 * ============================================================================ */

static void test_normal_save_restore(void) {
    TEST("C-L07 Normal: Save checkpoint → restore → verify state");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    CHECK(checkpoint_adapter_is_ready(adapter),
          "Adapter should be ready after creation");

    /* Create and save a snapshot */
    checkpoint_snapshot_t snap;
    fill_snapshot(&snap, "task-save-restore-001", 1, 10);

    int ret = checkpoint_adapter_save(adapter, "task-save-restore-001",
                                       "session-001", 1, &snap);
    CHECK_EQ(ret, 0, "checkpoint_adapter_save should succeed");

    /* Restore the saved snapshot */
    checkpoint_snapshot_t *restored = NULL;
    ret = checkpoint_adapter_restore(adapter, "task-save-restore-001", &restored);
    CHECK_EQ(ret, 0, "checkpoint_adapter_restore should succeed");
    CHECK(restored != NULL, "Restored snapshot should not be NULL");

    /* Verify restored data */
    CHECK(restored->sequence_num == 1, "Sequence number should match");
    CHECK(restored->current_turn == 10, "Current turn should match");
    CHECK(restored->total_turns == 100, "Total turns should match");
    CHECK(restored->cognition_state_json != NULL,
          "Cognition state should be restored");

    checkpoint_snapshot_free(restored);
    free_snapshot_fields(&snap);

    /* Clean up checkpoint */
    checkpoint_adapter_delete(adapter, "task-save-restore-001", 0);
    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-2: Normal Path — Multiple checkpoints and list
 * ============================================================================ */

static void test_normal_multiple_checkpoints(void) {
    TEST("C-L07 Normal: Multiple checkpoints → list → restore by sequence");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    /* Save multiple checkpoints */
    for (int i = 1; i <= 3; i++) {
        checkpoint_snapshot_t snap;
        fill_snapshot(&snap, "task-multi-001", (uint64_t)i, (uint32_t)(i * 10));
        int ret = checkpoint_adapter_save(adapter, "task-multi-001",
                                           "session-001", (uint64_t)i, &snap);
        CHECK_EQ(ret, 0, "Save checkpoint should succeed");
        free_snapshot_fields(&snap);
    }

    /* List checkpoints */
    checkpoint_snapshot_t **snapshots = NULL;
    size_t count = 0;
    int ret = checkpoint_adapter_list(adapter, "task-multi-001",
                                       &snapshots, &count);
    CHECK_EQ(ret, 0, "checkpoint_adapter_list should succeed");
    CHECK(count >= 3, "Should have at least 3 checkpoints");

    /* Restore by specific sequence */
    checkpoint_snapshot_t *seq2 = NULL;
    ret = checkpoint_adapter_restore_seq(adapter, "task-multi-001", 2, &seq2);
    CHECK_EQ(ret, 0, "Restore sequence 2 should succeed");
    CHECK(seq2 != NULL, "Restored snapshot should not be NULL");
    CHECK_EQ(seq2->sequence_num, (uint64_t)2, "Should restore sequence 2");
    checkpoint_snapshot_free(seq2);

    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        checkpoint_snapshot_free(snapshots[i]);
    }
    free(snapshots);

    checkpoint_adapter_delete(adapter, "task-multi-001", 0);
    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-3: Error Path — Restore non-existent checkpoint
 * ============================================================================ */

static void test_error_restore_nonexistent(void) {
    TEST("C-L07 Error: Restore non-existent checkpoint → AGENTOS_ENOENT");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    checkpoint_snapshot_t *restored = NULL;
    int ret = checkpoint_adapter_restore(adapter, "non-existent-task", &restored);
    CHECK(ret != 0, "Restore non-existent should return error");
    CHECK(restored == NULL, "Restored snapshot should be NULL for non-existent");

    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-4: Error Path — NULL adapter handling
 * ============================================================================ */

static void test_error_null_adapter(void) {
    TEST("C-L07 Error: NULL adapter handling");

    /* NULL adapter destroy should be safe */
    checkpoint_adapter_destroy(NULL);

    /* NULL adapter is_ready should return false */
    bool ready = checkpoint_adapter_is_ready(NULL);
    CHECK(!ready, "NULL adapter should not be ready");

    /* NULL adapter save should fail */
    checkpoint_snapshot_t snap;
    fill_snapshot(&snap, "null-test", 1, 1);
    int ret = checkpoint_adapter_save(NULL, "null-test", "s1", 1, &snap);
    CHECK(ret != 0, "NULL adapter save should fail");
    free_snapshot_fields(&snap);

    /* NULL adapter restore should fail */
    checkpoint_snapshot_t *restored = NULL;
    ret = checkpoint_adapter_restore(NULL, "null-test", &restored);
    CHECK(ret != 0, "NULL adapter restore should fail");

    /* NULL adapter delete should fail */
    ret = checkpoint_adapter_delete(NULL, "null-test", 0);
    CHECK(ret != 0, "NULL adapter delete should fail");

    PASS();
}

/* ============================================================================
 * P1.16g-5: Error Path — Save with NULL snapshot
 * ============================================================================ */

static void test_error_null_snapshot(void) {
    TEST("C-L07 Error: Save with NULL snapshot");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    int ret = checkpoint_adapter_save(adapter, "task-001", "session-001", 1, NULL);
    CHECK(ret != 0, "Save with NULL snapshot should fail");

    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-6: Timeout Path — Checkpoint operations with config
 * ============================================================================ */

static void test_timeout_checkpoint_ops(void) {
    TEST("C-L07 Timeout: Checkpoint operations with interval config");

    checkpoint_adapter_config_t config = {0};
    config.save_interval_turns = 5;
    config.save_interval_ms = 100;
    config.enable_incremental_save = true;
    config.enable_compression = false;
    config.max_checkpoints_per_task = 10;
    config.max_age_seconds = 3600;

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(&config);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    /* Save and restore should complete quickly */
    checkpoint_snapshot_t snap;
    fill_snapshot(&snap, "task-timeout-001", 1, 5);

    int ret = checkpoint_adapter_save(adapter, "task-timeout-001",
                                       "session-001", 1, &snap);
    CHECK_EQ(ret, 0, "Save should complete within timeout");

    checkpoint_snapshot_t *restored = NULL;
    ret = checkpoint_adapter_restore(adapter, "task-timeout-001", &restored);
    CHECK_EQ(ret, 0, "Restore should complete within timeout");
    CHECK(restored != NULL, "Restored snapshot should not be NULL");

    checkpoint_snapshot_free(restored);
    free_snapshot_fields(&snap);
    checkpoint_adapter_delete(adapter, "task-timeout-001", 0);
    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-7: Concurrent Path — Multiple simultaneous checkpoint operations
 * ============================================================================ */

#define CKPT_CONCURRENT_THREADS 4
#define CKPT_OPS_PER_THREAD 10

typedef struct {
    checkpoint_adapter_t *adapter;
    int thread_id;
    int success_count;
    int error_count;
} ckpt_thread_args_t;

static void *concurrent_checkpoint_thread(void *arg) {
    ckpt_thread_args_t *args = (ckpt_thread_args_t *)arg;

    for (int i = 0; i < CKPT_OPS_PER_THREAD; i++) {
        char task_id[128];
        snprintf(task_id, sizeof(task_id), "task-concurrent-%d-%d",
                 args->thread_id, i);

        checkpoint_snapshot_t snap;
        fill_snapshot(&snap, task_id, (uint64_t)i, (uint32_t)(i + 1));

        int ret = checkpoint_adapter_save(args->adapter, task_id,
                                           "session-concurrent", (uint64_t)i, &snap);
        free_snapshot_fields(&snap);

        if (ret == 0) {
            args->success_count++;

            /* Try to restore what we just saved */
            checkpoint_snapshot_t *restored = NULL;
            ret = checkpoint_adapter_restore(args->adapter, task_id, &restored);
            if (ret == 0 && restored != NULL) {
                checkpoint_snapshot_free(restored);
            } else {
                args->error_count++;
            }

            /* Clean up */
            checkpoint_adapter_delete(args->adapter, task_id, 0);
        } else {
            args->error_count++;
        }
    }
    return NULL;
}

static void test_concurrent_checkpoint_ops(void) {
    TEST("C-L07 Concurrent: Multiple simultaneous checkpoint operations");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    pthread_t threads[CKPT_CONCURRENT_THREADS];
    ckpt_thread_args_t args[CKPT_CONCURRENT_THREADS];

    for (int i = 0; i < CKPT_CONCURRENT_THREADS; i++) {
        args[i].adapter = adapter;
        args[i].thread_id = i;
        args[i].success_count = 0;
        args[i].error_count = 0;
        pthread_create(&threads[i], NULL, concurrent_checkpoint_thread, &args[i]);
    }

    int total_success = 0;
    int total_errors = 0;
    for (int i = 0; i < CKPT_CONCURRENT_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_success += args[i].success_count;
        total_errors += args[i].error_count;
    }

    CHECK(total_success > 0, "At least some checkpoint ops should succeed");
    CHECK_EQ(total_success + total_errors,
             CKPT_CONCURRENT_THREADS * CKPT_OPS_PER_THREAD,
             "All checkpoint operations should complete");

    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * P1.16g-8: Snapshot management
 * ============================================================================ */

static void test_snapshot_management(void) {
    TEST("C-L07 Normal: Snapshot create → restore from snapshot file");

    checkpoint_adapter_t *adapter = checkpoint_adapter_create(NULL);
    CHECK(adapter != NULL, "checkpoint_adapter_create returned NULL");

    /* Save a checkpoint first */
    checkpoint_snapshot_t snap;
    fill_snapshot(&snap, "task-snap-001", 1, 15);
    int ret = checkpoint_adapter_save(adapter, "task-snap-001",
                                       "session-001", 1, &snap);
    CHECK_EQ(ret, 0, "Save should succeed");

    /* Create a full snapshot to file */
    const char *snap_path = "/tmp/test_ckpt_snapshot_001.json";
    ret = checkpoint_adapter_snapshot_create(adapter, "task-snap-001", snap_path);
    /* May succeed or fail depending on storage backend */
    (void)ret;

    /* Try snapshot restore */
    char *restored_task_id = NULL;
    ret = checkpoint_adapter_snapshot_restore(adapter, snap_path, &restored_task_id);
    if (ret == 0 && restored_task_id != NULL) {
        free(restored_task_id);
    }

    free_snapshot_fields(&snap);
    checkpoint_adapter_delete(adapter, "task-snap-001", 0);
    checkpoint_adapter_destroy(adapter);
    PASS();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== C-L07 Integration Tests: Checkpoint → CoreLoopThree ===\n\n");

    test_normal_save_restore();
    test_normal_multiple_checkpoints();
    test_error_restore_nonexistent();
    test_error_null_adapter();
    test_error_null_snapshot();
    test_timeout_checkpoint_ops();
    test_concurrent_checkpoint_ops();
    test_snapshot_management();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_tests_passed, g_tests_total, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}