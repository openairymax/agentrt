/**
 * @file test_checkpoint.c
 * @brief 检查点机制单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "checkpoint.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("[PASS] %s\n", message); \
    } else { \
        tests_failed++; \
        printf("[FAIL] %s\n", message); \
    } \
} while(0)

void test_checkpoint_init(void) {
    agentos_error_t result = agentos_checkpoint_init("./test_checkpoints");
    TEST_ASSERT(result == AGENTOS_SUCCESS, "Checkpoint initialization should succeed");
}

void test_checkpoint_create(void) {
    agentos_task_checkpoint_t* checkpoint = NULL;

    char completed_nodes[] = {"node1", "node2"};
    char pending_nodes[] = {"node3", "node4"};

    agentos_error_t result = agentos_checkpoint_create(
        "task_001",
        "session_001",
        "{\"state\": \"running\"}",
        completed_nodes,
        2,
        pending_nodes,
        2,
        &checkpoint
    );

    TEST_ASSERT(result == AGENTOS_SUCCESS && checkpoint != NULL,
               "Checkpoint creation should succeed");
    TEST_ASSERT(strcmp(checkpoint->task_id, "task_001") == 0,
               "Task ID should match");
    TEST_ASSERT(checkpoint->completed_count == 2,
               "Completed count should be 2");
    TEST_ASSERT(checkpoint->pending_count == 2,
               "Pending count should be 2");
    TEST_ASSERT(checkpoint->state == CHECKPOINT_STATE_COMPLETED,
               "State should be COMPLETED");

    if (checkpoint) {
        agentos_checkpoint_delete(checkpoint);
    }
}

void test_checkpoint_save_and_restore(void) {
    agentos_task_checkpoint_t* checkpoint = NULL;
    char* completed[] = {"step1", "step2"};
    char* pending[] = {"step3"};

    agentos_error_t create_result = agentos_checkpoint_create(
        "task_restore_test",
        "session_restore",
        "{\"progress\": 66}",
        completed,
        2,
        pending,
        1,
        &checkpoint
    );

    TEST_ASSERT(create_result == AGENTOS_SUCCESS, "Create checkpoint for restore test");

    agentos_error_t save_result = agentos_checkpoint_save(checkpoint);
    TEST_ASSERT(save_result == AGENTOS_SUCCESS, "Save checkpoint should succeed");

    agentos_task_checkpoint_t* restored = NULL;
    agentos_error_t restore_result = agentos_checkpoint_restore(
        "task_restore_test",
        &restored
    );

    TEST_ASSERT(restore_result == AGENTOS_SUCCESS && restored != NULL,
               "Restore checkpoint should succeed");

    if (restored) {
        TEST_ASSERT(strcmp(restored->task_id, "task_restore_test") == 0,
                   "Restored task ID should match");
        TEST_ASSERT(restored->completed_count == 2,
                   "Restored completed count should match");
        agentos_checkpoint_delete(restored);
    }

    if (checkpoint) {
        agentos_checkpoint_delete(checkpoint);
    }
}

void test_checkpoint_verify(void) {
    agentos_task_checkpoint_t* checkpoint = NULL;
    char* completed[] = {"node1"};
    char* pending[] = {};

    agentos_checkpoint_create("task_verify", "session_verify", "{}",
                            completed, 1, pending, 0, &checkpoint);

    int is_valid = 0;
    agentos_error_t result = agentos_checkpoint_verify(checkpoint, &is_valid);

    TEST_ASSERT(result == AGENTOS_SUCCESS, "Verify should succeed");
    TEST_ASSERT(is_valid == 1, "Freshly created checkpoint should be valid");

    if (checkpoint) {
        agentos_checkpoint_delete(checkpoint);
    }
}

void test_checkpoint_stats(void) {
    agentos_checkpoint_stats_t stats;
    agentos_error_t result = agentos_checkpoint_get_stats(&stats);

    TEST_ASSERT(result == AGENTOS_SUCCESS, "Get stats should succeed");
    TEST_ASSERT(stats.total_checkpoints >= 0, "Total checkpoints should be >= 0");
}

void test_checkpoint_list(void) {
    agentos_task_checkpoint_t** list = NULL;
    size_t count = 0;

    agentos_error_t result = agentos_checkpoint_list("task_001", &list, &count);

    TEST_ASSERT(result == AGENTOS_SUCCESS, "List checkpoints should succeed");
    TEST_ASSERT(count >= 0, "Count should be >= 0");

    if (list) {
        for (size_t i = 0; i < count; i++) {
            if (list[i]) {
                agentos_checkpoint_delete(list[i]);
            }
        }
        AGENTOS_FREE(list);
    }
}

void test_null_params(void) {
    agentos_task_checkpoint_t* checkpoint = NULL;

    agentos_error_t create_null = agentos_checkpoint_create(NULL, NULL, NULL,
                                                            NULL, 0, NULL, 0, NULL);
    TEST_ASSERT(create_null != AGENTOS_SUCCESS, "Create with NULL params should fail");

    agentos_error_t save_null = agentos_checkpoint_save(NULL);
    TEST_ASSERT(save_null != AGENTOS_SUCCESS, "Save NULL checkpoint should fail");

    agentos_task_checkpoint_t* restored = NULL;
    agentos_error_t restore_null = agentos_checkpoint_restore(NULL, &restored);
    TEST_ASSERT(restore_null != AGENTOS_SUCCESS, "Restore with NULL task_id should fail");

    agentos_error_t delete_null = agentos_checkpoint_delete(NULL);
    TEST_ASSERT(delete_null == AGENTOS_SUCCESS || delete_null != AGENTOS_EFAULT,
               "Delete NULL should handle gracefully");

    agentos_error_t verify_null = agentos_checkpoint_verify(NULL, NULL);
    TEST_ASSERT(verify_null != AGENTOS_SUCCESS, "Verify NULL should fail");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("Checkpoint Mechanism Unit Tests\n");
    printf("========================================\n\n");

    printf("--- Initialization Tests ---\n");
    test_checkpoint_init();

    printf("\n--- Creation Tests ---\n");
    test_checkpoint_create();
    test_null_params();

    printf("\n--- Save/Restore Tests ---\n");
    test_checkpoint_save_and_restore();

    printf("\n--- Verification Tests ---\n");
    test_checkpoint_verify();

    printf("\n--- Statistics Tests ---\n");
    test_checkpoint_stats();
    test_checkpoint_list();

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
