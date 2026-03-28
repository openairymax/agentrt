/**
 * @file domes_runtime_protection.h
 * @brief 增强运行时保护 - seccomp、CFI 等多层防护
 * @author Spharx
 * @date 2026
 *
 * 设计原则：
 * - 多层防护：内存保护、控制流保护、系统调用过滤
 * - 零信任：所有操作都必须经过验证
 * - 防篡改：运行时完整性检查
 * - 最小权限：只允许必要的系统调用
 */

#ifndef DOMES_RUNTIME_PROTECTION_H
#define DOMES_RUNTIME_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 保护级别
 */
typedef enum {
    DOMES_PROTECT_NONE = 0,         /**< 无保护 */
    DOMES_PROTECT_BASIC = 1,        /**< 基础保护 */
    DOMES_PROTECT_ENHANCED = 2,     /**< 增强保护 */
    DOMES_PROTECT_MAXIMUM = 3       /**< 最高保护 */
} domes_protection_level_t;

/**
 * @brief 保护状态
 */
typedef enum {
    DOMES_PROTECT_STATUS_INACTIVE = 0,
    DOMES_PROTECT_STATUS_ACTIVE = 1,
    DOMES_PROTECT_STATUS_VIOLATION = 2,
    DOMES_PROTECT_STATUS_COMPROMISED = 3
} domes_protection_status_t;

/**
 * @brief 违规类型
 */
typedef enum {
    DOMES_VIOLATION_NONE = 0,
    DOMES_VIOLATION_SYSCALL = 1,        /**< 非法系统调用 */
    DOMES_VIOLATION_MEMORY = 2,         /**< 内存访问违规 */
    DOMES_VIOLATION_CONTROL_FLOW = 3,   /**< 控制流违规 */
    DOMES_VIOLATION_INTEGRITY = 4,      /**< 完整性违规 */
    DOMES_VIOLATION_RESOURCE = 5        /**< 资源超限 */
} domes_violation_type_t;

/**
 * @brief 内存保护配置
 */
typedef struct {
    bool enable_aslr;               /**< 地址空间随机化 */
    bool enable_dep;                /**< 数据执行保护 (NX bit) */
    bool enable_stack_protector;    /**< 栈保护 (Stack Canary) */
    bool enable_heap_guard;         /**< 堆保护 */
    bool enable_mprotect;           /**< 内存页保护 */
    bool enable_guard_pages;        /**< 保护页 */
    uint32_t stack_canary_type;     /**< 栈 Canary 类型 */
} domes_memory_protect_config_t;

/**
 * @brief 控制流保护配置
 */
typedef struct {
    bool enable_cfi;                /**< 控制流完整性 */
    bool enable_safestack;          /**< 安全栈 */
    bool enable_shadow_stack;       /**< 影子栈 */
    bool enable_ibt;                /**< 间接分支追踪 */
    bool enable_cet;                /**< 控制流执行技术 */
    uint32_t cfi_level;             /**< CFI 级别 (1-3) */
} domes_cfi_config_t;

/**
 * @brief 系统调用过滤配置
 */
typedef struct {
    bool enable_seccomp;            /**< 启用 seccomp */
    bool enable_seccomp_bpf;        /**< 启用 BPF 过滤 */
    int default_action;             /**< 默认动作 (允许/拒绝/杀死) */
    const char** allowed_syscalls;  /**< 允许的系统调用列表 */
    size_t syscall_count;           /**< 系统调用数量 */
    const char** log_syscalls;      /**< 需要记录的系统调用 */
    size_t log_count;               /**< 记录数量 */
} domes_seccomp_config_t;

/**
 * @brief 完整性检查配置
 */
typedef struct {
    bool enable_code_integrity;     /**< 代码完整性检查 */
    bool enable_data_integrity;     /**< 数据完整性检查 */
    bool enable_ro_sections;        /**< 只读段保护 */
    bool enable_self_check;         /**< 自检 */
    uint32_t check_interval_ms;     /**< 检查间隔 (毫秒) */
    uint32_t hash_algorithm;        /**< 哈希算法 */
} domes_integrity_config_t;

/**
 * @brief 运行时保护完整配置
 */
