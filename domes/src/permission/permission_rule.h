/**
 * @file permission_rule.h
 * @brief 权限规则管理器内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef DOMAIN_PERMISSION_RULE_H
#define DOMAIN_PERMISSION_RULE_H

#include "../platform/platform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 规则结构 */
typedef struct permission_rule {
    char* agent_id;
    char* action;
    char* resource;
    char* resource_pattern;
    int allow;
    int priority;
    struct permission_rule* next;
} permission_rule_t;

/* 规则管理器 */
typedef struct rule_manager {
    permission_rule_t* rules;
    domes_rwlock_t rwlock;
    char* path;
    uint64_t last_mtime;
    domes_atomic32_t version;
} rule_manager_t;

/**
 * @brief 从 YAML 文件加载规则
 * @param path 文件路径
 * @return 管理器句柄，失败返回 NULL
 */
rule_manager_t* rule_manager_create(const char* path);

/**
 * @brief 销毁管理器
 * @param mgr 管理器
 */
void rule_manager_destroy(rule_manager_t* mgr);

/**
 * @brief 匹配规则
 * @param mgr 管理器
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param context 上下文（暂未使用）
 * @return 1 允许，0 拒绝
 */
int rule_manager_match(rule_manager_t* mgr,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context);

/**
 * @brief 重新加载规则文件
 * @param mgr 管理器
 * @return 0 成功，其他失败
 */
int rule_manager_reload(rule_manager_t* mgr);

/**
 * @brief 添加规则
 * @param mgr 管理器
 * @param agent_id Agent ID（NULL 表示通配）
 * @param action 操作（NULL 表示通配）
 * @param resource 资源模式（支持 glob）
 * @param allow 1 允许，0 拒绝
 * @param priority 优先级（数值越大优先级越高）
 * @return 0 成功，其他失败
 */
int rule_manager_add(rule_manager_t* mgr,
                     const char* agent_id,
                     const char* action,
                     const char* resource,
                     int allow,
                     int priority);

/**
 * @brief 清空所有规则
 * @param mgr 管理器
 */
void rule_manager_clear(rule_manager_t* mgr);

/**
 * @brief 获取规则数量
 * @param mgr 管理器
 * @return 规则数量
 */
size_t rule_manager_count(rule_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_PERMISSION_RULE_H */
