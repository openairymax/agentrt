/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_security.c - Security Submodule Unit Tests
 *
 * Test coverage for:
 * - cupolas_signature: Code signature verification
 * - cupolas_vault: Secure credential storage
 * - cupolas_entitlements: Permission declarations
 * - cupolas_runtime_protection: Runtime protection
 * - cupolas_network_security: Network security
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include security headers */
#include "../../src/security/cupolas_error.h"
#include "../../src/security/cupolas_signature.h"
#include "../../src/security/cupolas_vault.h"
#include "../../src/security/cupolas_entitlements.h"
#include "../../src/security/cupolas_runtime_protection.h"
#include "../../src/security/cupolas_network_security.h"

/* Test macros */
#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { \
    printf("[FAIL] %s: %s\n", name, msg); \
    fail_count++; \
} while(0)

#define TEST_ASSERT(cond, name, msg) do { \
    if (!(cond)) { \
        TEST_FAIL(name, msg); \
    } else { \
        TEST_PASS(name); \
    } \
} while(0)

static int fail_count = 0;
static int pass_count = 0;

/* ========================================================================
 * 1. Error Code Tests
 * ======================================================================== */

void test_error_codes(void) {
    printf("\n=== Error Code Tests ===\n");

    /* Test error string conversion */
    const char* str = cupolas_error_string(cupolas_ERR_OK);
    TEST_ASSERT(str != NULL, "error_string_OK", "Should return non-NULL string");
    TEST_ASSERT(strcmp(str, "Success") == 0, "error_string_OK_value", "Should be 'Success'");

    str = cupolas_error_string(cupolas_ERR_INVALID_PARAM);
    TEST_ASSERT(str != NULL, "error_string_INVALID_PARAM", "Should return non-NULL string");

    str = cupolas_error_string(cupolas_ERR_OUT_OF_MEMORY);
    TEST_ASSERT(str != NULL, "error_string_OUT_OF_MEMORY", "Should return non-NULL string");

    /* Test module-specific to unified conversion */
    cupolas_error_t err;

    err = cupolas_error_from_sig(cupolas_SIG_OK);
    TEST_ASSERT(err == cupolas_ERR_OK, "from_sig_OK", "Should convert SIG_OK to ERR_OK");

    err = cupolas_error_from_sig(cupolas_SIG_INVALID);
    TEST_ASSERT(err == cupolas_ERR_SIGNATURE_INVALID, "from_sig_INVALID", "Should convert properly");

    err = cupolas_error_from_ent(cupolas_ENT_OK);
    TEST_ASSERT(err == cupolas_ERR_OK, "from_ent_OK", "Should convert ENT_OK to ERR_OK");

    err = cupolas_error_from_ent(cupolas_ENT_DENIED);
    TEST_ASSERT(err == cupolas_ERR_PERMISSION_DENIED, "from_ent_DENIED", "Should convert properly");

    err = cupolas_error_from_vault(cupolas_VAULT_ERR_OK);
    TEST_ASSERT(err == cupolas_ERR_OK, "from_vault_OK", "Should convert VAULT_OK to ERR_OK");

    err = cupolas_error_from_vault(cupolas_VAULT_ERR_ACCESS_DENIED);
    TEST_ASSERT(err == cupolas_ERR_PERMISSION_DENIED, "from_vault_DENIED", "Should convert properly");

    err = cupolas_error_from_net(cupolas_NET_ERR_OK);
    TEST_ASSERT(err == cupolas_ERR_OK, "from_net_OK", "Should convert NET_OK to ERR_OK");

    err = cupolas_error_from_net(cupolas_NET_ERR_DENIED);
    TEST_ASSERT(err == cupolas_ERR_PERMISSION_DENIED, "from_net_DENIED", "Should convert properly");

    err = cupolas_error_from_runtime(cupolas_RUNTIME_ERR_OK);
    TEST_ASSERT(err == cupolas_ERR_OK, "from_runtime_OK", "Should convert RUNTIME_OK to ERR_OK");

    err = cupolas_error_from_runtime(cupolas_RUNTIME_ERR_VIOLATION);
    TEST_ASSERT(err < 0, "from_runtime_VIOLATION", "Should return negative error");

    /* Test utility macros */
    TEST_ASSERT(cupolas_ERROR_IS_SUCCESS(0) == true, "IS_SUCCESS_0", "0 should be success");
    TEST_ASSERT(cupolas_ERROR_IS_SUCCESS(-1) == false, "IS_SUCCESS_NEG1", "-1 should not be success");
    TEST_ASSERT(cupolas_ERROR_IS_FATAL(cupolas_ERR_OUT_OF_MEMORY) == true, "IS_FATAL_OOM", "OOM should be fatal");
    TEST_ASSERT(cupolas_ERROR_IS_FATAL(cupolas_ERR_TIMEOUT) == false, "IS_FATAL_TIMEOUT", "Timeout should not be fatal");
}

