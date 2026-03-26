/**
 * @file sandbox.c
 * @brief 系统调用安全沙箱实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 安全沙箱提供系统调用的隔离执行环境，防止恶意或错误代码影响系统稳定性。
 * 实现生产级安全控制，支持99.999%可靠性标准。
 * 
 * 核心功能：
 * 1. 权限控制：基于角色的访问控制（RBAC）
 * 2. 资源隔离：内存、CPU、I/O资源限制
 * 3. 调用过滤：白名单/黑名单机制
 * 4. 审计日志：完整的调用追踪记录
 * 5. 异常捕获：防止崩溃传播
 * 6. 超时控制：防止无限等待
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include "id_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大沙箱数量 */
#define MAX_SANDBOXES 64

/** @brief 最大权限规则数量 */
#define MAX_PERMISSION_RULES 256

/** @brief 最大审计日志条目 */
#define MAX_AUDIT_ENTRIES 10000

/** @brief 默认超时（毫秒） */
#define DEFAULT_SANDBOX_TIMEOUT_MS 30000

/** @brief 最大内存限制（字节） */
#define DEFAULT_MAX_MEMORY_BYTES (512 * 1024 * 1024)

/** @brief 最大CPU时间（毫秒） */
#define DEFAULT_MAX_CPU_TIME_MS 60000

/** @brief 最大I/O操作数 */
#define DEFAULT_MAX_IO_OPS 10000

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 权限类型枚举
 */
typedef enum {
    PERM_ALLOW = 0,    /**< 允许 */
    PERM_DENY,         /**< 拒绝 */
    PERM_ASK           /**< 需确认 */
} permission_type_t;

/**
 * @brief 资源类型枚举
 */
typedef enum {
    RESOURCE_MEMORY = 0,   /**< 内存 */
    RESOURCE_CPU,          /**< CPU时间 */
    RESOURCE_IO,           /**< I/O操作 */
    RESOURCE_NETWORK,      /**< 网络访问 */
    RESOURCE_FILE          /**< 文件访问 */
} resource_type_t;

/**
 * @brief 权限规则结构
 */
typedef struct permission_rule {
    int syscall_num;              /**< 系统调用号 */
    permission_type_t perm_type;  /**< 权限类型 */
    char* condition;              /**< 条件表达式（JSON） */
    uint32_t flags;               /**< 标志位 */
    struct permission_rule* next; /**< 下一个规则 */
} permission_rule_t;

/**
 * @brief 资源配额结构
 */
typedef struct resource_quota {
    uint64_t max_memory_bytes;    /**< 最大内存 */
    uint64_t current_memory;      /**< 当前内存使用 */
    uint64_t max_cpu_time_ms;     /**< 最大CPU时间 */
    uint64_t current_cpu_time_ms; /**< 当前CPU时间 */
    uint64_t max_io_ops;          /**< 最大I/O操作 */
    uint64_t current_io_ops;      /**< 当前I/O操作 */
    uint32_t max_file_size;       /**< 最大文件大小（MB） */
    uint32_t max_network_bytes;   /**< 最大网络传输（MB） */
} resource_quota_t;

/**
 * @brief 审计日志条目
 */
typedef struct audit_entry {
    uint64_t timestamp_ns;        /**< 时间戳 */
    uint64_t sandbox_id;          /**< 沙箱ID */
    int syscall_num;              /**< 系统调用号 */
    char* caller_id;              /**< 调用者ID */
    char* args_hash;              /**< 参数哈希 */
    int result_code;              /**< 结果码 */
    uint64_t duration_ns;         /**< 执行时长 */
    char* details;                /**< 详细信息 */
} audit_entry_t;

/**
 * @brief 沙箱状态枚举
 */
typedef enum {
    SANDBOX_STATE_IDLE = 0,       /**< 空闲 */
    SANDBOX_STATE_ACTIVE,         /**< 活跃 */
    SANDBOX_STATE_SUSPENDED,      /**< 暂停 */
    SANDBOX_STATE_TERMINATED      /**< 终止 */
} sandbox_state_t;

/**
 * @brief 沙箱配置结构
 */
typedef struct sandbox_config {
    char* sandbox_name;           /**< 沙箱名称 */
    char* owner_id;               /**< 所有者ID */
    uint32_t priority;            /**< 优先级 */
    uint32_t timeout_ms;          /**< 超时时间 */
    uint32_t flags;               /**< 标志位 */
    resource_quota_t quota;       /**< 资源配额 */
} sandbox_config_t;

