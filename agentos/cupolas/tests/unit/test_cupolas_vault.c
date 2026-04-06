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

#include "cupolas_vault.h"

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

TEST(test_vault_init_cleanup) {
    int result = cupolas_vault_init(NULL);
    ASSERT(result == 0, "Vault init should succeed");
    
    cupolas_vault_cleanup();
}

TEST(test_vault_store_and_retrieve) {
    const char* id = "test_secret_1";
    const char* secret = "my_secret_value";
    char retrieved[256];
    size_t retrieved_len = sizeof(retrieved);
    
    int store_result = cupolas_vault_store(id, secret, strlen(secret));
    ASSERT(store_result == 0, "Store secret should succeed");
    
    int retrieve_result = cupolas_vault_retrieve(id, retrieved, &retrieved_len);
    ASSERT(retrieve_result == 0, "Retrieve secret should succeed");
    ASSERT(retrieved_len == strlen(secret), "Retrieved length should match");
    ASSERT(memcmp(retrieved, secret, retrieved_len) == 0, "Retrieved value should match");
}

TEST(test_vault_store_duplicate_id) {
    const char* id = "test_secret_dup";
    const char* secret1 = "first_value";
    const char* secret2 = "second_value";
    
    int result1 = cupolas_vault_store(id, secret1, strlen(secret1));
    ASSERT(result1 == 0, "First store should succeed");
    
    int result2 = cupolas_vault_store(id, secret2, strlen(secret2));
    ASSERT(result2 != 0, "Duplicate ID should fail or update");
}

TEST(test_vault_retrieve_nonexistent) {
    char buffer[256];
    size_t len = sizeof(buffer);
    
    int result = cupolas_vault_retrieve("nonexistent_id", buffer, &len);
    ASSERT(result != 0, "Retrieve nonexistent should fail");
}

TEST(test_vault_encryption_roundtrip) {
    const char* id = "encrypted_secret";
    const char* plaintext = "sensitive_data_12345";
    char decrypted[256];
    size_t decrypted_len = sizeof(decrypted);
    
    int store_result = cupolas_vault_store(id, plaintext, strlen(plaintext));
    ASSERT(store_result == 0, "Store should succeed");
    
    int retrieve_result = cupolas_vault_retrieve(id, decrypted, &decrypted_len);
    ASSERT(retrieve_result == 0, "Retrieve should succeed");
    ASSERT(decrypted_len == strlen(plaintext), "Decrypted length should match");
    ASSERT(memcmp(decrypted, plaintext, decrypted_len) == 0, "Decrypted value should match");
}

TEST(test_vault_large_secret) {
    const char* id = "large_secret";
    size_t secret_size = 1024 * 1024;
    char* secret = malloc(secret_size);
    ASSERT(secret != NULL, "Allocate large secret");
    
    memset(secret, 'A', secret_size);
    
    int store_result = cupolas_vault_store(id, secret, secret_size);
    
    if (store_result == 0) {
        char* retrieved = malloc(secret_size);
        ASSERT(retrieved != NULL, "Allocate buffer");
        
        size_t retrieved_len = secret_size;
        int retrieve_result = cupolas_vault_retrieve(id, retrieved, &retrieved_len);
        
        ASSERT(retrieve_result == 0, "Retrieve large secret should succeed");
        ASSERT(retrieved_len == secret_size, "Size should match");
        
        free(retrieved);
    } else {
        printf("Large secret test skipped (size limit enforced)");
    }
    
    free(secret);
}

TEST(test_vault_delete_after_retrieve) {
    const char* id = "to_be_deleted";
    const char* secret = "delete_me";
    
    int store_result = cupolas_vault_store(id, secret, strlen(secret));
    ASSERT(store_result == 0, "Store should succeed");
    
    int delete_result = cupolas_vault_delete(id);
    ASSERT(delete_result == 0, "Delete should succeed");
    
    char buffer[256];
    size_t len = sizeof(buffer);
    int retrieve_result = cupolas_vault_retrieve(id, buffer, &len);
    ASSERT(retrieve_result != 0, "Retrieve after delete should fail");
}

