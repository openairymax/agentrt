/**
 * @file permission_rule.h
 * @brief 权限规则管理器内部接口
 */
#ifndef DOMAIN_PERMISSION_RULE_H
#define DOMAIN_PERMISSION_RULE_H

#include <regex.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 规则结构 */
typedef struct permission_rule {
    char*   agent_id;    // 可为 NULL 表示通配
    char*   action;      // 可为 NULL 表示通配
    char*   resource;    // 可为 NULL 表示通配，支持 glob 模式
    int     allow;       // 1 允许，0 拒绝
    struct permission_rule* next;
} permission_rule_t;

/* 规则管理器 */
typedef struct rule_manager {
    permission_rule_t*  rules;
    pthread_rwlock_t    rwlock;
    char*               path;
    time_t              last_mtime;
} rule_manager_t;

/**
 * @brief 从 YAML 文件加载规则
 * @param path 文件路径
 * @return 管理器句柄，失败返回 NULL
 */
rule_manager_t* rule_manager_create(const char* path);

/**
 * @brief 销毁管理器
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

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_PERMISSION_RULE_H */