typedef struct {
    domes_protection_level_t level;
    
    domes_memory_protect_config_t memory;
    domes_cfi_config_t cfi;
    domes_seccomp_config_t seccomp;
    domes_integrity_config_t integrity;
    
    bool enable_audit;              /**< 启用审计 */
    bool enable_violation_handler;  /**< 启用违规处理 */
    void (*violation_callback)(domes_violation_type_t type, const char* details);
} domes_runtime_protect_config_t;

/**
 * @brief 违规事件
 */
typedef struct {
    domes_violation_type_t type;
    uint64_t timestamp;
    uint32_t pid;
    uint32_t tid;
    char* details;
    char* syscall_name;
    void* fault_address;
    int error_code;
} domes_violation_event_t;

/**
 * @brief 保护统计
 */
typedef struct {
    uint64_t total_checks;
    uint64_t violations_detected;
    uint64_t violations_blocked;
    uint64_t integrity_checks;
    uint64_t integrity_failures;
    uint64_t syscall_denied;
    uint64_t memory_violations;
    uint64_t cfi_violations;
} domes_protection_stats_t;

/* ============================================================================
 * 生命周期管理
 * ============================================================================ */

/**
 * @brief 初始化运行时保护模块
 * @param manager 配置参数 (NULL 使用默认配置)
 * @return 0 成功，非0 失败
 */
int domes_runtime_protect_init(const domes_runtime_protect_config_t* manager);

/**
 * @brief 清理运行时保护模块
 */
void domes_runtime_protect_cleanup(void);

/**
 * @brief 启用运行时保护
 * @param manager 保护配置
 * @return 0 成功，非0 失败
 */
int domes_runtime_protect_enable(const domes_runtime_protect_config_t* manager);

/**
 * @brief 禁用运行时保护
 * @return 0 成功，非0 失败
 */
int domes_runtime_protect_disable(void);

/**
 * @brief 获取保护状态
 * @return 保护状态
 */
domes_protection_status_t domes_runtime_protect_get_status(void);

/**
 * @brief 获取保护配置
 * @param manager 配置输出
 * @return 0 成功，非0 失败
 */
int domes_runtime_protect_get_config(domes_runtime_protect_config_t* manager);

/* ============================================================================
 * 内存保护
 * ============================================================================ */

/**
 * @brief 启用内存保护
 * @param manager 内存保护配置
 * @return 0 成功，非0 失败
 */
int domes_memory_protect_enable(const domes_memory_protect_config_t* manager);

/**
 * @brief 锁定内存页
 * @param addr 内存地址
 * @param len 长度
 * @return 0 成功，非0 失败
 */
int domes_memory_lock(void* addr, size_t len);

/**
 * @brief 解锁内存页
 * @param addr 内存地址
 * @param len 长度
 * @return 0 成功，非0 失败
 */
int domes_memory_unlock(void* addr, size_t len);

/**
 * @brief 设置内存保护
 * @param addr 内存地址
 * @param len 长度
 * @param prot 保护标志 (PROT_READ | PROT_WRITE | PROT_EXEC)
 * @return 0 成功，非0 失败
 */
int domes_memory_protect(void* addr, size_t len, int prot);

/**
 * @brief 分配保护内存
 * @param size 大小
 * @param prot 保护标志
 * @return 内存指针，NULL 失败
 */
void* domes_memory_alloc_protected(size_t size, int prot);

/**
 * @brief 释放保护内存
 * @param ptr 内存指针
 */
void domes_memory_free_protected(void* ptr);

/* ============================================================================
 * 控制流保护
 * ============================================================================ */

/**
 * @brief 启用控制流保护
 * @param manager CFI 配置
 * @return 0 成功，非0 失败
 */
int domes_cfi_enable(const domes_cfi_config_t* manager);

/**
 * @brief 注册有效跳转目标
 * @param source 源地址
 * @param target 目标地址
 * @return 0 成功，非0 失败
 */
int domes_cfi_register_target(void* source, void* target);

/**
 * @brief 验证控制流转移
 * @param source 源地址
 * @param target 目标地址
 * @return 1 有效，0 无效
 */
int domes_cfi_verify_transfer(void* source, void* target);

/**
 * @brief 获取 CFI 统计
 * @param stats 统计输出
 * @return 0 成功，非0 失败
 */
int domes_cfi_get_stats(uint64_t* checks, uint64_t* violations);

/* ============================================================================
 * 系统调用过滤
 * ============================================================================ */

/**
 * @brief 启用系统调用过滤
 * @param manager seccomp 配置
 * @return 0 成功，非0 失败
 */
