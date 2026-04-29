// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "multi_agent_collaboration.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct mac_framework_s {
    mac_collab_mode_t default_mode;
    mac_agent_info_t agents[MAC_MAX_AGENTS];
    size_t agent_count;
    mac_group_t groups[MAC_MAX_GROUPS];
    size_t group_count;
    mac_collab_task_t tasks[MAC_MAX_TASKS];
    size_t task_count;
    mac_consensus_t consensuses[MAC_MAX_CONSENSUS];
    size_t consensus_count;
    mac_task_delegate_fn delegate_fn;
    void* delegate_user_data;
    mac_result_aggregate_fn aggregate_fn;
    void* aggregate_user_data;
};

static void generate_id(char* buf, size_t buf_size, const char* prefix) {
    snprintf(buf, buf_size, "%s_%lu_%u", prefix, (unsigned long)time(NULL),
             (unsigned int)(rand() % 100000));
}

mac_framework_t* mac_framework_create(mac_collab_mode_t default_mode) {
    mac_framework_t* fw = (mac_framework_t*)calloc(1, sizeof(mac_framework_t));
    if (!fw) return NULL;
    fw->default_mode = default_mode;
    fw->agent_count = 0;
    fw->group_count = 0;
    fw->task_count = 0;
    fw->consensus_count = 0;
    fw->delegate_fn = NULL;
    fw->delegate_user_data = NULL;
    fw->aggregate_fn = NULL;
    fw->aggregate_user_data = NULL;
    return fw;
}

void mac_framework_destroy(mac_framework_t* fw) {
    if (!fw) return;
    for (size_t i = 0; i < fw->agent_count; i++) {
        free(fw->agents[i].capabilities_json);
    }
    for (size_t i = 0; i < fw->group_count; i++) {
        free(fw->groups[i].members);
        free(fw->groups[i].shared_context_json);
    }
    for (size_t i = 0; i < fw->task_count; i++) {
        free(fw->tasks[i].input_json);
        free(fw->tasks[i].output_json);
    }
    for (size_t i = 0; i < fw->consensus_count; i++) {
        free(fw->consensuses[i].proposal_json);
        for (size_t j = 0; j < fw->consensuses[i].vote_count; j++) {
            free(fw->consensuses[i].votes[j]);
        }
        free(fw->consensuses[i].votes);
        free(fw->consensuses[i].result_json);
    }
    free(fw);
}

int mac_framework_register_agent(mac_framework_t* fw, const mac_agent_info_t* agent) {
    if (!fw || !agent) return -1;
    if (fw->agent_count >= MAC_MAX_AGENTS) return -2;
    for (size_t i = 0; i < fw->agent_count; i++) {
        if (strcmp(fw->agents[i].id, agent->id) == 0) return -3;
    }
    mac_agent_info_t* slot = &fw->agents[fw->agent_count];
    memcpy(slot, agent, sizeof(mac_agent_info_t));
    slot->capabilities_json = agent->capabilities_json ? strdup(agent->capabilities_json) : NULL;
    slot->current_tasks = 0;
    slot->available = true;
    fw->agent_count++;
    return 0;
}

int mac_framework_unregister_agent(mac_framework_t* fw, const char* agent_id) {
    if (!fw || !agent_id) return -1;
    for (size_t i = 0; i < fw->agent_count; i++) {
        if (strcmp(fw->agents[i].id, agent_id) == 0) {
            free(fw->agents[i].capabilities_json);
            if (i < fw->agent_count - 1) {
                fw->agents[i] = fw->agents[fw->agent_count - 1];
            }
            fw->agent_count--;
            return 0;
        }
    }
    return -2;
}

int mac_framework_create_group(mac_framework_t* fw,
                                 const char* name,
                                 mac_collab_mode_t mode,
                                 const char** agent_ids,
                                 size_t agent_count,
                                 char** group_id) {
    if (!fw || !name) return -1;
    if (fw->group_count >= MAC_MAX_GROUPS) return -2;

    mac_group_t* group = &fw->groups[fw->group_count];
    memset(group, 0, sizeof(mac_group_t));

    generate_id(group->id, sizeof(group->id), "grp");
    strncpy(group->name, name, sizeof(group->name) - 1);
    group->mode = mode;
    group->created_at = (uint64_t)time(NULL);

    if (agent_count > 0 && agent_ids) {
        size_t valid_count = 0;
        mac_agent_info_t* members = (mac_agent_info_t*)calloc(agent_count, sizeof(mac_agent_info_t));
        if (!members) return -3;

        for (size_t i = 0; i < agent_count; i++) {
            for (size_t j = 0; j < fw->agent_count; j++) {
                if (strcmp(fw->agents[j].id, agent_ids[i]) == 0) {
                    memcpy(&members[valid_count], &fw->agents[j], sizeof(mac_agent_info_t));
                    if (members[valid_count].capabilities_json)
                        members[valid_count].capabilities_json = strdup(members[valid_count].capabilities_json);
                    if (valid_count == 0) {
                        strncpy(group->leader_id, fw->agents[j].id, sizeof(group->leader_id) - 1);
                    }
                    valid_count++;
                    break;
                }
            }
        }
        group->members = members;
        group->member_count = valid_count;
    }

    if (group_id) {
        *group_id = strdup(group->id);
    }

    fw->group_count++;
    return 0;
}

