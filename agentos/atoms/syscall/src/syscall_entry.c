/**
 * @file syscall_entry.c
 * @brief 系统调用统一入口（参数解析和分发）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "cognition.h"
#include "logger.h"
#include "memory_compat.h"
#include "memory_provider.h"
#include "platform.h"   /* P3.18 (ACC-DT27): agentos_process_run_capture */
#include "string_compat.h"
#include "syscalls.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void agentos_sys_set_memory_provider(void *provider);
extern void agentos_sys_init(void *cognition, void *execution, void *memory);
extern void agentos_sys_agent_cleanup(void);
extern void agentos_sys_skill_cleanup(void);

#define CHECK_ARGS()                                 \
    do {                                             \
        if (args == NULL && argc > 0)                \
            return (void *)(intptr_t)AGENTOS_EINVAL; \
    } while (0)

static inline uint32_t arg_to_uint32(void *p)
{
    return (uint32_t)(uintptr_t)p;
}
static inline int32_t arg_to_int32(void *p)
{
    return (int32_t)(intptr_t)p;
}
static inline size_t arg_to_size(void *p)
{
    return (size_t)(uintptr_t)p;
}

void *sys_task_submit(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 4)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *input = (const char *)args[0];
    size_t len = arg_to_size(args[1]);
    uint32_t timeout = arg_to_uint32(args[2]);
    char **out = (char **)args[3];
    intptr_t res = agentos_sys_task_submit(input, len, timeout, out);
    return (void *)res;
}

void *sys_task_query(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *task_id = (const char *)args[0];
    int *status = (int *)args[1];
    intptr_t res = agentos_sys_task_query(task_id, status);
    return (void *)res;
}

void *sys_task_wait(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 3)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *task_id = (const char *)args[0];
    uint32_t timeout = arg_to_uint32(args[1]);
    char **out = (char **)args[2];
    intptr_t res = agentos_sys_task_wait(task_id, timeout, out);
    return (void *)res;
}

void *sys_task_cancel(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *task_id = (const char *)args[0];
    intptr_t res = agentos_sys_task_cancel(task_id);
    return (void *)res;
}

void *sys_memory_write(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 4)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const void *data = args[0];
    size_t len = arg_to_size(args[1]);
    const char *metadata = (const char *)args[2];
    char **out_record_id = (char **)args[3];
    intptr_t res = agentos_sys_memory_write(data, len, metadata, out_record_id);
    return (void *)res;
}

void *sys_memory_search(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 5)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *query = (const char *)args[0];
    uint32_t limit = arg_to_uint32(args[1]);
    char ***out_record_ids = (char ***)args[2];
    float **out_scores = (float **)args[3];
    size_t *out_count = (size_t *)args[4];
    intptr_t res = agentos_sys_memory_search(query, limit, out_record_ids, out_scores, out_count);
    return (void *)res;
}

void *sys_memory_get(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 3)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *record_id = (const char *)args[0];
    void **out_data = (void **)args[1];
    size_t *out_len = (size_t *)args[2];
    intptr_t res = agentos_sys_memory_get(record_id, out_data, out_len);
    return (void *)res;
}

void *sys_memory_delete(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *record_id = (const char *)args[0];
    intptr_t res = agentos_sys_memory_delete(record_id);
    return (void *)res;
}

void *sys_session_create(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *metadata = (const char *)args[0];
    char **out_session_id = (char **)args[1];
    intptr_t res = agentos_sys_session_create(metadata, out_session_id);
    return (void *)res;
}

void *sys_session_get(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *session_id = (const char *)args[0];
    char **out_info = (char **)args[1];
    intptr_t res = agentos_sys_session_get(session_id, out_info);
    return (void *)res;
}

void *sys_session_close(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *session_id = (const char *)args[0];
    intptr_t res = agentos_sys_session_close(session_id);
    return (void *)res;
}

void *sys_session_list(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    char ***out_sessions = (char ***)args[0];
    size_t *out_count = (size_t *)args[1];
    intptr_t res = agentos_sys_session_list(out_sessions, out_count);
    return (void *)res;
}

void *sys_telemetry_metrics(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    char **out_metrics = (char **)args[0];
    intptr_t res = agentos_sys_telemetry_metrics(out_metrics);
    return (void *)res;
}

void *sys_telemetry_traces(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    char **out_traces = (char **)args[0];
    intptr_t res = agentos_sys_telemetry_traces(NULL, out_traces);
    return (void *)res;
}

void *sys_agent_spawn(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *agent_spec = (const char *)args[0];
    char **out_agent_id = (char **)args[1];
    intptr_t res = agentos_sys_agent_spawn(agent_spec, out_agent_id);
    return (void *)res;
}

void *sys_agent_terminate(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *agent_id = (const char *)args[0];
    intptr_t res = agentos_sys_agent_terminate(agent_id);
    return (void *)res;
}

void *sys_agent_invoke(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 4)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *agent_id = (const char *)args[0];
    const char *input = (const char *)args[1];
    size_t input_len = (size_t)(uintptr_t)args[2];
    char **out_output = (char **)args[3];
    intptr_t res = agentos_sys_agent_invoke(agent_id, input, input_len, out_output);
    return (void *)res;
}

