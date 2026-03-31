/**
 * @file agent.c
 * @brief Agent相关系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>

typedef struct agent_instance {
    char* agent_id;
    char* spec;
    struct agent_instance* next;
} agent_instance_t;

static agent_instance_t* agents = NULL;
static agentos_mutex_t* agent_lock = NULL;

/**
 * @brief 线程安全地确�?agent 锁已初始�?
 */
static void ensure_agent_lock(void) {
    if (!agent_lock) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return;
        if (!__sync_bool_compare_and_swap(&agent_lock, NULL, new_lock)) {
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
        agentos_mutex_lock(agent_lock);
        agent_instance_t** pp = &agents;
        while (*pp) {
            if (*pp == inst) { *pp = inst->next; break; }
            pp = &(*pp)->next;
        }
        agentos_mutex_unlock(agent_lock);
        AGENTOS_FREE(inst->agent_id);
        AGENTOS_FREE(inst->spec);
        AGENTOS_FREE(inst);
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销�?Agent 实例
 */
agentos_error_t agentos_sys_agent_kill(const char* agent_id) {
    if (!agent_id) return AGENTOS_EINVAL;
    ensure_agent_lock();
    agentos_mutex_lock(agent_lock);
    agent_instance_t** p = &agents;
    while (*p) {
        if (strcmp((*p)->agent_id, agent_id) == 0) {
            agent_instance_t* tmp = *p;
            *p = tmp->next;
            AGENTOS_FREE(tmp->agent_id);
            AGENTOS_FREE(tmp->spec);
            AGENTOS_FREE(tmp);
            agentos_mutex_unlock(agent_lock);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(agent_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 列出所�?Agent 实例
 */
agentos_error_t agentos_sys_agent_list(char*** out_agents, size_t* out_count) {
    if (!out_agents || !out_count) return AGENTOS_EINVAL;
    ensure_agent_lock();
    agentos_mutex_lock(agent_lock);
    size_t count = 0;
    agent_instance_t* a = agents;
    while (a) { count++; a = a->next; }
    char** list = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!list) {
        agentos_mutex_unlock(agent_lock);
        return AGENTOS_ENOMEM;
    }
    a = agents;
    size_t i = 0;
    while (a) {
        list[i] = AGENTOS_STRDUP(a->agent_id);
        if (!list[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(list[j]);
            AGENTOS_FREE(list);
            agentos_mutex_unlock(agent_lock);
            return AGENTOS_ENOMEM;
        }
        i++;
        a = a->next;
    }
    agentos_mutex_unlock(agent_lock);
    *out_agents = list;
    *out_count = count;
    return AGENTOS_SUCCESS;
}
