/**
 * @file sandbox_permission.h
 * @brief 沙箱权限规则管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SANDBOX_PERMISSION_H
#define AGENTOS_SANDBOX_PERMISSION_H

#include "agentos.h"
#include <stdint.h>

/* 前向声明 */
typedef struct agentos_sandbox agentos_sandbox_t;

/**
 * @brief 权限类型枚举
 */
typedef enum {
    PERM_ALLOW = 0,    /**< 允许 */
    PERM_DENY,         /**< 拒绝 */
    PERM_ASK           /**< 需确认 */
} permission_type_t;

/**
 * @brief 权限规则结构
 */
typedef struct permission_rule {
    int syscall_num;              /**< 系统调用号 */
    permission_type_t perm_type;  /**< 权限类型 */
    char* condition;              /**< 条件表达式（JSON 格式） */
    uint32_t flags;               /**< 标志位 */
    struct permission_rule* next; /**< 下一个规则 */
} permission_rule_t;

/**
 * @brief 创建权限规则
 * @param syscall_num 系统调用号
 * @param perm_type 权限类型
 * @param condition 条件表达式
 * @return 规则对象，失败返回 NULL
 */
permission_rule_t* sandbox_permission_create(int syscall_num,
                                            permission_type_t perm_type,
                                            const char* condition);

/**
 * @brief 释放权限规则
 * @param rule 规则对象
 */
void sandbox_permission_destroy(permission_rule_t* rule);

/**
 * @brief 释放权限规则链表
 * @param head 链表头
 */
void sandbox_permission_destroy_all(permission_rule_t* head);

/**
 * @brief 添加权限规则到沙箱
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param perm_type 权限类型
 * @param condition 条件表达式
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
agentos_error_t sandbox_permission_add(agentos_sandbox_t* sandbox, int syscall_num,
                                      permission_type_t perm_type, const char* condition);

/**
 * @brief 检查权限
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param args 参数
 * @param argc 参数数量
 * @return 权限类型
 */
permission_type_t sandbox_permission_check(agentos_sandbox_t* sandbox,
                                          int syscall_num,
                                          void** args, int argc);

/**
 * @brief 检查沙箱是否具有指定能力
 * @param sandbox 沙箱句柄
 * @param capability_id 能力ID（系统调用号）
 * @param resource 目标资源标识
 * @return 1=有能力，0=无能力
 */
int agentos_sandbox_capability_check(agentos_sandbox_t* sandbox,
                                     int capability_id,
                                     const char* resource);

/**
 * @brief 验证系统调用参数安全性
 * @param syscall_num 系统调用号
 * @param args 参数数组
 * @param argc 参数数量
 * @return AGENTOS_SUCCESS 安全，其他为错误码
 */
agentos_error_t agentos_sandbox_validate_syscall(int syscall_num,
                                                  void** args,
                                                  int argc);

#endif /* AGENTOS_SANDBOX_PERMISSION_H */