int domes_seccomp_enable(const domes_seccomp_config_t* manager);

/**
 * @brief 添加允许的系统调用
 * @param syscall_name 系统调用名称
 * @return 0 成功，非0 失败
 */
int domes_seccomp_allow(const char* syscall_name);

/**
 * @brief 添加拒绝的系统调用
 * @param syscall_name 系统调用名称
 * @return 0 成功，非0 失败
 */
int domes_seccomp_deny(const char* syscall_name);

/**
 * @brief 添加条件规则
 * @param syscall_name 系统调用名称
 * @param arg_index 参数索引
 * @param op 操作符 (==, !=, <, >, &)
 * @param value 值
 * @param action 动作
 * @return 0 成功，非0 失败
 */
int domes_seccomp_add_rule(const char* syscall_name, 
                            uint32_t arg_index,
                            const char* op,
                            uint64_t value,
                            int action);

/**
 * @brief 检查系统调用是否允许
 * @param syscall_name 系统调用名称
 * @return 1 允许，0 拒绝
 */
int domes_seccomp_check(const char* syscall_name);

/**
 * @brief 获取 seccomp 统计
 * @param allowed 允许次数输出
 * @param denied 拒绝次数输出
 * @return 0 成功，非0 失败
 */
int domes_seccomp_get_stats(uint64_t* allowed, uint64_t* denied);

/* ============================================================================
 * 完整性检查
 * ============================================================================ */

/**
 * @brief 启用完整性检查
 * @param manager 完整性配置
 * @return 0 成功，非0 失败
 */
int domes_integrity_enable(const domes_integrity_config_t* manager);

/**
 * @brief 执行完整性检查
 * @return 0 完整，非0 被篡改
 */
int domes_integrity_check(void);

/**
 * @brief 计算代码段哈希
 * @param hash_out 哈希输出 (32字节)
 * @return 0 成功，非0 失败
 */
int domes_integrity_compute_code_hash(uint8_t* hash_out);

/**
 * @brief 验证代码段完整性
 * @param expected_hash 预期哈希
 * @return 0 完整，非0 被篡改
 */
int domes_integrity_verify_code(const uint8_t* expected_hash);

/**
 * @brief 验证数据段完整性
 * @param expected_hash 预期哈希
 * @return 0 完整，非0 被篡改
 */
int domes_integrity_verify_data(const uint8_t* expected_hash);

/**
 * @brief 设置完整性检查回调
 * @param callback 回调函数
 * @return 0 成功，非0 失败
 */
int domes_integrity_set_callback(void (*callback)(int result));

/* ============================================================================
 * 违规处理
 * ============================================================================ */

/**
 * @brief 设置违规处理回调
 * @param callback 回调函数
 * @return 0 成功，非0 失败
 */
int domes_violation_set_callback(void (*callback)(const domes_violation_event_t* event));

/**
 * @brief 获取最近的违规事件
 * @param event 事件输出
 * @return 0 成功，非0 无事件
 */
int domes_violation_get_last(domes_violation_event_t* event);

/**
 * @brief 清除违规事件
 */
void domes_violation_clear(void);

/**
 * @brief 获取违规统计
 * @param stats 统计输出
 * @return 0 成功，非0 失败
 */
int domes_violation_get_stats(domes_protection_stats_t* stats);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取保护级别名称
 * @param level 保护级别
 * @return 级别名称字符串
 */
const char* domes_protection_level_string(domes_protection_level_t level);

/**
 * @brief 获取保护状态名称
 * @param status 保护状态
 * @return 状态名称字符串
 */
const char* domes_protection_status_string(domes_protection_status_t status);

/**
 * @brief 获取违规类型名称
 * @param type 违规类型
 * @return 类型名称字符串
 */
const char* domes_violation_type_string(domes_violation_type_t type);

/**
 * @brief 检查当前系统是否支持保护特性
 * @param feature 特性名称 (aslr, dep, cfi, seccomp, etc.)
 * @return true 支持，false 不支持
 */
bool domes_protection_is_supported(const char* feature);

/**
 * @brief 获取系统保护能力
 * @param capabilities 能力列表输出
 * @param count 数量输出
 * @return 0 成功，非0 失败
 */
int domes_protection_get_capabilities(char*** capabilities, size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_RUNTIME_PROTECTION_H */