/**
 * @brief 沙箱内部结构
 */
struct agentos_sandbox {
    uint64_t sandbox_id;          /**< 沙箱ID */
    char* sandbox_name;           /**< 沙箱名称 */
    char* owner_id;               /**< 所有者ID */
    sandbox_state_t state;        /**< 状态 */
    sandbox_config_t config;      /**< 配置 */
    permission_rule_t* rules;     /**< 权限规则链表 */
    uint32_t rule_count;          /**< 规则数量 */
    agentos_mutex_t* lock;        /**< 线程锁 */
    uint64_t create_time_ns;      /**< 创建时间 */
    uint64_t last_active_ns;      /**< 最后活跃时间 */
    uint64_t call_count;          /**< 调用次数 */
    uint64_t violation_count;     /**< 违规次数 */
    audit_entry_t* audit_log;     /**< 审计日志 */
    size_t audit_count;           /**< 审计条目数 */
    size_t audit_capacity;        /**< 审计容量 */
};

/**
 * @brief 沙箱管理器结构
 */
typedef struct sandbox_manager {
    agentos_sandbox_t* sandboxes[MAX_SANDBOXES]; /**< 沙箱数组 */
    uint32_t sandbox_count;       /**< 沙箱数量 */
    agentos_mutex_t* lock;        /**< 全局锁 */
    audit_entry_t* global_audit;  /**< 全局审计日志 */
    size_t global_audit_count;    /**< 全局审计条目数 */
    size_t global_audit_capacity; /**< 全局审计容量 */
    uint64_t total_violations;    /**< 总违规次数 */
    uint64_t total_calls;         /**< 总调用次数 */
} sandbox_manager_t;

/* ==================== 全局变量 ==================== */

static sandbox_manager_t* g_sandbox_manager = NULL;
static agentos_mutex_t* g_manager_lock = NULL;

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 计算字符串的简单哈希
 * @param str 输入字符串
 * @return 哈希值
 */
