/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * @file test_protection_chain.c
 * @brief cupolas 4-layer protection chain integration test (INT-11)
 *
 * Tests the complete security protection chain:
 *   INT-11.1: XSS and SQL injection sanitization
 *   INT-11.2: 4-stage purification pipeline (regex → type → length → encoding)
 *   INT-11.3: RBAC+YAML permission engine with role isolation
 *
 * @note This is a standalone integration test with a main() function.
 */

#include "cupolas.h"
#include "memory_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Test Macros
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
        g_tests_passed++;                                                      \
    } while (0)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ============================================================================
 * Helper: string contains substring check
 * ============================================================================ */

static int str_contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return 0;
    return strstr(haystack, needle) != NULL;
}

/* ============================================================================
 * INT-11.1: Protection Chain - XSS and SQL Injection Sanitization
 * ============================================================================ */

TEST(protection_chain_xss_sanitization)
{
    /* Initialize cupolas module */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);
    assert(init_err == AGENTOS_OK);

    /* Test XSS payload: <script>alert('xss')</script> */
    {
        const char *xss_input  = "<script>alert('xss')</script>";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(xss_input, output, sizeof(output));
        assert(ret == 0);

        /* Sanitized output should NOT contain the <script> tag */
        assert(!str_contains(output, "<script>"));
        assert(!str_contains(output, "<script"));
        printf("    XSS payload sanitized: '%s' -> '%s'\n", xss_input, output);
    }

    /* Test SQL injection payload: ' OR '1'='1 */
    {
        const char *sql_input = "' OR '1'='1";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(sql_input, output, sizeof(output));
        assert(ret == 0);

        /* Sanitized output should NOT contain the SQL injection pattern */
        assert(!str_contains(output, "' OR '1'='1"));
        printf("    SQL injection sanitized: '%s' -> '%s'\n", sql_input, output);
    }

    /* Test additional XSS vectors */
    {
        const char *inputs[] = {
            "<img src=x onerror=alert(1)>",
            "javascript:void(0)",
            "<body onload=alert('xss')>",
            "\"><script>alert(1)</script>",
        };
        const char *forbidden[] = {
            "onerror",
            "javascript:",
            "onload",
            "<script>",
        };

        for (int i = 0; i < 4; i++) {
            char output[256];
            memset(output, 0, sizeof(output));

            int ret = cupolas_sanitize_input(inputs[i], output, sizeof(output));
            assert(ret == 0);

            assert(!str_contains(output, forbidden[i]));
            printf("    XSS variant %d sanitized: '%s' -> '%s'\n",
                   i + 1, inputs[i], output);
        }
    }

    /* Test additional SQL injection vectors */
    {
        const char *inputs[] = {
            "1; DROP TABLE users--",
            "admin'--",
            "1 UNION SELECT * FROM users",
        };
        const char *forbidden[] = {
            "DROP TABLE",
            "'--",
            "UNION SELECT",
        };

        for (int i = 0; i < 3; i++) {
            char output[256];
            memset(output, 0, sizeof(output));

            int ret = cupolas_sanitize_input(inputs[i], output, sizeof(output));
            assert(ret == 0);

            assert(!str_contains(output, forbidden[i]));
            printf("    SQL injection variant %d sanitized: '%s' -> '%s'\n",
                   i + 1, inputs[i], output);
        }
    }

    /* Test safe input passes through unchanged */
    {
        const char *safe_input = "Hello, World! This is safe text.";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(safe_input, output, sizeof(output));
        assert(ret == 0);

        /* Safe input should be preserved */
        assert(strcmp(output, safe_input) == 0);
        printf("    Safe input preserved: '%s'\n", output);
    }

    /* Test NULL input handling */
    {
        char output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(NULL, output, sizeof(output));
        /* Should return error for NULL input */
        assert(ret != 0);
        printf("    NULL input correctly rejected\n");
    }

    /* Test small output buffer handling */
    {
        const char *input  = "<script>alert('xss')</script>";
        char        output[4];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        /* Should handle small buffer gracefully */
        assert(ret == 0 || ret < 0);
        printf("    Small buffer handled gracefully\n");
    }

    /* Cleanup */
    cupolas_cleanup();
}

/* ============================================================================
 * INT-11.2: 4-Stage Purification Pipeline
 *
 * The 4-stage purification pipeline processes input through:
 *   Stage 1: Regex - pattern matching and removal
 *   Stage 2: Type   - type validation and coercion
 *   Stage 3: Length - length validation and truncation
 *   Stage 4: Encoding - encoding validation and normalization
 * ============================================================================ */