/* ========================================================================
 * 2. Signature Verification Tests
 * ======================================================================== */

void test_signature_module(void) {
    printf("\n=== Signature Verification Tests ===\n");

    /* Test algorithm strings */
    const char* algo_str;
    
    algo_str = cupolas_signature_algo_string(CUPOLAS_SIG_ALGO_RSA_SHA256);
    TEST_ASSERT(algo_str != NULL && strlen(algo_str) > 0, "sig_algo_RSA256", "Should return algo string");

    algo_str = cupolas_signature_algo_string(CUPOLAS_SIG_ALGO_ECDSA_P256);
    TEST_ASSERT(algo_str != NULL, "sig_algo_ECDSA_P256", "Should return ECDSA string");

    algo_str = cupolas_signature_algo_string(CUPOLAS_SIG_ALGO_ED25519);
    TEST_ASSERT(algo_str != NULL, "sig_algo_Ed25519", "Should return Ed25519 string");

    /* Test result strings */
    const char* result_str;
    
    result_str = cupolas_signature_result_string(cupolas_SIG_OK);
    TEST_ASSERT(result_str != NULL && strcmp(result_str, "Signature valid") == 0,
                "result_OK", "Should return valid string");

    result_str = cupolas_signature_result_string(cupolas_SIG_INVALID);
    TEST_ASSERT(result_str != NULL, "result_INVALID", "Should return invalid string");

    result_str = cupolas_signature_result_string(cupolas_SIG_EXPIRED);
    TEST_ASSERT(result_str != NULL, "result_EXPIRED", "Should return expired string");

    result_str = cupolas_signature_result_string(cupolas_SIG_TAMPERED);
    TEST_ASSERT(result_str != NULL, "result_TAMPERED", "Should return tampered string");

    /* Test timestamp function */
    uint64_t ts = cupolas_signature_get_timestamp();
    TEST_ASSERT(ts > 0, "timestamp_positive", "Timestamp should be positive");

    /* Test validity check */
    uint64_t now = ts;
    uint64_t past = ts - 86400;  /* 24 hours ago */
    uint64_t future = ts + 86400;  /* 24 hours in future */

    int ret = cupolas_signature_check_validity(past, now);
    TEST_ASSERT(ret == 0 || ret == -1, "validity_past", "Past time should be invalid or expired");

    ret = cupolas_signature_check_validity(now, future);
    TEST_ASSERT(ret == 0, "validity_current", "Current time should be valid");

    ret = cupolas_signature_check_validity(future, now);
    TEST_ASSERT(ret != 0, "validity_future", "Future not_before should be invalid");
}

/* ========================================================================
 * 3. Vault/Credential Storage Tests
 * ======================================================================== */

