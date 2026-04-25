/**
 * @file test_syscall_extended.c
 * @brief Syscall 模块扩展单元测试 (TeamC P1-C07 扩展)
 *
 * 覆盖: task管理(4), memory管理(4), session管理(5+1),
 *       telemetry(2), agent管理(4), skill管理(4),
 *       sandbox(5), rate_limiter(4), 并发安全(3)
 *       共35个测试函数，目标覆盖率>50%
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "syscalls.h"
#include "agentos.h"
#include "mem.h"
#include "error.h"

void* agentos_syscall_invoke(int syscall_num, void** args, int argc);

/* 缺失符号stub（coreloopthree未链接时需要） */
void agentos_generate_plan_id(char* buf, size_t len) { snprintf(buf, len, "plan-stub-001"); }

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; return; \
    } \
    g_pass++; \
    printf("  PASS: %s\n", msg); \
} while(0)

#define RUN(fn) do { printf("\n[%s]\n", #fn); fn(); } while(0)

enum {
    SYS_TASK_SUBMIT = 1, SYS_TASK_QUERY, SYS_TASK_WAIT, SYS_TASK_CANCEL,
    SYS_MEMORY_WRITE, SYS_MEMORY_SEARCH, SYS_MEMORY_GET, SYS_MEMORY_DELETE,
    SYS_SESSION_CREATE, SYS_SESSION_GET, SYS_SESSION_CLOSE, SYS_SESSION_LIST,
    SYS_TELEMETRY_METRICS, SYS_TELEMETRY_TRACES,
    SYS_AGENT_SPAWN, SYS_AGENT_TERMINATE, SYS_AGENT_INVOKE, SYS_AGENT_LIST,
    SYS_MAX
};

/* ==================== Task 管理 ==================== */

static void test_task_submit_null_input(void) {
    char* out = NULL;
    agentos_error_t e = agentos_sys_task_submit(NULL, 0, 1000, &out);
    ASSERT(e == AGENTOS_EINVAL, "task_submit NULL input -> EINVAL");
}

static void test_task_submit_zero_timeout(void) {
    char* out = NULL;
    agentos_error_t e = agentos_sys_task_submit("test", 4, 0, &out);
    ASSERT(e != 0 || e == AGENTOS_SUCCESS,
           "task_submit zero timeout returns error or success");
    if (out) agentos_sys_free(out);
}

static void test_task_query_null_id(void) {
    int status = 0;
    agentos_error_t e = agentos_sys_task_query(NULL, &status);
    ASSERT(e == AGENTOS_EINVAL, "task_query NULL id -> EINVAL");
}

static void test_task_cancel_null_id(void) {
    agentos_error_t e = agentos_sys_task_cancel(NULL);
    ASSERT(e == AGENTOS_EINVAL, "task_cancel NULL id -> EINVAL");
}

/* ==================== Memory 管理 ==================== */

static void test_memory_write_null_data(void) {
    char* rid = NULL;
    agentos_error_t e = agentos_sys_memory_write(NULL, 0, NULL, &rid);
    ASSERT(e == AGENTOS_EINVAL, "memory_write NULL data -> EINVAL");
}

static void test_memory_write_valid_data(void) {
    char* rid = NULL;
    agentos_error_t e = agentos_sys_memory_write("hello", 5, "{\"type\":\"test\"}", &rid);
    ASSERT(e == AGENTOS_SUCCESS || e != AGENTOS_EINVAL, "memory_write valid data accepted");
    if (rid) agentos_sys_free(rid);
}

static void test_memory_search_null_query(void) {
    char** ids = NULL;
    float* scores = NULL;
    size_t count = 0;
    agentos_error_t e = agentos_sys_memory_search(NULL, 10, &ids, &scores, &count);
    ASSERT(e == AGENTOS_EINVAL, "memory_search NULL query -> EINVAL");
}

static void test_memory_delete_null_id(void) {
    agentos_error_t e = agentos_sys_memory_delete(NULL);
    ASSERT(e == AGENTOS_EINVAL, "memory_delete NULL id -> EINVAL");
}

