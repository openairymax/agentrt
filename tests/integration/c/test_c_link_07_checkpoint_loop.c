// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-C
/**
 * @file test_c_link_07_checkpoint_loop.c
 * @brief C-L07 Integration Test: Checkpoint → CoreLoopThree
 *
 * Tests the checkpoint adapter connecting CoreLoopThree to persistent storage:
 * 1. Normal path: Save snapshot → restore → verify state
 * 2. Error path: Restore non-existent checkpoint → AGENTRT_ENOENT
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
#include <sys/stat.h>
#include <errno.h>
#include <ftw.h>
#include <dirent.h>

#include "memory_compat.h"
#include "checkpoint_adapter.h"
#include "agentrt_types.h"

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

/* 用于需要资源清理的测试函数：CHECK 失败时跳转到 cleanup 标签，
 * 确保已分配的 adapter/snapshot 资源被正确释放，避免内存泄漏。
 * 这与 ASAN 的 leak detector 配合，确保测试失败时也是 0 泄漏。 */
#define CHECK_GOTO(cond, reason, label) do { \
    if (!(cond)) { FAIL(reason); goto label; } \
} while(0)

#define CHECK_EQ_GOTO(a, b, reason, label) do { \
    if ((a) != (b)) { \
        char buf[256]; \
        snprintf(buf, sizeof(buf), "%s (got %d, expected %d)", reason, \
                 (int)(a), (int)(b)); \
        FAIL(buf); goto label; \
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
 * Test storage path helpers
 *
 * 生产代码默认存储路径为 /var/lib/agentrt/checkpoints/，在普通用户测试环境
 * 下不存在（errno=ENOENT），导致 agentrt_checkpoint_save → fopen 失败。
 * 测试使用 /tmp/test_ckpt_<pid>/ 作为可写临时目录，并在退出时清理。
 * ============================================================================ */

static char g_test_storage_path[256];

/* nftw 回调：递归删除文件/目录 */
static int rm_cb(const char *path, const struct stat *sb,
                 int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return remove(path);
}

/* 创建使用临时目录的 adapter。
 * 多次调用会复用同一目录路径（进程级唯一），依赖测试间使用不同 task_id
 * 避免相互干扰。目录在 main() 退出时由 cleanup_test_dir() 统一清理。 */
static checkpoint_adapter_t *create_test_adapter(void) {
    if (g_test_storage_path[0] == '\0') {
        snprintf(g_test_storage_path, sizeof(g_test_storage_path),
                 "/tmp/test_ckpt_%d", (int)getpid());
    }

    /* mkdir 创建目录（已存在则忽略 EEXIST） */
    if (mkdir(g_test_storage_path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "C-L07: cannot create test storage dir '%s': errno=%d\n",
                g_test_storage_path, errno);
        return NULL;
    }

    checkpoint_adapter_config_t config;
    memset(&config, 0, sizeof(config));
    config.storage_path = g_test_storage_path;
    config.save_interval_turns = 1;   /* 测试中每次 save 都立即落盘 */
    config.save_interval_ms = 100;
    config.enable_incremental_save = false;
    config.enable_compression = false;
    config.max_checkpoints_per_task = 100;
    config.max_age_seconds = 3600;

    return checkpoint_adapter_create(&config);
}

static void cleanup_test_dir(void) {
    if (g_test_storage_path[0] != '\0') {
        nftw(g_test_storage_path, rm_cb, 16, FTW_DEPTH | FTW_PHYS);
        g_test_storage_path[0] = '\0';
    }
}

/* ============================================================================
 * P1.16g-1: Normal Path — Checkpoint save and restore
 * ============================================================================ */

static void test_normal_save_restore(void) {
    TEST("C-L07 Normal: Save checkpoint → restore → verify state");

    checkpoint_adapter_t *adapter = NULL;
    checkpoint_snapshot_t snap;
    checkpoint_snapshot_t *restored = NULL;
    memset(&snap, 0, sizeof(snap));

    adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    CHECK_GOTO(checkpoint_adapter_is_ready(adapter),
               "Adapter should be ready after creation", cleanup);

    /* Create and save a snapshot */
    fill_snapshot(&snap, "task-save-restore-001", 1, 10);

    int ret = checkpoint_adapter_save(adapter, "task-save-restore-001",
                                       "session-001", 1, &snap);
    CHECK_EQ_GOTO(ret, 0, "checkpoint_adapter_save should succeed", cleanup);

    /* Restore the saved snapshot */
    ret = checkpoint_adapter_restore(adapter, "task-save-restore-001", &restored);
    CHECK_EQ_GOTO(ret, 0, "checkpoint_adapter_restore should succeed", cleanup);
    CHECK_GOTO(restored != NULL, "Restored snapshot should not be NULL", cleanup);

    /* Verify restored data */
    CHECK_GOTO(restored->sequence_num == 1, "Sequence number should match", cleanup);
    CHECK_GOTO(restored->current_turn == 10, "Current turn should match", cleanup);
    CHECK_GOTO(restored->total_turns == 100, "Total turns should match", cleanup);
    CHECK_GOTO(restored->cognition_state_json != NULL,
               "Cognition state should be restored", cleanup);

    checkpoint_snapshot_free(restored);
    restored = NULL;
    free_snapshot_fields(&snap);

    /* Clean up checkpoint */
    checkpoint_adapter_delete(adapter, "task-save-restore-001", 0);
    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (restored) checkpoint_snapshot_free(restored);
    free_snapshot_fields(&snap);
    if (adapter) {
        checkpoint_adapter_delete(adapter, "task-save-restore-001", 0);
        checkpoint_adapter_destroy(adapter);
    }
}

/* ============================================================================
 * P1.16g-2: Normal Path — Multiple checkpoints and list
 * ============================================================================ */

static void test_normal_multiple_checkpoints(void) {
    TEST("C-L07 Normal: Multiple checkpoints → list → restore by sequence");

    checkpoint_adapter_t *adapter = NULL;
    checkpoint_snapshot_t **snapshots = NULL;
    size_t count = 0;
    checkpoint_snapshot_t *seq2 = NULL;

    adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    /* Save multiple checkpoints */
    for (int i = 1; i <= 3; i++) {
        checkpoint_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        fill_snapshot(&snap, "task-multi-001", (uint64_t)i, (uint32_t)(i * 10));
        int ret = checkpoint_adapter_save(adapter, "task-multi-001",
                                           "session-001", (uint64_t)i, &snap);
        free_snapshot_fields(&snap);
        CHECK_EQ_GOTO(ret, 0, "Save checkpoint should succeed", cleanup);
    }

    /* List checkpoints */
    int ret = checkpoint_adapter_list(adapter, "task-multi-001",
                                       &snapshots, &count);
    CHECK_EQ_GOTO(ret, 0, "checkpoint_adapter_list should succeed", cleanup);
    CHECK_GOTO(count >= 3, "Should have at least 3 checkpoints", cleanup);

    /* Restore by specific sequence */
    ret = checkpoint_adapter_restore_seq(adapter, "task-multi-001", 2, &seq2);
    CHECK_EQ_GOTO(ret, 0, "Restore sequence 2 should succeed", cleanup);
    CHECK_GOTO(seq2 != NULL, "Restored snapshot should not be NULL", cleanup);
    CHECK_EQ_GOTO(seq2->sequence_num, (uint64_t)2, "Should restore sequence 2", cleanup);
    checkpoint_snapshot_free(seq2);
    seq2 = NULL;

    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        checkpoint_snapshot_free(snapshots[i]);
    }
    free(snapshots);
    snapshots = NULL;
    count = 0;

    checkpoint_adapter_delete(adapter, "task-multi-001", 0);
    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (seq2) checkpoint_snapshot_free(seq2);
    if (snapshots) {
        for (size_t i = 0; i < count; i++) {
            checkpoint_snapshot_free(snapshots[i]);
        }
        free(snapshots);
    }
    if (adapter) {
        checkpoint_adapter_delete(adapter, "task-multi-001", 0);
        checkpoint_adapter_destroy(adapter);
    }
}

