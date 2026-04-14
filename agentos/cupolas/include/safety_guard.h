// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file safety_guard.h
 * @brief SafetyGuard - AgentOS细粒度安全守卫框架
 *
 * 事件驱动安全守卫框架，实现细粒度安全控制与动态策略管理。
 * 与现有Cupolas安全穹顶非侵入式集成，提供守卫链执行、
 * 策略协调和动态策略更新能力。
 *
 * 核心设计:
 * 1. 事件驱动架构 — 安全事件触发守卫链执行
 * 2. 守卫链模式 — 可组合的安全检查链
 * 3. 策略协调器 — 解决安全策略冲突
 * 4. 动态策略更新 — 运行时策略热加载
 * 5. 资源配额控制 — 细粒度资源限制
 * 6. 审计追踪集成 — 与Cupolas审计模块联动
 *
 * @since 2.0.0
 * @see cupolas
 */

#ifndef AGENTOS_SAFETY_GUARD_H
#define AGENTOS_SAFETY_GUARD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAFETY_GUARD_VERSION        "1.0.0"
#define SAFETY_MAX_GUARDS           64
#define SAFETY_MAX_POLICIES         128
#define SAFETY_MAX_RESOURCES        256
#define SAFETY_MAX_AUDIT_ENTRIES    10000
#define SAFETY_MAX_SUBJECT_LEN      128
#define SAFETY_MAX_ACTION_LEN       64
#define SAFETY_MAX_RESOURCE_LEN     256

typedef enum {
    SAFETY_EVENT_ACCESS_REQUEST = 0,
    SAFETY_EVENT_RESOURCE_ALLOCATE,
    SAFETY_EVENT_DATA_FLOW,
    SAFETY_EVENT_EXECUTION_START,
    SAFETY_EVENT_EXECUTION_COMPLETE,
    SAFETY_EVENT_POLICY_CHANGE,
    SAFETY_EVENT_QUOTA_EXCEEDED,
    SAFETY_EVENT_VIOLATION_DETECTED,
    SAFETY_EVENT_EMERGENCY_STOP
} safety_event_type_t;

typedef enum {
    SAFETY_DECISION_ALLOW = 0,
    SAFETY_DECISION_DENY,
    SAFETY_DECISION_CONDITIONAL,
    SAFETY_DECISION_DEFER,
    SAFETY_DECISION_ABORT
} safety_decision_t;

typedef enum {
    SAFETY_GUARD_PERMISSION = 0,
    SAFETY_GUARD_INPUT,
    SAFETY_GUARD_RESOURCE,
    SAFETY_GUARD_AUDIT,
    SAFETY_GUARD_RATE_LIMIT,
    SAFETY_GUARD_CONTENT_FILTER,
    SAFETY_GUARD_DATA_FLOW,
    SAFETY_GUARD_CUSTOM
} safety_guard_type_t;

typedef enum {
    SAFETY_PRIORITY_LOWEST = 0,
    SAFETY_PRIORITY_LOW = 25,
    SAFETY_PRIORITY_NORMAL = 50,
    SAFETY_PRIORITY_HIGH = 75,
    SAFETY_PRIORITY_CRITICAL = 100
} safety_priority_t;

typedef enum {
    SAFETY_SEVERITY_INFO = 0,
    SAFETY_SEVERITY_WARNING,
    SAFETY_SEVERITY_ERROR,
    SAFETY_SEVERITY_CRITICAL,
    SAFETY_SEVERITY_FATAL
} safety_severity_t;

typedef struct {
    safety_event_type_t type;
    char subject[SAFETY_MAX_SUBJECT_LEN];
    char action[SAFETY_MAX_ACTION_LEN];
    char resource[SAFETY_MAX_RESOURCE_LEN];
    const void* context;
    size_t context_size;
    uint64_t timestamp;
    uint32_t flags;
} safety_event_t;

typedef struct {
    safety_decision_t decision;
    char reason[256];
    safety_severity_t severity;
    uint32_t conditions;
    void* modified_context;
    size_t modified_context_size;
} safety_result_t;

typedef struct {
    char name[64];
    char description[256];
    safety_guard_type_t type;
    safety_priority_t priority;
    bool enabled;
    bool blocking;
    bool audit_enabled;
} safety_guard_descriptor_t;

typedef safety_decision_t (*safety_guard_check_fn)(
    const safety_guard_descriptor_t* guard,
    const safety_event_t* event,
    safety_result_t* result,
    void* user_data);

