/**
 * @file id_utils.c
 * @brief ID生成工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/id_utils.h"

#include "error.h"

#include <stdint.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"

/* 平台特定头文件 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <rpc.h>
#include <windows.h>
#pragma comment(lib, "rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

// 全局计数器用于生成唯一ID（使用统一原子类型）
static atomic_long task_counter = 0;
static atomic_long plan_counter = 0;
static atomic_long record_counter = 0;
static atomic_long session_counter = 0;

__attribute__((used)) void agentos_generate_task_id(const char *prefix, char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    unsigned long long id = atomic_fetch_add(&task_counter, 1) + 1;
    snprintf(buf, len, "%s_%llu", prefix ? prefix : "task", id);
}

__attribute__((used)) void agentos_generate_plan_id(char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    unsigned long long id = atomic_fetch_add(&plan_counter, 1) + 1;
    snprintf(buf, len, "plan_%llu", id);
}

__attribute__((used)) void agentos_generate_record_id(char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    unsigned long long id = atomic_fetch_add(&record_counter, 1) + 1;
    snprintf(buf, len, "record_%llu", id);
}

__attribute__((used)) void agentos_generate_session_id(char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    time_t now = (time_t)(agentos_time_ms() / 1000ULL);
    unsigned long long id = atomic_fetch_add(&session_counter, 1) + 1;
    snprintf(buf, len, "session_%lld_%llu", (long long)now, id);
}

__attribute__((used)) agentos_error_t agentos_generate_uuid(char *buf)
{
    if (!buf)
        return AGENTOS_EINVAL;

#ifdef _WIN32
    UUID uuid;
    RPC_STATUS status = UuidCreate(&uuid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
        return AGENTOS_EINVAL;
    }

    // 转换为字符串
    unsigned char *str = NULL;
    status = UuidToStringA(&uuid, &str);
    if (status != RPC_S_OK) {
        return AGENTOS_EINVAL;
    }

AGENTOS_STRNCPY_TERM(buf, (char *)str, 37);
    RpcStringFreeA(&str);
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, buf);
#endif

    return AGENTOS_SUCCESS;
}