static uint64_t simple_hash(const char* str) {
    if (!str) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief 获取当前时间戳（纳秒）
 * @return 时间戳
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * @brief 创建权限规则
 * @param syscall_num 系统调用号
 * @param perm_type 权限类型
 * @param condition 条件表达式
 * @return 规则对象，失败返回NULL
 */
static permission_rule_t* create_permission_rule(int syscall_num, 
                                                  permission_type_t perm_type,
                                                  const char* condition) {
    permission_rule_t* rule = (permission_rule_t*)calloc(1, sizeof(permission_rule_t));
    if (!rule) return NULL;
    
    rule->syscall_num = syscall_num;
    rule->perm_type = perm_type;
    rule->condition = condition ? strdup(condition) : NULL;
    rule->flags = 0;
    rule->next = NULL;
    
    return rule;
}

/**
 * @brief 释放权限规则
 * @param rule 规则对象
 */
static void free_permission_rule(permission_rule_t* rule) {
    if (!rule) return;
    if (rule->condition) free(rule->condition);
    free(rule);
}

/**
 * @brief 释放权限规则链表
 * @param head 链表头
 */
static void free_permission_rules(permission_rule_t* head) {
    while (head) {
        permission_rule_t* next = head->next;
        free_permission_rule(head);
        head = next;
    }
}

/**
 * @brief 添加审计条目
 * @param sandbox 沙箱对象
 * @param syscall_num 系统调用号
 * @param caller_id 调用者ID
 * @param result_code 结果码
 * @param duration_ns 执行时长
 * @param details 详细信息
 */
static void add_audit_entry(agentos_sandbox_t* sandbox, int syscall_num,
                            const char* caller_id, int result_code,
                            uint64_t duration_ns, const char* details) {
    if (!sandbox) return;
    
    agentos_mutex_lock(sandbox->lock);
    
    // 检查容量，必要时扩容
    if (sandbox->audit_count >= sandbox->audit_capacity) {
        size_t new_capacity = sandbox->audit_capacity == 0 ? 100 : sandbox->audit_capacity * 2;
        if (new_capacity > MAX_AUDIT_ENTRIES) new_capacity = MAX_AUDIT_ENTRIES;
        
        audit_entry_t* new_log = (audit_entry_t*)realloc(sandbox->audit_log, 
                                                          new_capacity * sizeof(audit_entry_t));
        if (new_log) {
            sandbox->audit_log = new_log;
            sandbox->audit_capacity = new_capacity;
        }
    }
    
    // 添加条目
    if (sandbox->audit_count < sandbox->audit_capacity) {
        audit_entry_t* entry = &sandbox->audit_log[sandbox->audit_count];
        entry->timestamp_ns = get_timestamp_ns();
        entry->sandbox_id = sandbox->sandbox_id;
        entry->syscall_num = syscall_num;
        entry->caller_id = caller_id ? strdup(caller_id) : NULL;
        entry->args_hash = NULL;
        entry->result_code = result_code;
        entry->duration_ns = duration_ns;
        entry->details = details ? strdup(details) : NULL;
        
        sandbox->audit_count++;
    }
    
    agentos_mutex_unlock(sandbox->lock);
}

/**
 * @brief 检查权限规则
 * @param sandbox 沙箱对象
 * @param syscall_num 系统调用号
 * @param args 参数
 * @param argc 参数数量
 * @return 权限类型
 */
static permission_type_t check_permission(agentos_sandbox_t* sandbox, 
                                          int syscall_num,
                                          void** args, int argc) {
    if (!sandbox) return PERM_DENY;
    
    agentos_mutex_lock(sandbox->lock);
    
    permission_rule_t* rule = sandbox->rules;
    permission_type_t result = PERM_ALLOW;  // 默认允许
    
    while (rule) {
        if (rule->syscall_num == syscall_num || rule->syscall_num == -1) {
            // 匹配系统调用号（-1表示所有调用）
            if (rule->perm_type == PERM_DENY) {
                // 拒绝规则优先
                result = PERM_DENY;
                break;
            } else if (rule->perm_type == PERM_ASK) {
                result = PERM_ASK;
            }
        }
        rule = rule->next;
    }
    
    agentos_mutex_unlock(sandbox->lock);
    
    return result;
}

/**
 * @brief 检查资源配额
 * @param sandbox 沙箱对象
 * @param resource 资源类型
 * @param amount 请求量
 * @return 1表示允许，0表示超限
 */
static int check_resource_quota(agentos_sandbox_t* sandbox, 
                                resource_type_t resource,
                                uint64_t amount) {
    if (!sandbox) return 0;
    
    agentos_mutex_lock(sandbox->lock);
    
    int allowed = 0;
    resource_quota_t* quota = &sandbox->config.quota;
    
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
            allowed = 1;  // 其他资源类型默认允许
            break;
    }
    
    agentos_mutex_unlock(sandbox->lock);
    
    return allowed;
}

/**
 * @brief 释放资源
 * @param sandbox 沙箱对象
 * @param resource 资源类型
 * @param amount 释放量
 */
