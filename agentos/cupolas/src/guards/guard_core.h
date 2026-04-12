// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file guard_core.h
 * @brief SafetyGuard Core Framework
 * 
 * 安全守卫框架核心定义，提供统一的安全检测接口和守卫管理器。
 * 支持多种守卫类型（规则、模型、行为分析等）和优先级调度。
 */

#ifndef CUPOLAS_GUARD_CORE_H
#define CUPOLAS_GUARD_CORE_H

#include "../../include/cupolas.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 常量定义
// ============================================================================

/** @brief 最大守卫名称长度 */
#define GUARD_NAME_MAX_LEN 64

/** @brief 最大守卫描述长度 */
#define GUARD_DESC_MAX_LEN 256

/** @brief 最大守卫规则数 */
#define GUARD_MAX_RULES 1024

/** @brief 守卫ID类型 */
typedef uint64_t guard_id_t;

/** @brief 守卫类型枚举 */
typedef enum {
    GUARD_TYPE_RULE_BASED = 0,      /**< 基于规则的守卫 */
    GUARD_TYPE_MODEL_BASED,         /**< 基于模型的守卫 */
    GUARD_TYPE_BEHAVIORAL,          /**< 基于行为分析的守卫 */
    GUARD_TYPE_HEURISTIC,           /**< 基于启发式的守卫 */
    GUARD_TYPE_EXTERNAL,            /**< 外部守卫（插件） */
    GUARD_TYPE_COMPOSITE,           /**< 复合守卫（多个守卫组合） */
    GUARD_TYPE_CUSTOM               /**< 自定义守卫 */
} guard_type_t;

/** @brief 安全风险等级枚举 */
typedef enum {
    RISK_LEVEL_SAFE = 0,            /**< 安全，无风险 */
    RISK_LEVEL_INFO,                /**< 信息级，低风险 */
    RISK_LEVEL_LOW,                 /**< 低风险 */
    RISK_LEVEL_MEDIUM,              /**< 中风险 */
    RISK_LEVEL_HIGH,                /**< 高风险 */
    RISK_LEVEL_CRITICAL,            /**< 严重风险 */
    RISK_LEVEL_MAX
} risk_level_t;

/** @brief 守卫动作枚举 */
typedef enum {
    GUARD_ACTION_ALLOW = 0,         /**< 允许，仅记录 */
    GUARD_ACTION_WARN,              /**< 警告，记录并警告 */
    GUARD_ACTION_BLOCK,             /**< 阻断，阻止操作 */
    GUARD_ACTION_ISOLATE,           /**< 隔离，隔离相关资源 */
    GUARD_ACTION_TERMINATE,         /**< 终止，终止相关进程 */
    GUARD_ACTION_ESCALATE           /**< 升级，上报更高级别守卫 */
} guard_action_t;

/** @brief 守卫优先级枚举 */
typedef enum {
    GUARD_PRIORITY_LOWEST = 0,      /**< 最低优先级 */
    GUARD_PRIORITY_LOW,             /**< 低优先级 */
    GUARD_PRIORITY_NORMAL,          /**< 普通优先级 */
    GUARD_PRIORITY_HIGH,            /**< 高优先级 */
    GUARD_PRIORITY_HIGHEST,         /**< 最高优先级 */
    GUARD_PRIORITY_CRITICAL         /**< 关键优先级（系统级） */
} guard_priority_t;

/** @brief 守卫状态枚举 */
typedef enum {
    GUARD_STATE_DISABLED = 0,       /**< 禁用 */
    GUARD_STATE_ENABLED,            /**< 启用 */
    GUARD_STATE_ACTIVE,             /**< 激活（正在检测） */
    GUARD_STATE_ERROR,              /**< 错误状态 */
    GUARD_STATE_UPDATING            /**< 更新中（规则/模型更新） */
} guard_state_t;

// ============================================================================
// 核心数据结构
// ============================================================================

/** @brief 安全检测上下文 */
typedef struct {
    const char* operation;          /**< 操作名称 */
    const char* resource;           /**< 资源标识 */
    const char* agent_id;           /**< 代理ID */
    const char* session_id;         /**< 会话ID */
    void* input_data;               /**< 输入数据 */
    size_t input_size;              /**< 输入数据大小 */
    void* context_data;             /**< 上下文数据（用户定义） */
    uint64_t timestamp;             /**< 时间戳 */
} guard_context_t;