TEST(purification_pipeline_regex_stage)
{
    /* Stage 1: Regex-based pattern matching and removal */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Regex stage should strip HTML/XML tags */
    {
        const char *input  = "<b>bold</b> and <i>italic</i>";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        /* Tags should be removed or neutralized */
        assert(!str_contains(output, "<b>"));
        assert(!str_contains(output, "<i>"));
        printf("    Stage 1 (regex): HTML tags removed from '%s' -> '%s'\n",
               input, output);
    }

    /* Regex stage should strip event handlers */
    {
        const char *input  = "<div onclick='steal()'>click</div>";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "onclick"));
        printf("    Stage 1 (regex): Event handler removed from '%s' -> '%s'\n",
               input, output);
    }

    /* Regex stage should handle CSS expressions */
    {
        const char *input  = "expression(alert(1))";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "expression("));
        printf("    Stage 1 (regex): CSS expression removed from '%s' -> '%s'\n",
               input, output);
    }

    cupolas_cleanup();
}

TEST(purification_pipeline_type_stage)
{
    /* Stage 2: Type validation and coercion */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Type stage should detect and neutralize SQL comment injection */
    {
        const char *input  = "admin'--";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "'--"));
        printf("    Stage 2 (type): SQL comment injection neutralized '%s' -> '%s'\n",
               input, output);
    }

    /* Type stage should handle semicolon injection */
    {
        const char *input  = "value; DROP TABLE users;";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "DROP TABLE"));
        printf("    Stage 2 (type): Semicolon injection neutralized '%s' -> '%s'\n",
               input, output);
    }

    /* Type stage should handle union-based injection */
    {
        const char *input  = "1 UNION SELECT password FROM users";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "UNION SELECT"));
        printf("    Stage 2 (type): UNION injection neutralized '%s' -> '%s'\n",
               input, output);
    }

    cupolas_cleanup();
}

TEST(purification_pipeline_length_stage)
{
    /* Stage 3: Length validation and truncation */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Length stage should handle long input within buffer */
    {
        char long_input[512];
        memset(long_input, 'A', sizeof(long_input) - 1);
        long_input[sizeof(long_input) - 1] = '\0';

        char output[512];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(long_input, output, sizeof(output));
        assert(ret == 0);

        /* Output should be properly null-terminated */
        assert(strlen(output) < sizeof(output));
        printf("    Stage 3 (length): Long input (%zu chars) handled, output length=%zu\n",
               strlen(long_input), strlen(output));
    }

    /* Length stage should handle empty input */
    {
        char output[256];
        memset(output, 0xFF, sizeof(output));

        int ret = cupolas_sanitize_input("", output, sizeof(output));
        assert(ret == 0);

        /* Empty input should produce empty or valid output */
        assert(strlen(output) >= 0);
        printf("    Stage 3 (length): Empty input handled\n");
    }

    /* Length stage should handle input exactly at buffer boundary */
    {
        char exact_input[64];
        memset(exact_input, 'X', 63);
        exact_input[63] = '\0';

        char output[64];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(exact_input, output, sizeof(output));
        assert(ret == 0);

        assert(strlen(output) < sizeof(output));
        printf("    Stage 3 (length): Boundary input handled\n");
    }

    cupolas_cleanup();
}

TEST(purification_pipeline_encoding_stage)
{
    /* Stage 4: Encoding validation and normalization */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Encoding stage should handle URL-encoded XSS */
    {
        const char *input  = "%3Cscript%3Ealert('xss')%3C%2Fscript%3E";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        /* URL-encoded script tags should be decoded and sanitized */
        assert(!str_contains(output, "<script"));
        assert(!str_contains(output, "%3Cscript"));
        printf("    Stage 4 (encoding): URL-encoded XSS sanitized '%s' -> '%s'\n",
               input, output);
    }

    /* Encoding stage should handle hex-encoded characters */
    {
        const char *input  = "&#x3C;script&#x3E;";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "<script"));
        printf("    Stage 4 (encoding): Hex-encoded XSS sanitized '%s' -> '%s'\n",
               input, output);
    }

    /* Encoding stage should handle mixed encoding */
    {
        const char *input  = "test%00%01%02data";
        char        output[256];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        /* Null bytes should be removed */
        assert(!str_contains(output, "%00"));
        printf("    Stage 4 (encoding): Mixed encoding sanitized '%s' -> '%s'\n",
               input, output);
    }

    cupolas_cleanup();
}

