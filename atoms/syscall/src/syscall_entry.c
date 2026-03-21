/**
 * @file syscall_entry.c
 * @brief 系统调用统一入口（参数解析和分发）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "syscalls.h"
#include <stdint.h>
#include <string.h>

/* 辅助宏：将 void* 转换为整数类型 */
static inline uint32_t arg_to_uint32(void* p) {
    return (uint32_t)(uintptr_t)p;
}
static inline int32_t arg_to_int32(void* p) {
    return (int32_t)(intptr_t)p;
}
static inline size_t arg_to_size(void* p) {
    return (size_t)(uintptr_t)p;
}

/* -------------------- 任务相关 -------------------- */

void* sys_task_submit(void** args, int argc) {
    if (argc != 4) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* input = (const char*)args[0];
    size_t len = arg_to_size(args[1]);
    uint32_t timeout = arg_to_uint32(args[2]);
    char** out = (char**)args[3];
    intptr_t res = agentos_sys_task_submit(input, len, timeout, out);
    return (void*)res;
}

void* sys_task_query(void** args, int argc) {
    if (argc != 2) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* task_id = (const char*)args[0];
    int* status = (int*)args[1];
    intptr_t res = agentos_sys_task_query(task_id, status);
    return (void*)res;
}

void* sys_task_wait(void** args, int argc) {
    if (argc != 3) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* task_id = (const char*)args[0];
    uint32_t timeout = arg_to_uint32(args[1]);
    char** out = (char**)args[2];
    intptr_t res = agentos_sys_task_wait(task_id, timeout, out);
    return (void*)res;
}

void* sys_task_cancel(void** args, int argc) {
    if (argc != 1) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* task_id = (const char*)args[0];
    intptr_t res = agentos_sys_task_cancel(task_id);
    return (void*)res;
}

/* -------------------- 记忆相关 -------------------- */

void* sys_memory_write(void** args, int argc) {
    if (argc != 4) return (void*)(intptr_t)AGENTOS_EINVAL;
    const void* data = args[0];
    size_t len = arg_to_size(args[1]);
    const char* metadata = (const char*)args[2];
    char** out_record_id = (char**)args[3];
    intptr_t res = agentos_sys_memory_write(data, len, metadata, out_record_id);
    return (void*)res;
}

void* sys_memory_search(void** args, int argc) {
    if (argc != 5) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* query = (const char*)args[0];
    uint32_t limit = arg_to_uint32(args[1]);
    char*** out_record_ids = (char***)args[2];
    float** out_scores = (float**)args[3];
    size_t* out_count = (size_t*)args[4];
    intptr_t res = agentos_sys_memory_search(query, limit, out_record_ids, out_scores, out_count);
    return (void*)res;
}

void* sys_memory_get(void** args, int argc) {
    if (argc != 3) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* record_id = (const char*)args[0];
    void** out_data = (void**)args[1];
    size_t* out_len = (size_t*)args[2];
    intptr_t res = agentos_sys_memory_get(record_id, out_data, out_len);
    return (void*)res;
}

void* sys_memory_delete(void** args, int argc) {
    if (argc != 1) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* record_id = (const char*)args[0];
    intptr_t res = agentos_sys_memory_delete(record_id);
    return (void*)res;
}

/* -------------------- 会话相关 -------------------- */

void* sys_session_create(void** args, int argc) {
    if (argc != 2) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* metadata = (const char*)args[0];
    char** out_session_id = (char**)args[1];
    intptr_t res = agentos_sys_session_create(metadata, out_session_id);
    return (void*)res;
}

void* sys_session_get(void** args, int argc) {
    if (argc != 2) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* session_id = (const char*)args[0];
    char** out_info = (char**)args[1];
    intptr_t res = agentos_sys_session_get(session_id, out_info);
    return (void*)res;
}

void* sys_session_close(void** args, int argc) {
    if (argc != 1) return (void*)(intptr_t)AGENTOS_EINVAL;
    const char* session_id = (const char*)args[0];
    intptr_t res = agentos_sys_session_close(session_id);
    return (void*)res;
}

void* sys_session_list(void** args, int argc) {
    if (argc != 2) return (void*)(intptr_t)AGENTOS_EINVAL;
    char*** out_sessions = (char***)args[0];
    size_t* out_count = (size_t*)args[1];
    intptr_t res = agentos_sys_session_list(out_sessions, out_count);
    return (void*)res;
}

/* -------------------- 可观测性相关 -------------------- */

void* sys_telemetry_metrics(void** args, int argc) {
    if (argc != 1) return (void*)(intptr_t)AGENTOS_EINVAL;
    char** out_metrics = (char**)args[0];
    intptr_t res = agentos_sys_telemetry_metrics(out_metrics);
    return (void*)res;
}

void* sys_telemetry_traces(void** args, int argc) {
    if (argc != 1) return (void*)(intptr_t)AGENTOS_EINVAL;
    char** out_traces = (char**)args[0];
    intptr_t res = agentos_sys_telemetry_traces(out_traces);
    return (void*)res;
}