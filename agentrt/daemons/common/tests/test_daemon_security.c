/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * test_daemon_security.c - Daemon Security Module Unit Tests
 */

#include "../include/daemon_security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); return; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

static void test_init_null_config(void)
{
    TEST("Init with NULL config");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    int ret = daemon_security_init(NULL, &err);
    ASSERT(ret == 0, "init with NULL config should succeed");
    daemon_security_shutdown();
    PASS();
}

static void test_double_init(void)
{
    TEST("Double init is idempotent");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    int ret = daemon_security_init(NULL, &err);
    ASSERT(ret == 0, "first init");
    ret = daemon_security_init(NULL, &err);
    ASSERT(ret == 0, "second init should be idempotent");
    daemon_security_shutdown();
    PASS();
}

static void test_shutdown_without_init(void)
{
    TEST("Shutdown without init is safe");
    daemon_security_shutdown();
    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_llm_input_normal(void)
{
    TEST("Sanitize LLM input - normal text");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char output[256];
    const char *input = "Hello world, how are you today?";
    int ret = daemon_sanitize_llm_input(input, output, sizeof(output));
    ASSERT(ret == 0, "sanitize should succeed");
    ASSERT(strlen(output) > 0, "output should not be empty");
    ASSERT(strstr(output, "Hello") != NULL, "normal text should pass through");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_llm_input_dangerous(void)
{
    TEST("Sanitize LLM input - dangerous pattern");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char output[256];
    const char *input = "run command; rm -rf /";
    int ret = daemon_sanitize_llm_input(input, output, sizeof(output));
    ASSERT(ret != 0, "dangerous input should be rejected");
    ASSERT(strstr(output, "SANITIZED") != NULL || strstr(output, "rejected") != NULL,
           "output should indicate sanitization");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_llm_input_null_params(void)
{
    TEST("Sanitize LLM input - null params rejected");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char output[256];
    int ret = daemon_sanitize_llm_input(NULL, output, sizeof(output));
    ASSERT(ret != 0, "null input should be rejected");

    ret = daemon_sanitize_llm_input("test", NULL, sizeof(output));
    ASSERT(ret != 0, "null output should be rejected");

    ret = daemon_sanitize_llm_input("test", output, 0);
    ASSERT(ret != 0, "zero output_size should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_tool_params_normal(void)
{
    TEST("Sanitize tool params - normal");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char sanitized_tool[128];
    char sanitized_params[256];
    int ret = daemon_sanitize_tool_params("read_file", "{\"path\":\"/tmp/test\"}",
        sanitized_tool, sizeof(sanitized_tool),
        sanitized_params, sizeof(sanitized_params));
    ASSERT(ret == 0, "sanitize tool params should succeed");
    ASSERT(strstr(sanitized_tool, "read_file") != NULL, "tool name should be preserved");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_tool_params_dangerous(void)
{
    TEST("Sanitize tool params - dangerous pattern");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char sanitized_tool[128];
    char sanitized_params[256];
    int ret = daemon_sanitize_tool_params("exec", "; cat /etc/passwd",
        sanitized_tool, sizeof(sanitized_tool),
        sanitized_params, sizeof(sanitized_params));
    ASSERT(ret != 0, "dangerous params should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_tool_params_null(void)
{
    TEST("Sanitize tool params - null params rejected");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char buf[64];
    int ret = daemon_sanitize_tool_params(NULL, "{}", buf, 64, buf, 64);
    ASSERT(ret != 0, "null tool_name should be rejected");

    ret = daemon_sanitize_tool_params("tool", NULL, buf, 64, buf, 64);
    ASSERT(ret != 0, "null params should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_check_tool_permission_no_acl(void)
{
    TEST("Check tool permission - no ACL entry denies");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int allowed = daemon_check_tool_permission("agent_001", "file_read", "execute");
    ASSERT(allowed != 0, "no ACL entry should deny by default");

    daemon_security_shutdown();
    PASS();
}

static void test_check_tool_permission_null_params(void)
{
    TEST("Check tool permission - null params denied");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int allowed = daemon_check_tool_permission(NULL, "tool", "execute");
    ASSERT(allowed != 0, "null agent_id should be denied");

    allowed = daemon_check_tool_permission("agent", NULL, "execute");
    ASSERT(allowed != 0, "null tool_name should be denied");

    allowed = daemon_check_tool_permission("agent", "tool", NULL);
    ASSERT(allowed != 0, "null action should be denied");

    daemon_security_shutdown();
    PASS();
}

static void test_check_llm_permission_no_acl(void)
{
    TEST("Check LLM permission - no ACL entry denies");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int allowed = daemon_check_llm_permission("agent_001", "gpt-4", "query");
    ASSERT(allowed != 0, "no ACL entry should deny by default");

    daemon_security_shutdown();
    PASS();
}

static void test_verify_package_signature_not_found(void)
{
    TEST("Verify package signature - file not found");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    bool is_valid = true;
    int ret = daemon_verify_package_signature("/nonexistent/package.bin", &is_valid, NULL);
    ASSERT(ret != 0, "nonexistent package should return error");
    ASSERT(is_valid == false, "should be invalid");

    daemon_security_shutdown();
    PASS();
}

static void test_verify_package_signature_null(void)
{
    TEST("Verify package signature - null params rejected");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    bool is_valid;
    int ret = daemon_verify_package_signature(NULL, &is_valid, NULL);
    ASSERT(ret != 0, "null package_path should be rejected");

    ret = daemon_verify_package_signature("/some/path", NULL, NULL);
    ASSERT(ret != 0, "null is_valid should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_store_and_retrieve_credential(void)
{
    TEST("Store and retrieve credential");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    const uint8_t secret[] = "my_api_key_12345";
    int ret = daemon_store_credential("test_key", CUPOLAS_VAULT_CRED_TOKEN,
        secret, strlen((const char *)secret), "agent_001");
    ASSERT(ret == 0, "store credential should succeed");

    uint8_t retrieved[256];
    size_t data_len = sizeof(retrieved);
    ret = daemon_retrieve_credential("test_key", "agent_001", retrieved, &data_len);
    ASSERT(ret == 0, "retrieve should succeed");
    ASSERT(data_len == strlen((const char *)secret), "retrieved length should match");
    ASSERT(memcmp(retrieved, secret, data_len) == 0, "retrieved data should match");

    daemon_security_shutdown();
    PASS();
}

static void test_retrieve_nonexistent_credential(void)
{
    TEST("Retrieve nonexistent credential");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    uint8_t buf[64];
    size_t len = sizeof(buf);
    int ret = daemon_retrieve_credential("nonexistent_key", "agent_001", buf, &len);
    ASSERT(ret != 0, "retrieve nonexistent should fail");

    daemon_security_shutdown();
    PASS();
}

static void test_store_credential_null_params(void)
{
    TEST("Store credential - null params rejected");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    const uint8_t data[] = "secret";
    int ret = daemon_store_credential(NULL, CUPOLAS_VAULT_CRED_TOKEN,
        data, sizeof(data), "agent");
    ASSERT(ret != 0, "null cred_id should be rejected");

    ret = daemon_store_credential("key", CUPOLAS_VAULT_CRED_TOKEN,
        NULL, sizeof(data), "agent");
    ASSERT(ret != 0, "null data should be rejected");

    ret = daemon_store_credential("key", CUPOLAS_VAULT_CRED_TOKEN,
        data, 0, "agent");
    ASSERT(ret != 0, "zero data_len should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_audit_log_event(void)
{
    TEST("Audit log event");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int ret = daemon_audit_log_event("tool_d", "execute", "/tool/read_file", 0, "agent_001");
    ASSERT(ret == 0, "audit success event should succeed");

    ret = daemon_audit_log_event("llm_d", "query_failed", "/llm/gpt-4", -1, "agent_002");
    ASSERT(ret == 0, "audit failure event should succeed");

    daemon_security_shutdown();
    PASS();
}

static void test_audit_log_event_null_params(void)
{
    TEST("Audit log event - null params handled");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int ret = daemon_audit_log_event(NULL, "op", "res", 0, "agent");
    ASSERT(ret != 0, "null service_name should be rejected");

    ret = daemon_audit_log_event("svc", NULL, "res", 0, "agent");
    ASSERT(ret != 0, "null operation should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_get_status(void)
{
    TEST("Get security status");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int san, perm, sig, vault, audit;
    int ret = daemon_security_get_status(&san, &perm, &sig, &vault, &audit);
    ASSERT(ret == 0, "get_status should succeed");
    ASSERT(san == 1, "sanitizer should be active after init");
    ASSERT(perm == 1, "permission should be enabled");

    daemon_security_shutdown();
    PASS();
}

static void test_get_status_null_params(void)
{
    TEST("Get security status - null params rejected");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    int san;
    int ret = daemon_security_get_status(&san, NULL, NULL, NULL, NULL);
    ASSERT(ret != 0, "null params should be rejected");

    daemon_security_shutdown();
    PASS();
}

static void test_auto_init_on_use(void)
{
    TEST("Auto-init when used without explicit init");
    daemon_security_shutdown();

    char output[256];
    int ret = daemon_sanitize_llm_input("hello world", output, sizeof(output));
    ASSERT(ret == 0, "auto-init should work for sanitize");

    uint8_t buf[64];
    size_t len = sizeof(buf);
    ret = daemon_retrieve_credential("no_such_key", "agent", buf, &len);
    ASSERT(ret != 0, "auto-init should work for retrieve");

    daemon_security_shutdown();
    PASS();
}

static void test_init_with_config(void)
{
    TEST("Init with explicit config");
    daemon_security_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sanitize_level = SANITIZE_LEVEL_STRICT;
    cfg.enable_permission_cache = true;
    cfg.enable_signature_verification = true;
    cfg.enable_vault = true;
    cfg.enable_audit_logging = true;

    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    int ret = daemon_security_init(&cfg, &err);
    ASSERT(ret == 0, "init with config should succeed");

    int san, perm, sig, vault, audit;
    daemon_security_get_status(&san, &perm, &sig, &vault, &audit);
    ASSERT(san == 1, "sanitizer should be active");
    ASSERT(perm == 1, "permission should be active");

    daemon_security_shutdown();
    PASS();
}

static void test_store_credential_overwrite(void)
{
    TEST("Store credential overwrite existing");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    const uint8_t v1[] = "version_one";
    const uint8_t v2[] = "version_two_updated";
    int ret = daemon_store_credential("overwrite_key", CUPOLAS_VAULT_CRED_TOKEN,
        v1, strlen((const char *)v1), "agent");
    ASSERT(ret == 0, "first store");

    ret = daemon_store_credential("overwrite_key", CUPOLAS_VAULT_CRED_TOKEN,
        v2, strlen((const char *)v2), "agent");
    ASSERT(ret == 0, "overwrite should succeed");

    uint8_t retrieved[256];
    size_t len = sizeof(retrieved);
    ret = daemon_retrieve_credential("overwrite_key", "agent", retrieved, &len);
    ASSERT(ret == 0, "retrieve after overwrite");
    ASSERT(len == strlen((const char *)v2), "length should match v2");
    ASSERT(memcmp(retrieved, v2, len) == 0, "data should be v2 after overwrite");

    daemon_security_shutdown();
    PASS();
}

static void test_credential_access_control(void)
{
    TEST("Credential access control - wrong agent denied");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    const uint8_t secret[] = "owner_only_secret";
    daemon_store_credential("owned_key", CUPOLAS_VAULT_CRED_TOKEN,
        secret, strlen((const char *)secret), "owner_agent");

    uint8_t buf[256];
    size_t len = sizeof(buf);
    int ret = daemon_retrieve_credential("owned_key", "other_agent", buf, &len);
    ASSERT(ret != 0, "other agent should be denied");

    len = sizeof(buf);
    ret = daemon_retrieve_credential("owned_key", "system", buf, &len);
    ASSERT(ret == 0, "system agent should be allowed");

    daemon_security_shutdown();
    PASS();
}

static void test_sanitize_llm_input_special_chars(void)
{
    TEST("Sanitize LLM input - tab chars stripped");
    agentrt_error_t err;
    memset(&err, 0, sizeof(err));
    daemon_security_init(NULL, &err);

    char output[256];
    const char *input = "hello\tworld\twith\ttabs";
    int ret = daemon_sanitize_llm_input(input, output, sizeof(output));
    ASSERT(ret == 0, "sanitize should succeed");
    ASSERT(strchr(output, '\t') == NULL, "tabs should be removed");

    daemon_security_shutdown();
    PASS();
}

int main(void)
{
    printf("\n=== Daemon Security Module Unit Tests ===\n\n");

    test_init_null_config();
    test_double_init();
    test_shutdown_without_init();
    test_sanitize_llm_input_normal();
    test_sanitize_llm_input_dangerous();
    test_sanitize_llm_input_null_params();
    test_sanitize_tool_params_normal();
    test_sanitize_tool_params_dangerous();
    test_sanitize_tool_params_null();
    test_check_tool_permission_no_acl();
    test_check_tool_permission_null_params();
    test_check_llm_permission_no_acl();
    test_verify_package_signature_not_found();
    test_verify_package_signature_null();
    test_store_and_retrieve_credential();
    test_retrieve_nonexistent_credential();
    test_store_credential_null_params();
    test_audit_log_event();
    test_audit_log_event_null_params();
    test_get_status();
    test_get_status_null_params();
    test_auto_init_on_use();
    test_init_with_config();
    test_store_credential_overwrite();
    test_credential_access_control();
    test_sanitize_llm_input_special_chars();

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}