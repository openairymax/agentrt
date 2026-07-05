/**
 * @file sandbox_permission.c
 * @brief 沙箱权限规则管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sandbox_permission.h"

#include "agentrt.h"
#include "logger.h"
#include "sandbox_internal.h"

/* 基础库兼容性层 */
#include "memory_compat.h"
#include "string_compat.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/* ==================== 权限规则管理 ==================== */

permission_rule_t *sandbox_permission_create(int syscall_num, permission_type_t perm_type,
                                             const char *condition)
{
    permission_rule_t *rule = (permission_rule_t *)AGENTRT_CALLOC(1, sizeof(permission_rule_t));
    if (!rule) return NULL;

    rule->syscall_num = syscall_num;
    rule->perm_type = perm_type;
    rule->condition = condition ? AGENTRT_STRDUP(condition) : NULL;
    rule->flags = 0;
    rule->next = NULL;

    return rule;
}

void sandbox_permission_destroy(permission_rule_t *rule)
{
    if (!rule)
        return;
    if (rule->condition)
        AGENTRT_FREE(rule->condition);
    AGENTRT_FREE(rule);
}

void sandbox_permission_destroy_all(permission_rule_t *head)
{
    while (head) {
        permission_rule_t *next = head->next;
        sandbox_permission_destroy(head);
        head = next;
    }
}

agentrt_error_t sandbox_permission_add(agentrt_sandbox_t *sandbox, int syscall_num,
                                       permission_type_t perm_type, const char *condition)
{
    if (!sandbox)
        ATM_RET_ERR(AGENTRT_EINVAL);

    permission_rule_t *new_rule = sandbox_permission_create(syscall_num, perm_type, condition);
    if (!new_rule) {
        AGENTRT_LOG_ERROR("Failed to create permission rule");
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    agentrt_mutex_lock(sandbox->lock);

    /* 添加到规则链表头部 */
    new_rule->next = sandbox->rules;
    sandbox->rules = new_rule;
    sandbox->rule_count++;

    agentrt_mutex_unlock(sandbox->lock);

    AGENTRT_LOG_INFO("Added permission rule: syscall=%d, type=%d", syscall_num, perm_type);
    return AGENTRT_SUCCESS;
}

permission_type_t sandbox_permission_check(agentrt_sandbox_t *sandbox, int syscall_num,
                                           void **args __attribute__((unused)),
                                           int argc __attribute__((unused)))
{
    if (!sandbox)
        return PERM_DENY;

    agentrt_mutex_lock(sandbox->lock);

    permission_rule_t *rule = sandbox->rules;
    permission_type_t result = PERM_ALLOW; /* 默认允许 */

    while (rule) {
        if (rule->syscall_num == syscall_num || rule->syscall_num == -1) {
            /* 匹配系统调用号（-1 表示所有调用） */
            if (rule->perm_type == PERM_DENY) {
                /* 拒绝规则优先 */
                result = PERM_DENY;
                break;
            } else if (rule->perm_type == PERM_ASK) {
                result = PERM_ASK;
            }
        }
        rule = rule->next;
    }

    agentrt_mutex_unlock(sandbox->lock);

    return result;
}