void *sys_agent_list(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    char ***out_agent_ids = (char ***)args[0];
    size_t *out_count = (size_t *)args[1];
    intptr_t res = agentos_sys_agent_list(out_agent_ids, out_count);
    return (void *)res;
}

void *sys_skill_install(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *skill_url = (const char *)args[0];
    char **out_skill_id = (char **)args[1];
    intptr_t res = agentos_sys_skill_install(skill_url, out_skill_id);
    return (void *)res;
}

void *sys_skill_execute(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 3)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *skill_id = (const char *)args[0];
    const char *input = (const char *)args[1];
    char **out_output = (char **)args[2];
    intptr_t res = agentos_sys_skill_execute(skill_id, input, out_output);
    return (void *)res;
}

void *sys_skill_list(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 2)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    char ***out_skills = (char ***)args[0];
    size_t *out_count = (size_t *)args[1];
    intptr_t res = agentos_sys_skill_list(out_skills, out_count);
    return (void *)res;
}

void *sys_skill_uninstall(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;
    const char *skill_id = (const char *)args[0];
    intptr_t res = agentos_sys_skill_uninstall(skill_id);
    return (void *)res;
}

/* P3.18 (ACC-DT27): 工具执行系统调用处理函数。
 *
 * 设计说明：
 * - args[0] 指向调用者构造的 tool_execute_args_t 结构体（栈或堆分配）。
 * - 结构体的 output_buffer 由调用者分配并保证 cap_size 字节可写，
 *   agentos_process_run_capture 将子进程 stdout 写入此缓冲区。
 * - exec_result 字段输出 agentos_process_run_capture 的原始返回值
 *   (0-255=exit code; -1=启动失败; -2=超时)，供调用者区分失败类型。
 * - 返回值：agentos_process_run_capture >= 0 时返回 AGENTOS_SUCCESS
 *   （进程启动成功，即使 exit code 非零也属于"执行完成"）；
 *   返回 -1/-2 时返回 AGENTOS_EFAIL（启动或超时失败）。
 *   调用者通过 targs.exec_result 获取具体退出码，通过返回值判断是否需 fail-closed。
 *
 * 安全说明：
 * - 此函数经 sandbox_invoke 调用，sandbox 的 permission_check 和 quota_check
 *   在本函数之前执行（见 sandbox.c:325-351），DENY 时本函数不会被调用。
 * - agentos_process_run_capture 使用 fork+execvp（不经 shell），消除命令注入风险。
 */
void *sys_tool_execute(void **args, int argc)
{
    CHECK_ARGS();
    if (argc != 1)
        return (void *)(intptr_t)AGENTOS_EINVAL;

    tool_execute_args_t *t = (tool_execute_args_t *)args[0];
    if (!t || !t->executable || !t->output_buffer || t->cap_size == 0)
        return (void *)(intptr_t)AGENTOS_EINVAL;

    t->exec_result = agentos_process_run_capture(
        t->executable, t->argv, NULL, t->timeout_ms, t->output_buffer, t->cap_size);

    /* >= 0: 子进程启动并退出（含非零 exit code）→ syscall 层面成功 */
    if (t->exec_result >= 0)
        return (void *)(intptr_t)AGENTOS_SUCCESS;
    /* -1=启动失败, -2=超时 → syscall 层面失败 */
    return (void *)(intptr_t)AGENTOS_EFAIL;
}

static agentos_cognition_engine_t *g_cognition_engine = NULL;

agentos_error_t agentos_syscalls_init(void)
{
    /* 幂等性保护：如果认知引擎已存在（重复 init 调用），直接返回成功，
     * 避免覆盖 g_cognition_engine 指针导致前一个引擎泄漏 */
    if (g_cognition_engine) {
        return AGENTOS_SUCCESS;
    }

    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &g_cognition_engine);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Cognition engine init failed: %d, continuing without cognition", err);
    }

    agentos_memory_provider_t *provider = agentos_memory_provider_get_active();
    if (!provider) {
        err = agentos_builtin_memory_provider_init(NULL);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Built-in memory provider init failed: %d", err);
        } else {
            provider = agentos_memory_provider_get_active();
        }
    }

    agentos_sys_init(g_cognition_engine, NULL, provider);

    agentos_sys_set_memory_provider(provider);

    AGENTOS_LOG_INFO("Syscalls layer initialized (cognition=%s, memory=%s)",
                     g_cognition_engine ? "ok" : "unavailable",
                     provider ? provider->name : "unavailable");

    return AGENTOS_SUCCESS;
}

void agentos_syscalls_cleanup(void)
{
    agentos_sys_agent_cleanup();
    agentos_sys_skill_cleanup();

    if (g_cognition_engine) {
        agentos_cognition_destroy(g_cognition_engine);
        g_cognition_engine = NULL;
    }

    agentos_sys_init(NULL, NULL, NULL);

    agentos_sys_set_memory_provider(NULL);

    agentos_memory_provider_unregister();

    AGENTOS_LOG_INFO("Syscalls layer cleanup completed");
}

void agentos_sys_free(void *ptr)
{
    if (ptr)
        AGENTOS_FREE(ptr);
}