int mac_framework_disband_group(mac_framework_t* fw, const char* group_id) {
    if (!fw || !group_id) return -1;
    for (size_t i = 0; i < fw->group_count; i++) {
        if (strcmp(fw->groups[i].id, group_id) == 0) {
            free(fw->groups[i].members);
            free(fw->groups[i].shared_context_json);
            if (i < fw->group_count - 1) {
                fw->groups[i] = fw->groups[fw->group_count - 1];
            }
            memset(&fw->groups[fw->group_count - 1], 0, sizeof(mac_group_t));
            fw->group_count--;
            return 0;
        }
    }
    return -2;
}

static mac_agent_info_t* select_agent_for_task(mac_framework_t* fw, const mac_group_t* group,
                                                 const mac_collab_task_t* task) {
    mac_agent_info_t* best = NULL;
    double best_score = -1.0;
    size_t start = 0;
    size_t end = fw->agent_count;

    if (group && group->members && group->member_count > 0) {
        start = 0;
        end = group->member_count;
    }

    for (size_t i = start; i < end; i++) {
        mac_agent_info_t* agent;
        if (group && group->members) {
            agent = &group->members[i];
        } else {
            agent = &fw->agents[i];
        }
        if (!agent->available) continue;
        if (agent->current_tasks >= agent->max_concurrent_tasks) continue;
        double score = agent->performance_score * 0.6 + agent->reliability_score * 0.4;
        if (score > best_score) {
            best_score = score;
            best = agent;
        }
    }
    return best;
}

int mac_framework_delegate_task(mac_framework_t* fw,
                                  const char* group_id,
                                  const mac_collab_task_t* task,
                                  char** assigned_agent_id) {
    if (!fw || !task) return -1;

    mac_group_t* group = NULL;
    if (group_id) {
        for (size_t i = 0; i < fw->group_count; i++) {
            if (strcmp(fw->groups[i].id, group_id) == 0) {
                group = &fw->groups[i];
                break;
            }
        }
        if (!group) return -2;
    }

    if (fw->task_count >= MAC_MAX_TASKS) return -3;

    mac_agent_info_t* agent = select_agent_for_task(fw, group, task);
    if (!agent) return -4;

    if (fw->delegate_fn) {
        int ret = fw->delegate_fn(fw, task, agent, fw->delegate_user_data);
        if (ret != 0) return ret;
    }

    mac_collab_task_t* slot = &fw->tasks[fw->task_count];
    memcpy(slot, task, sizeof(mac_collab_task_t));
    slot->input_json = task->input_json ? strdup(task->input_json) : NULL;
    slot->output_json = NULL;
    strncpy(slot->assigned_agent_id, agent->id, sizeof(slot->assigned_agent_id) - 1);
    slot->completed = false;
    slot->created_at = (uint64_t)time(NULL);
    if (group_id) strncpy(slot->group_id, group_id, sizeof(slot->group_id) - 1);

    agent->current_tasks++;
    fw->task_count++;

    if (assigned_agent_id) {
        *assigned_agent_id = strdup(agent->id);
    }

    return 0;
}

int mac_framework_collect_results(mac_framework_t* fw,
                                    const char* group_id,
                                    const char* task_id,
                                    char*** results,
                                    size_t* result_count) {
    if (!fw || !results || !result_count) return -1;

    size_t count = 0;
    size_t capacity = 16;
    char** out = (char**)calloc(capacity, sizeof(char*));
    if (!out) return -2;

    for (size_t i = 0; i < fw->task_count; i++) {
        mac_collab_task_t* t = &fw->tasks[i];
        if (group_id && t->group_id[0] && strcmp(t->group_id, group_id) != 0) continue;
        if (task_id && strcmp(t->id, task_id) != 0) continue;
        if (!t->completed) continue;

        if (count >= capacity) {
            capacity *= 2;
            char** new_out = (char**)realloc(out, capacity * sizeof(char*));
            if (!new_out) {
                for (size_t j = 0; j < count; j++) free(out[j]);
                free(out);
                return -3;
            }
            out = new_out;
        }
        out[count] = t->output_json ? strdup(t->output_json) : strdup("{}");
        count++;
    }

    if (fw->aggregate_fn && count > 0) {
        char* aggregated = NULL;
        fw->aggregate_fn(fw, group_id, task_id, (const char**)out, count, &aggregated, fw->aggregate_user_data);
        if (aggregated) {
            for (size_t j = 0; j < count; j++) free(out[j]);
            out[0] = aggregated;
            count = 1;
        }
    }

    *results = out;
    *result_count = count;
    return 0;
}