/** @brief 安全检测结果 */
typedef struct {
    risk_level_t risk_level;        /**< 风险等级 */
    guard_action_t recommended_action; /**< 推荐动作 */
    const char* risk_type;          /**< 风险类型 */
    const char* description;        /**< 风险描述 */
    float confidence;               /**< 置信度 (0.0-1.0) */
    void* evidence;                 /**< 证据数据 */
    size_t evidence_size;           /**< 证据数据大小 */
    uint64_t detection_time;        /**< 检测时间（纳秒） */
} guard_result_t;

/** @brief 守卫规则定义 */
typedef struct {
    const char* rule_id;            /**< 规则ID */
    const char* pattern;            /**< 匹配模式 */
    risk_level_t risk_level;        /**< 触发的风险等级 */
    guard_action_t action;          /**< 触发动作 */
    bool case_sensitive;            /**< 是否大小写敏感 */
    void* user_data;                /**< 用户数据 */
} guard_rule_t;

/** @brief 守卫配置 */
typedef struct {
    guard_type_t guard_type;        /**< 守卫类型 */
    guard_priority_t priority;      /**< 守卫优先级 */
    size_t max_rules;               /**< 最大规则数 */
    bool enable_logging;            /**< 是否启用日志 */
    bool enable_metrics;            /**< 是否启用指标收集 */
    size_t cache_size;              /**< 缓存大小 */
    uint32_t timeout_ms;            /**< 检测超时时间（毫秒） */
    void* custom_config;            /**< 自定义配置数据 */
    size_t custom_config_size;      /**< 自定义配置大小 */
} guard_config_t;

/** @brief 守卫统计信息 */
typedef struct {
    uint64_t total_checks;          /**< 总检测次数 */
    uint64_t safe_checks;           /**< 安全检测次数 */
    uint64_t risky_checks;          /**< 风险检测次数 */
    uint64_t blocked_operations;    /**< 阻断操作次数 */
    uint64_t warning_operations;    /**< 警告操作次数 */
    uint64_t false_positives;       /**< 误报次数 */
    uint64_t false_negatives;       /**< 漏报次数 */
    uint64_t total_detection_time;  /**< 总检测时间（纳秒） */
    uint64_t max_detection_time;    /**< 最大单次检测时间（纳秒） */
} guard_stats_t;

/** @brief 守卫操作函数表 */
typedef struct guard_ops {
    /** 初始化守卫 */
    int (*init)(void* guard, const guard_config_t* config);
    
    /** 清理守卫 */
    void (*cleanup)(void* guard);
    
    /** 执行安全检测 */
    int (*check)(void* guard, const guard_context_t* context, guard_result_t* result);
    
    /** 更新规则 */
    int (*update_rules)(void* guard, const guard_rule_t* rules, size_t count);
    
    /** 获取统计信息 */
    int (*get_stats)(void* guard, guard_stats_t* stats);
    
    /** 重置统计信息 */
    int (*reset_stats)(void* guard);
    
    /** 守卫自检 */
    int (*self_test)(void* guard);
} guard_ops_t;

/** @brief 守卫实例结构 */
typedef struct guard {
    guard_id_t id;                  /**< 守卫唯一ID */
    char name[GUARD_NAME_MAX_LEN];  /**< 守卫名称 */
    char description[GUARD_DESC_MAX_LEN]; /**< 守卫描述 */
    guard_type_t type;              /**< 守卫类型 */
    guard_priority_t priority;      /**< 守卫优先级 */
    guard_state_t state;            /**< 守卫状态 */
    guard_config_t config;          /**< 守卫配置 */
    guard_ops_t* ops;               /**< 守卫操作函数表 */
    void* priv_data;                /**< 私有数据 */
    guard_stats_t stats;            /**< 统计信息 */
    uint64_t created_time;          /**< 创建时间 */
    uint64_t last_used_time;        /**< 最后使用时间 */
} guard_t;

/** @brief 守卫管理器配置 */
typedef struct {
    size_t max_guards;              /**< 最大守卫数量 */
    size_t max_pending_checks;      /**< 最大待处理检测数 */
    bool enable_priority_scheduling; /**< 是否启用优先级调度 */
    bool enable_result_caching;     /**< 是否启用结果缓存 */
    size_t cache_ttl_seconds;       /**< 缓存TTL（秒） */
    uint32_t default_timeout_ms;    /**< 默认超时时间（毫秒） */
} guard_manager_config_t;

/** @brief 守卫管理器上下文 */
typedef struct guard_manager guard_manager_t;

// ============================================================================
// 核心API函数
// ============================================================================

/**
 * @brief 创建守卫管理器
 * @param config 管理器配置
 * @return 守卫管理器句柄，失败返回NULL
 */