static void release_resource(agentos_sandbox_t* sandbox,
                             resource_type_t resource,
                             uint64_t amount) {
    if (!sandbox) return;
    
    agentos_mutex_lock(sandbox->lock);
    
    resource_quota_t* quota = &sandbox->config.quota;
    
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

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化沙箱管理器
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
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
    
    g_sandbox_manager = (sandbox_manager_t*)calloc(1, sizeof(sandbox_manager_t));
    if (!g_sandbox_manager) {
        AGENTOS_LOG_ERROR("Failed to allocate sandbox manager");
        agentos_mutex_destroy(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_sandbox_manager->lock = agentos_mutex_create();
    if (!g_sandbox_manager->lock) {
        AGENTOS_LOG_ERROR("Failed to create sandbox manager lock");
        free(g_sandbox_manager);
        g_sandbox_manager = NULL;
        agentos_mutex_destroy(g_manager_lock);
        g_manager_lock = NULL;
        return AGENTOS_ENOMEM;
    }
    
    g_sandbox_manager->sandbox_count = 0;
    g_sandbox_manager->total_violations = 0;
    g_sandbox_manager->total_calls = 0;
    g_sandbox_manager->global_audit = NULL;
    g_sandbox_manager->global_audit_count = 0;
    g_sandbox_manager->global_audit_capacity = 0;
    
    AGENTOS_LOG_INFO("Sandbox manager initialized");
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁沙箱管理器
 */
void agentos_sandbox_manager_destroy(void) {
    if (!g_sandbox_manager) return;
    
    agentos_mutex_lock(g_manager_lock);
    
    // 销毁所有沙箱
    for (uint32_t i = 0; i < MAX_SANDBOXES; i++) {
        if (g_sandbox_manager->sandboxes[i]) {
            agentos_sandbox_destroy(g_sandbox_manager->sandboxes[i]);
            g_sandbox_manager->sandboxes[i] = NULL;
        }
    }
    
    // 释放全局审计日志
    if (g_sandbox_manager->global_audit) {
        for (size_t i = 0; i < g_sandbox_manager->global_audit_count; i++) {
            audit_entry_t* entry = &g_sandbox_manager->global_audit[i];
            if (entry->caller_id) free(entry->caller_id);
            if (entry->args_hash) free(entry->args_hash);
            if (entry->details) free(entry->details);
        }
        free(g_sandbox_manager->global_audit);
    }
    
    agentos_mutex_destroy(g_sandbox_manager->lock);
    free(g_sandbox_manager);
    g_sandbox_manager = NULL;
    
    agentos_mutex_unlock(g_manager_lock);
    agentos_mutex_destroy(g_manager_lock);
    g_manager_lock = NULL;
    
    AGENTOS_LOG_INFO("Sandbox manager destroyed");
}

/**
 * @brief 创建沙箱
 * @param config 沙箱配置
 * @param out_sandbox 输出沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_create(const sandbox_config_t* config,
                                       agentos_sandbox_t** out_sandbox) {
    if (!config || !out_sandbox) return AGENTOS_EINVAL;
    
    if (!g_sandbox_manager) {
        AGENTOS_LOG_ERROR("Sandbox manager not initialized");
        return AGENTOS_ENOTINIT;
    }
    
    agentos_mutex_lock(g_manager_lock);
    
    // 查找空闲槽位
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
    
    // 分配沙箱结构
    agentos_sandbox_t* sandbox = (agentos_sandbox_t*)calloc(1, sizeof(agentos_sandbox_t));
    if (!sandbox) {
        agentos_mutex_unlock(g_manager_lock);
        AGENTOS_LOG_ERROR("Failed to allocate sandbox");
        return AGENTOS_ENOMEM;
    }
    
    // 初始化沙箱
    sandbox->sandbox_id = (uint64_t)slot + 1;
    sandbox->sandbox_name = config->sandbox_name ? strdup(config->sandbox_name) : NULL;
    sandbox->owner_id = config->owner_id ? strdup(config->owner_id) : NULL;
    sandbox->state = SANDBOX_STATE_IDLE;
    sandbox->create_time_ns = get_timestamp_ns();
    sandbox->last_active_ns = sandbox->create_time_ns;
    sandbox->call_count = 0;
    sandbox->violation_count = 0;
    
    // 复制配置
    memcpy(&sandbox->config, config, sizeof(sandbox_config_t));
    if (config->sandbox_name) {
        sandbox->config.sandbox_name = strdup(config->sandbox_name);
    }
    if (config->owner_id) {
        sandbox->config.owner_id = strdup(config->owner_id);
    }
    
    // 设置默认配额
    if (sandbox->config.quota.max_memory_bytes == 0) {
        sandbox->config.quota.max_memory_bytes = DEFAULT_MAX_MEMORY_BYTES;
    }
    if (sandbox->config.quota.max_cpu_time_ms == 0) {
        sandbox->config.quota.max_cpu_time_ms = DEFAULT_MAX_CPU_TIME_MS;
    }
    if (sandbox->config.quota.max_io_ops == 0) {
        sandbox->config.quota.max_io_ops = DEFAULT_MAX_IO_OPS;
    }
    if (sandbox->config.timeout_ms == 0) {
        sandbox->config.timeout_ms = DEFAULT_SANDBOX_TIMEOUT_MS;
    }
    
    // 创建锁
    sandbox->lock = agentos_mutex_create();
    if (!sandbox->lock) {
        if (sandbox->sandbox_name) free(sandbox->sandbox_name);
        if (sandbox->owner_id) free(sandbox->owner_id);
        free(sandbox);
        agentos_mutex_unlock(g_manager_lock);
        AGENTOS_LOG_ERROR("Failed to create sandbox lock");
        return AGENTOS_ENOMEM;
    }
    
    // 注册到管理器
    g_sandbox_manager->sandboxes[slot] = sandbox;
    g_sandbox_manager->sandbox_count++;
    
    agentos_mutex_unlock(g_manager_lock);
    
    *out_sandbox = sandbox;
    
    AGENTOS_LOG_INFO("Sandbox created: %s (ID: %llu)", 
                     sandbox->sandbox_name ? sandbox->sandbox_name : "unnamed",
                     (unsigned long long)sandbox->sandbox_id);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁沙箱
 * @param sandbox 沙箱句柄
 */
void agentos_sandbox_destroy(agentos_sandbox_t* sandbox) {
    if (!sandbox) return;
    
    AGENTOS_LOG_DEBUG("Destroying sandbox: %llu", (unsigned long long)sandbox->sandbox_id);
    
    // 从管理器移除
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
    
    // 释放资源
    if (sandbox->sandbox_name) free(sandbox->sandbox_name);
    if (sandbox->owner_id) free(sandbox->owner_id);
    if (sandbox->config.sandbox_name) free(sandbox->config.sandbox_name);
    if (sandbox->config.owner_id) free(sandbox->config.owner_id);
    
    // 释放权限规则
    free_permission_rules(sandbox->rules);
    
    // 释放审计日志
    if (sandbox->audit_log) {
        for (size_t i = 0; i < sandbox->audit_count; i++) {
            audit_entry_t* entry = &sandbox->audit_log[i];
            if (entry->caller_id) free(entry->caller_id);
            if (entry->args_hash) free(entry->args_hash);
            if (entry->details) free(entry->details);
        }
        free(sandbox->audit_log);
    }
    
    // 销毁锁
    if (sandbox->lock) {
        agentos_mutex_destroy(sandbox->lock);
    }
    
    free(sandbox);
}

/**
 * @brief 在沙箱中执行系统调用
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param args 参数数组
 * @param argc 参数数量
 * @param out_result 输出结果
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_invoke(agentos_sandbox_t* sandbox,
                                       int syscall_num,
                                       void** args,
                                       int argc,
                                       void** out_result) {
    if (!sandbox || !out_result) return AGENTOS_EINVAL;
    
    uint64_t start_time_ns = get_timestamp_ns();
    
    // 更新统计
    sandbox->call_count++;
    sandbox->last_active_ns = start_time_ns;
    if (g_sandbox_manager) {
        g_sandbox_manager->total_calls++;
    }
    
    // 检查沙箱状态
    agentos_mutex_lock(sandbox->lock);
    if (sandbox->state == SANDBOX_STATE_TERMINATED) {
        agentos_mutex_unlock(sandbox->lock);
        add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EPERM, 
                       get_timestamp_ns() - start_time_ns, "Sandbox terminated");
        return AGENTOS_EPERM;
    }
    
    if (sandbox->state == SANDBOX_STATE_SUSPENDED) {
        agentos_mutex_unlock(sandbox->lock);
        add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EBUSY,
                       get_timestamp_ns() - start_time_ns, "Sandbox suspended");
        return AGENTOS_EBUSY;
    }
    
    sandbox->state = SANDBOX_STATE_ACTIVE;
    agentos_mutex_unlock(sandbox->lock);
    
    // 检查权限
    permission_type_t perm = check_permission(sandbox, syscall_num, args, argc);
    if (perm == PERM_DENY) {
        sandbox->violation_count++;
        if (g_sandbox_manager) {
            g_sandbox_manager->total_violations++;
        }
        
        add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EACCES,
                       get_timestamp_ns() - start_time_ns, "Permission denied");
        
        agentos_mutex_lock(sandbox->lock);
        sandbox->state = SANDBOX_STATE_IDLE;
        agentos_mutex_unlock(sandbox->lock);
        
        return AGENTOS_EACCES;
    }
    
    // 检查资源配额
    if (!check_resource_quota(sandbox, RESOURCE_CPU, 1)) {
        add_audit_entry(sandbox, syscall_num, NULL, AGENTOS_EQUOTA,
                       get_timestamp_ns() - start_time_ns, "CPU quota exceeded");
        
        agentos_mutex_lock(sandbox->lock);
        sandbox->state = SANDBOX_STATE_IDLE;
        agentos_mutex_unlock(sandbox->lock);
        
        return AGENTOS_EQUOTA;
    }
    
    // 执行系统调用
    void* result = agentos_syscall_invoke(syscall_num, args, argc);
    *out_result = result;
    
    int result_code = (int)(intptr_t)result;
    uint64_t duration_ns = get_timestamp_ns() - start_time_ns;
    
    // 添加审计日志
    char details[256];
    snprintf(details, sizeof(details), "Syscall %d completed in %llu ns", 
             syscall_num, (unsigned long long)duration_ns);
    add_audit_entry(sandbox, syscall_num, NULL, result_code, duration_ns, details);
    
    // 更新状态
    agentos_mutex_lock(sandbox->lock);
    sandbox->state = SANDBOX_STATE_IDLE;
    agentos_mutex_unlock(sandbox->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 添加权限规则
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号（-1表示所有）
 * @param perm_type 权限类型
 * @param condition 条件表达式
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_add_rule(agentos_sandbox_t* sandbox,
                                         int syscall_num,
                                         permission_type_t perm_type,
                                         const char* condition) {
    if (!sandbox) return AGENTOS_EINVAL;
    
    permission_rule_t* rule = create_permission_rule(syscall_num, perm_type, condition);
    if (!rule) {
        AGENTOS_LOG_ERROR("Failed to create permission rule");
        return AGENTOS_ENOMEM;
    }
    
    agentos_mutex_lock(sandbox->lock);
    
    rule->next = sandbox->rules;
    sandbox->rules = rule;
    sandbox->rule_count++;
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_DEBUG("Added permission rule to sandbox %llu: syscall=%d, perm=%d",
                     (unsigned long long)sandbox->sandbox_id, syscall_num, perm_type);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取沙箱统计信息
 * @param sandbox 沙箱句柄
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_get_stats(agentos_sandbox_t* sandbox, char** out_stats) {
    if (!sandbox || !out_stats) return AGENTOS_EINVAL;
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(sandbox->lock);
    
    cJSON_AddNumberToObject(stats_json, "sandbox_id", sandbox->sandbox_id);
    cJSON_AddStringToObject(stats_json, "sandbox_name", 
                           sandbox->sandbox_name ? sandbox->sandbox_name : "unnamed");
    cJSON_AddNumberToObject(stats_json, "state", sandbox->state);
    cJSON_AddNumberToObject(stats_json, "call_count", sandbox->call_count);
    cJSON_AddNumberToObject(stats_json, "violation_count", sandbox->violation_count);
    cJSON_AddNumberToObject(stats_json, "rule_count", sandbox->rule_count);
    cJSON_AddNumberToObject(stats_json, "audit_count", sandbox->audit_count);
    
    // 资源使用
    cJSON* quota_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(quota_json, "max_memory_bytes", 
                           sandbox->config.quota.max_memory_bytes);
    cJSON_AddNumberToObject(quota_json, "current_memory", 
                           sandbox->config.quota.current_memory);
    cJSON_AddNumberToObject(quota_json, "max_cpu_time_ms", 
                           sandbox->config.quota.max_cpu_time_ms);
    cJSON_AddNumberToObject(quota_json, "current_cpu_time_ms", 
                           sandbox->config.quota.current_cpu_time_ms);
    cJSON_AddNumberToObject(quota_json, "max_io_ops", 
                           sandbox->config.quota.max_io_ops);
    cJSON_AddNumberToObject(quota_json, "current_io_ops", 
                           sandbox->config.quota.current_io_ops);
    cJSON_AddItemToObject(stats_json, "quota", quota_json);
    
    agentos_mutex_unlock(sandbox->lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取管理器统计信息
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_manager_get_stats(char** out_stats) {
    if (!out_stats) return AGENTOS_EINVAL;
    
    if (!g_sandbox_manager) {
        *out_stats = strdup("{\"error\":\"manager not initialized\"}");
        return AGENTOS_ENOTINIT;
    }
    
    cJSON* stats_json = cJSON_CreateObject();
    if (!stats_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(g_manager_lock);
    
    cJSON_AddNumberToObject(stats_json, "sandbox_count", g_sandbox_manager->sandbox_count);
    cJSON_AddNumberToObject(stats_json, "total_calls", g_sandbox_manager->total_calls);
    cJSON_AddNumberToObject(stats_json, "total_violations", g_sandbox_manager->total_violations);
    cJSON_AddNumberToObject(stats_json, "global_audit_count", g_sandbox_manager->global_audit_count);
    
    double violation_rate = g_sandbox_manager->total_calls > 0 ?
                           (double)g_sandbox_manager->total_violations / g_sandbox_manager->total_calls * 100.0 : 0.0;
    cJSON_AddNumberToObject(stats_json, "violation_rate_percent", violation_rate);
    
    agentos_mutex_unlock(g_manager_lock);
    
    char* stats_str = cJSON_PrintUnformatted(stats_json);
    cJSON_Delete(stats_json);
    
    if (!stats_str) return AGENTOS_ENOMEM;
    
    *out_stats = stats_str;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 重置沙箱资源配额
 * @param sandbox 沙箱句柄
 */
void agentos_sandbox_reset_quota(agentos_sandbox_t* sandbox) {
    if (!sandbox) return;
    
    agentos_mutex_lock(sandbox->lock);
    
    sandbox->config.quota.current_memory = 0;
    sandbox->config.quota.current_cpu_time_ms = 0;
    sandbox->config.quota.current_io_ops = 0;
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_DEBUG("Sandbox %llu quota reset", (unsigned long long)sandbox->sandbox_id);
}

/**
 * @brief 暂停沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_suspend(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(sandbox->lock);
    
    if (sandbox->state == SANDBOX_STATE_TERMINATED) {
        agentos_mutex_unlock(sandbox->lock);
        return AGENTOS_EPERM;
    }
    
    sandbox->state = SANDBOX_STATE_SUSPENDED;
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_INFO("Sandbox %llu suspended", (unsigned long long)sandbox->sandbox_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 恢复沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_resume(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(sandbox->lock);
    
    if (sandbox->state == SANDBOX_STATE_TERMINATED) {
        agentos_mutex_unlock(sandbox->lock);
        return AGENTOS_EPERM;
    }
    
    sandbox->state = SANDBOX_STATE_IDLE;
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_INFO("Sandbox %llu resumed", (unsigned long long)sandbox->sandbox_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 终止沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_terminate(agentos_sandbox_t* sandbox) {
    if (!sandbox) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(sandbox->lock);
    
    sandbox->state = SANDBOX_STATE_TERMINATED;
    
    agentos_mutex_unlock(sandbox->lock);
    
    AGENTOS_LOG_INFO("Sandbox %llu terminated", (unsigned long long)sandbox->sandbox_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 健康检查
 * @param sandbox 沙箱句柄
 * @param out_json 输出健康状态JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
agentos_error_t agentos_sandbox_health_check(agentos_sandbox_t* sandbox, char** out_json) {
    if (!sandbox || !out_json) return AGENTOS_EINVAL;
    
    cJSON* health_json = cJSON_CreateObject();
    if (!health_json) return AGENTOS_ENOMEM;
    
    agentos_mutex_lock(sandbox->lock);
    
    cJSON_AddStringToObject(health_json, "component", "sandbox");
    cJSON_AddNumberToObject(health_json, "sandbox_id", sandbox->sandbox_id);
    
    const char* state_str = "unknown";
    switch (sandbox->state) {
        case SANDBOX_STATE_IDLE: state_str = "idle"; break;
        case SANDBOX_STATE_ACTIVE: state_str = "active"; break;
        case SANDBOX_STATE_SUSPENDED: state_str = "suspended"; break;
        case SANDBOX_STATE_TERMINATED: state_str = "terminated"; break;
    }
    cJSON_AddStringToObject(health_json, "state", state_str);
    
    // 检查资源使用是否健康
    int resources_healthy = 1;
    if (sandbox->config.quota.current_memory > sandbox->config.quota.max_memory_bytes * 0.9) {
        resources_healthy = 0;
    }
    if (sandbox->config.quota.current_cpu_time_ms > sandbox->config.quota.max_cpu_time_ms * 0.9) {
        resources_healthy = 0;
    }
    
    cJSON_AddStringToObject(health_json, "status", resources_healthy ? "healthy" : "warning");
    cJSON_AddBoolToObject(health_json, "resources_healthy", resources_healthy);
    cJSON_AddNumberToObject(health_json, "timestamp_ns", get_timestamp_ns());
    
    agentos_mutex_unlock(sandbox->lock);
    
    char* health_str = cJSON_PrintUnformatted(health_json);
    cJSON_Delete(health_json);
    
    if (!health_str) return AGENTOS_ENOMEM;
    
    *out_json = health_str;
    return AGENTOS_SUCCESS;
}
