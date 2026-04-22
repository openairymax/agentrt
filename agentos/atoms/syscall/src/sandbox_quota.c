/**
 * @file sandbox_quota.c
 * @brief 沙箱资源配额管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sandbox_quota.h"
#include "agentos.h"
#include "logger.h"
#include <string.h>

/* 基础库兼容性层 */
#include "memory_compat.h"

/* 沙箱内部结构 */
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
    /* 内联资源配额 */
    resource_quota_t quota;
};

/* ==================== 资源配额管理 ==================== */

void sandbox_quota_init(resource_quota_t* quota) {
    if (!quota) return;
    
    memset(quota, 0, sizeof(resource_quota_t));
    
    /* 设置默认限制 */
    quota->max_memory_bytes = 512 * 1024 * 1024;  /* 512MB */
    quota->max_cpu_time_ms = 60000;                /* 60 秒 */
    quota->max_io_ops = 10000;                     /* 10000 次 I/O */
    quota->max_file_size = 100;                    /* 100MB */
    quota->max_network_bytes = 100;                /* 100MB */
}

int sandbox_quota_check(agentos_sandbox_t* sandbox, resource_type_t resource, uint64_t amount) {
    if (!sandbox) return 0;

    agentos_mutex_lock(sandbox->lock);

    int allowed = 0;
    resource_quota_t* quota = &sandbox->quota;

    switch (resource) {
        case RESOURCE_MEMORY:
            allowed = (quota->current_memory + amount <= quota->max_memory_bytes);
            if (allowed) quota->current_memory += amount;
            break;

        case RESOURCE_CPU:
            allowed = (quota->current_cpu_time_ms + amount <= quota->max_cpu_time_ms);
            if (allowed) quota->current_cpu_time_ms += amount;
            break;

        case RESOURCE_IO:
            allowed = (quota->current_io_ops + amount <= quota->max_io_ops);
            if (allowed) quota->current_io_ops += amount;
            break;

        default:
            allowed = 1;  /* 其他资源类型默认允许 */
            break;
    }

    agentos_mutex_unlock(sandbox->lock);

    return allowed;
}

void sandbox_quota_release(agentos_sandbox_t* sandbox, resource_type_t resource, uint64_t amount) {
    if (!sandbox) return;

    agentos_mutex_lock(sandbox->lock);

    resource_quota_t* quota = &sandbox->quota;

    switch (resource) {
        case RESOURCE_MEMORY:
            if (quota->current_memory >= amount) {
                quota->current_memory -= amount;
            } else {
                quota->current_memory = 0;
            }
            break;

        case RESOURCE_CPU:
            if (quota->current_cpu_time_ms >= amount) {
                quota->current_cpu_time_ms -= amount;
            } else {
                quota->current_cpu_time_ms = 0;
            }
            break;

        case RESOURCE_IO:
            if (quota->current_io_ops >= amount) {
                quota->current_io_ops -= amount;
            } else {
                quota->current_io_ops = 0;
            }
            break;

        default:
            break;
    }

    agentos_mutex_unlock(sandbox->lock);
}

void sandbox_quota_reset(agentos_sandbox_t* sandbox) {
    if (!sandbox) return;

    agentos_mutex_lock(sandbox->lock);

    resource_quota_t* quota = &sandbox->quota;
    uint64_t max_memory = quota->max_memory_bytes;
    uint64_t max_cpu = quota->max_cpu_time_ms;
    uint64_t max_io = quota->max_io_ops;
    uint32_t max_file = quota->max_file_size;
    uint32_t max_network = quota->max_network_bytes;

    /* 重置为默认值 */
    memset(quota, 0, sizeof(resource_quota_t));
    
    quota->max_memory_bytes = max_memory;
    quota->max_cpu_time_ms = max_cpu;
    quota->max_io_ops = max_io;
    quota->max_file_size = max_file;
    quota->max_network_bytes = max_network;

    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_INFO("Reset resource quota for sandbox");
}

double sandbox_quota_get_usage_ratio(agentos_sandbox_t* sandbox, resource_type_t resource) {
    if (!sandbox) return 0.0;

    agentos_mutex_lock(sandbox->lock);

    resource_quota_t* quota = &sandbox->quota;
    double ratio = 0.0;

    switch (resource) {
        case RESOURCE_MEMORY:
            if (quota->max_memory_bytes > 0) {
                ratio = (double)quota->current_memory / (double)quota->max_memory_bytes;
            }
            break;

        case RESOURCE_CPU:
            if (quota->max_cpu_time_ms > 0) {
                ratio = (double)quota->current_cpu_time_ms / (double)quota->max_cpu_time_ms;
            }
            break;

        case RESOURCE_IO:
            if (quota->max_io_ops > 0) {
                ratio = (double)quota->current_io_ops / (double)quota->max_io_ops;
            }
            break;

        default:
            ratio = 0.0;
            break;
    }

    agentos_mutex_unlock(sandbox->lock);

    return ratio;
}
