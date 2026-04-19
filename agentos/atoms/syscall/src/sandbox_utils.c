/**
 * @file sandbox_utils.c
 * @brief 沙箱工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sandbox_utils.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>
#include <time.h>

/* 基础库兼容性层 */
#include "include/memory_compat.h"
#include "string_compat.h"

/* 沙箱内部结构（需要知道布局） */
struct agentos_sandbox {
    uint64_t sandbox_id;
    char* sandbox_name;
    char* owner_id;
    void* state;
    void* manager;
    void* rules;
    uint32_t rule_count;
    agentos_mutex_t* lock;
    uint64_t create_time_ns;
    uint64_t last_active_ns;
    uint64_t call_count;
    uint64_t violation_count;
    void* audit_log;
    size_t audit_count;
    size_t audit_capacity;
};

typedef struct audit_entry {
    uint64_t timestamp_ns;
    uint64_t sandbox_id;
    int syscall_num;
    char* caller_id;
    char* args_hash;
    int result_code;
    uint64_t duration_ns;
    char* details;
} audit_entry_t;

/* ==================== 工具函数 ==================== */

uint64_t sandbox_simple_hash(const char* str) {
    if (!str) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

agentos_error_t sandbox_add_audit_entry(agentos_sandbox_t* sandbox, int syscall_num,
                                       const char* caller_id, int result_code,
                                       uint64_t duration_ns, const char* details) {
    if (!sandbox) return AGENTOS_EINVAL;

    agentos_mutex_lock(sandbox->lock);

    /* 检查容量，必要时扩容 */
    if (sandbox->audit_count >= sandbox->audit_capacity) {
        size_t new_capacity = sandbox->audit_capacity == 0 ? 100 : sandbox->audit_capacity * 2;
        if (new_capacity > 10000) new_capacity = 10000;  /* MAX_AUDIT_ENTRIES */

        audit_entry_t* new_log = (audit_entry_t*)AGENTOS_REALLOC(sandbox->audit_log,
                                                          new_capacity * sizeof(audit_entry_t));
        if (new_log) {
            sandbox->audit_log = new_log;
            sandbox->audit_capacity = new_capacity;
        }
    }

    /* 添加条目 */
    if (sandbox->audit_count < sandbox->audit_capacity) {
        audit_entry_t* entry = &sandbox->audit_log[sandbox->audit_count];
        entry->timestamp_ns = get_timestamp_ns();
        entry->sandbox_id = sandbox->sandbox_id;
        entry->syscall_num = syscall_num;
        entry->caller_id = caller_id ? AGENTOS_STRDUP(caller_id) : NULL;
        entry->args_hash = NULL;
        entry->result_code = result_code;
        entry->duration_ns = duration_ns;
        entry->details = details ? AGENTOS_STRDUP(details) : NULL;

        sandbox->audit_count++;
    }

    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

void sandbox_release_resource(agentos_sandbox_t* sandbox, int syscall_num,
                             void* args, int result) {
    if (!sandbox) return;

    (void)syscall_num;  /* 未使用 */
    (void)args;         /* 未使用 */
    (void)result;       /* 未使用 */

    agentos_mutex_lock(sandbox->lock);

    /* 简化实现：重置资源配额 */
    /* 实际应该根据具体资源类型释放 */
    
    agentos_mutex_unlock(sandbox->lock);
}

char* sandbox_generate_args_hash(void* args) {
    if (!args) return NULL;
    
    /* 简化实现：返回参数的哈希表示 */
    /* 实际应该根据参数类型进行序列化后哈希 */
    char hash_str[32];
    snprintf(hash_str, sizeof(hash_str), "%p", args);
    return AGENTOS_STRDUP(hash_str);
}