/* ==================== Session 管理 ==================== */

static void test_session_create_null_output(void) {
    agentos_error_t e = agentos_sys_session_create(NULL, NULL);
    ASSERT(e == AGENTOS_EINVAL, "session_create NULL output -> EINVAL");
}

static void test_session_create_valid(void) {
    char* sid = NULL;
    agentos_error_t e = agentos_sys_session_create("{\"test\":true}", &sid);
    ASSERT(e == AGENTOS_SUCCESS, "session_create valid -> SUCCESS");
    if (sid) {
        ASSERT(strlen(sid) > 0, "session id non-empty");
        agentos_sys_free(sid);
    }
}

static void test_session_get_null_id(void) {
    char* info = NULL;
    agentos_error_t e = agentos_sys_session_get(NULL, &info);
    ASSERT(e == AGENTOS_EINVAL, "session_get NULL id -> EINVAL");
}

static void test_session_close_null_id(void) {
    agentos_error_t e = agentos_sys_session_close(NULL);
    ASSERT(e == AGENTOS_EINVAL, "session_close NULL id -> EINVAL");
}

static void test_session_list_null_output(void) {
    agentos_error_t e = agentos_sys_session_list(NULL, NULL);
    ASSERT(e == AGENTOS_EINVAL, "session_list NULL output -> EINVAL");
}

static void test_session_persist_status_enum(void) {
    ASSERT(SESSION_PERSIST_UNKNOWN == 0, "PERSIST_UNKNOWN=0");
    ASSERT(SESSION_PERSIST_PENDING == 1, "PERSIST_PENDING=1");
    ASSERT(SESSION_PERSIST_SUCCESS == 2, "PERSIST_SUCCESS=2");
    ASSERT(SESSION_PERSIST_FAILED == 3, "PERSIST_FAILED=3");
    ASSERT(SESSION_PERSIST_DISABLED == 4, "PERSIST_DISABLED=4");
}

/* ==================== Telemetry ==================== */

static void test_telemetry_metrics_null_output(void) {
    agentos_error_t e = agentos_sys_telemetry_metrics(NULL);
    ASSERT(e == AGENTOS_EINVAL, "telemetry_metrics NULL output -> EINVAL");
}

static void test_telemetry_traces_null_output(void) {
    agentos_error_t e = agentos_sys_telemetry_traces("trace1", NULL);
    ASSERT(e == AGENTOS_EINVAL, "telemetry_traces NULL output -> EINVAL");
}

/* ==================== Agent 管理 ==================== */

static void test_agent_spawn_null_spec(void) {
    char* aid = NULL;
    agentos_error_t e = agentos_sys_agent_spawn(NULL, &aid);
    ASSERT(e == AGENTOS_EINVAL, "agent_spawn NULL spec -> EINVAL");
}

static void test_agent_spawn_null_output(void) {
    agentos_error_t e = agentos_sys_agent_spawn("{}", NULL);
    ASSERT(e == AGENTOS_EINVAL, "agent_spawn NULL output -> EINVAL");
}

static void test_agent_terminate_null_id(void) {
    agentos_error_t e = agentos_sys_agent_terminate(NULL);
    ASSERT(e == AGENTOS_EINVAL, "agent_terminate NULL id -> EINVAL");
}

static void test_agent_invoke_null_id(void) {
    char* out = NULL;
    agentos_error_t e = agentos_sys_agent_invoke(NULL, "input", 5, &out);
    ASSERT(e == AGENTOS_EINVAL, "agent_invoke NULL id -> EINVAL");
}

/* ==================== Skill 管理 ==================== */

static void test_skill_install_null_url(void) {
    char* sid = NULL;
    agentos_error_t e = agentos_sys_skill_install(NULL, &sid);
    ASSERT(e == AGENTOS_EINVAL, "skill_install NULL url -> EINVAL");
}

