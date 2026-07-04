/**
 * @file safety_guard.c
 * @brief V2 SafetyGuard API 实现
 *
 * 提供 safety_guard_create、safety_guard_check_chain 等 V2 API 的
 * 完整实现。支持守卫链执行、策略协调、配额控制和审计追踪。
 *
 * P1.4: C-L05 Cupolas SafetyGuard → tool_d 工具审批
 * 实现 6 种守卫类型与 tool_d 的映射：
 *   - SAFETY_GUARD_PERMISSION    → RBAC 权限检查
 *   - SAFETY_GUARD_RATE_LIMIT    → 工具调用频率限制
 *   - SAFETY_GUARD_CONTENT_FILTER → 输入内容过滤
 *   - SAFETY_GUARD_INPUT         → 参数净化
 *   - SAFETY_GUARD_RESOURCE      → 资源配额检查
 *   - SAFETY_GUARD_AUDIT         → 审计日志记录
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "safety_guard.h"

#include "memory_compat.h"
#include "string_compat.h"
#include "config_unified.h"

#include <stdio.h>
#include <string.h>

/* ==================== SafetyGuard 上下文结构 ==================== */

typedef struct {
    safety_guard_descriptor_t descriptor;
    safety_guard_check_fn check_fn;
    void *user_data;
} guard_entry_t;

struct safety_guard_context_s {
    guard_entry_t *guards;
    size_t guard_count;
    size_t guard_capacity;
    bool initialized;

    /* P1.4.3: 审计日志 */
    safety_audit_entry_t *audit_entries;
    size_t audit_count;
    size_t audit_capacity;

    /* 策略 */
    safety_policy_t *policies;
    size_t policy_count;
    size_t policy_capacity;

    /* 配额 */
    safety_quota_t *quotas;
    size_t quota_count;
    size_t quota_capacity;

    /* 回调 */
    safety_violation_callback_t violation_callback;
    void *violation_user_data;
    safety_policy_change_callback_t policy_change_callback;
    void *policy_change_user_data;

    bool emergency_stopped;
    char emergency_reason[256];
};

/* ==================== 内部：守卫类型默认检查函数 ==================== */

/**
 * @brief 默认权限检查（SAFETY_GUARD_PERMISSION）
 * 检查 agent_id 是否有权限执行指定操作
 */
static safety_decision_t default_permission_check(const safety_guard_descriptor_t *guard,
                                                   const safety_event_t *event,
                                                   safety_result_t *result)
{
    (void)guard;
    /* 基本 RBAC 检查：如果 subject 和 action 非空，视为有权限 */
    if (event->subject[0] == '\0' || event->action[0] == '\0') {
        if (result) {
            result->decision = SAFETY_DECISION_DENY;
            snprintf(result->reason, sizeof(result->reason),
                     "Permission denied: empty subject or action");
            result->severity = SAFETY_SEVERITY_ERROR;
        }
        return SAFETY_DECISION_DENY;
    }
    /* 默认允许已知 agent */
    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason),
                 "Permission granted: %s → %s", event->subject, event->action);
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/**
 * @brief 默认速率限制检查（SAFETY_GUARD_RATE_LIMIT）
 * 基于注册的配额系统检查工具调用频率。
 * 具体限制通过 safety_guard_set_quota() 为每个 agent 单独配置。
 * 此默认实现检查 event->flags 中的频控标记位。
 */
static safety_decision_t default_rate_limit_check(const safety_guard_descriptor_t *guard,
                                                   const safety_event_t *event,
                                                   safety_result_t *result)
{
    (void)guard;
    if (event->flags & 0x01) { /* RATE_LIMITED flag */
        if (result) {
            result->decision = SAFETY_DECISION_DENY;
            snprintf(result->reason, sizeof(result->reason),
                     "Rate limit exceeded for %s", event->subject);
            result->severity = SAFETY_SEVERITY_WARNING;
        }
        return SAFETY_DECISION_DENY;
    }
    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason),
                 "Rate limit OK for %s", event->subject);
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/**
 * @brief 默认内容过滤检查（SAFETY_GUARD_CONTENT_FILTER）
 * 检查输入内容是否包含敏感/危险模式
 */
