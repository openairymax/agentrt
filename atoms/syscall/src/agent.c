/**
 * @file agent.c
 * @brief Agent相关系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
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

    agent_instance_t* inst = (agent_instance_t*)calloc(1, sizeof(agent_instance_t));
    if (!inst) return AGENTOS_ENOMEM;

    inst->agent_id = strdup(id_buf);
    inst->spec = strdup(agent_spec);
    if (!inst->agent_id || !inst->spec) {
        if (inst->agent_id) free(inst->agent_id);
        if (inst->spec) free(inst->spec);
        free(inst);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(agent_lock);
    inst->next = agents;
    agents = inst;
    agentos_mutex_unlock(agent_lock);

    *out_agent_id = strdup(inst->agent_id);
    if (!*out_agent_id) {
        agentos_mutex_lock(agent_lock);
        agent_instance_t** pp = &agents;
        while (*pp) {
            if (*pp == inst) { *pp = inst->next; break; }
            pp = &(*pp)->next;
        }
        agentos_mutex_unlock(agent_lock);
        free(inst->agent_id);
        free(inst->spec);
        free(inst);
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁 Agent 实例
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
            free(tmp->agent_id);
            free(tmp->spec);
            free(tmp);
            agentos_mutex_unlock(agent_lock);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(agent_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 列出所有 Agent 实例
 */
agentos_error_t agentos_sys_agent_list(char*** out_agents, size_t* out_count) {
    if (!out_agents || !out_count) return AGENTOS_EINVAL;
    ensure_agent_lock();
    agentos_mutex_lock(agent_lock);
    size_t count = 0;
    agent_instance_t* a = agents;
    while (a) { count++; a = a->next; }
    char** list = (char**)calloc(count, sizeof(char*));
    if (!list) {
        agentos_mutex_unlock(agent_lock);
        return AGENTOS_ENOMEM;
    }
    a = agents;
    size_t i = 0;
    while (a) {
        list[i] = strdup(a->agent_id);
        if (!list[i]) {
            for (size_t j = 0; j < i; j++) free(list[j]);
            free(list);
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
