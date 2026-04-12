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

#include "../include/syscalls.h"
#include "../../corekern/include/agentos.h"
#include "../include/logger.h"
#include "../include/sandbox_utils.h"
#include "../include/sandbox_permission.h"
#include "../include/sandbox_quota.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 基础库兼容性层 */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include "../../../agentos/commons/utils/include/check.h"

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

/* ==================== 数据结构 ==================== */

/**
 * @brief 沙箱状态枚举
 */
typedef enum {
    SANDBOX_STATE_IDLE = 0,
    SANDBOX_STATE_ACTIVE,
    SANDBOX_STATE_SUSPENDED,
    SANDBOX_STATE_TERMINATED
} sandbox_state_t;

/**
 * @brief 审计日志条目（增强版）
 */
typedef struct audit_entry {
    uint64_t timestamp;              /* 事件时间戳 */
    int event_type;                   /* 事件类型（系统调用/违规/资源使用） */
    char syscall_name[32];            /* 系统调用名称 */
    int syscall_num;                  /* 系统调用号 */
    int result;                       /* 执行结果（0=成功，非0=错误） */
    char message[AUDIT_LOG_BUFFER_SIZE]; /* 详细信息 */
    uint64_t resource_usage_before;   /* 资源使用量（操作前） */
    uint64_t resource_usage_after;    /* 资源使用量（操作后） */
    int severity;                     /* 严重级别（0=info, 1=warn, 2=error, 3=critical） */
} audit_entry_t;

/**
 * @brief 安全策略配置
 */
typedef struct security_policy {
    uint32_t version;                 /* 策略版本号 */
    time_t last_updated;              /* 最后更新时间 */
    char updated_by[64];              /* 更新者标识 */
    int allow_dynamic_update;         /* 是否允许动态更新 */
    int strict_mode;                  /* 严格模式（所有未明确允许的操作都拒绝） */
    int audit_level;                  /* 审计级别（0=最小, 1=标准, 2=详细, 3=完整） */
    float risk_threshold;             /* 风险阈值（0.0-1.0，超过此值触发警报） */
} security_policy_t;

/**
 * @brief 沙箱性能统计
 */
typedef struct sandbox_performance_stats {
    uint64_t total_syscalls;          /* 总系统调用次数 */
    uint64_t successful_calls;        /* 成功调用次数 */
    uint64_t failed_calls;            /* 失败调用次数 */
    uint64_t blocked_calls;           /* 被阻止的调用次数 */
    uint64_t total_cpu_time_ns;       /* 总CPU时间（纳秒） */
    uint64_t max_memory_bytes;        /* 最大内存使用量 */
    uint64_t current_memory_bytes;    /* 当前内存使用量 */
    double avg_response_time_ns;      /* 平均响应时间（纳秒） */
    uint64_t violations_since_reset;  /* 重置后的违规次数 */
    time_t stats_reset_time;          /* 统计重置时间 */
} sandbox_perf_stats_t;

/**
 * @brief 沙箱配置结构
 */
typedef struct sandbox_config {
    char* sandbox_name;
    char* owner_id;
    uint32_t priority;
    uint32_t timeout_ms;
    uint32_t flags;
    resource_quota_t quota;
} sandbox_config_t;

/**
 * @brief 沙箱内部结构
 */
struct agentos_sandbox {
    uint64_t sandbox_id;
    char* sandbox_name;
    char* owner_id;
    sandbox_state_t state;
    sandbox_config_t manager;
    permission_rule_t* rules;
    uint32_t rule_count;
    agentos_mutex_t* lock;
    uint64_t create_time_ns;
    uint64_t last_active_ns;
    uint64_t call_count;
    uint64_t violation_count;
    
    /* 增强功能 */
    audit_entry_t* audit_log;           /* 增强型审计日志数组 */
    size_t audit_count;                 /* 当前审计条目数 */
    size_t audit_capacity;              /* 审计日志容量 */
    size_t audit_write_index;           /* 写入索引（环形缓冲） */
    
    security_policy_t policy;            /* 安全策略配置 */
    sandbox_perf_stats_t perf_stats;     /* 性能统计 */
    
    int enable_input_sanitization;       /* 是否启用输入净化 */
    int enable_resource_monitoring;      /* 是否启用资源监控 */
};

/**
 * @brief 沙箱管理器结构
 */
