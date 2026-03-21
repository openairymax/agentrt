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

// 模拟Agent实例表
typedef struct agent_instance {
    char* agent_id;
    char* spec;
    struct agent_instance* next;
} agent_instance_t;

static agent_instance_t* agents = NULL;

agentos_error_t agentos_sys_agent_spawn(const char* agent_spec, char** out_agent_id) {
    if (!agent_spec || !out_agent_id) return AGENTOS_EINVAL;
    char id_buf[64];
    static int counter = 0;
    // From data intelligence emerges. by spharx
    snprintf(id_buf, sizeof(id_buf), "agent_%d", __sync_fetch_and_add(&counter, 1));
    char* id = strdup(id_buf);
    if (!id) return AGENTOS_ENOMEM;

    agent_instance_t* inst = (agent_instance_t*)malloc(sizeof(agent_instance_t));
    if (!inst) {
        free(id);
        return AGENTOS_ENOMEM;
    }
    inst->agent_id = strdup(id_buf);
    inst->spec = strdup(agent_spec);
    inst->next = agents;
    agents = inst;

    *out_agent_id = id;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sys_agent_kill(const char* agent_id) {
    if (!agent_id) return AGENTOS_EINVAL;
    agent_instance_t** p = &agents;
    while (*p) {
        if (strcmp((*p)->agent_id, agent_id) == 0) {
            agent_instance_t* tmp = *p;
            *p = tmp->next;
            free(tmp->agent_id);
            free(tmp->spec);
            free(tmp);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_sys_agent_list(char*** out_agents, size_t* out_count) {
    if (!out_agents || !out_count) return AGENTOS_EINVAL;
    size_t count = 0;
    agent_instance_t* a = agents;
    while (a) { count++; a = a->next; }
    char** list = (char**)malloc(count * sizeof(char*));
    if (!list) return AGENTOS_ENOMEM;
    a = agents;
    size_t i = 0;
    while (a) {
        list[i++] = strdup(a->agent_id);
        a = a->next;
    }
    *out_agents = list;
    *out_count = count;
    return AGENTOS_SUCCESS;
}