int mac_framework_start_consensus(mac_framework_t* fw,
                                    const char* group_id,
                                    const char* proposal_json,
                                    mac_consensus_strategy_t strategy,
                                    char** consensus_id) {
    if (!fw || !proposal_json) return -1;
    if (fw->consensus_count >= MAC_MAX_CONSENSUS) return -2;

    mac_consensus_t* c = &fw->consensuses[fw->consensus_count];
    memset(c, 0, sizeof(mac_consensus_t));

    generate_id(c->id, sizeof(c->id), "cns");
    if (group_id) strncpy(c->group_id, group_id, sizeof(c->group_id) - 1);
    c->strategy = strategy;
    c->proposal_json = strdup(proposal_json);
    c->votes = NULL;
    c->vote_count = 0;
    c->result_json = NULL;
    c->resolved = false;

    if (consensus_id) {
        *consensus_id = strdup(c->id);
    }

    fw->consensus_count++;
    return 0;
}

int mac_framework_vote(mac_framework_t* fw,
                         const char* consensus_id,
                         const char* agent_id,
                         const char* vote_json) {
    if (!fw || !consensus_id || !agent_id || !vote_json) return -1;

    for (size_t i = 0; i < fw->consensus_count; i++) {
        if (strcmp(fw->consensuses[i].id, consensus_id) == 0) {
            if (fw->consensuses[i].resolved) return -2;

            mac_consensus_t* c = &fw->consensuses[i];
            size_t new_count = c->vote_count + 1;
            char** new_votes = (char**)realloc(c->votes, new_count * sizeof(char*));
            if (!new_votes) return -3;
            c->votes = new_votes;
            c->votes[c->vote_count] = strdup(vote_json);
            c->vote_count = new_count;
            return 0;
        }
    }
    return -4;
}

int mac_framework_resolve_consensus(mac_framework_t* fw,
                                      const char* consensus_id,
                                      char** result_json) {
    if (!fw || !consensus_id || !result_json) return -1;

    for (size_t i = 0; i < fw->consensus_count; i++) {
        if (strcmp(fw->consensuses[i].id, consensus_id) == 0) {
            mac_consensus_t* c = &fw->consensuses[i];
            if (c->resolved) {
                *result_json = c->result_json ? strdup(c->result_json) : strdup("{}");
                return 0;
            }

            bool approved = false;
            switch (c->strategy) {
                case MAC_CONSENSUS_MAJORITY:
                    approved = c->vote_count > 0;
                    break;
                case MAC_CONSENSUS_UNANIMOUS:
                    approved = c->vote_count > 0;
                    break;
                case MAC_CONSENSUS_WEIGHTED:
                    approved = c->vote_count > 0;
                    break;
                case MAC_CONSENSUS_LEADER:
                    approved = c->vote_count > 0;
                    break;
                default:
                    approved = c->vote_count > 0;
                    break;
            }

            c->resolved = true;
            c->result_json = approved ? strdup(c->proposal_json) : strdup("{\"rejected\":true}");
            *result_json = strdup(c->result_json);
            return 0;
        }
    }
    return -2;
}

int mac_framework_set_delegate_fn(mac_framework_t* fw,
                                    mac_task_delegate_fn fn,
                                    void* user_data) {
    if (!fw) return -1;
    fw->delegate_fn = fn;
    fw->delegate_user_data = user_data;
    return 0;
}

int mac_framework_set_aggregate_fn(mac_framework_t* fw,
                                     mac_result_aggregate_fn fn,
                                     void* user_data) {
    if (!fw) return -1;
    fw->aggregate_fn = fn;
    fw->aggregate_user_data = user_data;
    return 0;
}

size_t mac_framework_get_agent_count(mac_framework_t* fw) {
    return fw ? fw->agent_count : 0;
}

size_t mac_framework_get_group_count(mac_framework_t* fw) {
    return fw ? fw->group_count : 0;
}
