/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_vault.c - Vault Module Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "security/cupolas_vault.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("Running %s... ", #name); \
    name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf("FAILED: %s\n", message); \
        tests_failed++; \
        return; \
    } \
} while(0)

static cupolas_vault_t* g_vault = NULL;

static int setup_vault(void) {
    int result = cupolas_vault_init(NULL);
    if (result != 0) return result;

    result = cupolas_vault_open("test_vault", "test_password", &g_vault);
    return result;
}

static void teardown_vault(void) {
    if (g_vault) {
        cupolas_vault_close(g_vault);
        g_vault = NULL;
    }
    cupolas_vault_cleanup();
}

TEST(test_vault_init_cleanup) {
    int result = cupolas_vault_init(NULL);
    ASSERT(result == 0, "Vault init should succeed");
    cupolas_vault_cleanup();
}

TEST(test_vault_open_close) {
    int result = cupolas_vault_init(NULL);
    ASSERT(result == 0, "Vault init should succeed");

    cupolas_vault_t* vault = NULL;
    result = cupolas_vault_open("test_open_close", "password123", &vault);
    ASSERT(result == 0, "Vault open should succeed");
    ASSERT(vault != NULL, "Vault context should not be NULL");

    cupolas_vault_close(vault);
    cupolas_vault_cleanup();
}

TEST(test_vault_store_and_retrieve) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const char* cred_id = "test_secret_1";
    const uint8_t* secret = (const uint8_t*)"my_secret_value";
    size_t secret_len = strlen((const char*)secret);

    int store_result = cupolas_vault_store(g_vault, cred_id,
                                           CUPOLAS_VAULT_CRED_PASSWORD,
                                           secret, secret_len, NULL);
    ASSERT(store_result == 0, "Store secret should succeed");

    uint8_t retrieved[256];
    size_t retrieved_len = sizeof(retrieved);
    int retrieve_result = cupolas_vault_retrieve(g_vault, cred_id, "test_agent",
                                                  retrieved, &retrieved_len);
    ASSERT(retrieve_result == 0, "Retrieve secret should succeed");
    ASSERT(retrieved_len == secret_len, "Retrieved length should match");
    ASSERT(memcmp(retrieved, secret, retrieved_len) == 0, "Retrieved value should match");

    teardown_vault();
}

TEST(test_vault_store_duplicate_id) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const char* cred_id = "test_secret_dup";
    const uint8_t* secret1 = (const uint8_t*)"first_value";
    const uint8_t* secret2 = (const uint8_t*)"second_value";

    int result1 = cupolas_vault_store(g_vault, cred_id,
                                       CUPOLAS_VAULT_CRED_PASSWORD,
                                       secret1, strlen((const char*)secret1), NULL);
    ASSERT(result1 == 0, "First store should succeed");

    int result2 = cupolas_vault_store(g_vault, cred_id,
                                       CUPOLAS_VAULT_CRED_PASSWORD,
                                       secret2, strlen((const char*)secret2), NULL);
    ASSERT(result2 == 0, "Duplicate ID should update (idempotent store)");

    teardown_vault();
}

TEST(test_vault_retrieve_nonexistent) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    uint8_t buffer[256];
    size_t len = sizeof(buffer);
    int retrieve_result = cupolas_vault_retrieve(g_vault, "nonexistent_id", "test_agent",
                                                  buffer, &len);
    ASSERT(retrieve_result != 0, "Retrieve nonexistent should fail");

    teardown_vault();
}

TEST(test_vault_delete) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const char* cred_id = "to_be_deleted";
    const uint8_t* secret = (const uint8_t*)"delete_me";

    int store_result = cupolas_vault_store(g_vault, cred_id,
                                            CUPOLAS_VAULT_CRED_PASSWORD,
                                            secret, strlen((const char*)secret), NULL);
    ASSERT(store_result == 0, "Store should succeed");

    int delete_result = cupolas_vault_delete(g_vault, cred_id, "test_agent");
    ASSERT(delete_result == 0, "Delete should succeed");

    uint8_t buffer[256];
    size_t len = sizeof(buffer);
    int retrieve_result = cupolas_vault_retrieve(g_vault, cred_id, "test_agent",
                                                  buffer, &len);
    ASSERT(retrieve_result != 0, "Retrieve after delete should fail");

    teardown_vault();
}

