/**
 * @file id_utils.c
 * @brief ID生成工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "id_utils.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#include "../../../../commons/utils/include/atomic_compat.h"
#else
#include <uuid/uuid.h>
#endif

// 全局计数器用于生成唯一ID
#ifdef _WIN32
static volatile LONG task_counter = 0;
static volatile LONG plan_counter = 0;
static volatile LONG record_counter = 0;
static volatile LONG session_counter = 0;
#else
#include <stdatomic.h>
static atomic_ullong task_counter = 0;
static atomic_ullong plan_counter = 0;
static atomic_ullong record_counter = 0;
static atomic_ullong session_counter = 0;
#endif

void agentos_generate_task_id(const char* prefix, char* buf, size_t len) {
    if (!buf || len == 0) return;

#ifdef _WIN32
    LONG id = InterlockedIncrement(&task_counter);
    snprintf(buf, len, "%s_%ld", prefix ? prefix : "task", id);
#else
    unsigned long long id = atomic_fetch_add(&task_counter, 1);
    snprintf(buf, len, "%s_%llu", prefix ? prefix : "task", id);
#endif
}

void agentos_generate_plan_id(char* buf, size_t len) {
    if (!buf || len == 0) return;

#ifdef _WIN32
    LONG id = InterlockedIncrement(&plan_counter);
    snprintf(buf, len, "plan_%ld", id);
#else
    unsigned long long id = atomic_fetch_add(&plan_counter, 1);
    snprintf(buf, len, "plan_%llu", id);
#endif
}

void agentos_generate_record_id(char* buf, size_t len) {
    if (!buf || len == 0) return;

#ifdef _WIN32
    LONG id = InterlockedIncrement(&record_counter);
    snprintf(buf, len, "record_%ld", id);
#else
    unsigned long long id = atomic_fetch_add(&record_counter, 1);
    snprintf(buf, len, "record_%llu", id);
#endif
}

void agentos_generate_session_id(char* buf, size_t len) {
    if (!buf || len == 0) return;

    // 使用时间戳和计数器生成会话ID
    time_t now = time(NULL);

#ifdef _WIN32
    LONG id = InterlockedIncrement(&session_counter);
    snprintf(buf, len, "session_%lld_%ld", (long long)now, id);
#else
    unsigned long long id = atomic_fetch_add(&session_counter, 1);
    snprintf(buf, len, "session_%lld_%llu", (long long)now, id);
#endif
}

agentos_error_t agentos_generate_uuid(char* buf) {
    if (!buf) return AGENTOS_EINVAL;

#ifdef _WIN32
    UUID uuid;
    RPC_STATUS status = UuidCreate(&uuid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
        return AGENTOS_EINVAL;
    }

    // 转换为字符串
    unsigned char* str = NULL;
    status = UuidToStringA(&uuid, &str);
    if (status != RPC_S_OK) {
        return AGENTOS_EINVAL;
    }

    strncpy(buf, (char*)str, 37);
    buf[36] = '\0';
    RpcStringFreeA(&str);
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, buf);
#endif

    return AGENTOS_SUCCESS;
}
