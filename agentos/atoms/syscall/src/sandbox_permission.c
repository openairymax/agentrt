/**
 * @file sandbox_permission.c
 * @brief 沙箱权限规则管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sandbox_permission.h"
#include "agentos.h"
#include "logger.h"

/* 基础库兼容性层 */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>

/* 沙箱内部结构 */
struct agentos_sandbox {
    uint64_t sandbox_id;
    char* sandbox_name;
    char* owner_id;
    void* state;
    void* manager;
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
};

/* ==================== 权限规则管理 ==================== */

permission_rule_t* sandbox_permission_create(int syscall_num,
                                            permission_type_t perm_type,
                                            const char* condition) {
    permission_rule_t* rule = (permission_rule_t*)AGENTOS_CALLOC(1, sizeof(permission_rule_t));
    if (!rule) return NULL;

    rule->syscall_num = syscall_num;
    rule->perm_type = perm_type;
    rule->condition = condition ? AGENTOS_STRDUP(condition) : NULL;
    rule->flags = 0;
    rule->next = NULL;

    return rule;
}

void sandbox_permission_destroy(permission_rule_t* rule) {
    if (!rule) return;
    if (rule->condition) AGENTOS_FREE(rule->condition);
    AGENTOS_FREE(rule);
}

void sandbox_permission_destroy_all(permission_rule_t* head) {
    while (head) {
        permission_rule_t* next = head->next;
        sandbox_permission_destroy(head);
        head = next;
    }
}

agentos_error_t sandbox_permission_add(agentos_sandbox_t* sandbox, int syscall_num,
                                      permission_type_t perm_type, const char* condition) {
    if (!sandbox) return AGENTOS_EINVAL;

    permission_rule_t* new_rule = sandbox_permission_create(syscall_num, perm_type, condition);
    if (!new_rule) {
        AGENTOS_LOG_ERROR("Failed to create permission rule");
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(sandbox->lock);

    /* 添加到规则链表头部 */
    new_rule->next = sandbox->rules;
    sandbox->rules = new_rule;
    sandbox->rule_count++;

    agentos_mutex_unlock(sandbox->lock);

    AGENTOS_LOG_INFO("Added permission rule: syscall=%d, type=%d", syscall_num, perm_type);
    return AGENTOS_SUCCESS;
}

permission_type_t sandbox_permission_check(agentos_sandbox_t* sandbox,
                                          int syscall_num,
                                          void** args, int argc) {
    if (!sandbox) return PERM_DENY;

    (void)args;  /* 未使用 */
    (void)argc;  /* 未使用 */

    agentos_mutex_lock(sandbox->lock);

    permission_rule_t* rule = sandbox->rules;
    permission_type_t result = PERM_ALLOW;  /* 默认允许 */

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

    agentos_mutex_unlock(sandbox->lock);

    return result;
}