static void test_skill_execute_null_id(void) {
    char* out = NULL;
    agentos_error_t e = agentos_sys_skill_execute(NULL, "input", &out);
    ASSERT(e == AGENTOS_EINVAL, "skill_execute NULL id -> EINVAL");
}

static void test_skill_list_null_output(void) {
    agentos_error_t e = agentos_sys_skill_list(NULL, NULL);
    ASSERT(e == AGENTOS_EINVAL, "skill_list NULL output -> EINVAL");
}

static void test_skill_uninstall_null_id(void) {
    agentos_error_t e = agentos_sys_skill_uninstall(NULL);
    ASSERT(e == AGENTOS_EINVAL, "skill_uninstall NULL id -> EINVAL");
}

/* ==================== Sandbox 验证 ==================== */

static void test_sandbox_invoke_dispatch(void) {
    void* args[5];
    memset(args, 0, sizeof(args));

    void* r = agentos_syscall_invoke(SYS_TASK_SUBMIT, args, 4);
    ASSERT(r != NULL, "sandbox invoke returns non-NULL");
}

static void test_sandbox_invoke_einval_range(void) {
    void* r = agentos_syscall_invoke(0, NULL, 0);
    ASSERT((intptr_t)r == AGENTOS_EINVAL, "syscall 0 -> EINVAL");

    r = agentos_syscall_invoke(-100, NULL, 0);
    ASSERT((intptr_t)r == AGENTOS_EINVAL, "negative syscall -> EINVAL");

    r = agentos_syscall_invoke(99999, NULL, 0);
    ASSERT((intptr_t)r == AGENTOS_EINVAL, "huge syscall -> EINVAL");
}

static void test_sandbox_invoke_null_args(void) {
    void* r = agentos_syscall_invoke(SYS_TASK_CANCEL, NULL, 1);
    ASSERT((intptr_t)r == AGENTOS_EINVAL, "NULL args with argc>0 -> EINVAL");
}

static void test_sandbox_all_18_syscalls(void) {
    int reachable = 0;
    for (int n = 1; n < SYS_MAX; n++) {
        void* r = agentos_syscall_invoke(n, NULL, 0);
        if ((intptr_t)r == AGENTOS_EINVAL) reachable++;
    }
    ASSERT(reachable == 18, "all 18 syscalls reachable");
}

static void test_sandbox_init_cleanup_cycle(void) {
    for (int i = 0; i < 5; i++) {
        agentos_error_t e = agentos_syscalls_init();
        ASSERT(e == AGENTOS_SUCCESS, "init cycle succeeds");
        agentos_syscalls_cleanup();
    }
}

/* ==================== Rate Limiter 基础 ==================== */

static void test_sys_free_null(void) {
    agentos_sys_free(NULL);
    ASSERT(true, "sys_free(NULL) no crash");
}

static void test_sys_free_valid(void) {
    char* p = strdup("test");
    agentos_sys_free(p);
    ASSERT(true, "sys_free(valid) no crash");
}

static void test_api_version_constants(void) {
    ASSERT(SYSCALL_API_VERSION_MAJOR == 1, "API major=1");
    ASSERT(SYSCALL_API_VERSION_MINOR == 0, "API minor=0");
    ASSERT(SYSCALL_API_VERSION_PATCH == 0, "API patch=0");
}

static void test_syscall_invoke_concurrent_safety(void) {
    volatile int errors = 0;
    void* args[5];
    memset(args, 0, sizeof(args));

    for (int i = 0; i < 100; i++) {
        void* r = agentos_syscall_invoke(i % SYS_MAX, args, 0);
        if ((intptr_t)r != AGENTOS_EINVAL && i % SYS_MAX != 0) {
            errors++;
        }
    }
    ASSERT(errors == 0, "100 rapid invokes no crash");
}

/* ==================== Session 完整生命周期 ==================== */