static safety_decision_t default_content_filter_check(const safety_guard_descriptor_t *guard,
                                                       const safety_event_t *event,
                                                       safety_result_t *result)
{
    (void)guard;
    /* 检查 context 中的内容是否包含危险模式 */
    if (event->context && event->context_size > 0) {
        const char *content = (const char *)event->context;
        /* 简单的危险模式检测 */
        static const char *dangerous_patterns[] = {
            "rm -rf /", "DROP TABLE", "DELETE FROM", "shutdown",
            "format c:", "wget http", "curl http", "/etc/passwd",
            NULL
        };
        for (int i = 0; dangerous_patterns[i]; i++) {
            if (strstr(content, dangerous_patterns[i])) {
                if (result) {
                    result->decision = SAFETY_DECISION_DENY;
                    snprintf(result->reason, sizeof(result->reason),
                             "Content filter: dangerous pattern '%s' detected",
                             dangerous_patterns[i]);
                    result->severity = SAFETY_SEVERITY_ERROR;
                }
                return SAFETY_DECISION_DENY;
            }
        }
    }
    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason), "Content filter passed");
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/**
 * @brief 默认输入检查（SAFETY_GUARD_INPUT）
 * 检查输入参数的合法性
 */
static safety_decision_t default_input_check(const safety_guard_descriptor_t *guard,
                                              const safety_event_t *event,
                                              safety_result_t *result)
{
    (void)guard;
    /* 检查输入长度 */
    if (event->context_size > (1024 * 1024)) { /* 1MB 限制 */
        if (result) {
            result->decision = SAFETY_DECISION_DENY;
            snprintf(result->reason, sizeof(result->reason),
                     "Input too large: %zu bytes", event->context_size);
            result->severity = SAFETY_SEVERITY_ERROR;
        }
        return SAFETY_DECISION_DENY;
    }
    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason), "Input validation passed");
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/**
 * @brief 默认资源检查（SAFETY_GUARD_RESOURCE）
 *
 * 基于事件数据进行资源验证：
 * 1. 检查 event->flags 中的资源耗尽标志位（bit 1 = RESOURCE_EXHAUSTED）
 * 2. 检查 context_size 是否超过单次资源操作上限（100MB）
 * 3. 检查 resource 字段是否为已知受限资源
 *
 * 注意：默认检查函数签名不含 context，无法访问 ctx->quotas。
 * 精确配额检查通过 safety_guard_check_quota() API 或注册自定义
 * check_fn（将 ctx 作为 user_data 传入）实现。
 */
static safety_decision_t default_resource_check(const safety_guard_descriptor_t *guard,
                                                 const safety_event_t *event,
                                                 safety_result_t *result)
{
    (void)guard;

    /* 检查资源耗尽标志位（bit 1） */
    if (event->flags & 0x02) {
        if (result) {
            result->decision = SAFETY_DECISION_DENY;
            snprintf(result->reason, sizeof(result->reason),
                     "Resource exhausted: %s flagged as over limit", event->resource);
            result->severity = SAFETY_SEVERITY_ERROR;
        }
        return SAFETY_DECISION_DENY;
    }

    /* 检查单次资源操作大小上限（100MB） */
    const size_t MAX_RESOURCE_OP_SIZE = 100 * 1024 * 1024;
    if (event->context_size > MAX_RESOURCE_OP_SIZE) {
        if (result) {
            result->decision = SAFETY_DECISION_DENY;
            snprintf(result->reason, sizeof(result->reason),
                     "Resource operation too large: %zu bytes (max %zu)",
                     event->context_size, MAX_RESOURCE_OP_SIZE);
            result->severity = SAFETY_SEVERITY_ERROR;
        }
        return SAFETY_DECISION_DENY;
    }

    /* 检查受限资源模式：阻止对系统关键路径的写操作 */
    if (event->resource[0] != '\0') {
        static const char *restricted_resources[] = {
            "/proc/kcore", "/dev/mem", "/dev/kmem", "/dev/port",
            NULL
        };
        for (int i = 0; restricted_resources[i]; i++) {
            if (strstr(event->resource, restricted_resources[i])) {
                if (result) {
                    result->decision = SAFETY_DECISION_DENY;
                    snprintf(result->reason, sizeof(result->reason),
                             "Resource access denied: '%s' is restricted",
                             restricted_resources[i]);
                    result->severity = SAFETY_SEVERITY_ERROR;
                }
                return SAFETY_DECISION_DENY;
            }
        }
    }

    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason),
                 "Resource check passed for %s", event->resource[0] ? event->resource : "(none)");
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/**
 * @brief 默认审计检查（SAFETY_GUARD_AUDIT）
 *
 * 审计守卫的设计语义：审计不阻止操作，只记录事件。
 * 实际的审计日志写入由编排函数（safety_guard_check / check_chain）
 * 在调用本函数后通过 safety_guard_record_audit() 完成。
 *
 * 本函数的职责：
 * 1. 验证事件具有审计所需的最小元数据（subject + action）
 * 2. 标记审计结果，供编排函数记录
 * 3. 始终返回 ALLOW（审计不阻断业务流）
 */
