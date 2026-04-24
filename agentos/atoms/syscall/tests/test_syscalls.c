/**
 * @file test_syscalls.c
 * @brief Syscall 模块单元测试 (TeamC P1-C07)
 *
 * 覆盖: init/cleanup, invoke分发, 参数验证( argc/NULL),
 *       边界条件, 所有18个syscall号
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syscalls.h"

/* 内部函数声明 (来自 syscall_table.c) */
void* agentos_syscall_invoke(int syscall_num, void** args, int argc);

/* ========== 测试宏 ========== */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return -1; \
    } \
} while(0)

#define TEST_PASS(name) printf("  PASS: %s\n", name)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(func) do { \
    int _ret = func(); \
    if (_ret == 0) { g_tests_passed++; } else { g_tests_failed++; } \
} while(0)

/* ========== 系统调用号 (与 syscall_table.c 同步) ========== */
enum {
    SYS_TASK_SUBMIT = 1,
    SYS_TASK_QUERY,
    SYS_TASK_WAIT,
    SYS_TASK_CANCEL,
    SYS_MEMORY_WRITE,
    SYS_MEMORY_SEARCH,
    SYS_MEMORY_GET,
    SYS_MEMORY_DELETE,
    SYS_SESSION_CREATE,
    SYS_SESSION_GET,
    SYS_SESSION_CLOSE,
    SYS_SESSION_LIST,
    SYS_TELEMETRY_METRICS,
    SYS_TELEMETRY_TRACES,
    SYS_AGENT_SPAWN,
    SYS_AGENT_TERMINATE,
    SYS_AGENT_INVOKE,
    SYS_AGENT_LIST,
    SYS_MAX
};

/* ========== 测试用例 ========== */

/**
 * T1: 系统调用层初始化和清理
 */
int test_sys_init_cleanup(void) {
    agentos_error_t err = agentos_syscalls_init();
    TEST_ASSERT(err == AGENTOS_SUCCESS, "init should return SUCCESS");

    agentos_syscalls_cleanup();

    err = agentos_syscalls_init();
    TEST_ASSERT(err == AGENTOS_SUCCESS, "re-init should succeed");

    agentos_syscalls_cleanup();
    TEST_PASS("sys_init_cleanup");
    return 0;
}

/**
 * T2: invoke 无效syscall号
 */
int test_sys_invoke_invalid_number(void) {
    void* result = NULL;

    result = agentos_syscall_invoke(0, NULL, 0);
    TEST_ASSERT(result != NULL, "syscall 0 should not be NULL");
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "syscall 0 should return EINVAL");

    result = agentos_syscall_invoke(-1, NULL, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "negative should return EINVAL");

    result = agentos_syscall_invoke(SYS_MAX, NULL, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "SYS_MAX should return EINVAL");

    result = agentos_syscall_invoke(99999, NULL, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "large number should return EINVAL");

    TEST_PASS("sys_invoke_invalid_number");
    return 0;
}

/**
 * T3: invoke 有效syscall号但argc不匹配
 */
