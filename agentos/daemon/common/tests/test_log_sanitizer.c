/**
 * @file test_log_sanitizer.c
 * @brief 日志脱敏过滤器单元测试 (P1-C06)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "log_sanitizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        fprintf(stderr, "  FAIL: %s\n", msg); \
    } \
} while (0)

static void test_init_destroy(void)
{
    printf("  [01] init/destroy lifecycle\n");
    log_sanitizer_init(16);
    log_sanitizer_destroy();
    TEST_ASSERT(1, "init/destroy should not crash");
}

static void test_default_patterns(void)
{
    printf("  [02] get default patterns\n");
    size_t count = 0;
    const sensitive_field_t *patterns = log_get_default_patterns(&count);
    TEST_ASSERT(patterns != NULL, "patterns should not be NULL");
    TEST_ASSERT(count > 0, "should have default patterns");
}

static void test_add_custom_pattern(void)
{
    printf("  [03] add custom pattern\n");
    log_sanitizer_init(16);
    bool ok = log_sanitizer_add_pattern("custom_secret", "***");
    TEST_ASSERT(ok, "add custom pattern should succeed");
    log_sanitizer_destroy();
}

static void test_sanitize_basic(void)
{
    printf("  [04] sanitize basic message (no sensitive data)\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "This is a normal log message";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strcmp(buffer, msg) == 0, "normal message should be unchanged");
    log_sanitizer_destroy();
}

static void test_sanitize_api_key(void)
{
    printf("  [05] sanitize api_key in message\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "Request with api_key=sk-abc123def456";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strstr(buffer, "sk-abc123def456") == NULL, "api_key value should be masked");
    TEST_ASSERT(strstr(buffer, "***") != NULL, "should contain replacement mask");
    log_sanitizer_destroy();
}

static void test_sanitize_password(void)
{
    printf("  [06] sanitize password in message\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "User login: password=mysecret123";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strstr(buffer, "mysecret123") == NULL, "password value should be masked");
    log_sanitizer_destroy();
}

static void test_sanitize_token(void)
{
    printf("  [07] sanitize token in message\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "Authorization: token=eyJhbGciOiJIUzI1NiJ9.xxx";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strstr(buffer, "eyJhbGci") == NULL, "token value should be masked");
    log_sanitizer_destroy();
}

static void test_sanitize_secret(void)
{
    printf("  [08] sanitize secret in message\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "secret=abcdefghijklmnop";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strstr(buffer, "abcdefghijklmnop") == NULL, "secret value should be masked");
    log_sanitizer_destroy();
}

static void test_sanitize_multiple(void)
{
    printf("  [09] sanitize multiple sensitive fields\n");
    log_sanitizer_init(16);
    char buffer[512];
    const char *msg = "password=pass1&api_key=key1&secret=sec1&token=tok1";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should succeed");
    TEST_ASSERT(strstr(buffer, "pass1") == NULL, "password should be masked");
    TEST_ASSERT(strstr(buffer, "key1") == NULL, "api_key should be masked");
    TEST_ASSERT(strstr(buffer, "sec1") == NULL, "secret should be masked");
    TEST_ASSERT(strstr(buffer, "tok1") == NULL, "token should be masked");
    log_sanitizer_destroy();
}

static void test_contains_sensitive(void)
{
    printf("  [10] check contains sensitive data\n");
    log_sanitizer_init(16);
    TEST_ASSERT(log_contains_sensitive("api_key=secret123"), "should detect api_key");
    TEST_ASSERT(log_contains_sensitive("password=pass123"), "should detect password");
    TEST_ASSERT(!log_contains_sensitive("normal log message"), "should not detect in normal message");
    TEST_ASSERT(!log_contains_sensitive(""), "empty string should not be sensitive");
    log_sanitizer_destroy();
}

static void test_sanitize_dup(void)
{
    printf("  [11] sanitize dup with dynamic allocation\n");
    log_sanitizer_init(16);
    const char *msg = "api_key=secret-dup-test";
    char *result = log_sanitize_dup(msg);
    TEST_ASSERT(result != NULL, "sanitize_dup should succeed");
    TEST_ASSERT(strstr(result, "secret-dup-test") == NULL, "sensitive value should be masked");
    free(result);
    log_sanitizer_destroy();
}

static void test_sanitize_null(void)
{
    printf("  [12] sanitize NULL input\n");
    log_sanitizer_init(16);
    char buffer[64];
    int len = log_sanitize(NULL, buffer, sizeof(buffer));
    TEST_ASSERT(len < 0, "NULL input should return error");
    log_sanitizer_destroy();
}

static void test_sanitize_small_buffer(void)
{
    printf("  [13] sanitize with small buffer\n");
    log_sanitizer_init(16);
    char buffer[8];
    const char *msg = "api_key=secret123";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len != -2, "should not crash with small buffer");
    log_sanitizer_destroy();
}

static void test_multiple_init(void)
{
    printf("  [14] multiple init calls\n");
    log_sanitizer_init(16);
    log_sanitizer_init(32);
    TEST_ASSERT(1, "multiple init should not crash");
    log_sanitizer_destroy();
}

static void test_case_insensitive(void)
{
    printf("  [15] case insensitive pattern matching\n");
    log_sanitizer_init(16);
    char buffer[256];
    const char *msg = "API_KEY=secret123";
    int len = log_sanitize(msg, buffer, sizeof(buffer));
    TEST_ASSERT(len > 0, "sanitize should handle case");
    TEST_ASSERT(strstr(buffer, "secret123") == NULL, "should mask regardless of case");
    log_sanitizer_destroy();
}

int main(void)
{
    printf("=== log_sanitizer Unit Tests ===\n\n");

    test_init_destroy();
    test_default_patterns();
    test_add_custom_pattern();
    test_sanitize_basic();
    test_sanitize_api_key();
    test_sanitize_password();
    test_sanitize_token();
    test_sanitize_secret();
    test_sanitize_multiple();
    test_contains_sensitive();
    test_sanitize_dup();
    test_sanitize_null();
    test_sanitize_small_buffer();
    test_multiple_init();
    test_case_insensitive();

    printf("\n=== Results: %d run, %d passed, %d failed ===\n",
           tests_run, tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}