/**
 * @file sandbox.c
 * @brief 系统调用安全沙箱 - 增强版（支持动态策略和审计）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 安全沙箱提供系统调用的隔离执行环境，防止恶意或错误代码影响系统稳定性。
 * 基于 sandbox_utils、sandbox_permission、sandbox_quota 模块构建。
 *
 * 增强功能：
 * - 动态安全策略更新
 * - 完善的审计日志追踪
 * - 输入净化和边界检查增强
 * - 性能监控和资源使用统计
 */

#include "logger.h"
#include "../include/syscalls.h"
#include "agentrt.h"
/* P3.18 (ACC-DT27): sandbox 公共 API 声明（本文件实现其全部函数） */
#include "agentrt_sandbox.h"
#include "sandbox_permission.h"
#include "sandbox_quota.h"
#include "sandbox_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 基础库兼容性层 */
#include "check.h"
#include "memory_compat.h"
#include "string_compat.h"

/* 前向声明：syscall分发入口（定义在 syscall_table.c） */
void *agentrt_syscall_invoke(int syscall_num, void **args, int argc);

/* ==================== 常量定义 ==================== */

#define MAX_SANDBOXES 64
#define DEFAULT_SANDBOX_TIMEOUT_MS 30000
#define DEFAULT_MAX_MEMORY_BYTES (512 * 1024 * 1024)
#define DEFAULT_MAX_CPU_TIME_MS 60000
#define DEFAULT_MAX_IO_OPS 10000

/* 缓冲区大小常量 */
#define STATS_BUFFER_SIZE 1024
#define MANAGER_STATS_BUFFER_SIZE 512
#define HEALTH_CHECK_BUFFER_SIZE 512

/* 审计日志相关常量 */
#define MAX_AUDIT_ENTRIES 1000        /* 最大审计条目数 */
#define AUDIT_LOG_BUFFER_SIZE 256     /* 单条审计记录缓冲区大小 */
#define MAX_INPUT_LENGTH 4096         /* 最大输入长度限制 */
#define SANITIZATION_PATTERN_SIZE 128 /* 净化模式匹配缓冲区大小 */

#include "sandbox_internal.h"
#include "error.h"

/* 保留本地常量 */

/**
 * @brief 沙箱管理器结构
 */
typedef struct sandbox_manager {
    agentrt_sandbox_t *sandboxes[MAX_SANDBOXES];
    uint32_t sandbox_count;
    agentrt_mutex_t *lock;
    uint64_t total_violations;
    uint64_t total_calls;
} sandbox_manager_t;

/* ==================== 全局变量 ==================== */

static sandbox_manager_t *g_sandbox_manager = NULL;
static agentrt_mutex_t *g_manager_lock = NULL;

/* 前向声明 */
void agentrt_sandbox_destroy(agentrt_sandbox_t *sandbox);

/* ==================== 沙箱管理器 ==================== */