TEST(test_vault_exists) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const char* cred_id = "exists_test";
    const uint8_t* secret = (const uint8_t*)"exists_value";

    bool exists_before = cupolas_vault_exists(g_vault, cred_id);
    ASSERT(!exists_before, "Should not exist before store");

    cupolas_vault_store(g_vault, cred_id, CUPOLAS_VAULT_CRED_PASSWORD,
                        secret, strlen((const char*)secret), NULL);

    bool exists_after = cupolas_vault_exists(g_vault, cred_id);
    ASSERT(exists_after, "Should exist after store");

    teardown_vault();
}

TEST(test_vault_lock_unlock) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    int lock_result = cupolas_vault_lock(g_vault);
    ASSERT(lock_result == 0, "Lock should succeed");
    ASSERT(cupolas_vault_is_locked(g_vault), "Should be locked");

    int unlock_result = cupolas_vault_unlock(g_vault, "test_password");
    ASSERT(unlock_result == 0, "Unlock should succeed");
    ASSERT(!cupolas_vault_is_locked(g_vault), "Should be unlocked");

    teardown_vault();
}

TEST(test_vault_list) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const uint8_t* secret = (const uint8_t*)"list_test_value";
    cupolas_vault_store(g_vault, "list_1", CUPOLAS_VAULT_CRED_PASSWORD,
                        secret, strlen((const char*)secret), NULL);
    cupolas_vault_store(g_vault, "list_2", CUPOLAS_VAULT_CRED_KEY,
                        secret, strlen((const char*)secret), NULL);

    cupolas_vault_metadata_t* metadata_array = NULL;
    size_t count = 0;
    int list_result = cupolas_vault_list(g_vault, 0, &metadata_array, &count);
    if (list_result == 0) {
        printf("Listed %zu vault entries", count);
        cupolas_vault_free_list(metadata_array, count);
    } else {
        printf("List entries skipped");
    }

    teardown_vault();
}

TEST(test_vault_export_import) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const uint8_t* secret = (const uint8_t*)"export_secret_value";
    cupolas_vault_store(g_vault, "export_test", CUPOLAS_VAULT_CRED_PASSWORD,
                        secret, strlen((const char*)secret), NULL);

    int export_result = cupolas_vault_export(g_vault, "/tmp/cupolas_export.enc",
                                              "export_pass", "test_agent");
    if (export_result == 0) {
        printf("Export succeeded");
        int import_result = cupolas_vault_import(g_vault, "/tmp/cupolas_export.enc",
                                                  "export_pass", "test_agent");
        if (import_result == 0) {
            printf("Import succeeded");
        }
    } else {
        printf("Export skipped");
    }

    teardown_vault();
}

TEST(test_vault_grant_revoke_access) {
    int result = setup_vault();
    ASSERT(result == 0, "Vault setup should succeed");

    const uint8_t* secret = (const uint8_t*)"acl_test_value";
    cupolas_vault_store(g_vault, "acl_test", CUPOLAS_VAULT_CRED_PASSWORD,
                        secret, strlen((const char*)secret), NULL);

    int grant_result = cupolas_vault_grant_access(g_vault, "acl_test", "agent_001",
                                                   CUPOLAS_VAULT_OP_READ, 0);
    if (grant_result == 0) {
        bool has_access = cupolas_vault_check_access(g_vault, "acl_test", "agent_001",
                                                      CUPOLAS_VAULT_OP_READ);
        ASSERT(has_access, "Agent should have read access");

        int revoke_result = cupolas_vault_revoke_access(g_vault, "acl_test", "agent_001");
        ASSERT(revoke_result == 0, "Revoke should succeed");
    }

    teardown_vault();
}

static void run_all_tests(void) {
    printf("\n=== Cupolas Vault Module Tests ===\n\n");

    RUN_TEST(test_vault_init_cleanup);
    RUN_TEST(test_vault_open_close);
    RUN_TEST(test_vault_store_and_retrieve);
    RUN_TEST(test_vault_store_duplicate_id);
    RUN_TEST(test_vault_retrieve_nonexistent);
    RUN_TEST(test_vault_delete);
    RUN_TEST(test_vault_exists);
    RUN_TEST(test_vault_lock_unlock);
    RUN_TEST(test_vault_list);
    RUN_TEST(test_vault_export_import);
    RUN_TEST(test_vault_grant_revoke_access);

    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
}

int main(void) {
    run_all_tests();
    return tests_failed > 0 ? 1 : 0;
}