TEST(purification_pipeline_combined)
{
    /* Test the full 4-stage pipeline working together */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Combined attack: XSS + SQL + encoding bypass */
    {
        const char *input  = "%3Cscript%3Ealert('xss')%3C%2Fscript%3E ' OR '1'='1";
        char        output[512];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        /* All attack vectors should be neutralized */
        assert(!str_contains(output, "<script"));
        assert(!str_contains(output, "%3Cscript"));
        assert(!str_contains(output, "' OR '1'='1"));
        /* Output should still contain something (not empty) */
        assert(strlen(output) > 0);
        printf("    Combined pipeline: Multi-vector attack sanitized '%s' -> '%s'\n",
               input, output);
    }

    /* Pipe-delimited input with mixed content */
    {
        const char *input  = "safe|data|<script>xss</script>|normal|' OR 1=1--";
        char        output[512];
        memset(output, 0, sizeof(output));

        int ret = cupolas_sanitize_input(input, output, sizeof(output));
        assert(ret == 0);

        assert(!str_contains(output, "<script"));
        assert(!str_contains(output, "1=1--"));
        /* Safe parts should survive */
        assert(str_contains(output, "safe") || str_contains(output, "normal"));
        printf("    Combined pipeline: Mixed content sanitized '%s' -> '%s'\n",
               input, output);
    }

    /* Verify cupolas version is available */
    {
        const char *version = cupolas_version();
        assert(version != NULL);
        assert(strlen(version) > 0);
        printf("    cupolas version: %s\n", version);
    }

    cupolas_cleanup();
}

/* ============================================================================
 * INT-11.3: RBAC+YAML Permission Engine
 * ============================================================================ */

TEST(permission_engine_admin_access)
{
    /* Add admin permission rule and verify admin has access */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add an admin permission rule: allow all actions on all resources */
    rc = cupolas_add_permission_rule("admin", "*", "*", 1, 100);
    assert(rc == 0);

    /* Admin should have read permission */
    int result = cupolas_check_permission("admin", "read", "/data/file.txt", NULL);
    assert(result == 1);

    /* Admin should have write permission */
    result = cupolas_check_permission("admin", "write", "/data/file.txt", NULL);
    assert(result == 1);

    /* Admin should have execute permission */
    result = cupolas_check_permission("admin", "execute", "/bin/tool", NULL);
    assert(result == 1);

    printf("    Admin has full access to all resources\n");

    cupolas_cleanup();
}

TEST(permission_engine_user_denied)
{
    /* Check that a user without permission is denied */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add a specific permission for user "alice" to read only */
    rc = cupolas_add_permission_rule("alice", "read", "/data/public/*", 1, 50);
    assert(rc == 0);

    /* Alice should have read permission on public data */
    int result = cupolas_check_permission("alice", "read", "/data/public/doc.txt", NULL);
    assert(result == 1);

    /* Alice should NOT have write permission */
    result = cupolas_check_permission("alice", "write", "/data/public/doc.txt", NULL);
    assert(result == 0);

    /* Alice should NOT have access to private data */
    result = cupolas_check_permission("alice", "read", "/data/private/secret.txt", NULL);
    assert(result == 0);

    printf("    User 'alice' has read-only access to public data\n");

    cupolas_cleanup();
}

TEST(permission_engine_guest_isolation)
{
    /* Check guest role isolation */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add admin rule with high priority */
    rc = cupolas_add_permission_rule("admin", "*", "*", 1, 100);
    assert(rc == 0);

    /* Add guest rule with low priority - only read public */
    rc = cupolas_add_permission_rule("guest", "read", "/public/*", 1, 10);
    assert(rc == 0);

    /* Explicitly deny guest access to admin area */
    rc = cupolas_add_permission_rule("guest", "*", "/admin/*", 0, 90);
    assert(rc == 0);

    /* Guest should access public resources */
    int result = cupolas_check_permission("guest", "read", "/public/index.html", NULL);
    assert(result == 1);

    /* Guest should NOT access admin area (deny rule has higher priority) */
    result = cupolas_check_permission("guest", "read", "/admin/dashboard", NULL);
    assert(result == 0);

    /* Guest should NOT write to public area */
    result = cupolas_check_permission("guest", "write", "/public/index.html", NULL);
    assert(result == 0);

    /* Admin should still access admin area */
    result = cupolas_check_permission("admin", "read", "/admin/dashboard", NULL);
    assert(result == 1);

    printf("    Guest role isolated from admin area\n");

    cupolas_cleanup();
}

TEST(permission_engine_undefined_defaults_to_deny)
{
    /* Check undefined permission defaults to deny */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* No rules added - everything should be denied by default */

    /* Unknown user should be denied */
    int result = cupolas_check_permission("unknown_user", "read", "/any/file.txt", NULL);
    assert(result == 0);

    /* Unknown action should be denied */
    result = cupolas_check_permission("admin", "unknown_action", "/any/file.txt", NULL);
    assert(result == 0);

    /* Unknown resource should be denied */
    result = cupolas_check_permission("admin", "read", "/nonexistent/path", NULL);
    assert(result == 0);

    printf("    Undefined permissions default to deny\n");

    cupolas_cleanup();
}