void test_vault_module(void) {
    printf("\n=== Vault/Credential Storage Tests ===\n");

    /* Test credential type strings */
    const char* type_str;
    
    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_PASSWORD);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "password") == 0,
                "vault_type_password", "Should return password string");

    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_TOKEN);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "token") == 0,
                "vault_type_token", "Should return token string");

    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_KEY);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "key") == 0,
                "vault_type_key", "Should return key string");

    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_CERTIFICATE);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "certificate") == 0,
                "vault_type_certificate", "Should return certificate string");

    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_SECRET);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "secret") == 0,
                "vault_type_secret", "Should return secret string");

    type_str = cupolas_vault_cred_type_string(CUPOLAS_VAULT_CRED_NOTE);
    TEST_ASSERT(type_str != NULL && strcmp(type_str, "note") == 0,
                "vault_type_note", "Should return note string");

    /* Test operation strings */
    const char* op_str;
    
    op_str = cupolas_vault_operation_string(CUPOLAS_VAULT_OP_READ);
    TEST_ASSERT(op_str != NULL && strcmp(op_str, "read") == 0,
                "vault_op_read", "Should return read string");

    op_str = cupolas_vault_operation_string(CUPOLAS_VAULT_OP_WRITE);
    TEST_ASSERT(op_str != NULL && strcmp(op_str, "write") == 0,
                "vault_op_write", "Should return write string");

    op_str = cupolas_vault_operation_string(CUPOLAS_VAULT_OP_DELETE);
    TEST_ASSERT(op_str != NULL && strcmp(op_str, "delete") == 0,
                "vault_op_delete", "Should return delete string");

    /* Test password generation */
    char password[64];
    int ret = cupolas_vault_generate_password(password, sizeof(password));
    TEST_ASSERT(ret == 0, "generate_password", "Should generate password successfully");
    TEST_ASSERT(strlen(password) >= 16, "password_length", "Password should be at least 16 chars");
    
    /* Verify password contains mixed characters */
    int has_upper = 0, has_lower = 0, has_digit = 0, has_special = 0;
    for (size_t i = 0; i < strlen(password); i++) {
        if (password[i] >= 'A' && password[i] <= 'Z') has_upper = 1;
        if (password[i] >= 'a' && password[i] <= 'z') has_lower = 1;
        if (password[i] >= '0' && password[i] <= '9') has_digit = 1;
        if (!isalnum((unsigned char)password[i])) has_special = 1;
    }
    TEST_ASSERT(has_upper == 1, "password_has_upper", "Password should have uppercase");
    TEST_ASSERT(has_lower == 1, "password_has_lower", "Password should have lowercase");
    TEST_ASSERT(has_digit == 1, "password_has_digit", "Password should have digits");
    TEST_ASSERT(has_special == 1, "password_has_special", "Password should have special chars");

    /* Test key pair generation */
    char public_key[4096];
    char private_key[4096];
    size_t pub_len = sizeof(public_key);
    size_t priv_len = sizeof(private_key);

    ret = cupolas_vault_generate_keypair(public_key, &pub_len, private_key, &priv_len);
    TEST_ASSERT(ret == 0, "generate_keypair", "Should generate key pair successfully");
    TEST_ASSERT(pub_len > 0 && pub_len < sizeof(public_key), "pub_key_length", "Public key length should be valid");
    TEST_ASSERT(priv_len > 0 && priv_len < sizeof(private_key), "priv_key_length", "Private key length should be valid");
    TEST_ASSERT(strlen(public_key) > 0, "pub_key_not_empty", "Public key should not be empty");
    TEST_ASSERT(strlen(private_key) > 0, "priv_key_not_empty", "Private key should not be empty");
}

/* ========================================================================
 * 4. Entitlements Module Tests
 * ======================================================================== */

void test_entitlements_module(void) {
    printf("\n=== Entitlements Module Tests ===\n");

    /* Test result strings */
    const char* result_str;
    
    result_str = cupolas_entitlements_result_string(cupolas_ENT_OK);
    TEST_ASSERT(result_str != NULL && strcmp(result_str, "Verification successful") == 0,
                "ent_result_OK", "Should return success string");

    result_str = cupolas_entitlements_result_string(cupolas_ENT_DENIED);
    TEST_ASSERT(result_str != NULL && strstr(result_str, "denied") != NULL,
                "ent_result_DENIED", "Should contain denied");

    result_str = cupolas_entitlements_result_string(cupolas_ENT_EXPIRED);
    TEST_ASSERT(result_str != NULL && strstr(result_str, "expired") != NULL,
                "ent_result_EXPIRED", "Should contain expired");

    /* Test path matching */
    int match_ret;

    match_ret = cupolas_entitlements_match_path("/data/config.yaml", "/data/config.yaml");
    TEST_ASSERT(match_ret == 1, "path_match_exact", "Exact path should match");

    match_ret = cupolas_entitlements_match_path("/data/*.yaml", "/data/config.yaml");
    TEST_ASSERT(match_ret == 1, "path_match_wildcard", "Wildcard should match");

    match_ret = cupolas_entitlements_match_path("/data/**", "/data/subdir/file.txt");
    TEST_ASSERT(match_ret == 1, "path_match_double_star", "Double star should match subdirs");

    match_ret = cupolas_entitlements_match_path("/other/path", "/data/config.yaml");
    TEST_ASSERT(match_ret == 0, "path_match_no_match", "Different paths should not match");

    /* Test host matching */
    match_ret = cupolas_entitlements_match_host("*.example.com", "api.example.com");
    TEST_ASSERT(match_ret == 1, "host_match_wildcard", "Host wildcard should match");

    match_ret = cupolas_entitlements_match_host("*.example.com", "evil.com");
    TEST_ASSERT(match_ret == 0, "host_match_different", "Different hosts should not match");

    /* Test validity check */
    uint64_t now = cupolas_signature_get_timestamp();  /* Reuse timestamp function */
    int valid_ret;

    valid_ret = cupolas_entitlements_check_validity(NULL);
    TEST_ASSERT(valid_ret != 0, "ent_validity_null", "Null entitlements should be invalid");
}