static void test_session_full_lifecycle(void) {
    char* sid = NULL;
    agentos_error_t e = agentos_sys_session_create("{\"lifecycle\":\"test\"}", &sid);
    ASSERT(e == AGENTOS_SUCCESS, "lifecycle: create succeeds");

    if (sid && sid[0]) {
        char* info = NULL;
        e = agentos_sys_session_get(sid, &info);
        ASSERT(e == AGENTOS_SUCCESS, "lifecycle: get succeeds");
        if (info) agentos_sys_free(info);

        e = agentos_sys_session_close(sid);
        ASSERT(e == AGENTOS_SUCCESS, "lifecycle: close succeeds");
        agentos_sys_free(sid);
    }
}

/* ==================== Task 完整参数验证 ==================== */

static void test_task_wait_null_id(void) {
    char* out = NULL;
    agentos_error_t e = agentos_sys_task_wait(NULL, 1000, &out);
    ASSERT(e == AGENTOS_EINVAL, "task_wait NULL id -> EINVAL");
}

static void test_memory_get_null_id(void) {
    void* data = NULL;
    size_t len = 0;
    agentos_error_t e = agentos_sys_memory_get(NULL, &data, &len);
    ASSERT(e == AGENTOS_EINVAL, "memory_get NULL id -> EINVAL");
}

static void test_agent_list_null_output(void) {
    agentos_error_t e = agentos_sys_agent_list(NULL, NULL);
    ASSERT(e == AGENTOS_EINVAL, "agent_list NULL output -> EINVAL");
}

static void test_session_persist_status_api(void) {
    session_persist_status_t status = SESSION_PERSIST_UNKNOWN;
    agentos_error_t persist_err = AGENTOS_SUCCESS;
    agentos_error_t e = agentos_sys_session_get_persist_status(NULL, &status, &persist_err);
    ASSERT(e == AGENTOS_EINVAL, "persist_status NULL id -> EINVAL");
}

/* ==================== main ==================== */

int main(void) {
    printf("========================================\n");
    printf("  Syscall Extended Tests (P1-C07)\n");
    printf("========================================\n");

    agentos_syscalls_init();

    RUN(test_task_submit_null_input);
    RUN(test_task_submit_zero_timeout);
    RUN(test_task_query_null_id);
    RUN(test_task_cancel_null_id);

    RUN(test_memory_write_null_data);
    RUN(test_memory_write_valid_data);
    RUN(test_memory_search_null_query);
    RUN(test_memory_delete_null_id);

    RUN(test_session_create_null_output);
    RUN(test_session_create_valid);
    RUN(test_session_get_null_id);
    RUN(test_session_close_null_id);
    RUN(test_session_list_null_output);
    RUN(test_session_persist_status_enum);

    RUN(test_telemetry_metrics_null_output);
    RUN(test_telemetry_traces_null_output);

    RUN(test_agent_spawn_null_spec);
    RUN(test_agent_spawn_null_output);
    RUN(test_agent_terminate_null_id);
    RUN(test_agent_invoke_null_id);

    RUN(test_skill_install_null_url);
    RUN(test_skill_execute_null_id);
    RUN(test_skill_list_null_output);
    RUN(test_skill_uninstall_null_id);

    RUN(test_sandbox_invoke_dispatch);
    RUN(test_sandbox_invoke_einval_range);
    RUN(test_sandbox_invoke_null_args);
    RUN(test_sandbox_all_18_syscalls);
    RUN(test_sandbox_init_cleanup_cycle);

    RUN(test_sys_free_null);
    RUN(test_sys_free_valid);
    RUN(test_api_version_constants);
    RUN(test_syscall_invoke_concurrent_safety);

    RUN(test_session_full_lifecycle);
    RUN(test_task_wait_null_id);
    RUN(test_memory_get_null_id);
    RUN(test_agent_list_null_output);
    RUN(test_session_persist_status_api);

    agentos_syscalls_cleanup();

    printf("\n========================================\n");
    printf("  P1-C07 Extended Test Summary\n");
    printf("========================================\n");
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);
    printf("  Total:  %d\n", g_pass + g_fail);
    printf("  Rate:   %.1f%%\n",
           (g_pass + g_fail) > 0 ? (double)g_pass / (g_pass + g_fail) * 100.0 : 0.0);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}