typedef struct {
    char id[64];
    char name[128];
    char description[256];
    safety_decision_t default_decision;
    safety_priority_t priority;
    bool enabled;
    bool overridable;
    uint64_t valid_from;
    uint64_t valid_until;
    char* rules_json;
} safety_policy_t;

typedef struct {
    char resource_id[SAFETY_MAX_RESOURCE_LEN];
    int64_t limit;
    int64_t current_usage;
    int64_t reserved;
    uint64_t reset_interval_ms;
    uint64_t last_reset;
} safety_quota_t;

typedef struct {
    uint64_t event_id;
    safety_event_type_t event_type;
    char subject[SAFETY_MAX_SUBJECT_LEN];
    char action[SAFETY_MAX_ACTION_LEN];
    safety_decision_t decision;
    char reason[256];
    char guard_name[64];
    char policy_id[64];
    uint64_t timestamp;
} safety_audit_entry_t;

typedef struct safety_guard_context_s safety_guard_context_t;

typedef void (*safety_violation_callback_t)(
    const safety_event_t* event,
    const safety_result_t* result,
    void* user_data);

typedef void (*safety_policy_change_callback_t)(
    const char* policy_id,
    const char* change_type,
    void* user_data);

safety_guard_context_t* safety_guard_create(void);
void safety_guard_destroy(safety_guard_context_t* ctx);

int safety_guard_register_guard(safety_guard_context_t* ctx,
                                  const safety_guard_descriptor_t* descriptor,
                                  safety_guard_check_fn check_fn,
                                  void* user_data);

int safety_guard_unregister_guard(safety_guard_context_t* ctx, const char* name);

int safety_guard_enable_guard(safety_guard_context_t* ctx, const char* name);
int safety_guard_disable_guard(safety_guard_context_t* ctx, const char* name);

safety_decision_t safety_guard_check(safety_guard_context_t* ctx,
                                       const safety_event_t* event,
                                       safety_result_t* result);

safety_decision_t safety_guard_check_chain(safety_guard_context_t* ctx,
                                              const safety_event_t* event,
                                              safety_result_t** results,
                                              size_t* result_count);

int safety_guard_add_policy(safety_guard_context_t* ctx,
                              const safety_policy_t* policy);

int safety_guard_remove_policy(safety_guard_context_t* ctx, const char* policy_id);

int safety_guard_update_policy(safety_guard_context_t* ctx,
                                 const char* policy_id,
                                 const char* new_rules_json);

int safety_guard_load_policies(safety_guard_context_t* ctx,
                                 const char* policies_json);

int safety_guard_resolve_conflict(safety_guard_context_t* ctx,
                                    const char* policy_a_id,
                                    const char* policy_b_id,
                                    safety_decision_t* resolved_decision);

int safety_guard_set_quota(safety_guard_context_t* ctx,
                             const char* resource_id,
                             int64_t limit,
                             uint64_t reset_interval_ms);

int safety_guard_check_quota(safety_guard_context_t* ctx,
                               const char* resource_id,
                               int64_t requested,
                               bool* allowed);

int safety_guard_consume_quota(safety_guard_context_t* ctx,
                                 const char* resource_id,
                                 int64_t amount);

int safety_guard_release_quota(safety_guard_context_t* ctx,
                                 const char* resource_id,
                                 int64_t amount);

int safety_guard_record_audit(safety_guard_context_t* ctx,
                                const safety_event_t* event,
                                const safety_result_t* result,
                                const char* guard_name);

int safety_guard_query_audit(safety_guard_context_t* ctx,
                               const char* subject,
                               uint64_t from_timestamp,
                               uint64_t to_timestamp,
                               safety_audit_entry_t** entries,
                               size_t* entry_count);

int safety_guard_set_violation_callback(safety_guard_context_t* ctx,
                                          safety_violation_callback_t callback,
                                          void* user_data);

int safety_guard_set_policy_change_callback(safety_guard_context_t* ctx,
                                               safety_policy_change_callback_t callback,
                                               void* user_data);

int safety_guard_emergency_stop(safety_guard_context_t* ctx, const char* reason);
int safety_guard_emergency_release(safety_guard_context_t* ctx);

size_t safety_guard_get_guard_count(safety_guard_context_t* ctx);
size_t safety_guard_get_policy_count(safety_guard_context_t* ctx);
size_t safety_guard_get_audit_count(safety_guard_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SAFETY_GUARD_H */