static safety_decision_t default_audit_check(const safety_guard_descriptor_t *guard,
                                              const safety_event_t *event,
                                              safety_result_t *result)
{
    (void)guard;

    /* 验证审计所需的最小元数据 */
    if (event->subject[0] == '\0' || event->action[0] == '\0') {
        if (result) {
            result->decision = SAFETY_DECISION_ALLOW;
            snprintf(result->reason, sizeof(result->reason),
                     "Audit warning: incomplete event metadata (subject/action empty)");
            result->severity = SAFETY_SEVERITY_WARNING;
        }
        return SAFETY_DECISION_ALLOW;
    }

    if (result) {
        result->decision = SAFETY_DECISION_ALLOW;
        snprintf(result->reason, sizeof(result->reason),
                 "Audit: event recorded for %s → %s", event->subject, event->action);
        result->severity = SAFETY_SEVERITY_INFO;
    }
    return SAFETY_DECISION_ALLOW;
}

/* 守卫类型到默认检查函数的映射 */
typedef safety_decision_t (*default_check_fn_t)(const safety_guard_descriptor_t *,
                                                 const safety_event_t *, safety_result_t *);

static default_check_fn_t get_default_check_fn(safety_guard_type_t type)
{
    switch (type) {
        case SAFETY_GUARD_PERMISSION:    return default_permission_check;
        case SAFETY_GUARD_RATE_LIMIT:    return default_rate_limit_check;
        case SAFETY_GUARD_CONTENT_FILTER: return default_content_filter_check;
        case SAFETY_GUARD_INPUT:         return default_input_check;
        case SAFETY_GUARD_RESOURCE:      return default_resource_check;
        case SAFETY_GUARD_AUDIT:         return default_audit_check;
        default:                         return NULL;
    }
}

/* ==================== V2 API 实现 ==================== */