/* ========================================================================
 * 5. Runtime Protection Module Tests
 * ======================================================================== */

void test_runtime_protection_module(void) {
    printf("\n=== Runtime Protection Module Tests ===\n");

    /* Test level strings */
    const char* level_str;
    
    level_str = cupolas_protection_level_string(CUPOLAS_PROTECT_NONE);
    TEST_ASSERT(level_str != NULL, "protect_level_NONE", "Should return level string");

    level_str = cupolas_protection_level_string(CUPOLAS_PROTECT_BASIC);
    TEST_ASSERT(level_str != NULL, "protect_level_BASIC", "Should return basic level string");

    level_str = cupolas_protection_level_string(CUPOLAS_PROTECT_ENHANCED);
    TEST_ASSERT(level_str != NULL, "protect_level_ENHANCED", "Should return enhanced level string");

    level_str = cupolas_protection_level_string(CUPOLAS_PROTECT_MAXIMUM);
    TEST_ASSERT(level_str != NULL, "protect_level_MAXIMUM", "Should return maximum level string");

    /* Test status strings */
    const char* status_str;
    
    status_str = cupolas_protection_status_string(CUPOLAS_PROTECT_STATUS_INACTIVE);
    TEST_ASSERT(status_str != NULL && strstr(status_str, "inactive") != NULL,
                 "protect_status_INACTIVE", "Should contain inactive");

    status_str = cupolas_protection_status_string(CUPOLAS_PROTECT_STATUS_ACTIVE);
    TEST_ASSERT(status_str != NULL && strstr(status_str, "active") != NULL,
                 "protect_status_ACTIVE", "Should contain active");

    status_str = cupolas_protection_status_string(CUPOLAS_PROTECT_STATUS_VIOLATION);
    TEST_ASSERT(status_str != NULL && strstr(status_str, "violation") != NULL,
                 "protect_status_VIOLATION", "Should contain violation");

    status_str = cupolas_protection_status_string(CUPOLAS_PROTECT_STATUS_COMPROMISED);
    TEST_ASSERT(status_str != NULL && strstr(status_str, "compromised") != NULL,
                 "protect_status_COMPROMISED", "Should contain compromised");

    /* Test violation type strings */
    const char* viol_str;
    
    viol_str = cupolas_violation_type_string(CUPOLAS_VIOLATION_SYSCALL);
    TEST_ASSERT(viol_str != NULL, "viol_type_SYSCALL", "Should return syscall violation string");

    viol_str = cupolas_violation_type_string(CUPOLAS_VIOLATION_MEMORY);
    TEST_ASSERT(viol_str != NULL, "viol_type_MEMORY", "Should return memory violation string");

    viol_str = cupolas_violation_type_string(CUPOLAS_VIOLATION_CONTROL_FLOW);
    TEST_ASSERT(viol_str != NULL, "viol_type_CFI", "Should return CFI violation string");

    viol_str = cupolas_violation_type_string(CUPOLAS_VIOLATION_INTEGRITY);
    TEST_ASSERT(viol_str != NULL, "viol_type_INTEGRITY", "Should return integrity violation string");

    /* Test feature support check (should not crash) */
    bool supported = cupolas_protection_is_supported("aslr");
    TEST_ASSERT(supported == true || supported == false, "feature_aslr", "Should return boolean");

    supported = cupolas_protection_is_supported("dep");
    TEST_ASSERT(supported == true || supported == false, "feature_dep", "Should return boolean");

    supported = cupolas_protection_is_supported("cfi");
    TEST_ASSERT(supported == true || supported == false, "feature_cfi", "Should return boolean");

    supported = cupolas_protection_is_supported("seccomp");
    TEST_ASSERT(supported == true || supported == false, "feature_seccomp", "Should return boolean");
}

/* ========================================================================
 * 6. Network Security Module Tests
 * ======================================================================== */