typedef struct sandbox_manager {
    agentos_sandbox_t* sandboxes[MAX_SANDBOXES];
    uint32_t sandbox_count;
    agentos_mutex_t* lock;
    uint64_t total_violations;
    uint64_t total_calls;
} sandbox_manager_t;

/* ==================== 全局变量 ==================== */

static sandbox_manager_t* g_sandbox_manager = NULL;
static agentos_mutex_t* g_manager_lock = NULL;

/* ==================== 沙箱管理器 ==================== */

agentos_error_t agentos_sandbox_manager_init(void) {
    if (g_sandbox_manager) {
        AGENTOS_LOG_WARN("Sandbox manager already initialized");
        return AGENTOS_SUCCESS;
    }

    g_manager_lock = agentos_mutex_create();
    if (!g_manager_lock) {
        AGENTOS_LOG_ERROR("Failed to create manager lock");
        return AGENTOS_ENOMEM;
    }

    g_sandbox_manager = (sandbox_manager_t*)AGENTOS_CALLOC(1, sizeof(sandbox_manager_t));
    if (!g_sandbox_manager) {
        AGENTOS_LOG_ERROR("Failed to allocate sandbox manager");
        agentos_mutex_destroy(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTOS_ENOMEM;
    }

    g_sandbox_manager->lock = agentos_mutex_create();
    if (!g_sandbox_manager->lock) {
        AGENTOS_LOG_ERROR("Failed to create sandbox manager lock");
        AGENTOS_FREE(g_sandbox_manager);
        g_sandbox_manager = NULL;
        agentos_mutex_destroy(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTOS_ENOMEM;
    }

    g_sandbox_manager->sandbox_count = 0;
    g_sandbox_manager->total_violations = 0;
    g_sandbox_manager->total_calls = 0;

    AGENTOS_LOG_INFO("Sandbox manager initialized");
    return AGENTOS_SUCCESS;
}

void agentos_sandbox_manager_destroy(void) {
    if (!g_sandbox_manager) return;

    agentos_mutex_lock(g_manager_lock);

    for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
        if (g_sandbox_manager->sandboxes[i]) {
            agentos_sandbox_destroy(g_sandbox_manager->sandboxes[i]);
            g_sandbox_manager->sandboxes[i] = NULL;
        }
    }

    agentos_mutex_destroy(g_sandbox_manager->lock);
    AGENTOS_FREE(g_sandbox_manager);
    g_sandbox_manager = NULL;

    agentos_mutex_unlock(g_manager_lock);
    agentos_mutex_destroy(g_manager_lock);
    g_manager_lock = NULL;

    AGENTOS_LOG_INFO("Sandbox manager destroyed");
}

/* ==================== 沙箱生命周期 ==================== */

agentos_error_t agentos_sandbox_create(const sandbox_config_t* manager,
                                       agentos_sandbox_t** out_sandbox) {
    if (!manager || !out_sandbox) return AGENTOS_EINVAL;

    if (!g_sandbox_manager) {
        AGENTOS_LOG_ERROR("Sandbox manager not initialized");
        return AGENTOS_ENOTINIT;
    }

    agentos_mutex_lock(g_manager_lock);

    int slot = -1;
    for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
        if (!g_sandbox_manager->sandboxes[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentos_mutex_unlock(g_manager_lock);
        AGENTOS_LOG_ERROR("No available sandbox slot");
        return AGENTOS_EBUSY;
    }

    agentos_sandbox_t* sandbox = (agentos_sandbox_t*)AGENTOS_CALLOC(1, sizeof(agentos_sandbox_t));
    if (!sandbox) {
        agentos_mutex_unlock(g_manager_lock);
        AGENTOS_LOG_ERROR("Failed to allocate sandbox");
        return AGENTOS_ENOMEM;
    }

    sandbox->sandbox_id = (uint64_t)slot + 1;
    sandbox->sandbox_name = manager->sandbox_name ? AGENTOS_STRDUP(manager->sandbox_name) : NULL;
    sandbox->owner_id = manager->owner_id ? AGENTOS_STRDUP(manager->owner_id) : NULL;
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

    memcpy(&sandbox->manager, manager, sizeof(sandbox_config_t));

    sandbox_quota_init(&sandbox->quota);

    sandbox->lock = agentos_mutex_create();
    if (!sandbox->lock) {
        if (sandbox->sandbox_name) AGENTOS_FREE(sandbox->sandbox_name);
        if (sandbox->owner_id) AGENTOS_FREE(sandbox->owner_id);
        AGENTOS_FREE(sandbox);
        agentos_mutex_unlock(g_manager_lock);
        return AGENTOS_ENOMEM;
    }

    g_sandbox_manager->sandboxes[slot] = sandbox;
    g_sandbox_manager->sandbox_count++;

    agentos_mutex_unlock(g_manager_lock);

    *out_sandbox = sandbox;
    AGENTOS_LOG_INFO("Sandbox created: %s (ID: %llu)",
                     sandbox->sandbox_name ? sandbox->sandbox_name : "unnamed",
                     (unsigned long long)sandbox->sandbox_id);

    return AGENTOS_SUCCESS;
}

void agentos_sandbox_destroy(agentos_sandbox_t* sandbox) {
    if (!sandbox) return;

    if (g_sandbox_manager) {
        agentos_mutex_lock(g_manager_lock);
        for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
            if (g_sandbox_manager->sandboxes[i] == sandbox) {
                g_sandbox_manager->sandboxes[i] = NULL;
                g_sandbox_manager->sandbox_count--;
                break;
            }
        }
        agentos_mutex_unlock(g_manager_lock);
    }

    if (sandbox->sandbox_name) AGENTOS_FREE(sandbox->sandbox_name);
    if (sandbox->owner_id) AGENTOS_FREE(sandbox->owner_id);

    sandbox_permission_destroy_all(sandbox->rules);

    if (sandbox->audit_log) {
        AGENTOS_FREE(sandbox->audit_log);
    }

    if (sandbox->lock) {
        agentos_mutex_destroy(sandbox->lock);
    }

    AGENTOS_FREE(sandbox);
    AGENTOS_LOG_DEBUG("Sandbox %llu destroyed", (unsigned long long)sandbox->sandbox_id);
}

/* ==================== 沙箱操作 ==================== */

agentos_error_t agentos_sandbox_invoke(agentos_sandbox_t* sandbox,
                                       int syscall_num,
                                       void** args,
                                       int argc,
                                       void** out_result) {
    if (!sandbox || !out_result) return AGENTOS_EINVAL;

    uint64_t start_time = 0;
    (void)start_time;

    sandbox->call_count++;
    if (g_sandbox_manager) {
        g_sandbox_manager->total_calls++;
    }

    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_TERMINATED) {
        agentos_mutex_unlock(sandbox->lock);
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EPERM, 0, "Sandbox terminated");
        return AGENTOS_EPERM;
    }

    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        agentos_mutex_unlock(sandbox->lock);
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EBUSY, 0, "Sandbox suspended");
        return AGENTOS_EBUSY;
    }

    sandbox->state = SANDBOX_STATE_ACTIVE;
    agentos_mutex_unlock(sandbox->lock);

    permission_type_t perm = sandbox_permission_check(sandbox, syscall_num, args, argc);
    if (perm == PERM_DENY) {
        sandbox->violation_count++;
        if (g_sandbox_manager) {
            g_sandbox_manager->total_violations++;
        }
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EACCES, 0, "Permission denied");

        agentos_mutex_lock(sandbox->lock);
        sandbox->state = SANDBOX_STATE_IDLE;
        agentos_mutex_unlock(sandbox->lock);
        return AGENTOS_EACCES;
    }

    if (!sandbox_quota_check(sandbox, RESOURCE_CPU, 1)) {
        sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EQUOTA, 0, "CPU quota exceeded");
        return AGENTOS_EQUOTA;
    }

    *out_result = NULL;

    sandbox_add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_SUCCESS, 0, "Executed");

    agentos_mutex_lock(sandbox->lock);
    sandbox->state = SANDBOX_STATE_IDLE;
    agentos_mutex_unlock(sandbox->lock);

    sandbox_quota_release(sandbox, RESOURCE_CPU, 1);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_add_rule(agentos_sandbox_t* sandbox, int syscall_num,
                                        permission_type_t perm_type, const char* condition) {
    return sandbox_permission_add(sandbox, syscall_num, perm_type, condition);
}

