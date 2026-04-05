/**
 * @file sandbox.c
 * @brief 系统调用安全沙箱 - 精简版
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 安全沙箱提供系统调用的隔离执行环境，防止恶意或错误代码影响系统稳定性。
 * 基于 sandbox_utils、sandbox_permission、sandbox_quota 模块构建。
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include "sandbox_utils.h"
#include "sandbox_permission.h"
#include "sandbox_quota.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 基础库兼容性层 */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"

/* ==================== 常量定义 ==================== */

#define MAX_SANDBOXES 64
#define DEFAULT_SANDBOX_TIMEOUT_MS 30000
#define DEFAULT_MAX_MEMORY_BYTES (512 * 1024 * 1024)
#define DEFAULT_MAX_CPU_TIME_MS 60000
#define DEFAULT_MAX_IO_OPS 10000

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
    void* audit_log;
    size_t audit_count;
    size_t audit_capacity;
    resource_quota_t quota;
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

    char* stats = (char*)AGENTOS_MALLOC(1024);
    if (!stats) return AGENTOS_ENOMEM;

    snprintf(stats, 1024,
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
    if (!out_stats) return AGENTOS_EINVAL;

    if (!g_sandbox_manager) {
        *out_stats = AGENTOS_STRDUP("{\"error\":\"Manager not initialized\"}");
        return AGENTOS_ENOTINIT;
    }

    char* stats = (char*)AGENTOS_MALLOC(512);
    if (!stats) return AGENTOS_ENOMEM;

    snprintf(stats, 512,
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
    if (!sandbox) return AGENTOS_EINVAL;

    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state != SANDBOX_STATE_ACTIVE) {
        sandbox->state = SANDBOX_STATE_SUSPENDED;
        AGENTOS_LOG_INFO("Sandbox %llu suspended", (unsigned long long)sandbox->sandbox_id);
    }
    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_resume(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;

    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        sandbox->state = SANDBOX_STATE_IDLE;
        AGENTOS_LOG_INFO("Sandbox %llu resumed", (unsigned long long)sandbox->sandbox_id);
    }
    agentos_mutex_unlock(sandbox->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sandbox_terminate(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;

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

    char* json = (char*)AGENTOS_MALLOC(512);
    if (!json) return AGENTOS_ENOMEM;

    snprintf(json, 512,
             "{\"id\":%llu,\"state\":\"%s\",\"healthy\":%s}",
             (unsigned long long)sandbox->sandbox_id,
             state_str,
             (sandbox->state != SANDBOX_STATE_TERMINATED) ? "true" : "false");

    *out_json = json;
    return AGENTOS_SUCCESS;
}
