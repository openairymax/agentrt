/**
 * @file syscall_table.c
 * @brief 系统调用表（注册和分发）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include <stddef.h>

/* 系统调用号枚�?*/
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
    SYS_MAX
};

typedef void* (*syscall_func_t)(void** args, int argc);

/* 函数声明（实现位�?syscall_entry.c�?*/
extern void* sys_task_submit(void** args, int argc);
extern void* sys_task_query(void** args, int argc);
extern void* sys_task_wait(void** args, int argc);
extern void* sys_task_cancel(void** args, int argc);
extern void* sys_memory_write(void** args, int argc);
extern void* sys_memory_search(void** args, int argc);
extern void* sys_memory_get(void** args, int argc);
extern void* sys_memory_delete(void** args, int argc);
extern void* sys_session_create(void** args, int argc);
extern void* sys_session_get(void** args, int argc);
extern void* sys_session_close(void** args, int argc);
extern void* sys_session_list(void** args, int argc);
extern void* sys_telemetry_metrics(void** args, int argc);
extern void* sys_telemetry_traces(void** args, int argc);
extern void* sys_agent_spawn(void** args, int argc);
extern void* sys_agent_terminate(void** args, int argc);
extern void* sys_agent_invoke(void** args, int argc);
extern void* sys_agent_list(void** args, int argc);

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
};

void* agentos_syscall_invoke(int syscall_num, void** args, int argc) {
    if (syscall_num < 1 || syscall_num >= SYS_MAX) return (void*)(intptr_t)AGENTOS_EINVAL;
    syscall_func_t func = syscall_table[syscall_num];
    if (!func) return (void*)(intptr_t)AGENTOS_ENOTSUP;
    return func(args, argc);
}