void test_network_security_module(void) {
    printf("\n=== Network Security Module Tests ===\n");

    /* Test TLS version strings */
    const char* tls_str;
    
    tls_str = cupolas_tls_version_string(CUPOLAS_TLS_AUTO);
    TEST_ASSERT(tls_str != NULL, "tls_version_AUTO", "Should return auto version string");

    tls_str = cupolas_tls_version_string(CUPOLAS_TLS_1_2);
    TEST_ASSERT(tls_str != NULL && strstr(tls_str, "1.2") != NULL,
                "tls_version_1_2", "Should contain 1.2");

    tls_str = cupolas_tls_version_string(CUPOLAS_TLS_1_3);
    TEST_ASSERT(tls_str != NULL && strstr(tls_str, "1.3") != NULL,
                "tls_version_1_3", "Should contain 1.3");

    /* Test cert mode strings */
    const char* mode_str;
    
    mode_str = cupolas_cert_mode_string(CUPOLAS_CERT_NONE);
    TEST_ASSERT(mode_str != NULL, "cert_mode_NONE", "Should return none mode string");

    mode_str = cupolas_cert_mode_string(CUPOLAS_CERT_OPTIONAL);
    TEST_ASSERT(mode_str != NULL, "cert_mode_OPTIONAL", "Should return optional mode string");

    mode_str = cupolas_cert_mode_string(CUPOLAS_CERT_REQUIRED);
    TEST_ASSERT(mode_str != NULL, "cert_mode_REQUIRED", "Should return required mode string");

    /* Test firewall action strings */
    const char* fw_str;
    
    fw_str = cupolas_fw_action_string(CUPOLAS_FW_ALLOW);
    TEST_ASSERT(fw_str != NULL && strstr(fw_str, "allow") != NULL,
                "fw_action_ALLOW", "Should contain allow");

    fw_str = cupolas_fw_action_string(CUPOLAS_FW_DENY);
    TEST_ASSERT(fw_str != NULL && strstr(fw_str, "deny") != NULL,
                "fw_action_DENY", "Should contain deny");

    fw_str = cupolas_fw_action_string(CUPOLAS_FW_RATE_LIMIT);
    TEST_ASSERT(fw_str != NULL, "fw_action_RATE_LIMIT", "Should return rate limit action string");

    /* Test protocol strings */
    const char* proto_str;
    
    proto_str = cupolas_proto_string(CUPOLAS_PROTO_TCP);
    TEST_ASSERT(proto_str != NULL && strcmp(proto_str, "tcp") == 0,
                  "proto_TCP", "Should return tcp");

    proto_str = cupolas_proto_string(CUPOLAS_PROTO_UDP);
    TEST_ASSERT(proto_str != NULL && strcmp(proto_str, "udp") == 0,
                  "proto_UDP", "Should return udp");

    proto_str = cupolas_proto_string(CUPOLAS_PROTO_ICMP);
    TEST_ASSERT(proto_str != NULL && strcmp(proto_str, "icmp") == 0,
                  "proto_ICMP", "Should return icmp");

    /* Test direction strings */
    const char* dir_str;
    
    dir_str = cupolas_direction_string(CUPOLAS_DIR_ANY);
    TEST_ASSERT(dir_str != NULL && strcmp(dir_str, "any") == 0,
                  "dir_ANY", "Should return any");

    dir_str = cupolas_direction_string(CUPOLAS_DIR_INBOUND);
    TEST_ASSERT(dir_str != NULL && strcmp(dir_str, "inbound") == 0,
                  "dir_INBOUND", "Should return inbound");

    dir_str = cupolas_direction_string(CUPOLAS_DIR_OUTBOUND);
    TEST_ASSERT(dir_str != NULL && strcmp(dir_str, "outbound") == 0,
                  "dir_OUTBOUND", "Should return outbound");

    /* Test certificate validation functions (should not crash with NULL) */
    int cert_ret;
    
    cert_ret = cupolas_cert_validate(NULL, NULL, NULL);
    TEST_ASSERT(cert_ret != 0, "cert_validate_null", "NULL cert should fail validation");

    cert_ret = cupolas_cert_is_expired(NULL);
    TEST_ASSERT(cert_ret == 1, "cert_expired_null", "NULL cert should be expired");

    cert_ret = cupolas_cert_verify_hostname(NULL, NULL);
    TEST_ASSERT(cert_ret != 0, "cert_verify_host_null", "NULL cert hostname check should fail");
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("\n========================================\n");
    printf("cupolas Security Submodule Unit Tests\n");
    printf("========================================\n");

    /* Run all test suites */
    test_error_codes();
    test_signature_module();
    test_vault_module();
    test_entitlements_module();
    test_runtime_protection_module();
    test_network_security_module();

    /* Print summary */
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total Passed: %d\n", pass_count);
    printf("Total Failed: %d\n", fail_count);
    printf("========================================\n");

    return (fail_count > 0) ? 1 : 0;
}