agentrt_error_t agentrt_sandbox_manager_init(void)
{
    if (g_sandbox_manager) {
        AGENTRT_LOG_WARN("Sandbox manager already initialized");
        return AGENTRT_SUCCESS;
    }

    g_manager_lock = agentrt_mutex_create();
    if (!g_manager_lock) {
        AGENTRT_LOG_ERROR("Failed to create manager lock");
        return AGENTRT_ENOMEM;
    }

    g_sandbox_manager = (sandbox_manager_t *)AGENTRT_CALLOC(1, sizeof(sandbox_manager_t));
    if (!g_sandbox_manager) {
        AGENTRT_LOG_ERROR("Failed to allocate sandbox manager");
        agentrt_mutex_free(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTRT_ENOMEM;
    }

    g_sandbox_manager->lock = agentrt_mutex_create();
    if (!g_sandbox_manager->lock) {
        AGENTRT_LOG_ERROR("Failed to create sandbox manager lock");
        AGENTRT_FREE(g_sandbox_manager);
        g_sandbox_manager = NULL;
        agentrt_mutex_free(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTRT_ENOMEM;
    }

    g_sandbox_manager->sandbox_count = 0;
    g_sandbox_manager->total_violations = 0;
    g_sandbox_manager->total_calls = 0;

    AGENTRT_LOG_INFO("Sandbox manager initialized");
    return AGENTRT_SUCCESS;
}

void agentrt_sandbox_manager_destroy(void)
{
    if (!g_sandbox_manager)
        return;

    agentrt_mutex_lock(g_manager_lock);

    for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
        if (g_sandbox_manager->sandboxes[i]) {
            agentrt_sandbox_destroy(g_sandbox_manager->sandboxes[i]);
            g_sandbox_manager->sandboxes[i] = NULL;
        }
    }

    agentrt_mutex_free(g_sandbox_manager->lock);
    AGENTRT_FREE(g_sandbox_manager);
    g_sandbox_manager = NULL;

    agentrt_mutex_unlock(g_manager_lock);
    agentrt_mutex_free(g_manager_lock);
    g_manager_lock = NULL;

    AGENTRT_LOG_INFO("Sandbox manager destroyed");
}

/* ==================== 沙箱生命周期 ==================== */

agentrt_error_t agentrt_sandbox_create(const sandbox_config_t *manager,
                                       agentrt_sandbox_t **out_sandbox)
{
    if (!manager || !out_sandbox)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to create sandbox: null manager or out_sandbox");

    if (!g_sandbox_manager) {
        AGENTRT_LOG_ERROR("Sandbox manager not initialized");
        return AGENTRT_ENOTINIT;
    }

    agentrt_mutex_lock(g_manager_lock);

    int slot = -1;
    for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
        if (!g_sandbox_manager->sandboxes[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentrt_mutex_unlock(g_manager_lock);
        AGENTRT_LOG_ERROR("No available sandbox slot");
        return AGENTRT_EBUSY;
    }

    agentrt_sandbox_t *sandbox = (agentrt_sandbox_t *)AGENTRT_CALLOC(1, sizeof(agentrt_sandbox_t));
    if (!sandbox) {
        agentrt_mutex_unlock(g_manager_lock);
        AGENTRT_LOG_ERROR("Failed to allocate sandbox");
        return AGENTRT_ENOMEM;
    }

    sandbox->sandbox_id = (uint64_t)slot + 1;
    sandbox->sandbox_name = manager->sandbox_name ? AGENTRT_STRDUP(manager->sandbox_name) : NULL;
    sandbox->owner_id = manager->owner_id ? AGENTRT_STRDUP(manager->owner_id) : NULL;
    sandbox->state = SANDBOX_STATE_IDLE;
    sandbox->create_time_ns = 0;
    sandbox->last_active_ns = sandbox->create_time_ns;
    sandbox->call_count = 0;
    sandbox->violation_count = 0;
    sandbox->rules = NULL;
    sandbox->rule_count = 0;
    sandbox->audit_log = NULL;
    sandbox->audit_count = 0;
    sandbox->audit_capacity = 0;

    __builtin_memcpy(&sandbox->manager, manager, sizeof(sandbox_config_t));

    sandbox_quota_init(&sandbox->quota);

    /* 初始化增强功能（内联实现） */
    {
        sandbox->audit_capacity = MAX_AUDIT_ENTRIES;
        sandbox->audit_log =
            (audit_entry_t *)AGENTRT_CALLOC(sandbox->audit_capacity, sizeof(audit_entry_t));
        if (!sandbox->audit_log) {
            sandbox->audit_capacity = 0;
        }
        sandbox->audit_count = 0;
        sandbox->audit_write_index = 0;

        __builtin_memset(&sandbox->policy, 0, sizeof(security_policy_t));
        sandbox->policy.version = 1;
        sandbox->policy.last_updated = (time_t)(agentrt_time_ms() / 1000ULL);
AGENTRT_STRNCPY_TERM(sandbox->policy.updated_by, "system", sizeof(sandbox->policy.updated_by));
        sandbox->policy.allow_dynamic_update = 1;
        sandbox->policy.strict_mode = 0;
        sandbox->policy.audit_level = 1;
        sandbox->policy.risk_threshold = 0.7f;

        __builtin_memset(&sandbox->perf_stats, 0, sizeof(sandbox_perf_stats_t));
        sandbox->perf_stats.stats_reset_time = (time_t)(agentrt_time_ms() / 1000ULL);

        sandbox->enable_input_sanitization = 1;
        sandbox->enable_resource_monitoring = 1;
    }

    sandbox->lock = agentrt_mutex_create();
    if (!sandbox->lock) {
        if (sandbox->sandbox_name)
            AGENTRT_FREE(sandbox->sandbox_name);
        if (sandbox->owner_id)
            AGENTRT_FREE(sandbox->owner_id);
        if (sandbox->audit_log)
            AGENTRT_FREE(sandbox->audit_log);
        AGENTRT_FREE(sandbox);
        agentrt_mutex_unlock(g_manager_lock);
        return AGENTRT_ENOMEM;
    }

    g_sandbox_manager->sandboxes[slot] = sandbox;
    g_sandbox_manager->sandbox_count++;

    agentrt_mutex_unlock(g_manager_lock);

    *out_sandbox = sandbox;
    AGENTRT_LOG_INFO("Sandbox created: %s (ID: %llu)",
                     sandbox->sandbox_name ? sandbox->sandbox_name : "unnamed",
                     (unsigned long long)sandbox->sandbox_id);

    return AGENTRT_SUCCESS;
}

/* P3.18 (ACC-DT27): 使用默认配置创建沙箱 — agentrt_sandbox_create 的便捷封装。
 *
 * 设计说明：
 * - agentrt_sandbox_create 需要调用者构造 sandbox_config_t（含 resource_quota_t），
 *   这些内部类型不暴露在公共 agentrt_sandbox.h 中。本函数在内部构造默认 config
 *   后调用真实 create，使 tool_d 等外部调用方无需了解内部配置结构。
 * - 默认值：priority=0, timeout_ms=30000（与 tool_d 默认一致）, flags=0,
 *   quota 由 sandbox_quota_init 初始化为默认上限。
 * - 非桩函数：功能完整，等价于调用方自行构造 config + agentrt_sandbox_create。 */
agentrt_error_t agentrt_sandbox_create_default(const char *name, const char *owner_id,
                                               agentrt_sandbox_t **out_sandbox)
{
    if (!out_sandbox)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to create sandbox: null out_sandbox");

    sandbox_config_t config;
    __builtin_memset(&config, 0, sizeof(config));
    config.sandbox_name = (char *)name;     /* agentrt_sandbox_create 内部会 strdup */
    config.owner_id = (char *)owner_id;     /* agentrt_sandbox_create 内部会 strdup */
    config.priority = 0;
    config.timeout_ms = DEFAULT_SANDBOX_TIMEOUT_MS;  /* 30000ms，与 tool_d 默认一致 */
    config.flags = 0;
    /* quota 由 agentrt_sandbox_create 内部 sandbox_quota_init 初始化 */

    return agentrt_sandbox_create(&config, out_sandbox);
}

void agentrt_sandbox_destroy(agentrt_sandbox_t *sandbox)
{
    if (!sandbox)
        return;

    if (g_sandbox_manager) {
        agentrt_mutex_lock(g_manager_lock);
        for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
            if (g_sandbox_manager->sandboxes[i] == sandbox) {
                g_sandbox_manager->sandboxes[i] = NULL;
                g_sandbox_manager->sandbox_count--;
                break;
            }
        }
        agentrt_mutex_unlock(g_manager_lock);
    }

    if (sandbox->sandbox_name)
        AGENTRT_FREE(sandbox->sandbox_name);
    if (sandbox->owner_id)
        AGENTRT_FREE(sandbox->owner_id);

    sandbox_permission_destroy_all(sandbox->rules);

    if (sandbox->audit_log) {
        AGENTRT_FREE(sandbox->audit_log);
    }

    if (sandbox->lock) {
        agentrt_mutex_free(sandbox->lock);
    }

    uint64_t sandbox_id __attribute__((unused)) = sandbox->sandbox_id;
    AGENTRT_FREE(sandbox);
    AGENTRT_LOG_DEBUG("Sandbox %llu destroyed", (unsigned long long)sandbox_id);
}

/* ==================== 沙箱操作 ==================== */

agentrt_error_t agentrt_sandbox_invoke(agentrt_sandbox_t *sandbox, int syscall_num, void **args,
                                       int argc, void **out_result)
{
    if (!sandbox || !out_result)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to invoke syscall: null sandbox or out_result");
    uint64_t start_time = agentrt_time_ns();

    agentrt_mutex_lock(sandbox->lock);
    sandbox->call_count++;
    agentrt_mutex_unlock(sandbox->lock);

    if (g_sandbox_manager) {
        agentrt_mutex_lock(g_manager_lock);
        g_sandbox_manager->total_calls++;
        agentrt_mutex_unlock(g_manager_lock);
    }

    agentrt_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_TERMINATED) {
        agentrt_mutex_unlock(sandbox->lock);
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTRT_EPERM, 0, "Sandbox terminated");
        return AGENTRT_EPERM;
    }

    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        agentrt_mutex_unlock(sandbox->lock);
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTRT_EBUSY, 0, "Sandbox suspended");
        return AGENTRT_EBUSY;
    }

    sandbox->state = SANDBOX_STATE_ACTIVE;
    agentrt_mutex_unlock(sandbox->lock);

    permission_type_t perm = sandbox_permission_check(sandbox, syscall_num, args, argc);
    if (perm == PERM_DENY) {
        agentrt_mutex_lock(sandbox->lock);
        sandbox->violation_count++;
        agentrt_mutex_unlock(sandbox->lock);
        if (g_sandbox_manager) {
            agentrt_mutex_lock(g_manager_lock);
            g_sandbox_manager->total_violations++;
            agentrt_mutex_unlock(g_manager_lock);
        }
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTRT_EACCES, 0, "Permission denied");

        agentrt_mutex_lock(sandbox->lock);
        sandbox->state = SANDBOX_STATE_IDLE;
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_EACCES;
    }

    if (!sandbox_quota_check(sandbox, RESOURCE_CPU, 1)) {
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTRT_EQUOTA, 0,
                                "CPU quota exceeded");

        agentrt_mutex_lock(sandbox->lock);
        sandbox->state = SANDBOX_STATE_IDLE;
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_EQUOTA;
    }

    /* 实际执行系统调用（通过syscall入口分发） */
    void *invoke_result = agentrt_syscall_invoke(syscall_num, args, argc);
    *out_result = invoke_result;

    sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTRT_SUCCESS, 0, "Executed");

    /* 更新性能统计（使用start_time计算耗时） */
    uint64_t end_time = agentrt_time_ns();
    uint64_t elapsed_ns = end_time - start_time;

    agentrt_mutex_lock(sandbox->lock);
    sandbox->perf_stats.total_syscalls++;
    sandbox->perf_stats.total_cpu_time_ns += elapsed_ns;
    if (elapsed_ns > 0) {
        double avg =
            (double)sandbox->perf_stats.total_cpu_time_ns / sandbox->perf_stats.total_syscalls;
        sandbox->perf_stats.avg_response_time_ns = avg;
    }
    sandbox->state = SANDBOX_STATE_IDLE;
    agentrt_mutex_unlock(sandbox->lock);

    sandbox_quota_release(sandbox, RESOURCE_CPU, 1);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_sandbox_add_rule(agentrt_sandbox_t *sandbox, int syscall_num,
                                         permission_type_t perm_type, const char *condition)
{
    return sandbox_permission_add(sandbox, syscall_num, perm_type, condition);
}

