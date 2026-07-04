/**
 * @file test_sandbox_integration.c
 * @brief P3.18 (ACC-DT27): tool_d sandbox 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验证 ACC-DT27 验收标准：
 *   1. sandbox ALLOW 路径 — agentos_sandbox_invoke(SYS_TOOL_EXECUTE) 执行 /usr/bin/echo 成功
 *   2. sandbox DENY 路径 — PERM_DENY 规则使工具执行被拒绝（EACCES）
 *   3. sandbox NULL fail-closed — sandbox 句柄为 NULL 时 invoke 返回 EINVAL
 *   4. tool_executor 集成 — tool_executor_create 内部初始化 sandbox 不破坏现有行为
 *
 * 双层 fail-closed 安全架构验证：
 *   - SafetyGuard 审批（approval_ctx）+ Sandbox 拦截（permission/quota/audit）
 *   - sandbox 为 NULL（初始化失败）时 tool_executor_run 拒绝执行任何工具
 *
 * @note 不使用 assert() 执行副作用操作：Release 构建类型定义 NDEBUG，会将
 *       assert(expr) 展开为 ((void)0)，导致 expr 中的函数调用根本不执行。
 *       所有副作用操作必须用显式 if 检查 + TEST_FAIL。详见 project_memory.md
 *       的 assert/NDEBUG heisenbug 教训。
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "agentos_sandbox.h"
#include "executor.h"
#include "memory_compat.h"
#include "syscalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 测试统计 ========== */

static int test_count = 0;
static int pass_count = 0;

#define TEST_PASS() do { pass_count++; test_count++; } while (0)
#define TEST_FAIL(msg) do { printf("    FAIL: %s\n", msg); test_count++; } while (0)

/* ========== 辅助：构造 echo 工具参数 ========== */

static tool_execute_args_t *make_echo_args(const char *message, uint32_t timeout_ms)
{
    tool_execute_args_t *t = (tool_execute_args_t *)malloc(sizeof(tool_execute_args_t));
    if (!t)
        return NULL;

    /* argv: {"/usr/bin/echo", "<message>", NULL}
     * argv[0] 习惯为程序名，execvp 会用 executable 字段定位二进制。 */
    char **argv = (char **)malloc(3 * sizeof(char *));
    if (!argv) {
        free(t);
        return NULL;
    }
    argv[0] = strdup("/usr/bin/echo");
    argv[1] = strdup(message);
    argv[2] = NULL;
    if (!argv[0] || !argv[1]) {
        free(argv[0]);
        free(argv[1]);
        free(argv);
        free(t);
        return NULL;
    }

    t->executable = "/usr/bin/echo";
    t->argv = (char *const *)argv;
    t->timeout_ms = timeout_ms;
    t->cap_size = 4096;
    t->output_buffer = (char *)malloc(t->cap_size);
    if (!t->output_buffer) {
        free(argv[0]);
        free(argv[1]);
        free(argv);
        free(t);
        return NULL;
    }
    t->output_buffer[0] = '\0';
    t->exec_result = -999; /* 哨兵值：未执行 */
    return t;
}

static void free_echo_args(tool_execute_args_t *t)
{
    if (!t)
        return;
    if (t->argv) {
        free(t->argv[0]);
        free(t->argv[1]);
        free((void *)t->argv);
    }
    free(t->output_buffer);
    free(t);
}

/* ========== TEST 1: sandbox ALLOW 路径 — 执行 echo 成功 ==========
 *
 * 验证：
 *   - agentos_sandbox_create_default 创建沙箱
 *   - add_rule(SYS_TOOL_EXECUTE, PERM_ALLOW) 配置允许
 *   - invoke 执行 /usr/bin/echo hello
 *   - 返回 SUCCESS，exec_result==0（echo 退出码），output 含 "hello"
 */