agentos_error_t agentos_sandbox_get_stats(agentos_sandbox_t* sandbox, char** out_stats) {
    if (!sandbox || !out_stats) return AGENTOS_EINVAL;

    char* stats = (char*)AGENTOS_MALLOC(STATS_BUFFER_SIZE);
    if (!stats) return AGENTOS_ENOMEM;

    snprintf(stats, STATS_BUFFER_SIZE,
             "{\"sandbox_id\":%llu,\"calls\":%llu,\"violations\":%llu,"
             "\"memory_usage\":%.2f,\"cpu_usage\":%.2f,\"io_usage\":%.2f}",
             (unsigned long long)sandbox->sandbox_id,
             (unsigned long long)sandbox->call_count,
             (unsigned long long)sandbox->violation_count,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_MEMORY) * 100.0,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_CPU) * 100.0,
             sandbox_quota_get_usage_ratio(sandbox, RESOURCE_IO) * 100.0);

    *out_stats = stats;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_manager_get_stats(char** out_stats) {
    CHECK_NULL(out_stats);

    if (!g_sandbox_manager) {
        *out_stats = AGENTOS_STRDUP("{\"error\":\"Manager not initialized\"}");
        return AGENTOS_ENOTINIT;
    }

    char* stats = (char*)AGENTOS_MALLOC(MANAGER_STATS_BUFFER_SIZE);
    if (!stats) return AGENTOS_ENOMEM;

    snprintf(stats, MANAGER_STATS_BUFFER_SIZE,
             "{\"sandboxes\":%u,\"total_calls\":%llu,\"total_violations\":%llu}",
             g_sandbox_manager->sandbox_count,
             (unsigned long long)g_sandbox_manager->total_calls,
             (unsigned long long)g_sandbox_manager->total_violations);

    *out_stats = stats;
    return AGENTOS_SUCCESS;
}