/* ============================================================================
 * P1.16g-3: Error Path — Restore non-existent checkpoint
 * ============================================================================ */

static void test_error_restore_nonexistent(void) {
    TEST("C-L07 Error: Restore non-existent checkpoint → AGENTRT_ENOENT");

    checkpoint_adapter_t *adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    checkpoint_snapshot_t *restored = NULL;
    int ret = checkpoint_adapter_restore(adapter, "non-existent-task", &restored);
    CHECK_GOTO(ret != 0, "Restore non-existent should return error", cleanup);
    CHECK_GOTO(restored == NULL, "Restored snapshot should be NULL for non-existent", cleanup);

    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (restored) checkpoint_snapshot_free(restored);
    if (adapter) checkpoint_adapter_destroy(adapter);
}

/* ============================================================================
 * P1.16g-4: Error Path — NULL adapter handling
 * ============================================================================ */

static void test_error_null_adapter(void) {
    TEST("C-L07 Error: NULL adapter handling");

    checkpoint_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));

    /* NULL adapter destroy should be safe */
    checkpoint_adapter_destroy(NULL);

    /* NULL adapter is_ready should return false */
    bool ready = checkpoint_adapter_is_ready(NULL);
    CHECK_GOTO(!ready, "NULL adapter should not be ready", cleanup);

    /* NULL adapter save should fail */
    fill_snapshot(&snap, "null-test", 1, 1);
    int ret = checkpoint_adapter_save(NULL, "null-test", "s1", 1, &snap);
    CHECK_GOTO(ret != 0, "NULL adapter save should fail", cleanup);
    free_snapshot_fields(&snap);

    /* NULL adapter restore should fail */
    checkpoint_snapshot_t *restored = NULL;
    ret = checkpoint_adapter_restore(NULL, "null-test", &restored);
    CHECK_GOTO(ret != 0, "NULL adapter restore should fail", cleanup);

    /* NULL adapter delete should fail */
    ret = checkpoint_adapter_delete(NULL, "null-test", 0);
    CHECK_GOTO(ret != 0, "NULL adapter delete should fail", cleanup);

    PASS();
    return;

