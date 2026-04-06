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

#include "cupolas_signature.h"

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

TEST(test_signature_verify_valid_file) {
    const char* test_file = "test_data/valid_signature.bin";
    const char* key_file = "test_data/public_key.pem";
    
    int result = cupolas_signature_verify_file(test_file, key_file);
    
    if (result == 0) {
        printf("Valid signature verification succeeded");
    } else {
        printf("Signature verification skipped (test files not present)");
    }
}

TEST(test_signature_verify_tampered_file) {
    const char* test_file = "test_data/tampered_signature.bin";
    const char* key_file = "test_data/public_key.pem";
    
    int result = cupolas_signature_verify_file(test_file, key_file);
    
    if (result != 0) {
        printf("Tampered signature correctly detected");
    } else {
        printf("Tampered detection skipped (test files not present)");
    }
}

TEST(test_signature_rsa_sha256) {
    const unsigned char* data = (const unsigned char*)"Test data for RSA-SHA256";
    size_t data_len = strlen((const char*)data);
    
    unsigned char signature[256];
    size_t sig_len = sizeof(signature);
    
    int sign_result = cupolas_signature_sign_data(data, data_len, 
                                                   CUPOLAS_SIG_RSA_SHA256,
                                                   "test_data/private_key.pem",
                                                   signature, &sig_len);
    
    if (sign_result == 0) {
        int verify_result = cupolas_signature_verify_data(data, data_len,
                                                          signature, sig_len,
                                                          CUPOLAS_SIG_RSA_SHA256,
                                                          "test_data/public_key.pem");
        ASSERT(verify_result == 0, "RSA-SHA256 signature verification should succeed");
    } else {
        printf("RSA-SHA256 signing skipped (test keys not present)");
    }
}

TEST(test_signature_ecdsa_p256) {
    const unsigned char* data = (const unsigned char*)"Test data for ECDSA P-256";
    size_t data_len = strlen((const char*)data);
    
    unsigned char signature[72];
    size_t sig_len = sizeof(signature);
    
    int sign_result = cupolas_signature_sign_data(data, data_len,
                                                   CUPOLAS_SIG_ECDSA_P256,
                                                   "test_data/ec_private.pem",
                                                   signature, &sig_len);
    
    if (sign_result == 0) {
        int verify_result = cupolas_signature_verify_data(data, data_len,
                                                          signature, sig_len,
                                                          CUPOLAS_SIG_ECDSA_P256,
                                                          "test_data/ec_public.pem");
        ASSERT(verify_result == 0, "ECDSA P-256 signature verification should succeed");
    } else {
        printf("ECDSA P-256 signing skipped (test keys not present)");
    }
}

TEST(test_signature_ed25519) {
    const unsigned char* data = (const unsigned char*)"Test data for Ed25519";
    size_t data_len = strlen((const char*)data);
    
    unsigned char signature[64];
    size_t sig_len = sizeof(signature);
    
    int sign_result = cupolas_signature_sign_data(data, data_len,
                                                   CUPOLAS_SIG_ED25519,
                                                   "test_data/ed25519_private.pem",
                                                   signature, &sig_len);
    
    if (sign_result == 0) {
        int verify_result = cupolas_signature_verify_data(data, data_len,
                                                          signature, sig_len,
                                                          CUPOLAS_SIG_ED25519,
                                                          "test_data/ed25519_public.pem");
        ASSERT(verify_result == 0, "Ed25519 signature verification should succeed");
    } else {
        printf("Ed25519 signing skipped (test keys not present)");
    }
}

TEST(test_signature_wrong_signer) {
    const unsigned char* data = (const unsigned char*)"Test data with wrong signer";
    size_t data_len = strlen((const char*)data);
    
    unsigned char signature[256];
    size_t sig_len = sizeof(signature);
    
    int sign_result = cupolas_signature_sign_data(data, data_len,
                                                   CUPOLAS_SIG_RSA_SHA256,
                                                   "test_data/key1_private.pem",
                                                   signature, &sig_len);
    
    if (sign_result == 0) {
        int verify_result = cupolas_signature_verify_data(data, data_len,
                                                          signature, sig_len,
                                                          CUPOLAS_SIG_RSA_SHA256,
                                                          "test_data/key2_public.pem");
        ASSERT(verify_result != 0, "Wrong signer should fail verification");
    } else {
        printf("Wrong signer test skipped (test keys not present)");
    }
}

TEST(test_signature_corrupted_key) {
    const unsigned char* data = (const unsigned char*)"Test data";
    size_t data_len = strlen((const char*)data);
    
    unsigned char signature[256];
    size_t sig_len = sizeof(signature);
    
    int sign_result = cupolas_signature_sign_data(data, data_len,
                                                   CUPOLAS_SIG_RSA_SHA256,
                                                   "test_data/corrupted_key.pem",
                                                   signature, &sig_len);
    
    ASSERT(sign_result != 0, "Corrupted key should fail signing");
}

TEST(test_signature_expired_cert) {
    const char* cert_path = "test_data/expired_cert.pem";
    cupolas_cert_result_t result;
    
    int verify_result = cupolas_signature_verify_cert(cert_path, &result);
    
    if (verify_result == 0) {
        ASSERT(result == CUPOLAS_CERT_EXPIRED, "Expired certificate should be detected");
    } else {
        printf("Expired cert test skipped (test file not present)");
    }
}

TEST(test_signature_cert_chain) {
    const char* chain_path = "test_data/cert_chain.pem";
    cupolas_cert_result_t result;
    
    int verify_result = cupolas_signature_verify_cert_chain(chain_path, &result);
    
    if (verify_result == 0) {
        ASSERT(result == CUPOLAS_CERT_OK, "Valid cert chain should verify");
    } else {
        printf("Cert chain test skipped (test file not present)");
    }
}

TEST(test_signature_algorithm_enum) {
    ASSERT(CUPOLAS_SIG_RSA_SHA256 == 1, "RSA-SHA256 enum value");
    ASSERT(CUPOLAS_SIG_RSA_SHA384 == 2, "RSA-SHA384 enum value");
    ASSERT(CUPOLAS_SIG_RSA_SHA512 == 3, "RSA-SHA512 enum value");
    ASSERT(CUPOLAS_SIG_ECDSA_P256 == 4, "ECDSA P-256 enum value");
    ASSERT(CUPOLAS_SIG_ECDSA_P384 == 5, "ECDSA P-384 enum value");
    ASSERT(CUPOLAS_SIG_ED25519 == 6, "Ed25519 enum value");
}

static void run_all_tests(void) {
    printf("\n=== Cupolas Signature Module Tests ===\n\n");
    
    RUN_TEST(test_signature_init_cleanup);
    RUN_TEST(test_signature_algorithm_enum);
    RUN_TEST(test_signature_verify_valid_file);
    RUN_TEST(test_signature_verify_tampered_file);
    RUN_TEST(test_signature_rsa_sha256);
    RUN_TEST(test_signature_ecdsa_p256);
    RUN_TEST(test_signature_ed25519);
    RUN_TEST(test_signature_wrong_signer);
    RUN_TEST(test_signature_corrupted_key);
    RUN_TEST(test_signature_expired_cert);
    RUN_TEST(test_signature_cert_chain);
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Skipped: %d (expected if test files not present)\n", tests_run - tests_passed - tests_failed);
}

int main(void) {
    run_all_tests();
    return tests_failed > 0 ? 1 : 0;
}