int test_sys_invoke_wrong_argc(void) {
    void* args[4] = { NULL };
    void* result;

    /* task_submit 需要4个参数，给0个 */
    result = agentos_syscall_invoke(SYS_TASK_SUBMIT, args, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for submit");

    /* memory_write 需要4个参数，给2个 */
    result = agentos_syscall_invoke(SYS_MEMORY_WRITE, args, 2);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for write");

    /* session_create 需要2个参数，给5个 */
    result = agentos_syscall_invoke(SYS_SESSION_CREATE, args, 5);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for session_create");

    /* agent_spawn 需要2个参数，给0个 */
    result = agentos_syscall_invoke(SYS_AGENT_SPAWN, args, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for spawn");

    /* memory_search 需要5个参数，给3个 */
    result = agentos_syscall_invoke(SYS_MEMORY_SEARCH, args, 3);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for search");

    /* agent_invoke 需要4个参数，给1个 */
    result = agentos_syscall_invoke(SYS_AGENT_INVOKE, args, 1);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "wrong argc for invoke");

    TEST_PASS("sys_invoke_wrong_argc");
    return 0;
}

/**
 * T4: invoke 有效syscall号 + 正确argc (验证不崩溃)
 */
int test_sys_invoke_valid_dispatch(void) {
    void* args[5];
    void* result;

    memset(args, 0, sizeof(args));

    /* task_submit - 4 args */
    char* out_task = NULL;
    args[0] = (void*)"test input";
    args[1] = (void*)(uintptr_t)10;
    args[2] = (void*)(uintptr_t)5000;
    args[3] = &out_task;
    result = agentos_syscall_invoke(SYS_TASK_SUBMIT, args, 4);
    printf("    (task_submit returned %ld)\n", (long)(intptr_t)result);

    /* task_cancel - 1 arg */
    args[0] = (void*)"fake-task-id";
    result = agentos_syscall_invoke(SYS_TASK_CANCEL, args, 1);

    /* memory_delete - 1 arg */
    args[0] = (void*)"fake-record-id";
    result = agentos_syscall_invoke(SYS_MEMORY_DELETE, args, 1);

    /* session_close - 1 arg */
    args[0] = (void*)"fake-session-id";
    result = agentos_syscall_invoke(SYS_SESSION_CLOSE, args, 1);

    /* agent_terminate - 1 arg */
    args[0] = (void*)"fake-agent-id";
    result = agentos_syscall_invoke(SYS_AGENT_TERMINATE, args, 1);

    /* telemetry_metrics - 1 arg */
    char* out_metrics = NULL;
    args[0] = &out_metrics;
    result = agentos_syscall_invoke(SYS_TELEMETRY_METRICS, args, 1);
    if (out_metrics) {
        free(out_metrics);
    }

    TEST_PASS("sys_invoke_valid_dispatch");
    return 0;
}

/**
 * T5: 全部18个syscall号可达性检查
 */
int test_sys_all_syscalls_reachable(void) {
    int reachable = 0;
    int total = 0;

    for (int num = 1; num < SYS_MAX; num++) {
        void* args[5];
        memset(args, 0, sizeof(args));
        void* r = agentos_syscall_invoke(num, args, 0);
        total++;
        if ((intptr_t)r == AGENTOS_EINVAL) {
            reachable++;
        }
    }

    TEST_ASSERT(total == 18, "should have 18 syscalls");
    TEST_ASSERT(reachable == 18, "all 18 should reject with EINVAL on wrong argc");

    printf("    (checked %d syscalls)\n", total);
    TEST_PASS("sys_all_syscalls_reachable");
    return 0;
}

/**
 * T6: agentos_sys_free 安全性
 */
int test_sys_free(void) {
    agentos_sys_free(NULL);
    TEST_PASS("sys_free_null");

    char* ptr = strdup("test data");
    agentos_sys_free(ptr);
    TEST_PASS("sys_free_valid");
    return 0;
}

/**
 * T7: API版本号正确性
 */
int test_sys_api_version(void) {
    TEST_ASSERT(SYSCALL_API_VERSION_MAJOR == 1, "major version should be 1");
    TEST_ASSERT(SYSCALL_API_VERSION_MINOR == 0, "minor version should be 0");
    TEST_ASSERT(SYSCALL_API_VERSION_PATCH == 0, "patch version should be 0");
    TEST_PASS("sys_api_version");
    return 0;
}

/**
 * T8: session_persist_status_t 枚举值完整性
 */
int test_session_persist_enum(void) {
    TEST_ASSERT(SESSION_PERSIST_UNKNOWN == 0, "UNKNOWN should be 0");
    TEST_ASSERT(SESSION_PERSIST_PENDING == 1, "PENDING should be 1");
    TEST_ASSERT(SESSION_PERSIST_SUCCESS == 2, "SUCCESS should be 2");
    TEST_ASSERT(SESSION_PERSIST_FAILED == 3, "FAILED should be 3");
    TEST_ASSERT(SESSION_PERSIST_DISABLED == 4, "DISABLED should be 4");
    TEST_PASS("session_persist_enum");
    return 0;
}

/**
 * T9: 多次初始化/清理幂等性
 */
int test_sys_idempotent_init(void) {
    for (int i = 0; i < 10; i++) {
        agentos_error_t err = agentos_syscalls_init();
        if (err != AGENTOS_SUCCESS) {
            printf("  FAIL: init #%d returned %d\n", i, (int)err);
            return -1;
        }
    }
    for (int i = 0; i < 10; i++) {
        agentos_syscalls_cleanup();
    }
    TEST_PASS("sys_idempotent_init");
    return 0;
}

/**
 * T10: 边界syscall号 (SYS_MAX-1 和 1)
 */
int test_sys_boundary_numbers(void) {
    void* args[5];
    memset(args, 0, sizeof(args));
    void* result;

    result = agentos_syscall_invoke(1, args, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "syscall 1 with 0 args -> EINVAL");

    result = agentos_syscall_invoke(SYS_MAX - 1, args, 0);
    TEST_ASSERT((intptr_t)result == AGENTOS_EINVAL, "last valid syscall with 0 args -> EINVAL");

    TEST_PASS("sys_boundary_numbers");
    return 0;
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printf("\n========================================\n");
    printf("  Syscall Module Unit Tests (P1-C07)\n");
    printf("========================================\n\n");

    RUN_TEST(test_sys_init_cleanup);
    RUN_TEST(test_sys_invoke_invalid_number);
    RUN_TEST(test_sys_invoke_wrong_argc);
    RUN_TEST(test_sys_invoke_valid_dispatch);
    RUN_TEST(test_sys_all_syscalls_reachable);
    RUN_TEST(test_sys_free);
    RUN_TEST(test_sys_api_version);
    RUN_TEST(test_session_persist_enum);
    RUN_TEST(test_sys_idempotent_init);
    RUN_TEST(test_sys_boundary_numbers);

    printf("\n----------------------------------------\n");
    printf("  Results: %d/%d passed", g_tests_passed, g_tests_passed + g_tests_failed);
    if (g_tests_failed > 0) {
        printf(" (%d FAILED)", g_tests_failed);
    }
    printf("\n");

    return g_tests_failed > 0 ? 1 : 0;
}