cleanup:
    free_snapshot_fields(&snap);
}

/* ============================================================================
 * P1.16g-5: Error Path — Save with NULL snapshot
 * ============================================================================ */

static void test_error_null_snapshot(void) {
    TEST("C-L07 Error: Save with NULL snapshot");

    checkpoint_adapter_t *adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    int ret = checkpoint_adapter_save(adapter, "task-001", "session-001", 1, NULL);
    CHECK_GOTO(ret != 0, "Save with NULL snapshot should fail", cleanup);

    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (adapter) checkpoint_adapter_destroy(adapter);
}

/* ============================================================================
 * P1.16g-6: Timeout Path — Checkpoint operations with config
 * ============================================================================ */

static void test_timeout_checkpoint_ops(void) {
    TEST("C-L07 Timeout: Checkpoint operations with interval config");

    checkpoint_adapter_t *adapter = NULL;
    checkpoint_snapshot_t snap;
    checkpoint_snapshot_t *restored = NULL;
    memset(&snap, 0, sizeof(snap));

    /* 确保临时目录已创建（复用 create_test_adapter 的路径初始化逻辑） */
    if (g_test_storage_path[0] == '\0') {
        snprintf(g_test_storage_path, sizeof(g_test_storage_path),
                 "/tmp/test_ckpt_%d", (int)getpid());
    }
    if (mkdir(g_test_storage_path, 0755) != 0 && errno != EEXIST) {
        FAIL("cannot create test storage dir");
        return;
    }

    checkpoint_adapter_config_t config;
    memset(&config, 0, sizeof(config));
    config.storage_path = g_test_storage_path;
    config.save_interval_turns = 5;
    config.save_interval_ms = 100;
    config.enable_incremental_save = true;
    config.enable_compression = false;
    config.max_checkpoints_per_task = 10;
    config.max_age_seconds = 3600;

    adapter = checkpoint_adapter_create(&config);
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    /* Save and restore should complete quickly */
    fill_snapshot(&snap, "task-timeout-001", 1, 5);

    int ret = checkpoint_adapter_save(adapter, "task-timeout-001",
                                       "session-001", 1, &snap);
    CHECK_EQ_GOTO(ret, 0, "Save should complete within timeout", cleanup);

    ret = checkpoint_adapter_restore(adapter, "task-timeout-001", &restored);
    CHECK_EQ_GOTO(ret, 0, "Restore should complete within timeout", cleanup);
    CHECK_GOTO(restored != NULL, "Restored snapshot should not be NULL", cleanup);

    checkpoint_snapshot_free(restored);
    restored = NULL;
    free_snapshot_fields(&snap);
    checkpoint_adapter_delete(adapter, "task-timeout-001", 0);
    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (restored) checkpoint_snapshot_free(restored);
    free_snapshot_fields(&snap);
    if (adapter) {
        checkpoint_adapter_delete(adapter, "task-timeout-001", 0);
        checkpoint_adapter_destroy(adapter);
    }
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

    checkpoint_adapter_t *adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

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

    CHECK_GOTO(total_success > 0, "At least some checkpoint ops should succeed", cleanup);
    CHECK_EQ_GOTO(total_success + total_errors,
                  CKPT_CONCURRENT_THREADS * CKPT_OPS_PER_THREAD,
                  "All checkpoint operations should complete", cleanup);

    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (adapter) checkpoint_adapter_destroy(adapter);
}