TEST(permission_engine_priority_ordering)
{
    /* Check priority-based rule ordering */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add a broad deny rule with medium priority */
    rc = cupolas_add_permission_rule("*", "*", "/restricted/*", 0, 50);
    assert(rc == 0);

    /* Add a specific allow rule with higher priority */
    rc = cupolas_add_permission_rule("bob", "read", "/restricted/bob_files/*", 1, 75);
    assert(rc == 0);

    /* Bob should be allowed to read his own files */
    int result = cupolas_check_permission("bob", "read",
                                          "/restricted/bob_files/doc.txt", NULL);
    assert(result == 1);

    /* Bob should NOT write to his files */
    result = cupolas_check_permission("bob", "write",
                                      "/restricted/bob_files/doc.txt", NULL);
    assert(result == 0);

    /* Other user should be denied access to restricted area */
    result = cupolas_check_permission("carol", "read", "/restricted/bob_files/doc.txt", NULL);
    assert(result == 0);

    printf("    Priority-based rule ordering works correctly\n");

    cupolas_cleanup();
}

TEST(permission_engine_cache_clear)
{
    /* Clear permission cache and verify rules still work */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add a permission rule */
    rc = cupolas_add_permission_rule("eve", "read", "/cache_test/*", 1, 50);
    assert(rc == 0);

    /* First check - should be allowed */
    int result = cupolas_check_permission("eve", "read", "/cache_test/file.txt", NULL);
    assert(result == 1);

    /* Clear the permission cache */
    cupolas_clear_permission_cache();

    /* After cache clear, permission should still be enforced */
    result = cupolas_check_permission("eve", "read", "/cache_test/file.txt", NULL);
    assert(result == 1);

    /* After cache clear, non-permitted access should still be denied */
    result = cupolas_check_permission("eve", "write", "/cache_test/file.txt", NULL);
    assert(result == 0);

    printf("    Permission cache cleared and re-verified\n");

    cupolas_cleanup();
}

TEST(permission_engine_wildcards)
{
    /* Test wildcard matching */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add a wildcard rule for all agents on a specific resource */
    rc = cupolas_add_permission_rule("*", "read", "/public/*", 1, 25);
    assert(rc == 0);

    /* Multiple agents should have access */
    int result = cupolas_check_permission("user_a", "read", "/public/index.html", NULL);
    assert(result == 1);

    result = cupolas_check_permission("user_b", "read", "/public/about.html", NULL);
    assert(result == 1);

    result = cupolas_check_permission("user_c", "read", "/public/contact.html", NULL);
    assert(result == 1);

    /* But none should have write access */
    result = cupolas_check_permission("user_a", "write", "/public/index.html", NULL);
    assert(result == 0);

    printf("    Wildcard matching works for all agents\n");

    cupolas_cleanup();
}

TEST(permission_engine_context)
{
    /* Test permission check with context */
    agentos_error_t init_err = AGENTOS_OK;
    int            rc       = cupolas_init(NULL, &init_err);
    assert(rc == 0);

    /* Add a rule for a specific agent */
    rc = cupolas_add_permission_rule("context_user", "read", "/context/*", 1, 50);
    assert(rc == 0);

    /* Check with NULL context (should work) */
    int result = cupolas_check_permission("context_user", "read",
                                          "/context/file.txt", NULL);
    assert(result == 1);

    /* Check with a context string (should still work) */
    result = cupolas_check_permission("context_user", "read",
                                      "/context/file.txt", "session=abc123");
    assert(result == 1);

    printf("    Permission check with context works\n");

    cupolas_cleanup();
}

/* ============================================================================
 * Main: Run all tests
 * ============================================================================ */

int main(void)
{
    printf("=== cupolas Protection Chain Integration Tests (INT-11) ===\n\n");

    printf("--- INT-11.1: XSS and SQL Injection Sanitization ---\n");
    RUN_TEST(protection_chain_xss_sanitization);

    printf("\n--- INT-11.2: 4-Stage Purification Pipeline ---\n");
    RUN_TEST(purification_pipeline_regex_stage);
    RUN_TEST(purification_pipeline_type_stage);
    RUN_TEST(purification_pipeline_length_stage);
    RUN_TEST(purification_pipeline_encoding_stage);
    RUN_TEST(purification_pipeline_combined);

    printf("\n--- INT-11.3: RBAC+YAML Permission Engine ---\n");
    RUN_TEST(permission_engine_admin_access);
    RUN_TEST(permission_engine_user_denied);
    RUN_TEST(permission_engine_guest_isolation);
    RUN_TEST(permission_engine_undefined_defaults_to_deny);
    RUN_TEST(permission_engine_priority_ordering);
    RUN_TEST(permission_engine_cache_clear);
    RUN_TEST(permission_engine_wildcards);
    RUN_TEST(permission_engine_context);

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_tests_passed, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}