agentrt_error_t agentrt_sandbox_get_stats(agentrt_sandbox_t *sandbox, char **out_stats)
{
    if (!sandbox || !out_stats)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to get sandbox stats: null sandbox or out_stats");

    char *stats = (char *)AGENTRT_MALLOC(STATS_BUFFER_SIZE);
    if (!stats)
        return AGENTRT_ENOMEM;

    agentrt_mutex_lock(sandbox->lock);
    snprintf(stats, STATS_BUFFER_SIZE,
             "{\"sandbox_id\":%llu,\"calls\":%llu,\"violations\":%llu,"
             "\"memory_usage\":%.2f,\"cpu_usage\":%.2f,\"io_usage\":%.2f}",
             (unsigned long long)sandbox->sandbox_id, (unsigned long long)sandbox->call_count,
             (unsigned long long)sandbox->violation_count,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_MEMORY) * 100.0,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_CPU) * 100.0,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_IO) * 100.0);
    agentrt_mutex_unlock(sandbox->lock);

    *out_stats = stats;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_sandbox_manager_get_stats(char **out_stats)
{
    CHECK_NULL(out_stats);

    if (!g_sandbox_manager) {
        *out_stats = AGENTRT_STRDUP("{\"error\":\"Manager not initialized\"}");
        return AGENTRT_ENOTINIT;
    }

    char *stats = (char *)AGENTRT_MALLOC(MANAGER_STATS_BUFFER_SIZE);
    if (!stats)
        return AGENTRT_ENOMEM;

    agentrt_mutex_lock(g_manager_lock);
    snprintf(stats, MANAGER_STATS_BUFFER_SIZE,
             "{\"sandboxes\":%u,\"total_calls\":%llu,\"total_violations\":%llu}",
             g_sandbox_manager->sandbox_count, (unsigned long long)g_sandbox_manager->total_calls,
             (unsigned long long)g_sandbox_manager->total_violations);
    agentrt_mutex_unlock(g_manager_lock);

    *out_stats = stats;
    return AGENTRT_SUCCESS;
}