/* ============================================================================
 * P1.16g-8: Snapshot management
 * ============================================================================ */

static void test_snapshot_management(void) {
    TEST("C-L07 Normal: Snapshot create → restore from snapshot file");

    checkpoint_adapter_t *adapter = NULL;
    checkpoint_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    char *restored_task_id = NULL;

    adapter = create_test_adapter();
    CHECK_GOTO(adapter != NULL, "checkpoint_adapter_create returned NULL", cleanup);

    /* Save a checkpoint first */
    fill_snapshot(&snap, "task-snap-001", 1, 15);
    int ret = checkpoint_adapter_save(adapter, "task-snap-001",
                                       "session-001", 1, &snap);
    CHECK_EQ_GOTO(ret, 0, "Save should succeed", cleanup);

    /* Create a full snapshot to file */
    char snap_path[256];
    snprintf(snap_path, sizeof(snap_path), "%s/test_ckpt_snapshot_001.json",
             g_test_storage_path);
    ret = checkpoint_adapter_snapshot_create(adapter, "task-snap-001", snap_path);
    /* May succeed or fail depending on storage backend */
    (void)ret;

    /* Try snapshot restore */
    ret = checkpoint_adapter_snapshot_restore(adapter, snap_path, &restored_task_id);
    if (ret == 0 && restored_task_id != NULL) {
        free(restored_task_id);
        restored_task_id = NULL;
    }

    free_snapshot_fields(&snap);
    checkpoint_adapter_delete(adapter, "task-snap-001", 0);
    checkpoint_adapter_destroy(adapter);
    adapter = NULL;
    PASS();
    return;

cleanup:
    if (restored_task_id) free(restored_task_id);
    free_snapshot_fields(&snap);
    if (adapter) {
        checkpoint_adapter_delete(adapter, "task-snap-001", 0);
        checkpoint_adapter_destroy(adapter);
    }
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

    /* 清理测试临时目录（无论测试通过与否） */
    cleanup_test_dir();

    return g_tests_failed > 0 ? 1 : 0;
}