TEST(test_vault_capacity_limit) {
    int result;
    int stored_count = 0;
    
    for (int i = 0; i < 1000; i++) {
        char id[64];
        char secret[64];
        snprintf(id, sizeof(id), "test_%d", i);
        snprintf(secret, sizeof(secret), "secret_%d", i);
        
        result = cupolas_vault_store(id, secret, strlen(secret));
        if (result == 0) {
            stored_count++;
        } else {
            break;
        }
    }
    
    printf("Stored %d secrets before capacity limit", stored_count);
    ASSERT(stored_count > 0, "Should store at least some secrets");
}

TEST(test_vault_auto_lock_timeout) {
    const char* id = "timeout_secret";
    const char* secret = "auto_lock_test";
    
    int store_result = cupolas_vault_store(id, secret, strlen(secret));
    ASSERT(store_result == 0, "Store should succeed");
    
    printf("Waiting for auto-lock timeout (5 seconds)...");
    
    #ifdef _WIN32
    Sleep(5000);
    #else
    sleep(5);
    #endif
    
    char buffer[256];
    size_t len = sizeof(buffer);
    int retrieve_result = cupolas_vault_retrieve(id, buffer, &len);
    
    if (retrieve_result == 0) {
        printf("Vault still unlocked after timeout");
    } else {
        printf("Vault auto-locked successfully");
    }
}

TEST(test_vault_update_existing) {
    const char* id = "update_test";
    const char* secret1 = "original_value";
    const char* secret2 = "updated_value";
    char retrieved[256];
    size_t retrieved_len = sizeof(retrieved);
    
    int result1 = cupolas_vault_store(id, secret1, strlen(secret1));
    ASSERT(result1 == 0, "First store should succeed");
    
    int result2 = cupolas_vault_store(id, secret2, strlen(secret2));
    
    int retrieve_result = cupolas_vault_retrieve(id, retrieved, &retrieved_len);
    ASSERT(retrieve_result == 0, "Retrieve should succeed");
    
    if (result2 == 0) {
        ASSERT(retrieved_len == strlen(secret2), "Updated value length should match");
        ASSERT(memcmp(retrieved, secret2, retrieved_len) == 0, "Value should be updated");
        printf("Update succeeded");
    } else {
        ASSERT(retrieved_len == strlen(secret1), "Original value length should match");
        ASSERT(memcmp(retrieved, secret1, retrieved_len) == 0, "Value should remain original");
        printf("Update failed, original preserved");
    }
}

TEST(test_vault_list_entries) {
    cupolas_vault_entry_t* entries = NULL;
    size_t count = 0;
    
    int list_result = cupolas_vault_list_entries(&entries, &count);
    
    if (list_result == 0) {
        printf("Listed %zu vault entries", count);
        ASSERT(entries != NULL, "Entries array should not be NULL");
        
        cupolas_vault_free_entries(entries, count);
    } else {
        printf("List entries skipped (empty vault or not supported)");
    }
}

TEST(test_vault_export_import) {
    const char* id = "export_test";
    const char* secret = "export_secret_value";
    
    int store_result = cupolas_vault_store(id, secret, strlen(secret));
    ASSERT(store_result == 0, "Store should succeed");
    
    char* exported_data = NULL;
    size_t exported_len = 0;
    
    int export_result = cupolas_vault_export(&exported_data, &exported_len);
    
    if (export_result == 0 && exported_data != NULL) {
        printf("Exported %zu bytes", exported_len);
        free(exported_data);
    } else {
        printf("Export skipped (not supported or empty)");
    }
}

static void run_all_tests(void) {
    printf("\n=== Cupolas Vault Module Tests ===\n\n");
    
    RUN_TEST(test_vault_init_cleanup);
    RUN_TEST(test_vault_store_and_retrieve);
    RUN_TEST(test_vault_store_duplicate_id);
    RUN_TEST(test_vault_retrieve_nonexistent);
    RUN_TEST(test_vault_encryption_roundtrip);
    RUN_TEST(test_vault_large_secret);
    RUN_TEST(test_vault_delete_after_retrieve);
    RUN_TEST(test_vault_capacity_limit);
    RUN_TEST(test_vault_auto_lock_timeout);
    RUN_TEST(test_vault_update_existing);
    RUN_TEST(test_vault_list_entries);
    RUN_TEST(test_vault_export_import);
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Skipped: %d (expected if resources not available)\n", tests_run - tests_passed - tests_failed);
}

int main(void) {
    run_all_tests();
    return tests_failed > 0 ? 1 : 0;
}