void agentrt_sandbox_reset_quota(agentrt_sandbox_t *sandbox)
{
    if (sandbox) {
        agentrt_mutex_lock(sandbox->lock);
        sandbox_quota_reset(sandbox);
        agentrt_mutex_unlock(sandbox->lock);
    }
}

agentrt_error_t agentrt_sandbox_suspend(agentrt_sandbox_t *sandbox)
{
    CHECK_NULL(sandbox);

    agentrt_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_ACTIVE) {
        sandbox->state = SANDBOX_STATE_SUSPENDED;
        AGENTRT_LOG_INFO("Sandbox %llu suspended", (unsigned long long)sandbox->sandbox_id);
    }
    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_sandbox_resume(agentrt_sandbox_t *sandbox)
{
    CHECK_NULL(sandbox);

    agentrt_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        sandbox->state = SANDBOX_STATE_IDLE;
        AGENTRT_LOG_INFO("Sandbox %llu resumed", (unsigned long long)sandbox->sandbox_id);
    }
    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_sandbox_terminate(agentrt_sandbox_t *sandbox)
{
    CHECK_NULL(sandbox);

    agentrt_mutex_lock(sandbox->lock);
    sandbox->state = SANDBOX_STATE_TERMINATED;
    AGENTRT_LOG_INFO("Sandbox %llu terminated", (unsigned long long)sandbox->sandbox_id);
    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_sandbox_health_check(agentrt_sandbox_t *sandbox, char **out_json)
{
    if (!sandbox || !out_json)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to check sandbox health: null sandbox or out_json");

    agentrt_mutex_lock(sandbox->lock);
    sandbox_state_t state = sandbox->state;
    uint64_t sandbox_id __attribute__((unused)) = sandbox->sandbox_id;
    agentrt_mutex_unlock(sandbox->lock);

    const char *state_str = "unknown";
    switch (state) {
    case SANDBOX_STATE_IDLE:
        state_str = "idle";
        break;
    case SANDBOX_STATE_ACTIVE:
        state_str = "active";
        break;
    case SANDBOX_STATE_SUSPENDED:
        state_str = "suspended";
        break;
    case SANDBOX_STATE_TERMINATED:
        state_str = "terminated";
        break;
    }

    char *json = (char *)AGENTRT_MALLOC(HEALTH_CHECK_BUFFER_SIZE);
    if (!json)
        return AGENTRT_ENOMEM;

    snprintf(json, HEALTH_CHECK_BUFFER_SIZE, "{\"id\":%llu,\"state\":\"%s\",\"healthy\":%s}",
             (unsigned long long)sandbox_id, state_str,
             (state != SANDBOX_STATE_TERMINATED) ? "true" : "false");

    *out_json = json;
    return AGENTRT_SUCCESS;
}

/* ==================== 安全增强功能 ==================== */

static int sanitize_input(const char *input, size_t max_length)
{
    if (!input)
        return AGENTRT_EINVAL;

    size_t len = 0;
    while (len < max_length && input[len] != '\0')
        len++;
    if (len >= max_length)
        return 1;

    const char *dangerous_patterns[] = {"..", "<script", "javascript:", "data:", NULL};

    for (int i = 0; dangerous_patterns[i] != NULL; i++) {
        if (strstr(input, dangerous_patterns[i]) != NULL) {
            return 2 + i;
        }
    }

    return 0;
}

int agentrt_sandbox_capability_check(agentrt_sandbox_t *sandbox, int capability_id,
                                     const char *resource)
{
    if (!sandbox)
        return 0;

    agentrt_mutex_lock(sandbox->lock);

    int has_capability = 0;
    permission_rule_t *rule = sandbox->rules;
    while (rule) {
        if (rule->syscall_num == capability_id) {
            if (rule->perm_type == PERM_ALLOW) {
                if (!rule->condition || !resource || strstr(resource, rule->condition) != NULL) {
                    has_capability = 1;
                }
            }
            break;
        }
        rule = rule->next;
    }

    if (has_capability && sandbox->policy.strict_mode) {
        has_capability = 0;
        rule = sandbox->rules;
        while (rule) {
            if (rule->syscall_num == capability_id && rule->perm_type == PERM_ALLOW) {
                if (rule->condition && resource && strcmp(resource, rule->condition) == 0) {
                    has_capability = 1;
                    break;
                }
            }
            rule = rule->next;
        }
    }

    agentrt_mutex_unlock(sandbox->lock);
    return has_capability;
}

agentrt_error_t agentrt_sandbox_validate_syscall(int syscall_num, void **args, int argc)
{
    if (syscall_num < 0)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to validate syscall: invalid syscall number");

    for (int i = 0; i < argc; i++) {
        if (!args[i])
            continue;

        char *str_arg = (char *)args[i];
        size_t arg_len = strnlen(str_arg, MAX_INPUT_LENGTH);
        int safe = 1;
        if (arg_len >= MAX_INPUT_LENGTH) {
            safe = 0;
        } else {
            int result = sanitize_input(str_arg, MAX_INPUT_LENGTH);
            if (result != 0)
                safe = 0;
        }

        if (!safe) {
            return AGENTRT_EINVAL;
        }
    }

    return AGENTRT_SUCCESS;
}

static size_t json_escape_into(char *dst, size_t dst_size, const char *src)
{
    size_t written = 0;
    for (const char *p = src; *p && written + 6 < dst_size; p++) {
        switch (*p) {
        case '"':
            written += snprintf(dst + written, dst_size - written, "\\\"");
            break;
        case '\\':
            written += snprintf(dst + written, dst_size - written, "\\\\");
            break;
        case '\n':
            written += snprintf(dst + written, dst_size - written, "\\n");
            break;
        case '\r':
            written += snprintf(dst + written, dst_size - written, "\\r");
            break;
        case '\t':
            written += snprintf(dst + written, dst_size - written, "\\t");
            break;
        default:
            dst[written++] = *p;
            break;
        }
    }
    dst[written] = '\0';
    return written;
}

/**
 * @brief 记录增强型审计日志条目
 * @param sandbox 沙箱实例
 * @param syscall_num 系统调用号
 * @param syscall_name 系统调用名称
 * @param result 执行结果
 * @param severity 严重级别
 * @param message 详细信息
 */
static void sandbox_add_enhanced_audit_entry(agentrt_sandbox_t *sandbox, int syscall_num,
                                             const char *syscall_name, agentrt_error_t result,
                                             int severity, const char *message)
{

    if (!sandbox || !sandbox->audit_log)
        return;

    /* 如果审计日志未初始化，跳过 */
    if (sandbox->audit_capacity == 0)
        return;

    agentrt_mutex_lock(sandbox->lock);

    audit_entry_t *entry = &sandbox->audit_log[sandbox->audit_write_index];

    /* 填充审计记录 */
    entry->timestamp = (uint64_t)(time_t)(agentrt_time_ms() / 1000ULL);
    entry->event_type = (result == AGENTRT_SUCCESS) ? 1 : 2; /* 1=成功, 2=失败/违规 */
    entry->syscall_num = syscall_num;
    entry->result = (int)result;
    entry->severity = severity;

    /* 复制系统调用名称（带截断保护） */
    if (syscall_name) {
AGENTRT_STRNCPY_TERM(entry->syscall_name, syscall_name, sizeof(entry->syscall_name));
        entry->syscall_name[sizeof(entry->syscall_name) - 1] = '\0';
    } else {
        snprintf(entry->syscall_name, sizeof(entry->syscall_name), "%d", syscall_num);
    }

    /* 复制消息（带截断保护） */
    if (message) {
AGENTRT_STRNCPY_TERM(entry->message, message, sizeof(entry->message));
        entry->message[sizeof(entry->message) - 1] = '\0';
    } else {
        entry->message[0] = '\0';
    }

    /* 更新索引（环形缓冲） */
    sandbox->audit_write_index = (sandbox->audit_write_index + 1) % sandbox->audit_capacity;
    if (sandbox->audit_count < sandbox->audit_capacity) {
        sandbox->audit_count++;
    }

    agentrt_mutex_unlock(sandbox->lock);
}

/**
 * @brief 动态更新安全策略
 * @param sandbox 沙箱实例
 * @param new_policy 新的策略配置
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_update_security_policy(agentrt_sandbox_t *sandbox,
                                                       const security_policy_t *new_policy)
{

    if (!sandbox || !new_policy)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to update security policy: null sandbox or new_policy");

    agentrt_mutex_lock(sandbox->lock);

    /* 检查是否允许动态更新 */
    if (!sandbox->policy.allow_dynamic_update) {
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_EPERM;
    }

    /* 验证策略参数有效性 */
    if (new_policy->risk_threshold < 0.0f || new_policy->risk_threshold > 1.0f) {
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_EINVAL;
    }

    if (new_policy->audit_level < 0 || new_policy->audit_level > 3) {
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_EINVAL;
    }

    /* 应用新策略 */
    __builtin_memcpy(&sandbox->policy, new_policy, sizeof(security_policy_t));
    sandbox->policy.last_updated = (time_t)(agentrt_time_ms() / 1000ULL);

    /* 记录策略更新事件 */
    char log_msg[AUDIT_LOG_BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg),
             "Security policy updated to v%u by %s (strict=%d, audit_level=%d)",
             new_policy->version, new_policy->updated_by, new_policy->strict_mode,
             new_policy->audit_level);

    sandbox_add_enhanced_audit_entry(sandbox, 0, "POLICY_UPDATE", AGENTRT_SUCCESS, 1, log_msg);

    agentrt_mutex_unlock(sandbox->lock);

    AGENTRT_LOG_INFO("Sandbox %llu: Security policy updated",
                     (unsigned long long)sandbox->sandbox_id);

    return AGENTRT_SUCCESS;
}