static void test_sandbox_allow_path(void)
{
    /* 1. 初始化管理器（幂等） */
    if (agentos_sandbox_manager_init() != AGENTOS_SUCCESS) {
        TEST_FAIL("agentos_sandbox_manager_init failed");
        return;
    }

    /* 2. 创建沙箱 */
    agentos_sandbox_t *sb = NULL;
    if (agentos_sandbox_create_default("test_allow", "test_owner", &sb) != AGENTOS_SUCCESS || !sb) {
        TEST_FAIL("agentos_sandbox_create_default failed");
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 3. 添加 PERM_ALLOW 规则（显式，便于审计） */
    if (agentos_sandbox_add_rule(sb, SYS_TOOL_EXECUTE, PERM_ALLOW, NULL) != AGENTOS_SUCCESS) {
        TEST_FAIL("agentos_sandbox_add_rule(ALLOW) failed");
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 4. 构造 echo 参数并执行 */
    tool_execute_args_t *t = make_echo_args("hello_airymax", 5000);
    if (!t) {
        TEST_FAIL("make_echo_args failed (OOM)");
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    void *invoke_args[1] = { t };
    void *out_result = NULL;
    agentos_error_t rc = agentos_sandbox_invoke(sb, SYS_TOOL_EXECUTE, invoke_args, 1, &out_result);

    /* 5. 验证返回 SUCCESS */
    if (rc != AGENTOS_SUCCESS) {
        char msg[128];
        snprintf(msg, sizeof(msg), "invoke returned %d (expected SUCCESS=%d)",
                 (int)rc, (int)AGENTOS_SUCCESS);
        TEST_FAIL(msg);
        free_echo_args(t);
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 6. 验证 exec_result == 0（echo 退出码） */
    if (t->exec_result != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "exec_result=%d (expected 0)", t->exec_result);
        TEST_FAIL(msg);
        free_echo_args(t);
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 7. 验证 output 含 "hello_airymax" */
    if (!t->output_buffer || strstr(t->output_buffer, "hello_airymax") == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "output='%s' (expected to contain 'hello_airymax')",
                 t->output_buffer ? t->output_buffer : "(null)");
        TEST_FAIL(msg);
        free_echo_args(t);
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 8. 清理 */
    (void)out_result; /* sys_tool_execute 返回 SUCCESS/EFAIL，已由 rc 覆盖 */
    printf("    ALLOW path: echo executed via sandbox, output='%s'\n", t->output_buffer);
    free_echo_args(t);
    agentos_sandbox_destroy(sb);
    agentos_sandbox_manager_destroy();
    TEST_PASS();
}

/* ========== TEST 2: sandbox DENY 路径 — 工具被拒绝 ==========
 *
 * 验证：
 *   - add_rule(SYS_TOOL_EXECUTE, PERM_DENY) 配置拒绝
 *   - invoke 返回 EACCES（权限拒绝）
 *   - exec_result 保持哨兵值（工具未实际执行）
 */
static void test_sandbox_deny_path(void)
{
    if (agentos_sandbox_manager_init() != AGENTOS_SUCCESS) {
        TEST_FAIL("agentos_sandbox_manager_init failed");
        return;
    }

    agentos_sandbox_t *sb = NULL;
    if (agentos_sandbox_create_default("test_deny", "test_owner", &sb) != AGENTOS_SUCCESS || !sb) {
        TEST_FAIL("agentos_sandbox_create_default failed");
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 添加 PERM_DENY 规则 — DENY 优先于默认 ALLOW */
    if (agentos_sandbox_add_rule(sb, SYS_TOOL_EXECUTE, PERM_DENY, NULL) != AGENTOS_SUCCESS) {
        TEST_FAIL("agentos_sandbox_add_rule(DENY) failed");
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    tool_execute_args_t *t = make_echo_args("should_not_run", 5000);
    if (!t) {
        TEST_FAIL("make_echo_args failed (OOM)");
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    void *invoke_args[1] = { t };
    void *out_result = NULL;
    agentos_error_t rc = agentos_sandbox_invoke(sb, SYS_TOOL_EXECUTE, invoke_args, 1, &out_result);

    /* 验证返回 EACCES（权限拒绝） */
    if (rc != AGENTOS_EACCES) {
        char msg[128];
        snprintf(msg, sizeof(msg), "invoke returned %d (expected EACCES=%d)",
                 (int)rc, (int)AGENTOS_EACCES);
        TEST_FAIL(msg);
        free_echo_args(t);
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    /* 验证工具未实际执行（exec_result 保持哨兵值 -999） */
    if (t->exec_result != -999) {
        char msg[128];
        snprintf(msg, sizeof(msg), "exec_result=%d (expected -999 sentinel, tool should NOT have run)",
                 t->exec_result);
        TEST_FAIL(msg);
        free_echo_args(t);
        agentos_sandbox_destroy(sb);
        agentos_sandbox_manager_destroy();
        return;
    }

    (void)out_result;
    free_echo_args(t);
    agentos_sandbox_destroy(sb);
    agentos_sandbox_manager_destroy();
    TEST_PASS();
    printf("    DENY path: tool execution blocked by sandbox (EACCES)\n");
}

/* ========== TEST 3: sandbox NULL fail-closed ==========
 *
 * 验证 sandbox 句柄为 NULL 时 invoke 返回 EINVAL。
 * 这对应 tool_executor_run 中 `if (!exec->sandbox) return AGENTOS_EPERM` 的防御。
 */
static void test_sandbox_null_fail_closed(void)
{
    void *out_result = NULL;
    /* 构造无效 args（NULL sandbox 下不会解引用） */
    void *invoke_args[1] = { NULL };
    agentos_error_t rc = agentos_sandbox_invoke(NULL, SYS_TOOL_EXECUTE, invoke_args, 1, &out_result);

    if (rc != AGENTOS_EINVAL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "invoke(NULL,...) returned %d (expected EINVAL=%d)",
                 (int)rc, (int)AGENTOS_EINVAL);
        TEST_FAIL(msg);
        return;
    }

    TEST_PASS();
    printf("    NULL fail-closed: invoke(NULL,...) returned EINVAL\n");
}

/* ========== TEST 4: tool_executor 集成 — sandbox 在 create 中初始化 ==========
 *
 * 验证 executor.c 的 sandbox 集成不破坏现有行为：
 *   - tool_executor_create 内部调用 agentos_sandbox_manager_init + create_default
 *   - 不设置 approval_ctx，调用 tool_executor_run → 应返回 EPERM（approval_ctx NULL fail-closed）
 *   - 这验证 sandbox 初始化代码路径正常，且双层 fail-closed 中
 *     approval_ctx 层在前（先拒绝），sandbox 层不会被触及
 */
static void test_executor_sandbox_integration(void)
{
    tool_executor_t *exec = tool_executor_create(NULL);
    if (!exec) {
        TEST_FAIL("tool_executor_create failed (sandbox init may have crashed)");
        return;
    }

    /* 构造工具元数据 — /usr/bin/echo */
    tool_metadata_t meta;
    AGENTOS_MEMSET(&meta, 0, sizeof(meta));
    meta.id = "test_echo";
    meta.name = "echo_test";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 5;

    tool_result_t *result = NULL;
    int ret = tool_executor_run(exec, &meta, "hello", &result);

    /* 验证：未设置 approval_ctx → fail-closed 拒绝（EPERM）
     * 注意：AGENTOS_EPERM 在 executor.c 中作为 approval_ctx NULL 的返回值。
     * 这验证了 sandbox 初始化不会绕过 approval_ctx 的 fail-closed。 */
    if (ret != AGENTOS_EPERM) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "tool_executor_run returned %d (expected EPERM=%d, approval_ctx NULL should fail-closed)",
                 ret, (int)AGENTOS_EPERM);
        TEST_FAIL(msg);
        if (result)
            tool_result_free(result);
        tool_executor_destroy(exec);
        return;
    }

    if (result)
        tool_result_free(result);
    tool_executor_destroy(exec);
    TEST_PASS();
    printf("    executor integration: sandbox init OK, approval_ctx NULL fail-closed preserved\n");
}

/* ========== main ========== */
int main(void)
{
    printf("=== P3.18 (ACC-DT27) tool_d sandbox integration tests ===\n\n");

    printf("[Test 1] sandbox ALLOW path (execute /usr/bin/echo)\n");
    test_sandbox_allow_path();

    printf("\n[Test 2] sandbox DENY path (tool blocked)\n");
    test_sandbox_deny_path();

    printf("\n[Test 3] sandbox NULL fail-closed\n");
    test_sandbox_null_fail_closed();

    printf("\n[Test 4] tool_executor sandbox integration (create + fail-closed)\n");
    test_executor_sandbox_integration();

    printf("\n=== Results: %d/%d passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
