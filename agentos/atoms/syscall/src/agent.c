/**
 * @file agent.c
 * @brief Agent 相关系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>

typedef struct agent_instance {
    char* agent_id;
    char* spec;
    struct agent_instance* next;
} agent_instance_t;

static agent_instance_t* agents = NULL;
static agentos_mutex_t* agent_lock = NULL;

/**
 * @brief 线程安全地确保 agent 锁已初始化
 */
static void ensure_agent_lock(void) {
    agentos_mutex_t* current = __atomic_load_n(&agent_lock, __ATOMIC_ACQUIRE);
    if (!current) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return;
        agentos_mutex_t* expected = NULL;
        if (!__atomic_compare_exchange_n(&agent_lock, &expected, new_lock,
                                          false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            agentos_mutex_destroy(new_lock);
        }
    }
}

/**
 * @brief 创建 Agent 实例
 */
agentos_error_t agentos_sys_agent_spawn(const char* agent_spec, char** out_agent_id) {
    if (!agent_spec || !out_agent_id) return AGENTOS_EINVAL;
    ensure_agent_lock();

    char id_buf[64];
    static int counter = 0;
    snprintf(id_buf, sizeof(id_buf), "agent_%d", __sync_fetch_and_add(&counter, 1));

    agent_instance_t* inst = (agent_instance_t*)AGENTOS_CALLOC(1, sizeof(agent_instance_t));
    if (!inst) return AGENTOS_ENOMEM;

    inst->agent_id = AGENTOS_STRDUP(id_buf);
    inst->spec = AGENTOS_STRDUP(agent_spec);
    if (!inst->agent_id || !inst->spec) {
        if (inst->agent_id) AGENTOS_FREE(inst->agent_id);
        if (inst->spec) AGENTOS_FREE(inst->spec);
        AGENTOS_FREE(inst);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(agent_lock);
    inst->next = agents;
    agents = inst;
    agentos_mutex_unlock(agent_lock);

    *out_agent_id = AGENTOS_STRDUP(inst->agent_id);
    if (!*out_agent_id) {
        return AGENTOS_ENOMEM;
    }

    AGENTOS_LOG_INFO("Agent spawned: %s", *out_agent_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁 Agent 实例
 */
agentos_error_t agentos_sys_agent_terminate(const char* agent_id) {
    if (!agent_id) return AGENTOS_EINVAL;
    ensure_agent_lock();

    agentos_mutex_lock(agent_lock);
    agent_instance_t** prev = &agents;
    agent_instance_t* curr = agents;

    while (curr) {
        if (strcmp(curr->agent_id, agent_id) == 0) {
            *prev = curr->next;
            AGENTOS_FREE(curr->agent_id);
            AGENTOS_FREE(curr->spec);
            AGENTOS_FREE(curr);
            agentos_mutex_unlock(agent_lock);
            AGENTOS_LOG_INFO("Agent terminated: %s", agent_id);
            return AGENTOS_SUCCESS;
        }
        prev = &curr->next;
        curr = curr->next;
    }

    agentos_mutex_unlock(agent_lock);
    AGENTOS_LOG_WARN("Agent not found: %s", agent_id);
    return AGENTOS_ENOTFOUND;
}

/**
 * @brief 调用 Agent 执行任务
 */
agentos_error_t agentos_sys_agent_invoke(const char* agent_id, const char* input,
                                         size_t input_len, char** out_output) {
    if (!agent_id || !input || !out_output) return AGENTOS_EINVAL;
    ensure_agent_lock();

    // 查找 Agent
    agentos_mutex_lock(agent_lock);
    agent_instance_t* inst = agents;
    while (inst) {
        if (strcmp(inst->agent_id, agent_id) == 0) break;
        inst = inst->next;
    }
    agentos_mutex_unlock(agent_lock);

    if (!inst) {
        AGENTOS_LOG_WARN("Agent not found: %s", agent_id);
        return AGENTOS_ENOTFOUND;
    }

    // 简单实现：回显输入
    // 生产环境应调用 Agent 的实际执行逻辑
    *out_output = AGENTOS_STRDUP(input);
    if (!*out_output) return AGENTOS_ENOMEM;

    AGENTOS_LOG_DEBUG("Agent invoked: %s", agent_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 列出所有 Agent
 */
agentos_error_t agentos_sys_agent_list(char*** out_agent_ids, size_t* out_count) {
    if (!out_agent_ids || !out_count) return AGENTOS_EINVAL;
    ensure_agent_lock();

    agentos_mutex_lock(agent_lock);

    // 计数
    size_t count = 0;
    agent_instance_t* inst = agents;
    while (inst) {
        count++;
        inst = inst->next;
    }

    if (count == 0) {
        agentos_mutex_unlock(agent_lock);
        *out_agent_ids = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }

    // 分配数组
    char** ids = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!ids) {
        agentos_mutex_unlock(agent_lock);
        return AGENTOS_ENOMEM;
    }

    // 填充数组
    inst = agents;
    for (size_t i = 0; i < count; i++) {
        ids[i] = AGENTOS_STRDUP(inst->agent_id);
        if (!ids[i]) {
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE(ids[j]);
            }
            AGENTOS_FREE(ids);
            agentos_mutex_unlock(agent_lock);
            return AGENTOS_ENOMEM;
        }
        inst = inst->next;
    }

    agentos_mutex_unlock(agent_lock);

    *out_agent_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理 Agent 系统调用资源
 */
void agentos_sys_agent_cleanup(void) {
    if (!agent_lock) return;

    agentos_mutex_lock(agent_lock);
    agent_instance_t* inst = agents;
    while (inst) {
        agent_instance_t* next = inst->next;
        AGENTOS_FREE(inst->agent_id);
        AGENTOS_FREE(inst->spec);
        AGENTOS_FREE(inst);
        inst = next;
    }
    agents = NULL;
    agentos_mutex_unlock(agent_lock);

    agentos_mutex_destroy(agent_lock);
    agent_lock = NULL;

    AGENTOS_LOG_INFO("Agent syscall cleanup completed");
}
