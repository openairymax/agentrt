/**
 * @file syscall_table.c
 * @brief 系统调用表（注册和分发）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"

#include <stddef.h>
#include <stdint.h> /* intptr_t */

/* 系统调用号 enum 现已提升为 syscalls.h 的公共 agentrt_syscall_num_t，
 * 消除 syscall_table.c / test_syscalls.c / test_syscall_extended.c 三处
 * 重复定义（零债务约束）。SYS_TOOL_EXECUTE 等符号直接来自 syscalls.h。 */

/* 编译期校验：SYS_TOOL_EXECUTE 必须为 23，SYS_MAX 必须为 24。
 * 若未来在 enum 中插入新项导致值漂移，此处立即编译失败。
 * 零债务约束：错误早暴露，不留运行期隐患。 */
_Static_assert(SYS_TOOL_EXECUTE == 23,
               "SYS_TOOL_EXECUTE must be 23 (see syscalls.h agentrt_syscall_num_t)");
_Static_assert(SYS_MAX == 24,
               "SYS_MAX must be 24 (see syscalls.h agentrt_syscall_num_t)");

typedef void *(*syscall_func_t)(void **args, int argc);

/* 函数声明（实现位?syscall_entry.c?*/
extern void *sys_task_submit(void **args, int argc);
extern void *sys_task_query(void **args, int argc);
extern void *sys_task_wait(void **args, int argc);
extern void *sys_task_cancel(void **args, int argc);
extern void *sys_memory_write(void **args, int argc);
extern void *sys_memory_search(void **args, int argc);
extern void *sys_memory_get(void **args, int argc);
extern void *sys_memory_delete(void **args, int argc);
extern void *sys_session_create(void **args, int argc);
extern void *sys_session_get(void **args, int argc);
extern void *sys_session_close(void **args, int argc);
extern void *sys_session_list(void **args, int argc);
extern void *sys_telemetry_metrics(void **args, int argc);
extern void *sys_telemetry_traces(void **args, int argc);
extern void *sys_agent_spawn(void **args, int argc);
extern void *sys_agent_terminate(void **args, int argc);
extern void *sys_agent_invoke(void **args, int argc);
extern void *sys_agent_list(void **args, int argc);
extern void *sys_skill_install(void **args, int argc);
extern void *sys_skill_execute(void **args, int argc);
extern void *sys_skill_list(void **args, int argc);
extern void *sys_skill_uninstall(void **args, int argc);
/* P3.18 (ACC-DT27): 工具执行 syscall（实现在 syscall_entry.c） */
extern void *sys_tool_execute(void **args, int argc);

static syscall_func_t syscall_table[SYS_MAX] = {
    [SYS_TASK_SUBMIT] = sys_task_submit,
    [SYS_TASK_QUERY] = sys_task_query,
    [SYS_TASK_WAIT] = sys_task_wait,
    [SYS_TASK_CANCEL] = sys_task_cancel,
    [SYS_MEMORY_WRITE] = sys_memory_write,
    [SYS_MEMORY_SEARCH] = sys_memory_search,
    [SYS_MEMORY_GET] = sys_memory_get,
    [SYS_MEMORY_DELETE] = sys_memory_delete,
    [SYS_SESSION_CREATE] = sys_session_create,
    [SYS_SESSION_GET] = sys_session_get,
    [SYS_SESSION_CLOSE] = sys_session_close,
    [SYS_SESSION_LIST] = sys_session_list,
    [SYS_TELEMETRY_METRICS] = sys_telemetry_metrics,
    [SYS_TELEMETRY_TRACES] = sys_telemetry_traces,
    [SYS_AGENT_SPAWN] = sys_agent_spawn,
    [SYS_AGENT_TERMINATE] = sys_agent_terminate,
    [SYS_AGENT_INVOKE] = sys_agent_invoke,
    [SYS_AGENT_LIST] = sys_agent_list,
    [SYS_SKILL_INSTALL] = sys_skill_install,
    [SYS_SKILL_EXECUTE] = sys_skill_execute,
    [SYS_SKILL_LIST] = sys_skill_list,
    [SYS_SKILL_UNINSTALL] = sys_skill_uninstall,
    [SYS_TOOL_EXECUTE] = sys_tool_execute,  /* P3.18 (ACC-DT27) */
};

void *agentrt_syscall_invoke(int syscall_num, void **args, int argc)
{
    if (syscall_num < 1 || syscall_num >= SYS_MAX)
        return (void *)(intptr_t)AGENTRT_EINVAL;
    syscall_func_t func = syscall_table[syscall_num];
    if (!func)
        return (void *)(intptr_t)AGENTRT_EPROTONOSUPPORT;
    return func(args, argc);
}