/**
 * @file sandbox_utils.c
 * @brief 沙箱工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sandbox_utils.h"

#include "agentrt.h"
#include "logger.h"
#include "sandbox_internal.h"
#include "sandbox_quota.h"

#include <string.h>
#include <time.h>

/* 基础库兼容性层 */
#include "memory_compat.h"
#include "string_compat.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/* ==================== 审计日志管理 ==================== */

uint64_t sandbox_simple_hash(const char *str)
{
    if (!str)
        return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static uint64_t get_timestamp_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

agentrt_error_t sandbox_add_audit_entry(agentrt_sandbox_t *sandbox, int syscall_num,
                                        const char *caller_id, int result_code,
                                        uint64_t duration_ns, const char *details)
{
    if (!sandbox)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agentrt_mutex_lock(sandbox->lock);

    /* 检查容量，必要时扩容 */
    if (sandbox->audit_count >= sandbox->audit_capacity) {
        size_t new_capacity = sandbox->audit_capacity == 0 ? 100 : sandbox->audit_capacity * 2;
        if (new_capacity > 10000)
            new_capacity = 10000; /* MAX_AUDIT_ENTRIES */

        audit_entry_t *new_log = (audit_entry_t *)AGENTRT_REALLOC(
            sandbox->audit_log, new_capacity * sizeof(audit_entry_t));
        if (new_log) {
            sandbox->audit_log = new_log;
            sandbox->audit_capacity = new_capacity;
        }
    }

    /* 添加条目 */
    if (sandbox->audit_count < sandbox->audit_capacity) {
        audit_entry_t *entry = (audit_entry_t *)sandbox->audit_log + sandbox->audit_count;
        entry->timestamp = get_timestamp_ns();
        entry->event_type = 0;
        entry->syscall_num = syscall_num;
        entry->result = result_code;
        entry->resource_usage_before = 0;
        entry->resource_usage_after = duration_ns;
        entry->severity = (result_code != 0) ? 2 : 0;
        entry->syscall_name[0] = '\0';
        snprintf(entry->message, sizeof(entry->message), "caller=%s dur=%llu %s",
                 caller_id ? caller_id : "?", (unsigned long long)duration_ns,
                 details ? details : "");

        sandbox->audit_count++;
    }

    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

void sandbox_release_resource(agentrt_sandbox_t *sandbox, int syscall_num, void *args, int result)
{
    if (!sandbox)
        return;

    agentrt_mutex_lock(sandbox->lock);

    if (result != 0) {
        sandbox_quota_release(sandbox, RESOURCE_CPU, 1);
        if (args) {
            sandbox_quota_release(sandbox, RESOURCE_MEMORY, 256);
        }
    } else {
        sandbox_quota_release(sandbox, RESOURCE_CPU, 1);
        if (syscall_num == 2 && args) {
            sandbox_quota_release(sandbox, RESOURCE_IO, 1);
        } else if (syscall_num == 3 && args) {
            sandbox_quota_release(sandbox, RESOURCE_MEMORY, 4096);
        }
    }

    agentrt_mutex_unlock(sandbox->lock);
}

char *sandbox_generate_args_hash(void *args)
{
    if (!args) return NULL;

    unsigned char *bytes = (unsigned char *)args;
    uint32_t hash = 5381;
    for (size_t i = 0; i < sizeof(void *) * 2; i++) {
        hash = ((hash << 5) + hash) + bytes[i % sizeof(void *)];
    }
    char hash_str[32];
    snprintf(hash_str, sizeof(hash_str), "%08x", hash);
    return AGENTRT_STRDUP(hash_str);
}