/**
 * @brief 获取当前安全策略
 * @param sandbox 沙箱实例
 * @param[out] policy 输出的策略配置
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_get_security_policy(agentrt_sandbox_t *sandbox,
                                                    security_policy_t **policy)
{

    if (!sandbox || !policy)
        return AGENTRT_EINVAL;

    *policy = &sandbox->policy;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 获取性能统计信息
 * @param sandbox 沙箱实例
 * @param[out] stats 输出的统计信息
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_get_performance_stats(agentrt_sandbox_t *sandbox,
                                                      sandbox_perf_stats_t **stats)
{

    if (!sandbox || !stats)
        return AGENTRT_EINVAL;

    *stats = &sandbox->perf_stats;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 重置性能统计信息
 * @param sandbox 沙箱实例
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_reset_performance_stats(agentrt_sandbox_t *sandbox)
{
    if (!sandbox)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(sandbox->lock);

    __builtin_memset(&sandbox->perf_stats, 0, sizeof(sandbox_perf_stats_t));
    sandbox->perf_stats.stats_reset_time = (time_t)(agentrt_time_ms() / 1000ULL);

    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

/**
 * @brief 启用或禁用输入净化
 * @param sandbox 沙箱实例
 * @param enable 是否启用
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_set_input_sanitization(agentrt_sandbox_t *sandbox, int enable)
{

    if (!sandbox)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(sandbox->lock);
    sandbox->enable_input_sanitization = enable ? 1 : 0;
    agentrt_mutex_unlock(sandbox->lock);

    return AGENTRT_SUCCESS;
}

/**
 * @brief 导出审计日志为JSON格式
 * @param sandbox 沙箱实例
 * @param[out] out_json JSON格式的审计日志
 * @return 错误码
 */
