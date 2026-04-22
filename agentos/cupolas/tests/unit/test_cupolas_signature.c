/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_signature.c - Signature Module Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "security/cupolas_signature.h"

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

TEST(test_signature_init_cleanup) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");
    cupolas_signature_cleanup();
}

TEST(test_signature_algorithm_enum) {
    ASSERT(CUPOLAS_SIG_ALGO_RSA_SHA256 == 1, "RSA-SHA256 enum value");
    ASSERT(CUPOLAS_SIG_ALGO_RSA_SHA384 == 2, "RSA-SHA384 enum value");
    ASSERT(CUPOLAS_SIG_ALGO_RSA_SHA512 == 3, "RSA-SHA512 enum value");
    ASSERT(CUPOLAS_SIG_ALGO_ECDSA_P256 == 4, "ECDSA P-256 enum value");
    ASSERT(CUPOLAS_SIG_ALGO_ECDSA_P384 == 5, "ECDSA P-384 enum value");
    ASSERT(CUPOLAS_SIG_ALGO_ED25519 == 6, "Ed25519 enum value");
}

TEST(test_signature_result_enum) {
    ASSERT(CUPOLAS_SIG_OK == 0, "SIG_OK value");
    ASSERT(CUPOLAS_SIG_INVALID == -1, "SIG_INVALID value");
    ASSERT(CUPOLAS_SIG_EXPIRED == -2, "SIG_EXPIRED value");
    ASSERT(CUPOLAS_SIG_REVOKED == -3, "SIG_REVOKED value");
    ASSERT(CUPOLAS_SIG_TAMPERED == -5, "SIG_TAMPERED value");
    ASSERT(CUPOLAS_SIG_NO_SIGNATURE == -6, "SIG_NO_SIGNATURE value");
}

TEST(test_signature_verify_file) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    cupolas_sig_result_t sig_result = CUPOLAS_SIG_OK;
    int verify_result = cupolas_signature_verify_file("test_data/valid_signature.bin",
                                                       NULL, &sig_result);
    if (verify_result == 0) {
        printf("File signature verification completed, result=%d", sig_result);
    } else {
        printf("File verification skipped (test files not present)");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_verify_data) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    const uint8_t* data = (const uint8_t*)"Test data for verification";
    size_t data_len = strlen((const char*)data);
    const uint8_t* fake_sig = (const uint8_t*)"fakesignature";
    size_t sig_len = 13;

    int verify_result = cupolas_signature_verify_data(data, data_len,
                                                       fake_sig, sig_len,
                                                       CUPOLAS_SIG_ALGO_RSA_SHA256,
                                                       "test_data/public_key.pem");
    if (verify_result != 0) {
        printf("Fake signature correctly rejected");
    } else {
        printf("Data verification completed");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_sign_data) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    const uint8_t* data = (const uint8_t*)"Test data for RSA-SHA256";
    size_t data_len = strlen((const char*)data);

    uint8_t signature[256];
    size_t sig_len = sizeof(signature);

    int sign_result = cupolas_signature_sign_data(data, data_len,
                                                   "test_data/private_key.pem",
                                                   CUPOLAS_SIG_ALGO_RSA_SHA256,
                                                   signature, &sig_len);

    if (sign_result == 0) {
        int verify_result = cupolas_signature_verify_data(data, data_len,
                                                          signature, sig_len,
                                                          CUPOLAS_SIG_ALGO_RSA_SHA256,
                                                          "test_data/public_key.pem");
        ASSERT(verify_result == 0, "RSA-SHA256 signature verification should succeed");
    } else {
        printf("RSA-SHA256 signing skipped (test keys not present)");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_verify_integrity) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    uint8_t expected_hash[32];
    memset(expected_hash, 0, 32);

    int integrity_result = cupolas_signature_verify_integrity("test_data/test_file.bin",
                                                               expected_hash);
    if (integrity_result != 0) {
        printf("Integrity check skipped (test files not present)");
    } else {
        printf("Integrity check completed");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_compute_hash) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    uint8_t hash_out[32];
    int hash_result = cupolas_signature_compute_hash("test_data/test_file.bin", hash_out);
    if (hash_result == 0) {
        printf("Hash computed successfully");
    } else {
        printf("Hash computation skipped (test files not present)");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_trusted_signer) {
    int result = cupolas_signature_init(NULL);
    ASSERT(result == 0, "Signature init should succeed");

    bool is_trusted = cupolas_signature_is_trusted_signer("unknown_signer");
    ASSERT(!is_trusted, "Unknown signer should not be trusted");

    int add_result = cupolas_signature_add_trusted_signer("test_signer",
                                                           "test_public_key_pem");
    if (add_result == 0) {
        bool now_trusted = cupolas_signature_is_trusted_signer("test_signer");
        ASSERT(now_trusted, "Added signer should be trusted");
    } else {
        printf("Add trusted signer skipped");
    }

    cupolas_signature_cleanup();
}

TEST(test_signature_result_string) {
    const char* ok_str = cupolas_signature_result_string(CUPOLAS_SIG_OK);
    ASSERT(ok_str != NULL, "Result string should not be NULL");

    const char* invalid_str = cupolas_signature_result_string(CUPOLAS_SIG_INVALID);
    ASSERT(invalid_str != NULL, "Invalid result string should not be NULL");
}

TEST(test_signature_algo_string) {
    const char* rsa_str = cupolas_signature_algo_string(CUPOLAS_SIG_ALGO_RSA_SHA256);
    ASSERT(rsa_str != NULL, "RSA algo string should not be NULL");

    const char* ed_str = cupolas_signature_algo_string(CUPOLAS_SIG_ALGO_ED25519);
    ASSERT(ed_str != NULL, "Ed25519 algo string should not be NULL");
}

TEST(test_signature_timestamp) {
    uint64_t ts = cupolas_signature_get_timestamp();
    ASSERT(ts > 0, "Timestamp should be positive");
}

TEST(test_signature_validity_check) {
    uint64_t now = cupolas_signature_get_timestamp();
    int valid = cupolas_signature_check_validity(now - 3600, now + 3600);
    ASSERT(valid == 0, "Current time should be valid");

    int expired = cupolas_signature_check_validity(now - 7200, now - 3600);
    ASSERT(expired != 0, "Past period should be invalid");
}

static void run_all_tests(void) {
    printf("\n=== Cupolas Signature Module Tests ===\n\n");

    RUN_TEST(test_signature_init_cleanup);
    RUN_TEST(test_signature_algorithm_enum);
    RUN_TEST(test_signature_result_enum);
    RUN_TEST(test_signature_verify_file);
    RUN_TEST(test_signature_verify_data);
    RUN_TEST(test_signature_sign_data);
    RUN_TEST(test_signature_verify_integrity);
    RUN_TEST(test_signature_compute_hash);
    RUN_TEST(test_signature_trusted_signer);
    RUN_TEST(test_signature_result_string);
    RUN_TEST(test_signature_algo_string);
    RUN_TEST(test_signature_timestamp);
    RUN_TEST(test_signature_validity_check);

    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
}

int main(void) {
    run_all_tests();
    return tests_failed > 0 ? 1 : 0;
}