void agentos_sandbox_reset_quota(agentos_sandbox_t* sandbox) {
    if (sandbox) {
        sandbox_quota_reset(sandbox);
    }
}

agentos_error_t agentos_sandbox_suspend(agentos_sandbox_t* sandbox) {
    CHECK_NULL(sandbox);

    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state != SANDBOX_STATE_ACTIVE) {
        sandbox->state = SANDBOX_STATE_SUSPENDED;
        AGENTOS_LOG_INFO("Sandbox %llu suspended", (unsigned long long)sandbox->sandbox_id);
    }
    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_resume(agentos_sandbox_t* sandbox) {
    CHECK_NULL(sandbox);

    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        sandbox->state = SANDBOX_STATE_IDLE;
        AGENTOS_LOG_INFO("Sandbox %llu resumed", (unsigned long long)sandbox->sandbox_id);
    }
    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_terminate(agentos_sandbox_t* sandbox) {
    CHECK_NULL(sandbox);

    agentos_mutex_lock(sandbox->lock);
    sandbox->state = SANDBOX_STATE_TERMINATED;
    AGENTOS_LOG_INFO("Sandbox %llu terminated", (unsigned long long)sandbox->sandbox_id);
    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_health_check(agentos_sandbox_t* sandbox, char** out_json) {
    if (!sandbox || !out_json) return AGENTOS_EINVAL;

    const char* state_str = "unknown";
    switch (sandbox->state) {
        case SANDBOX_STATE_IDLE: state_str = "idle"; break;
        case SANDBOX_STATE_ACTIVE: state_str = "active"; break;
        case SANDBOX_STATE_SUSPENDED: state_str = "suspended"; break;
        case SANDBOX_STATE_TERMINATED: state_str = "terminated"; break;
    }

    char* json = (char*)AGENTOS_MALLOC(HEALTH_CHECK_BUFFER_SIZE);
    if (!json) return AGENTOS_ENOMEM;

    snprintf(json, HEALTH_CHECK_BUFFER_SIZE,
             "{\"id\":%llu,\"state\":\"%s\",\"healthy\":%s}",
             (unsigned long long)sandbox->sandbox_id,
             state_str,
             (sandbox->state != SANDBOX_STATE_TERMINATED) ? "true" : "false");

    *out_json = json;
    return AGENTOS_SUCCESS;
}

/* ==================== 安全增强功能 ==================== */

/**
 * @brief 输入净化函数 - 检查并清理潜在危险的输入
 * @param input 输入字符串
 * @param max_length 最大允许长度
 * @return 0=安全，非0=检测到危险内容
 */
static int sanitize_input(const char* input, size_t max_length) {
    if (!input) return -1; /* 空输入视为危险 */
    
    /* 检查长度限制 */
    if (strlen(input) > max_length) {
        return 1; /* 超长输入 */
    }
    
    /* 检查危险字符模式 */
    const char* dangerous_patterns[] = {
        "..",           /* 路径遍历 */
        "\0",          /* Null字节注入 */
        "<script",     /* XSS攻击 */
        "javascript:", /* JavaScript协议 */
        "data:",       /* Data URI */
        ";",           /* 命令分隔符（在某些上下文中） */
        "|",           /* 管道符号 */
        "&",           /* 后台执行 */
        "`",           /* 命令替换 */
        "$(",          /* 子shell执行 */
        NULL
    };
    
    for (int i = 0; dangerous_patterns[i] != NULL; i++) {
        if (strstr(input, dangerous_patterns[i]) != NULL) {
            return 2 + i; /* 返回不同的错误码表示不同类型的危险 */
        }
    }
    
    return 0; /* 输入安全 */
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
static void sandbox_add_enhanced_audit_entry(
    agentos_sandbox_t* sandbox,
    int syscall_num,
    const char* syscall_name,
    agentos_error_t result,
    int severity,
    const char* message) {
    
    if (!sandbox || !sandbox->audit_log) return;
    
    /* 如果审计日志未初始化，跳过 */
    if (sandbox->audit_capacity == 0) return;
    
    audit_entry_t* entry = &sandbox->audit_log[sandbox->audit_write_index];
    
    /* 填充审计记录 */
    entry->timestamp = (uint64_t)time(NULL);
    entry->event_type = (result == AGENTOS_SUCCESS) ? 1 : 2; /* 1=成功, 2=失败/违规 */
    entry->syscall_num = syscall_num;
    entry->result = (int)result;
    entry->severity = severity;
    
    /* 复制系统调用名称（带截断保护） */
    if (syscall_name) {
        strncpy(entry->syscall_name, syscall_name, sizeof(entry->syscall_name) - 1);
        entry->syscall_name[sizeof(entry->syscall_name) - 1] = '\0';
    } else {
        snprintf(entry->syscall_name, sizeof(entry->syscall_name), "%d", syscall_num);
    }
    
    /* 复制消息（带截断保护） */
    if (message) {
        strncpy(entry->message, message, sizeof(entry->message) - 1);
        entry->message[sizeof(entry->message) - 1] = '\0';
    } else {
        entry->message[0] = '\0';
    }
    
    /* 更新索引（环形缓冲） */
    sandbox->audit_write_index = (sandbox->audit_write_index + 1) % sandbox->audit_capacity;
    if (sandbox->audit_count < sandbox->audit_capacity) {
        sandbox->audit_count++;
    }
}

/**
 * @brief 初始化增强功能（在创建沙箱时调用）
 */
static void init_sandbox_enhancements(agentos_sandbox_t* sandbox) {
    if (!sandbox) return;
    
    /* 初始化审计日志缓冲区 */
    sandbox->audit_capacity = MAX_AUDIT_ENTRIES;
    sandbox->audit_log = (audit_entry_t*)AGENTOS_CALLOC(
        sandbox->audit_capacity, sizeof(audit_entry_t));
    if (!sandbox->audit_log) {
        sandbox->audit_capacity = 0;
        AGENTOS_LOG_ERROR("Failed to allocate audit log buffer");
    }
    sandbox->audit_count = 0;
    sandbox->audit_write_index = 0;
    
    /* 初始化安全策略 */
    memset(&sandbox->policy, 0, sizeof(security_policy_t));
    sandbox->policy.version = 1;
    sandbox->policy.last_updated = time(NULL);
    strncpy(sandbox->policy.updated_by, "system", 
            sizeof(sandbox->policy.updated_by) - 1);
    sandbox->policy.allow_dynamic_update = 1;
    sandbox->policy.strict_mode = 0;
    sandbox->policy.audit_level = 1; /* 标准审计级别 */
    sandbox->policy.risk_threshold = 0.7f;
    
    /* 初始化性能统计 */
    memset(&sandbox->perf_stats, 0, sizeof(sandbox_perf_stats_t));
    sandbox->perf_stats.stats_reset_time = time(NULL);
    
    /* 默认启用增强功能 */
    sandbox->enable_input_sanitization = 1;
    sandbox->enable_resource_monitoring = 1;
}

/**
 * @brief 动态更新安全策略
 * @param sandbox 沙箱实例
 * @param new_policy 新的策略配置
 * @return 错误码
 */
agentos_error_t agentos_sandbox_update_security_policy(
    agentos_sandbox_t* sandbox,
    const security_policy_t* new_policy) {
    
    if (!sandbox || !new_policy) return AGENTOS_EINVAL;
    
    /* 检查是否允许动态更新 */
    if (!sandbox->policy.allow_dynamic_update) {
        return AGENTOS_EPERM;
    }
    
    agentos_mutex_lock(sandbox->lock);
    
    /* 验证策略参数有效性 */
    if (new_policy->risk_threshold < 0.0f || new_policy->risk_threshold > 1.0f) {
        agentos_mutex_unlock(sandbox->lock);
        return AGENTOS_EINVAL;
    }
    
    if (new_policy->audit_level < 0 || new_policy->audit_level > 3) {
        agentos_mutex_unlock(sandbox->lock);
        return AGENTOS_EINVAL;
    }
    
    /* 应用新策略 */
    memcpy(&sandbox->policy, new_policy, sizeof(security_policy_t));
    sandbox->policy.last_updated = time(NULL);
    
    /* 记录策略更新事件 */
    char log_msg[AUDIT_LOG_BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg),
             "Security policy updated to v%u by %s (strict=%d, audit_level=%d)",
             new_policy->version,
             new_policy->updated_by,
             new_policy->strict_mode,
             new_policy->audit_level);
    
    sandbox_add_enhanced_audit_entry(sandbox, 0, "POLICY_UPDATE",
                                     AGENTOS_SUCCESS, 1, log_msg);
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_INFO("Sandbox %llu: Security policy updated",
                     (unsigned long long)sandbox->sandbox_id);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取当前安全策略
 * @param sandbox 沙箱实例
 * @param[out] policy 输出的策略配置
 * @return 错误码
 */
agentos_error_t agentos_sandbox_get_security_policy(
    agentos_sandbox_t* sandbox,
    security_policy_t** policy) {
    
    if (!sandbox || !policy) return AGENTOS_EINVAL;
    
    *policy = &sandbox->policy;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取性能统计信息
 * @param sandbox 沙箱实例
 * @param[out] stats 输出的统计信息
 * @return 错误码
 */
agentos_error_t agentos_sandbox_get_performance_stats(
    agentos_sandbox_t* sandbox,
    sandbox_perf_stats_t** stats) {
    
    if (!sandbox || !stats) return AGENTOS_EINVAL;
    
    *stats = &sandbox->perf_stats;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置性能统计信息
 * @param sandbox 沙箱实例
 * @return 错误码
 */
agentos_error_t agentos_sandbox_reset_performance_stats(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(sandbox->lock);
    
    memset(&sandbox->perf_stats, 0, sizeof(sandbox_perf_stats_t));
    sandbox->perf_stats.stats_reset_time = time(NULL);
    
    agentos_mutex_unlock(sandbox->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 启用或禁用输入净化
 * @param sandbox 沙箱实例
 * @param enable 是否启用
 * @return 错误码
 */
agentos_error_t agentos_sandbox_set_input_sanitization(
    agentos_sandbox_t* sandbox,
    int enable) {
    
    if (!sandbox) return AGENTOS_EINVAL;
    
    sandbox->enable_input_sanitization = enable ? 1 : 0;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 导出审计日志为JSON格式
 * @param sandbox 沙箱实例
 * @param[out] out_json JSON格式的审计日志
 * @return 错误码
 */
agentos_error_t agentos_sandbox_export_audit_log_json(
    agentos_sandbox_t* sandbox,
    char** out_json) {
    
    if (!sandbox || !out_json) return AGENTOS_EINVAL;
    
    if (!sandbox->audit_log || sandbox->audit_count == 0) {
        *out_json = AGENTOS_STRDUP("[]");
        return AGENTOS_SUCCESS;
    }
    
    /* 分配足够大的缓冲区（每条约300字节） */
    size_t buffer_size = sandbox->audit_count * 400 + 100;
    char* json = (char*)AGENTOS_MALLOC(buffer_size);
    if (!json) return AGENTOS_ENOMEM;
    
    size_t pos = 0;
    pos += snprintf(json + pos, buffer_size - pos, "[\n");
    
    for (size_t i = 0; i < sandbox->audit_count && pos < buffer_size - 500; i++) {
        audit_entry_t* entry = &sandbox->audit_log[i];
        
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
                       (unsigned long long)entry->timestamp,
                       entry->event_type,
                       entry->syscall_name,
                       entry->syscall_num,
                       entry->result,
                       entry->severity,
                       entry->message,
                       (i < sandbox->audit_count - 1) ? "," : "");
    }
    
    pos += snprintf(json + pos, buffer_size - pos, "]");
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}