CUPOLAS_API guard_manager_t* guard_manager_create(const guard_manager_config_t* config);

/**
 * @brief 销毁守卫管理器
 * @param manager 守卫管理器句柄
 */
CUPOLAS_API void guard_manager_destroy(guard_manager_t* manager);

/**
 * @brief 注册守卫到管理器
 * @param manager 守卫管理器
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int guard_manager_register_guard(guard_manager_t* manager, guard_t* guard);

/**
 * @brief 从管理器注销守卫
 * @param manager 守卫管理器
 * @param guard_id 守卫ID
 * @return 错误码
 */
CUPOLAS_API int guard_manager_unregister_guard(guard_manager_t* manager, guard_id_t guard_id);

/**
 * @brief 根据名称查找守卫
 * @param manager 守卫管理器
 * @param name 守卫名称
 * @return 守卫实例，未找到返回NULL
 */
CUPOLAS_API guard_t* guard_manager_find_guard_by_name(guard_manager_t* manager, const char* name);

/**
 * @brief 根据ID查找守卫
 * @param manager 守卫管理器
 * @param id 守卫ID
 * @return 守卫实例，未找到返回NULL
 */
CUPOLAS_API guard_t* guard_manager_find_guard_by_id(guard_manager_t* manager, guard_id_t id);

/**
 * @brief 执行安全检测（同步）
 * @param manager 守卫管理器
 * @param context 检测上下文
 * @param results 结果数组（输出）
 * @param max_results 最大结果数
 * @param actual_results 实际结果数（输出）
 * @return 错误码
 */
CUPOLAS_API int guard_manager_check_sync(
    guard_manager_t* manager,
    const guard_context_t* context,
    guard_result_t* results,
    size_t max_results,
    size_t* actual_results);

/**
 * @brief 执行安全检测（异步）
 * @param manager 守卫管理器
 * @param context 检测上下文
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 请求ID，失败返回0
 */
CUPOLAS_API uint64_t guard_manager_check_async(
    guard_manager_t* manager,
    const guard_context_t* context,
    void (*callback)(uint64_t request_id, const guard_result_t* results, 
                     size_t count, void* user_data),
    void* user_data);

/**
 * @brief 获取管理器统计信息
 * @param manager 守卫管理器
 * @param stats 统计信息（输出）
 * @return 错误码
 */
CUPOLAS_API int guard_manager_get_stats(guard_manager_t* manager, guard_stats_t* stats);

/**
 * @brief 重置管理器统计信息
 * @param manager 守卫管理器
 * @return 错误码
 */
CUPOLAS_API int guard_manager_reset_stats(guard_manager_t* manager);

// ============================================================================
// 守卫实例管理函数
// ============================================================================

/**
 * @brief 创建守卫实例
 * @param name 守卫名称
 * @param description 守卫描述
 * @param type 守卫类型
 * @param ops 守卫操作函数表
 * @return 守卫实例，失败返回NULL
 */
CUPOLAS_API guard_t* guard_create(
    const char* name,
    const char* description,
    guard_type_t type,
    const guard_ops_t* ops);

/**
 * @brief 销毁守卫实例
 * @param guard 守卫实例
 */
CUPOLAS_API void guard_destroy(guard_t* guard);

/**
 * @brief 初始化守卫
 * @param guard 守卫实例
 * @param config 守卫配置
 * @return 错误码
 */
CUPOLAS_API int guard_init(guard_t* guard, const guard_config_t* config);

/**
 * @brief 执行守卫检测
 * @param guard 守卫实例
 * @param context 检测上下文
 * @param result 检测结果（输出）
 * @return 错误码
 */
CUPOLAS_API int guard_check(guard_t* guard, const guard_context_t* context, guard_result_t* result);

/**
 * @brief 启用守卫
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int guard_enable(guard_t* guard);

/**
 * @brief 禁用守卫
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int guard_disable(guard_t* guard);

/**
 * @brief 更新守卫规则
 * @param guard 守卫实例
 * @param rules 规则数组
 * @param count 规则数量
 * @return 错误码
 */
CUPOLAS_API int guard_update_rules(guard_t* guard, const guard_rule_t* rules, size_t count);

/**
 * @brief 获取守卫统计信息
 * @param guard 守卫实例
 * @param stats 统计信息（输出）
 * @return 错误码
 */
CUPOLAS_API int guard_get_stats(guard_t* guard, guard_stats_t* stats);

/**
 * @brief 重置守卫统计信息
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int guard_reset_stats(guard_t* guard);

#ifdef __cplusplus
}
#endif

#endif // CUPOLAS_GUARD_CORE_H