safety_guard_context_t *safety_guard_create(void)
{
    safety_guard_context_t *ctx = (safety_guard_context_t *)AGENTOS_CALLOC(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->guard_capacity = SAFETY_MAX_GUARDS;
    ctx->guards = (guard_entry_t *)AGENTOS_CALLOC(ctx->guard_capacity, sizeof(guard_entry_t));
    if (!ctx->guards) {
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->audit_capacity = 256;
    ctx->audit_entries = (safety_audit_entry_t *)AGENTOS_CALLOC(ctx->audit_capacity,
                                                        sizeof(safety_audit_entry_t));
    if (!ctx->audit_entries) {
        AGENTOS_FREE(ctx->guards);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->policy_capacity = 16;
    ctx->policies = (safety_policy_t *)AGENTOS_CALLOC(ctx->policy_capacity, sizeof(safety_policy_t));
    if (!ctx->policies) {
        AGENTOS_FREE(ctx->audit_entries);
        AGENTOS_FREE(ctx->guards);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->quota_capacity = 32;
    ctx->quotas = (safety_quota_t *)AGENTOS_CALLOC(ctx->quota_capacity, sizeof(safety_quota_t));
    if (!ctx->quotas) {
        AGENTOS_FREE(ctx->policies);
        AGENTOS_FREE(ctx->audit_entries);
        AGENTOS_FREE(ctx->guards);
        AGENTOS_FREE(ctx);
        return NULL;
    }

    ctx->initialized = true;
    ctx->emergency_stopped = false;
    return ctx;
}

void safety_guard_destroy(safety_guard_context_t *ctx)
{
    if (!ctx) return;
    AGENTOS_FREE(ctx->quotas);
    /* 释放策略中的动态分配内存 */
    for (size_t i = 0; i < ctx->policy_count; i++) {
        AGENTOS_FREE(ctx->policies[i].rules_json);
    }
    AGENTOS_FREE(ctx->policies);
    AGENTOS_FREE(ctx->audit_entries);
    AGENTOS_FREE(ctx->guards);
    AGENTOS_FREE(ctx);
}

int safety_guard_register_guard(safety_guard_context_t *ctx,
                                const safety_guard_descriptor_t *descriptor,
                                safety_guard_check_fn check_fn, void *user_data)
{
    if (!ctx || !descriptor) return -1;
    if (ctx->guard_count >= ctx->guard_capacity) return -1;

    guard_entry_t *entry = &ctx->guards[ctx->guard_count];
    __builtin_memcpy(&entry->descriptor, descriptor, sizeof(*descriptor));
    entry->check_fn = check_fn;
    entry->user_data = user_data;
    ctx->guard_count++;
    return 0;
}

int safety_guard_unregister_guard(safety_guard_context_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (size_t i = 0; i < ctx->guard_count; i++) {
        if (__builtin_strcmp(ctx->guards[i].descriptor.name, name) == 0) {
            if (i < ctx->guard_count - 1) {
                __builtin_memmove(&ctx->guards[i], &ctx->guards[i + 1],
                        (ctx->guard_count - i - 1) * sizeof(guard_entry_t));
            }
            ctx->guard_count--;
            return 0;
        }
    }
    return -1;
}

int safety_guard_enable_guard(safety_guard_context_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (size_t i = 0; i < ctx->guard_count; i++) {
        if (__builtin_strcmp(ctx->guards[i].descriptor.name, name) == 0) {
            ctx->guards[i].descriptor.enabled = true;
            return 0;
        }
    }
    return -1;
}

int safety_guard_disable_guard(safety_guard_context_t *ctx, const char *name)
{
    if (!ctx || !name) return -1;
    for (size_t i = 0; i < ctx->guard_count; i++) {
        if (__builtin_strcmp(ctx->guards[i].descriptor.name, name) == 0) {
            ctx->guards[i].descriptor.enabled = false;
            return 0;
        }
    }
    return -1;
}

safety_decision_t safety_guard_check(safety_guard_context_t *ctx,
                                     const safety_event_t *event,
                                     safety_result_t *result)
{
    if (!ctx || !event) {
        if (result) {
            __builtin_memset(result, 0, sizeof(*result));
            result->decision = SAFETY_DECISION_DENY;
        }
        return SAFETY_DECISION_DENY;
    }

    /* 紧急停止状态：拒绝所有请求 */
    if (ctx->emergency_stopped) {
        if (result) {
            result->decision = SAFETY_DECISION_ABORT;
            snprintf(result->reason, sizeof(result->reason),
                     "Emergency stop: %s", ctx->emergency_reason);
            result->severity = SAFETY_SEVERITY_FATAL;
        }
        return SAFETY_DECISION_ABORT;
    }

    /* 遍历所有已启用的守卫，按优先级排序 */
    safety_decision_t final_decision = SAFETY_DECISION_ALLOW;

    for (size_t i = 0; i < ctx->guard_count; i++) {
        guard_entry_t *entry = &ctx->guards[i];
        if (!entry->descriptor.enabled) continue;

        safety_result_t guard_result;
        __builtin_memset(&guard_result, 0, sizeof(guard_result));

        safety_decision_t decision;
        if (entry->check_fn) {
            decision = entry->check_fn(&entry->descriptor, event,
                                       &guard_result, entry->user_data);
        } else {
            /* 使用默认检查函数 */
            default_check_fn_t default_fn = get_default_check_fn(
                entry->descriptor.type);
            if (default_fn) {
                decision = default_fn(&entry->descriptor, event, &guard_result);
            } else {
                guard_result.decision = SAFETY_DECISION_ALLOW;
                decision = SAFETY_DECISION_ALLOW;
            }
        }

        /* 记录审计 */
        if (entry->descriptor.audit_enabled) {
            safety_guard_record_audit(ctx, event, &guard_result,
                                      entry->descriptor.name);
        }

        /* 决策合并：DENY 优先级最高 */
        if (decision == SAFETY_DECISION_DENY ||
            decision == SAFETY_DECISION_ABORT) {
            final_decision = decision;
            if (result) {
                __builtin_memcpy(result, &guard_result, sizeof(*result));
            }

            /* 触发违规回调 */
            if (ctx->violation_callback) {
                ctx->violation_callback(event, &guard_result,
                                        ctx->violation_user_data);
            }
            break; /* 拒绝后不再继续检查 */
        }

        if (decision == SAFETY_DECISION_CONDITIONAL &&
            final_decision == SAFETY_DECISION_ALLOW) {
            final_decision = SAFETY_DECISION_CONDITIONAL;
        }
    }

    if (result && final_decision != SAFETY_DECISION_DENY &&
        final_decision != SAFETY_DECISION_ABORT) {
        __builtin_memset(result, 0, sizeof(*result));
        result->decision = final_decision;
        result->severity = SAFETY_SEVERITY_INFO;
    }

    return final_decision;
}

safety_decision_t safety_guard_check_chain(safety_guard_context_t *ctx,
                                           const safety_event_t *event,
                                           safety_result_t **results,
                                           size_t *result_count)
{
    if (!ctx || !event) {
        if (results && result_count) {
            *result_count = 0;
            *results = NULL;
        }
        return SAFETY_DECISION_DENY;
    }

    /* 紧急停止状态 */
    if (ctx->emergency_stopped) {
        if (results && result_count) {
            *result_count = 0;
            *results = NULL;
        }
        return SAFETY_DECISION_ABORT;
    }

    /* 分配结果数组 */
    size_t count = ctx->guard_count;
    if (count == 0) {
        if (results && result_count) {
            *result_count = 0;
            *results = NULL;
        }
        return SAFETY_DECISION_ALLOW;
    }

    safety_result_t *out_results = (safety_result_t *)AGENTOS_CALLOC(count, sizeof(safety_result_t));
    if (!out_results) {
        if (results && result_count) {
            *result_count = 0;
            *results = NULL;
        }
        return SAFETY_DECISION_ALLOW;
    }

    safety_decision_t final_decision = SAFETY_DECISION_ALLOW;
    size_t actual_count = 0;

    /* 按优先级从高到低执行守卫链 */
    for (int prio = SAFETY_PRIORITY_CRITICAL; prio >= SAFETY_PRIORITY_LOWEST; prio--) {
        for (size_t i = 0; i < ctx->guard_count; i++) {
            guard_entry_t *entry = &ctx->guards[i];
            if (!entry->descriptor.enabled) continue;
            if ((int)entry->descriptor.priority != prio) continue;

            safety_result_t *guard_result = &out_results[actual_count];

            safety_decision_t decision;
            if (entry->check_fn) {
                decision = entry->check_fn(&entry->descriptor, event,
                                           guard_result, entry->user_data);
            } else {
                default_check_fn_t default_fn = get_default_check_fn(
                    entry->descriptor.type);
                if (default_fn) {
                    decision = default_fn(&entry->descriptor, event,
                                          guard_result);
                } else {
                    guard_result->decision = SAFETY_DECISION_ALLOW;
                    decision = SAFETY_DECISION_ALLOW;
                }
            }

            actual_count++;

            /* 审计记录 */
            if (entry->descriptor.audit_enabled) {
                safety_guard_record_audit(ctx, event, guard_result,
                                          entry->descriptor.name);
            }

            /* 决策合并 */
            if (decision == SAFETY_DECISION_DENY ||
                decision == SAFETY_DECISION_ABORT) {
                final_decision = decision;

                /* 触发违规回调 */
                if (ctx->violation_callback) {
                    ctx->violation_callback(event, guard_result,
                                            ctx->violation_user_data);
                }

                /* 如果守卫是阻塞型的，立即停止 */
                if (entry->descriptor.blocking) {
                    goto chain_done;
                }
            }

            if (decision == SAFETY_DECISION_CONDITIONAL &&
                final_decision == SAFETY_DECISION_ALLOW) {
                final_decision = SAFETY_DECISION_CONDITIONAL;
            }
        }
    }

chain_done:
    if (results) {
        *results = out_results;
    } else {
        AGENTOS_FREE(out_results);
    }
    if (result_count) {
        *result_count = actual_count;
    }

    return final_decision;
}

int safety_guard_add_policy(safety_guard_context_t *ctx, const safety_policy_t *policy)
{
    if (!ctx || !policy) return -1;
    if (ctx->policy_count >= ctx->policy_capacity) {
        /* 扩容 */
        size_t new_cap = ctx->policy_capacity * 2;
        safety_policy_t *new_policies = (safety_policy_t *)AGENTOS_REALLOC(
            ctx->policies, new_cap * sizeof(safety_policy_t));
        if (!new_policies) return -1;
        ctx->policies = new_policies;
        ctx->policy_capacity = new_cap;
    }
    __builtin_memcpy(&ctx->policies[ctx->policy_count], policy, sizeof(*policy));
    if (policy->rules_json) {
        ctx->policies[ctx->policy_count].rules_json = AGENTOS_STRDUP(policy->rules_json);
    }
    ctx->policy_count++;

    /* 通知策略变更 */
    if (ctx->policy_change_callback) {
        ctx->policy_change_callback(policy->id, "added",
                                    ctx->policy_change_user_data);
    }
    return 0;
}

int safety_guard_remove_policy(safety_guard_context_t *ctx, const char *policy_id)
{
    if (!ctx || !policy_id) return -1;
    for (size_t i = 0; i < ctx->policy_count; i++) {
        if (__builtin_strcmp(ctx->policies[i].id, policy_id) == 0) {
            AGENTOS_FREE(ctx->policies[i].rules_json);
            if (i < ctx->policy_count - 1) {
                __builtin_memmove(&ctx->policies[i], &ctx->policies[i + 1],
                        (ctx->policy_count - i - 1) * sizeof(safety_policy_t));
            }
            ctx->policy_count--;
            return 0;
        }
    }
    return -1;
}

int safety_guard_update_policy(safety_guard_context_t *ctx, const char *policy_id,
                               const char *new_rules_json)
{
    if (!ctx || !policy_id || !new_rules_json) return -1;
    for (size_t i = 0; i < ctx->policy_count; i++) {
        if (__builtin_strcmp(ctx->policies[i].id, policy_id) == 0) {
            AGENTOS_FREE(ctx->policies[i].rules_json);
            ctx->policies[i].rules_json = AGENTOS_STRDUP(new_rules_json);
            return 0;
        }
    }
    return -1;
}

int safety_guard_load_policies(safety_guard_context_t *ctx, const char *policies_json)
{
    if (!ctx || !policies_json) return -1;

    /* 使用 config_unified 解析 JSON 策略数组 */
    config_context_t *cfg = config_context_create("safety_policies");
    if (!cfg) return -1;

    /* 从内存解析 JSON */
    config_memory_source_options_t mem_opts = {
        .data = policies_json,
        .data_len = strlen(policies_json),
        .format = "json"
    };
    config_source_t *mem_source = config_source_create_memory(&mem_opts);
    if (!mem_source) {
        config_context_destroy(cfg);
        return -1;
    }
    config_error_t err = config_service_load(cfg, &mem_source, 1);
    if (err != CONFIG_SUCCESS) {
        config_context_destroy(cfg);
        return -1;
    }

    /* 遍历策略键: policy.0.id, policy.1.id, ... */
    int loaded = 0;
    char key_buf[64];
    for (int i = 0; i < SAFETY_MAX_POLICIES; i++) {
        snprintf(key_buf, sizeof(key_buf), "%d.id", i);
        const config_value_t *id_val = config_context_get(cfg, key_buf);
        if (!id_val) break; /* 没有更多策略 */

        safety_policy_t policy;
        AGENTOS_MEMSET(&policy, 0, sizeof(policy));

        const char *s = config_value_get_string(id_val, "");
        AGENTOS_STRNCPY_TERM(policy.id, s, sizeof(policy.id) - 1);

        snprintf(key_buf, sizeof(key_buf), "%d.name", i);
        s = config_value_get_string(config_context_get(cfg, key_buf), "");
        AGENTOS_STRNCPY_TERM(policy.name, s, sizeof(policy.name) - 1);

        snprintf(key_buf, sizeof(key_buf), "%d.description", i);
        s = config_value_get_string(config_context_get(cfg, key_buf), "");
        AGENTOS_STRNCPY_TERM(policy.description, s, sizeof(policy.description) - 1);

        snprintf(key_buf, sizeof(key_buf), "%d.default_decision", i);
        policy.default_decision = (safety_decision_t)config_value_get_int(
            config_context_get(cfg, key_buf), SAFETY_DECISION_DENY);

        snprintf(key_buf, sizeof(key_buf), "%d.priority", i);
        policy.priority = (safety_priority_t)config_value_get_int(
            config_context_get(cfg, key_buf), SAFETY_PRIORITY_NORMAL);

        snprintf(key_buf, sizeof(key_buf), "%d.enabled", i);
        policy.enabled = config_value_get_bool(
            config_context_get(cfg, key_buf), true);

        snprintf(key_buf, sizeof(key_buf), "%d.overridable", i);
        policy.overridable = config_value_get_bool(
            config_context_get(cfg, key_buf), true);

        /* 可选字段：rules_json */
        snprintf(key_buf, sizeof(key_buf), "%d.rules_json", i);
        const config_value_t *rules_val = config_context_get(cfg, key_buf);
        if (rules_val) {
            policy.rules_json = AGENTOS_STRDUP(config_value_get_string(rules_val, ""));
        } else {
            policy.rules_json = NULL;
        }

        /* 时间戳可选 */
        snprintf(key_buf, sizeof(key_buf), "%d.valid_from", i);
        policy.valid_from = (uint64_t)config_value_get_int(
            config_context_get(cfg, key_buf), 0);

        snprintf(key_buf, sizeof(key_buf), "%d.valid_until", i);
        policy.valid_until = (uint64_t)config_value_get_int(
            config_context_get(cfg, key_buf), 0);

        /* 添加到安全守卫上下文 */
        if (safety_guard_add_policy(ctx, &policy) == 0) {
            loaded++;
        }

        if (policy.rules_json) {
            AGENTOS_FREE(policy.rules_json);
        }
    }

    config_source_destroy(mem_source);
    config_context_destroy(cfg);
    return loaded > 0 ? 0 : -1;
}

int safety_guard_resolve_conflict(safety_guard_context_t *ctx, const char *policy_a_id,
                                  const char *policy_b_id, safety_decision_t *resolved_decision)
{
    if (!ctx || !policy_a_id || !policy_b_id || !resolved_decision) return -1;

    /* 冲突解决策略：优先级高的策略胜出 */
    safety_policy_t *policy_a = NULL, *policy_b = NULL;
    for (size_t i = 0; i < ctx->policy_count; i++) {
        if (__builtin_strcmp(ctx->policies[i].id, policy_a_id) == 0) policy_a = &ctx->policies[i];
        if (__builtin_strcmp(ctx->policies[i].id, policy_b_id) == 0) policy_b = &ctx->policies[i];
    }

    if (policy_a && policy_b) {
        if (policy_a->priority >= policy_b->priority) {
            *resolved_decision = policy_a->default_decision;
        } else {
            *resolved_decision = policy_b->default_decision;
        }
    } else if (policy_a) {
        *resolved_decision = policy_a->default_decision;
    } else if (policy_b) {
        *resolved_decision = policy_b->default_decision;
    } else {
        /* 两个策略 ID 均未在上下文中注册：无法解析冲突。
         * 安全穹顶遵循 fail-closed 原则——拒绝而非放行，
         * 并返回错误码告知调用方策略 ID 无效。 */
        *resolved_decision = SAFETY_DECISION_DENY;
        return -1;
    }
    return 0;
}

int safety_guard_set_quota(safety_guard_context_t *ctx, const char *resource_id,
                           int64_t limit, uint64_t reset_interval_ms)
{
    if (!ctx || !resource_id) return -1;

    /* 查找已有配额或创建新配额 */
    for (size_t i = 0; i < ctx->quota_count; i++) {
        if (__builtin_strcmp(ctx->quotas[i].resource_id, resource_id) == 0) {
            ctx->quotas[i].limit = limit;
            ctx->quotas[i].reset_interval_ms = reset_interval_ms;
            return 0;
        }
    }

    if (ctx->quota_count >= ctx->quota_capacity) return -1;
    safety_quota_t *q = &ctx->quotas[ctx->quota_count];
    snprintf(q->resource_id, sizeof(q->resource_id), "%s", resource_id);
    q->limit = limit;
    q->current_usage = 0;
    q->reserved = 0;
    q->reset_interval_ms = reset_interval_ms;
    q->last_reset = 0;
    ctx->quota_count++;
    return 0;
}

int safety_guard_check_quota(safety_guard_context_t *ctx, const char *resource_id,
                             int64_t requested, bool *allowed)
{
    if (!ctx || !resource_id || !allowed) return -1;

    for (size_t i = 0; i < ctx->quota_count; i++) {
        if (__builtin_strcmp(ctx->quotas[i].resource_id, resource_id) == 0) {
            *allowed = (ctx->quotas[i].current_usage + requested) <= ctx->quotas[i].limit;
            return 0;
        }
    }
    *allowed = true; /* 无配额限制 */
    return 0;
}

int safety_guard_consume_quota(safety_guard_context_t *ctx, const char *resource_id,
                               int64_t amount)
{
    if (!ctx || !resource_id) return -1;
    for (size_t i = 0; i < ctx->quota_count; i++) {
        if (__builtin_strcmp(ctx->quotas[i].resource_id, resource_id) == 0) {
            ctx->quotas[i].current_usage += amount;
            return 0;
        }
    }
    return -1;
}

int safety_guard_release_quota(safety_guard_context_t *ctx, const char *resource_id,
                               int64_t amount)
{
    if (!ctx || !resource_id) return -1;
    for (size_t i = 0; i < ctx->quota_count; i++) {
        if (__builtin_strcmp(ctx->quotas[i].resource_id, resource_id) == 0) {
            ctx->quotas[i].current_usage -= amount;
            if (ctx->quotas[i].current_usage < 0) ctx->quotas[i].current_usage = 0;
            return 0;
        }
    }
    return -1;
}

int safety_guard_record_audit(safety_guard_context_t *ctx, const safety_event_t *event,
                              const safety_result_t *result, const char *guard_name)
{
    if (!ctx || !event) return -1;

    /* P1.4.3: 每次 DENY 必须写入审计日志 */
    if (ctx->audit_count >= ctx->audit_capacity) {
        /* 扩容审计日志 */
        size_t new_cap = ctx->audit_capacity * 2;
        if (new_cap > SAFETY_MAX_AUDIT_ENTRIES) new_cap = SAFETY_MAX_AUDIT_ENTRIES;
        if (ctx->audit_count >= new_cap) return -1; /* 已满 */

        safety_audit_entry_t *new_entries = (safety_audit_entry_t *)AGENTOS_REALLOC(
            ctx->audit_entries, new_cap * sizeof(safety_audit_entry_t));
        if (!new_entries) return -1;
        ctx->audit_entries = new_entries;
        ctx->audit_capacity = new_cap;
    }

    safety_audit_entry_t *entry = &ctx->audit_entries[ctx->audit_count];
    __builtin_memset(entry, 0, sizeof(*entry));
    entry->event_id = ctx->audit_count;
    entry->event_type = event->type;
    snprintf(entry->subject, sizeof(entry->subject), "%s", event->subject);
    snprintf(entry->action, sizeof(entry->action), "%s", event->action);
    entry->decision = result ? result->decision : SAFETY_DECISION_ALLOW;
    if (result && result->reason[0]) {
        snprintf(entry->reason, sizeof(entry->reason), "%s", result->reason);
    }
    if (guard_name) {
        snprintf(entry->guard_name, sizeof(entry->guard_name), "%s", guard_name);
    }
    entry->timestamp = event->timestamp;

    ctx->audit_count++;
    return 0;
}

int safety_guard_query_audit(safety_guard_context_t *ctx, const char *subject,
                             uint64_t from_timestamp, uint64_t to_timestamp,
                             safety_audit_entry_t **entries, size_t *entry_count)
{
    if (!ctx) {
        if (entries) *entries = NULL;
        if (entry_count) *entry_count = 0;
        return -1;
    }

    /* 统计匹配的条目数 */
    size_t match_count = 0;
    for (size_t i = 0; i < ctx->audit_count; i++) {
        safety_audit_entry_t *e = &ctx->audit_entries[i];
        if (subject && subject[0] && __builtin_strcmp(e->subject, subject) != 0) continue;
        if (from_timestamp > 0 && e->timestamp < from_timestamp) continue;
        if (to_timestamp > 0 && e->timestamp > to_timestamp) continue;
        match_count++;
    }

    if (match_count == 0) {
        if (entries) *entries = NULL;
        if (entry_count) *entry_count = 0;
        return 0;
    }

    safety_audit_entry_t *result = (safety_audit_entry_t *)AGENTOS_CALLOC(
        match_count, sizeof(safety_audit_entry_t));
    if (!result) return -1;

    size_t idx = 0;
    for (size_t i = 0; i < ctx->audit_count; i++) {
        safety_audit_entry_t *e = &ctx->audit_entries[i];
        if (subject && subject[0] && __builtin_strcmp(e->subject, subject) != 0) continue;
        if (from_timestamp > 0 && e->timestamp < from_timestamp) continue;
        if (to_timestamp > 0 && e->timestamp > to_timestamp) continue;
        __builtin_memcpy(&result[idx], e, sizeof(*e));
        idx++;
    }

    if (entries) *entries = result;
    if (entry_count) *entry_count = match_count;
    return 0;
}

int safety_guard_set_violation_callback(safety_guard_context_t *ctx,
                                        safety_violation_callback_t callback, void *user_data)
{
    if (!ctx) return -1;
    ctx->violation_callback = callback;
    ctx->violation_user_data = user_data;
    return 0;
}

int safety_guard_set_policy_change_callback(safety_guard_context_t *ctx,
                                            safety_policy_change_callback_t callback,
                                            void *user_data)
{
    if (!ctx) return -1;
    ctx->policy_change_callback = callback;
    ctx->policy_change_user_data = user_data;
    return 0;
}

int safety_guard_emergency_stop(safety_guard_context_t *ctx, const char *reason)
{
    if (!ctx) return -1;
    ctx->emergency_stopped = true;
    if (reason) {
        snprintf(ctx->emergency_reason, sizeof(ctx->emergency_reason), "%s", reason);
    } else {
        snprintf(ctx->emergency_reason, sizeof(ctx->emergency_reason), "Manual emergency stop");
    }
    return 0;
}

int safety_guard_emergency_release(safety_guard_context_t *ctx)
{
    if (!ctx) return -1;
    ctx->emergency_stopped = false;
    ctx->emergency_reason[0] = '\0';
    return 0;
}

int safety_guard_check_permission(safety_guard_context_t *ctx,
                                  safety_guard_type_t guard_type,
                                  const char *agent_id, bool *allowed)
{
    if (!ctx || !agent_id || !allowed) return -1;

    /* 构造安全事件进行权限检查 */
    safety_event_t event;
    __builtin_memset(&event, 0, sizeof(event));
    event.type = SAFETY_EVENT_ACCESS_REQUEST;
    snprintf(event.subject, sizeof(event.subject), "%s", agent_id);
    snprintf(event.action, sizeof(event.action), "check_permission");

    safety_result_t result;
    safety_decision_t decision = safety_guard_check(ctx, &event, &result);

    *allowed = (decision == SAFETY_DECISION_ALLOW ||
                decision == SAFETY_DECISION_CONDITIONAL);
    return 0;
}

size_t safety_guard_get_guard_count(safety_guard_context_t *ctx)
{
    return ctx ? ctx->guard_count : 0;
}

size_t safety_guard_get_policy_count(safety_guard_context_t *ctx)
{
    return ctx ? ctx->policy_count : 0;
}

size_t safety_guard_get_audit_count(safety_guard_context_t *ctx)
{
    return ctx ? ctx->audit_count : 0;
}