agentrt_error_t agentrt_sandbox_export_audit_log_json(agentrt_sandbox_t *sandbox, char **out_json)
{

    if (!sandbox || !out_json)
        AGENTRT_ERROR(AGENTRT_EINVAL, "failed to export audit log: null sandbox or out_json");

    agentrt_mutex_lock(sandbox->lock);

    if (!sandbox->audit_log || sandbox->audit_count == 0) {
        agentrt_mutex_unlock(sandbox->lock);
        *out_json = AGENTRT_STRDUP("[]");
        return AGENTRT_SUCCESS;
    }

    size_t audit_count = sandbox->audit_count;
    size_t audit_capacity = sandbox->audit_capacity;
    size_t buffer_size = audit_count * 400 + 100;
    char *json = (char *)AGENTRT_MALLOC(buffer_size);
    if (!json) {
        agentrt_mutex_unlock(sandbox->lock);
        return AGENTRT_ENOMEM;
    }

    size_t start_idx =
        (audit_capacity > 0)
            ? (sandbox->audit_write_index - audit_count + audit_capacity) % audit_capacity
            : 0;

    char esc_buf[512];
    size_t pos = 0;
    pos += snprintf(json + pos, buffer_size - pos, "[\n");

    for (size_t i = 0; i < audit_count && pos < buffer_size - 500; i++) {
        size_t idx = (start_idx + i) % audit_capacity;
        audit_entry_t *entry = &sandbox->audit_log[idx];

        json_escape_into(esc_buf, sizeof(esc_buf), entry->syscall_name);
        char esc_msg[512];
        json_escape_into(esc_msg, sizeof(esc_msg), entry->message);

        pos += snprintf(json + pos, buffer_size - pos,
                        "  {\n"
                        "    \"timestamp\":%llu,\n"
                        "    \"event_type\":%d,\n"
                        "    \"syscall\":\"%s\",\n"
                        "    \"syscall_num\":%d,\n"
                        "    \"result\":%d,\n"
                        "    \"severity\":%d,\n"
                        "    \"message\":\"%s\"\n"
                        "  }%s\n",
                        (unsigned long long)entry->timestamp, entry->event_type, esc_buf,
                        entry->syscall_num, entry->result, entry->severity, esc_msg,
                        (i < audit_count - 1) ? "," : "");
    }

    pos += snprintf(json + pos, buffer_size - pos, "]");

    agentrt_mutex_unlock(sandbox->lock);

    *out_json = json;
    return AGENTRT_SUCCESS;
}
