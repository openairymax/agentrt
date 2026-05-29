/**
 * @file sandbox_internal.h
 * @brief 沙箱内部数据结构定义（sandbox.c / sandbox_utils.c / sandbox_permission.c / sandbox_quota.c
 * 共享）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 此头文件包含 struct agentos_sandbox 和所有依赖类型的权威定义。
 * 所有沙箱子模块（sandbox.c, sandbox_utils.c, sandbox_permission.c, sandbox_quota.c）
 * 必须包含此头文件，而非各自私下定义 struct agentos_sandbox 以避免结构体偏移量不一致。
 */

#ifndef SANDBOX_INTERNAL_H
#define SANDBOX_INTERNAL_H

#include "agentos.h"
#include "sandbox_permission.h"
#include "sandbox_quota.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */

typedef enum {
    SANDBOX_STATE_IDLE = 0,
    SANDBOX_STATE_ACTIVE,
    SANDBOX_STATE_SUSPENDED,
    SANDBOX_STATE_TERMINATED
} sandbox_state_t;

#define AUDIT_LOG_BUFFER_SIZE 256

typedef struct audit_entry {
    uint64_t timestamp;
    int event_type;
    char syscall_name[32];
    int syscall_num;
    int result;
    char message[AUDIT_LOG_BUFFER_SIZE];
    uint64_t resource_usage_before;
    uint64_t resource_usage_after;
    int severity;
} audit_entry_t;

typedef struct security_policy {
    uint32_t version;
    time_t last_updated;
    char updated_by[64];
    int allow_dynamic_update;
    int strict_mode;
    int audit_level;
    float risk_threshold;
} security_policy_t;

typedef struct sandbox_performance_stats {
    uint64_t total_syscalls;
    uint64_t successful_calls;
    uint64_t failed_calls;
    uint64_t blocked_calls;
    uint64_t total_cpu_time_ns;
    uint64_t max_memory_bytes;
    uint64_t current_memory_bytes;
    double avg_response_time_ns;
    uint64_t violations_since_reset;
    time_t stats_reset_time;
} sandbox_perf_stats_t;

typedef struct sandbox_config {
    char *sandbox_name;
    char *owner_id;
    uint32_t priority;
    uint32_t timeout_ms;
    uint32_t flags;
    resource_quota_t quota;
} sandbox_config_t;

struct agentos_sandbox {
    uint64_t sandbox_id;
    char *sandbox_name;
    char *owner_id;
    sandbox_state_t state;
    sandbox_config_t manager;
    permission_rule_t *rules;
    uint32_t rule_count;
    agentos_mutex_t *lock;
    uint64_t create_time_ns;
    uint64_t last_active_ns;
    uint64_t call_count;
    uint64_t violation_count;
    audit_entry_t *audit_log;
    size_t audit_count;
    size_t audit_capacity;
    size_t audit_write_index;
    security_policy_t policy;
    sandbox_perf_stats_t perf_stats;
    resource_quota_t quota;
    int enable_input_sanitization;
    int enable_resource_monitoring;
};

#ifdef __cplusplus
}
#endif

#endif /* SANDBOX_INTERNAL_H */