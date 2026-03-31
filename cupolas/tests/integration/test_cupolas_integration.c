/**
 * @file test_cupolas_integration.c
 * @brief cupolas 模块集成测试
 * @author Spharx
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/cupolas.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

/* ============================================================================
 * 集成测试：权限 + 审计
 * ============================================================================ */

static void test_permission_with_audit(void) {
    printf("\n--- Testing Permission with Audit ---\n");
    
    assert(cupolas_init(NULL) == cupolas_OK);
    
    cupolas_add_permission_rule("agent1", "read", "/data/*", 1, 100);
    cupolas_add_permission_rule("agent1", "write", "/data/*", 0, 100);
    
    int result = cupolas_check_permission("agent1", "read", "/data/file.txt", NULL);
    printf("  Permission check (read): %s\n", result ? "ALLOWED" : "DENIED");
    assert(result == 1);
    
    result = cupolas_check_permission("agent1", "write", "/data/file.txt", NULL);
    printf("  Permission check (write): %s\n", result ? "ALLOWED" : "DENIED");
    assert(result == 0);
    
    cupolas_flush_audit_log();
    cupolas_cleanup();
    
    TEST_PASS("permission_with_audit");
}

/* ============================================================================
 * 集成测试：净化 + 审计
 * ============================================================================ */

static void test_sanitize_with_audit(void) {
    printf("\n--- Testing Sanitize with Audit ---\n");
    
    assert(cupolas_init(NULL) == cupolas_OK);
    
    char output[256];
    
    int ret = cupolas_sanitize_input("Hello, World!", output, sizeof(output));
    printf("  Sanitize clean input: %s\n", output);
    assert(ret == cupolas_OK);
    assert(strcmp(output, "Hello, World!") == 0);
    
    ret = cupolas_sanitize_input("<script>alert(1)</script>", output, sizeof(output));
    printf("  Sanitize malicious input: %s\n", output);
    
    cupolas_flush_audit_log();
    cupolas_cleanup();
    
    TEST_PASS("sanitize_with_audit");
}

/* ============================================================================
 * 集成测试：完整工作流
 * ============================================================================ */

static void test_full_workflow(void) {
    printf("\n--- Testing Full Workflow ---\n");
    
    assert(cupolas_init(NULL) == cupolas_OK);
    
    printf("  Step 1: Add permission rules\n");
    cupolas_add_permission_rule("worker_agent", "execute", "/usr/bin/*", 1, 100);
    cupolas_add_permission_rule("worker_agent", "execute", "/bin/*", 1, 100);
    cupolas_add_permission_rule("worker_agent", "read", "/data/*", 1, 100);
    cupolas_add_permission_rule("worker_agent", "write", "/data/output/*", 1, 100);
    cupolas_add_permission_rule("worker_agent", "write", "/data/private/*", 0, 200);
    
    printf("  Step 2: Check permissions\n");
    int can_read = cupolas_check_permission("worker_agent", "read", "/data/input.txt", NULL);
    int can_write_public = cupolas_check_permission("worker_agent", "write", "/data/output/result.txt", NULL);
    int can_write_private = cupolas_check_permission("worker_agent", "write", "/data/private/secret.txt", NULL);
    
    printf("    Read /data/input.txt: %s\n", can_read ? "ALLOWED" : "DENIED");
    printf("    Write /data/output/result.txt: %s\n", can_write_public ? "ALLOWED" : "DENIED");
    printf("    Write /data/private/secret.txt: %s\n", can_write_private ? "ALLOWED" : "DENIED");
    
    assert(can_read == 1);
    assert(can_write_public == 1);
    assert(can_write_private == 0);
    
    printf("  Step 3: Sanitize user input\n");
    char sanitized[256];
    cupolas_sanitize_input("User provided input with <html> tags", sanitized, sizeof(sanitized));
    printf("    Sanitized: %s\n", sanitized);
    
    printf("  Step 4: Flush audit log\n");
    cupolas_flush_audit_log();
    
    cupolas_cleanup();
    
    TEST_PASS("full_workflow");
}

/* ============================================================================
 * 集成测试：并发场景模拟
 * ============================================================================ */

static void test_concurrent_access(void) {
    printf("\n--- Testing Concurrent Access Simulation ---\n");
    
    assert(cupolas_init(NULL) == cupolas_OK);
    
    cupolas_add_permission_rule("agent_*", "read", "/shared/*", 1, 100);
    
    printf("  Simulating multiple agents accessing shared resource...\n");
    for (int i = 0; i < 10; i++) {
        char agent_id[32];
        snprintf(agent_id, sizeof(agent_id), "agent_%d", i);
        
        int result = cupolas_check_permission(agent_id, "read", "/shared/data.txt", NULL);
        assert(result == 1);
    }
    printf("  All 10 agents successfully accessed shared resource\n");
    
    cupolas_cleanup();
    
    TEST_PASS("concurrent_access");
}

/* ============================================================================
 * 集成测试：错误处理
 * ============================================================================ */

static void test_error_handling(void) {
    printf("\n--- Testing Error Handling ---\n");
    
    printf("  Testing operations before init...\n");
    int result = cupolas_check_permission("agent1", "read", "/data", NULL);
    assert(result == 0);
    printf("    Permission check without init: DENIED (expected)\n");
    
    printf("  Testing with NULL parameters...\n");
    assert(cupolas_init(NULL) == cupolas_OK);
    result = cupolas_check_permission(NULL, "read", "/data", NULL);
    assert(result == 0);
    printf("    Permission check with NULL agent: DENIED (expected)\n");
    
    cupolas_cleanup();
    
    TEST_PASS("error_handling");
}

/* ============================================================================
 * 集成测试：版本信息
 * ============================================================================ */

static void test_version_info(void) {
    printf("\n--- Testing Version Info ---\n");
    
    const char* version = cupolas_version();
    printf("  cupolas version: %s\n", version);
    assert(version != NULL);
    assert(strlen(version) > 0);
    
    TEST_PASS("version_info");
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("cupolas Module Integration Tests\n");
    printf("========================================\n");
    
    test_version_info();
    test_permission_with_audit();
    test_sanitize_with_audit();
    test_full_workflow();
    test_concurrent_access();
    test_error_handling();
    
    printf("\n========================================\n");
    printf("All integration tests passed!\n");
    printf("========================================\n");
    
    return 0;
}
