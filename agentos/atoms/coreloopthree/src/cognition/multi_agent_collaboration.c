// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "multi_agent_collaboration.h"

#include "agentos.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAC_MAX_AGENTS 1024
#define MAC_MAX_TASKS 4096
#define MAC_MAX_GROUPS 128
#define MAC_MAX_CONSENSUS 64
#define MAC_HASH_SIZE  4096
#define MAC_HASH_MASK  (MAC_HASH_SIZE - 1)

struct mac_framework_s {
    mac_collab_mode_t default_mode;
    mac_agent_info_t agents[MAC_MAX_AGENTS];
    size_t agent_count;
    agentos_mutex_t lock;
    int lock_init;

    mac_group_t groups[MAC_MAX_GROUPS];
    size_t group_count;
    mac_collab_task_t tasks[MAC_MAX_TASKS];
    size_t task_count;
    mac_consensus_t consensuses[MAC_MAX_CONSENSUS];
    size_t consensus_count;
    mac_task_delegate_fn delegate_fn;
    void *delegate_user_data;
    mac_result_aggregate_fn aggregate_fn;
    void *aggregate_user_data;

    size_t agent_hash[MAC_HASH_SIZE];
    size_t group_hash[MAC_HASH_SIZE];
    size_t task_hash[MAC_HASH_SIZE];
    size_t consensus_hash[MAC_HASH_SIZE];
};

static void ensure_lock(mac_framework_t *fw)
{
    if (!fw->lock_init) {
        agentos_mutex_init(&fw->lock);
        fw->lock_init = 1;
    }
}

static bool is_approval_vote(const char *vote)
{
    if (!vote)
        return false;
    const char *v = vote;
    while (*v == ' ' || *v == '\t')
        v++;
    if (strncasecmp(v, "approve", 7) == 0 || strncasecmp(v, "yes", 3) == 0 ||
        strncasecmp(v, "true", 4) == 0 || strncasecmp(v, "1", 1) == 0)
        return true;
    if (strncmp(v, "{\"approve\"", 11) == 0)
        return true;
    if (strstr(vote, "\"vote\":\"approve\"") || strstr(vote, "\"vote\":\"yes\"") ||
        strstr(vote, "\"vote\":true") || strstr(vote, "\"vote\":1") ||
        strstr(vote, "\"approved\":true") || strstr(vote, "\"approved\":1") ||
        strstr(vote, "\"approve\":true") || strstr(vote, "\"approve\":1"))
        return true;
    return false;
}

static bool is_rejection_vote(const char *vote)
{
    if (!vote)
        return false;
    const char *v = vote;
    while (*v == ' ' || *v == '\t')
        v++;
    if (strncasecmp(v, "reject", 6) == 0 || strncasecmp(v, "no", 2) == 0 ||
        strncasecmp(v, "false", 5) == 0 || strncasecmp(v, "0", 1) == 0)
        return true;
    if (strstr(vote, "\"vote\":\"reject\"") || strstr(vote, "\"vote\":\"no\"") ||
        strstr(vote, "\"vote\":false") || strstr(vote, "\"vote\":0") ||
        strstr(vote, "\"rejected\":true") || strstr(vote, "\"reject\":true"))
        return true;
    return false;
}

static void generate_id(char *buf, size_t buf_size, const char *prefix)
{
    snprintf(buf, buf_size, "%s_%lu_%u", prefix, (unsigned long)(agentos_time_ns() / 1000000ULL),
             (unsigned int)((agentos_time_ns() ^ (size_t)buf) % 100000));
}

static size_t mac_hash_str(const char *str)
{
    if (!str || !str[0])
        return 0;
    size_t h = 5381;
    while (*str) {
        h = ((h << 5) + h) ^ (size_t)(unsigned char)(*str);
        str++;
    }
    return h & MAC_HASH_MASK;
}

static void mac_hash_insert(size_t *table, const char *key, size_t index)
{
    if (!key || !key[0])
        return;
    size_t slot = mac_hash_str(key);
    for (size_t probe = 0; probe < MAC_HASH_SIZE; probe++) {
        size_t pos = (slot + probe) & MAC_HASH_MASK;
        if (table[pos] == 0) {
            table[pos] = index + 1;
            return;
        }
    }
}

static void mac_hash_remove(size_t *table, const char *key, size_t index)
{
    if (!key || !key[0])
        return;
    size_t slot = mac_hash_str(key);
    for (size_t probe = 0; probe < MAC_HASH_SIZE; probe++) {
        size_t pos = (slot + probe) & MAC_HASH_MASK;
        if (table[pos] == index + 1) {
            table[pos] = 0;
            return;
        }
        if (table[pos] == 0)
            return;
    }
}

static ssize_t mac_hash_find_agent(mac_framework_t *fw, const char *agent_id)
{
    if (!agent_id || !agent_id[0])
        return -1;
    size_t slot = mac_hash_str(agent_id);
    for (size_t probe = 0; probe < MAC_HASH_SIZE; probe++) {
        size_t pos = (slot + probe) & MAC_HASH_MASK;
        size_t idx = fw->agent_hash[pos];
        if (idx == 0)
            return -1;
        if (idx - 1 < fw->agent_count && strcmp(fw->agents[idx - 1].id, agent_id) == 0)
            return (ssize_t)(idx - 1);
    }
    return -1;
}

static ssize_t mac_hash_find_group(mac_framework_t *fw, const char *group_id)
{
    if (!group_id || !group_id[0])
        return -1;
    size_t slot = mac_hash_str(group_id);
    for (size_t probe = 0; probe < MAC_HASH_SIZE; probe++) {
        size_t pos = (slot + probe) & MAC_HASH_MASK;
        size_t idx = fw->group_hash[pos];
        if (idx == 0)
            return -1;
        if (idx - 1 < fw->group_count && strcmp(fw->groups[idx - 1].id, group_id) == 0)
            return (ssize_t)(idx - 1);
    }
    return -1;
}

static ssize_t mac_hash_find_consensus(mac_framework_t *fw, const char *consensus_id)
{
    if (!consensus_id || !consensus_id[0])
        return -1;
    size_t slot = mac_hash_str(consensus_id);
    for (size_t probe = 0; probe < MAC_HASH_SIZE; probe++) {
        size_t pos = (slot + probe) & MAC_HASH_MASK;
        size_t idx = fw->consensus_hash[pos];
        if (idx == 0)
            return -1;
        if (idx - 1 < fw->consensus_count && strcmp(fw->consensuses[idx - 1].id, consensus_id) == 0)
            return (ssize_t)(idx - 1);
    }
    return -1;
}

mac_framework_t *mac_framework_create(mac_collab_mode_t default_mode)
{
    mac_framework_t *fw = (mac_framework_t *)calloc(1, sizeof(mac_framework_t));
    if (!fw)
        return NULL;
    fw->default_mode = default_mode;
    fw->lock_init = 0;
    memset(fw->agent_hash, 0, sizeof(fw->agent_hash));
    memset(fw->group_hash, 0, sizeof(fw->group_hash));
    memset(fw->task_hash, 0, sizeof(fw->task_hash));
    memset(fw->consensus_hash, 0, sizeof(fw->consensus_hash));
    return fw;
}

void mac_framework_destroy(mac_framework_t *fw)
{
    if (!fw)
        return;
    for (size_t i = 0; i < fw->agent_count; i++) {
        free(fw->agents[i].capabilities_json);
    }
    for (size_t i = 0; i < fw->group_count; i++) {
        for (size_t m = 0; m < fw->groups[i].member_count; m++) {
            free(fw->groups[i].members[m].capabilities_json);
        }
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
            free(fw->consensuses[i].voter_ids[j]);
        }
        free(fw->consensuses[i].votes);
        free(fw->consensuses[i].voter_ids);
        free(fw->consensuses[i].result_json);
    }
    if (fw->lock_init)
        agentos_mutex_destroy(&fw->lock);
    free(fw);
}

int mac_framework_register_agent(mac_framework_t *fw, const mac_agent_info_t *agent)
{
    if (!fw || !agent)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    if (fw->agent_count >= MAC_MAX_AGENTS) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }
    if (mac_hash_find_agent(fw, agent->id) >= 0) {
        agentos_mutex_unlock(&fw->lock);
        return -3;
    }
    mac_agent_info_t *slot = &fw->agents[fw->agent_count];
    memcpy(slot, agent, sizeof(mac_agent_info_t));
    slot->capabilities_json = agent->capabilities_json ? strdup(agent->capabilities_json) : NULL;
    slot->current_tasks = 0;
    slot->available = true;
    mac_hash_insert(fw->agent_hash, agent->id, fw->agent_count);
    fw->agent_count++;
    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_unregister_agent(mac_framework_t *fw, const char *agent_id)
{
    if (!fw || !agent_id)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    ssize_t idx = mac_hash_find_agent(fw, agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    free(fw->agents[idx].capabilities_json);
    mac_hash_remove(fw->agent_hash, agent_id, (size_t)idx);

    if ((size_t)idx < fw->agent_count - 1) {
        fw->agents[idx] = fw->agents[fw->agent_count - 1];
        mac_hash_remove(fw->agent_hash, fw->agents[idx].id, fw->agent_count - 1);
        mac_hash_insert(fw->agent_hash, fw->agents[idx].id, (size_t)idx);
    }
    memset(&fw->agents[fw->agent_count - 1], 0, sizeof(mac_agent_info_t));
    fw->agent_count--;
    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_create_group(mac_framework_t *fw, const char *name, mac_collab_mode_t mode,
                               const char **agent_ids, size_t agent_count, char **group_id)
{
    if (!fw || !name)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    if (fw->group_count >= MAC_MAX_GROUPS) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    mac_group_t *group = &fw->groups[fw->group_count];
    memset(group, 0, sizeof(mac_group_t));

    generate_id(group->id, sizeof(group->id), "grp");
    strncpy(group->name, name, sizeof(group->name) - 1);
    group->name[sizeof(group->name) - 1] = '\0';
    group->mode = mode;
    group->created_at = agentos_time_ms();

    if (agent_count > 0 && agent_ids) {
        size_t valid_count = 0;
        mac_agent_info_t *members =
            (mac_agent_info_t *)calloc(agent_count, sizeof(mac_agent_info_t));
        if (!members) {
            agentos_mutex_unlock(&fw->lock);
            return -3;
        }

        for (size_t i = 0; i < agent_count; i++) {
            ssize_t aidx = mac_hash_find_agent(fw, agent_ids[i]);
            if (aidx >= 0) {
                memcpy(&members[valid_count], &fw->agents[aidx], sizeof(mac_agent_info_t));
                if (members[valid_count].capabilities_json)
                    members[valid_count].capabilities_json =
                        strdup(members[valid_count].capabilities_json);
                if (valid_count == 0) {
                    strncpy(group->leader_id, fw->agents[aidx].id, sizeof(group->leader_id) - 1);
                    group->leader_id[sizeof(group->leader_id) - 1] = '\0';
                }
                valid_count++;
            }
        }
        group->members = members;
        group->member_count = valid_count;
    }

    if (group_id)
        *group_id = strdup(group->id);
    mac_hash_insert(fw->group_hash, group->id, fw->group_count);
    fw->group_count++;

    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_disband_group(mac_framework_t *fw, const char *group_id)
{
    if (!fw || !group_id)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    ssize_t idx = mac_hash_find_group(fw, group_id);
    if (idx < 0) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    for (size_t m = 0; m < fw->groups[idx].member_count; m++) {
        free(fw->groups[idx].members[m].capabilities_json);
    }
    free(fw->groups[idx].members);
    free(fw->groups[idx].shared_context_json);
    mac_hash_remove(fw->group_hash, group_id, (size_t)idx);

    if ((size_t)idx < fw->group_count - 1) {
        fw->groups[idx] = fw->groups[fw->group_count - 1];
        mac_hash_remove(fw->group_hash, fw->groups[idx].id, fw->group_count - 1);
        mac_hash_insert(fw->group_hash, fw->groups[idx].id, (size_t)idx);
    }
    memset(&fw->groups[fw->group_count - 1], 0, sizeof(mac_group_t));
    fw->group_count--;
    agentos_mutex_unlock(&fw->lock);
    return 0;
}

static mac_agent_info_t *select_agent_for_task(mac_framework_t *fw, const mac_group_t *group,
                                               const mac_collab_task_t *task)
{
    mac_agent_info_t *best = NULL;
    double best_score = -1.0;
    size_t start = 0, end = fw->agent_count;

    if (group && group->members && group->member_count > 0) {
        start = 0;
        end = group->member_count;
    }

    for (size_t i = start; i < end; i++) {
        mac_agent_info_t *agent;
        if (group && group->members) {
            agent = &group->members[i];
        } else {
            agent = &fw->agents[i];
        }
        if (!agent->available)
            continue;
        if (agent->current_tasks >= agent->max_concurrent_tasks)
            continue;
        double score = agent->performance_score * 0.6 + agent->reliability_score * 0.4;
        if (score > best_score) {
            best_score = score;
            best = agent;
        }
    }
    return best;
}

int mac_framework_delegate_task(mac_framework_t *fw, const char *group_id,
                                const mac_collab_task_t *task, char **assigned_agent_id)
{
    if (!fw || !task)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    mac_group_t *group = NULL;
    if (group_id) {
        ssize_t gidx = mac_hash_find_group(fw, group_id);
        if (gidx >= 0) {
            group = &fw->groups[gidx];
        } else {
            agentos_mutex_unlock(&fw->lock);
            return -2;
        }
    }

    if (fw->task_count >= MAC_MAX_TASKS) {
        agentos_mutex_unlock(&fw->lock);
        return -3;
    }

    mac_agent_info_t *agent = select_agent_for_task(fw, group, task);
    if (!agent) {
        agentos_mutex_unlock(&fw->lock);
        return -4;
    }

    if (fw->delegate_fn) {
        int ret = fw->delegate_fn(fw, task, agent, fw->delegate_user_data);
        if (ret != 0) {
            agentos_mutex_unlock(&fw->lock);
            return ret;
        }
    }

    mac_collab_task_t *slot = &fw->tasks[fw->task_count];
    memcpy(slot, task, sizeof(mac_collab_task_t));
    slot->input_json = task->input_json ? strdup(task->input_json) : NULL;
    slot->output_json = NULL;
    snprintf(slot->assigned_agent_id, sizeof(slot->assigned_agent_id), "%s", agent->id);
    slot->status = MAC_TASK_STATUS_ASSIGNED;
    slot->created_at = agentos_time_ms();
    if (group_id) {
        strncpy(slot->group_id, group_id, sizeof(slot->group_id) - 1);
        slot->group_id[sizeof(slot->group_id) - 1] = '\0';
    }

    agent->current_tasks++;
    mac_hash_insert(fw->task_hash, slot->id, fw->task_count);
    fw->task_count++;

    if (assigned_agent_id)
        *assigned_agent_id = strdup(agent->id);
    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_collect_results(mac_framework_t *fw, const char *group_id, const char *task_id,
                                  char ***results, size_t *result_count)
{
    if (!fw || !results || !result_count)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    size_t count = 0, capacity = 16;
    char **out = (char **)calloc(capacity, sizeof(char *));
    if (!out) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    for (size_t i = 0; i < fw->task_count; i++) {
        mac_collab_task_t *t = &fw->tasks[i];
        if (group_id && t->group_id[0] && strcmp(t->group_id, group_id) != 0)
            continue;
        if (task_id && strcmp(t->id, task_id) != 0)
            continue;
        if (!t->output_json)
            continue;
        if (t->status != MAC_TASK_STATUS_COMPLETED)
            continue;

        if (count >= capacity) {
            capacity *= 2;
            char **new_out = (char **)realloc(out, capacity * sizeof(char *));
            if (!new_out) {
                for (size_t j = 0; j < count; j++)
                    free(out[j]);
                free(out);
                agentos_mutex_unlock(&fw->lock);
                return -3;
            }
            out = new_out;
        }
        out[count] = t->output_json ? strdup(t->output_json) : strdup("{}");
        if (!out[count]) {
            for (size_t j = 0; j < count; j++)
                free(out[j]);
            free(out);
            agentos_mutex_unlock(&fw->lock);
            return -3;
        }
        count++;
    }

    if (fw->aggregate_fn && count > 0) {
        char *aggregated = NULL;
        fw->aggregate_fn(fw, group_id, task_id, (const char **)out, count, &aggregated,
                         fw->aggregate_user_data);
        if (aggregated) {
            for (size_t j = 0; j < count; j++)
                free(out[j]);
            out[0] = aggregated;
            count = 1;
        }
    }

    *results = out;
    *result_count = count;
    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_start_consensus(mac_framework_t *fw, const char *group_id,
                                  const char *proposal_json, mac_consensus_strategy_t strategy,
                                  char **consensus_id)
{
    if (!fw || !proposal_json)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    if (fw->consensus_count >= MAC_MAX_CONSENSUS) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    mac_consensus_t *c = &fw->consensuses[fw->consensus_count];
    memset(c, 0, sizeof(mac_consensus_t));

    generate_id(c->id, sizeof(c->id), "cns");
    if (group_id) {
        strncpy(c->group_id, group_id, sizeof(c->group_id) - 1);
        c->group_id[sizeof(c->group_id) - 1] = '\0';
    }
    c->strategy = strategy;
    c->proposal_json = strdup(proposal_json);
    c->votes = NULL;
    c->voter_ids = NULL;
    c->vote_count = 0;
    c->result_json = NULL;
    c->resolved = false;

    if (consensus_id)
        *consensus_id = strdup(c->id);
    mac_hash_insert(fw->consensus_hash, c->id, fw->consensus_count);
    fw->consensus_count++;

    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_vote(mac_framework_t *fw, const char *consensus_id, const char *agent_id,
                       const char *vote_json)
{
    if (!fw || !consensus_id || !agent_id || !vote_json)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    ssize_t cidx = mac_hash_find_consensus(fw, consensus_id);
    if (cidx < 0) {
        agentos_mutex_unlock(&fw->lock);
        return -4;
    }

    mac_consensus_t *c = &fw->consensuses[cidx];
    if (c->resolved) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    for (size_t v = 0; v < c->vote_count; v++) {
        if (c->voter_ids[v] && strcmp(c->voter_ids[v], agent_id) == 0) {
            agentos_mutex_unlock(&fw->lock);
            return -5;
        }
    }

    size_t new_count = c->vote_count + 1;
    char **new_votes = (char **)realloc(c->votes, new_count * sizeof(char *));
    if (!new_votes) {
        agentos_mutex_unlock(&fw->lock);
        return -3;
    }
    c->votes = new_votes;
    char **new_voter_ids = (char **)realloc(c->voter_ids, new_count * sizeof(char *));
    if (!new_voter_ids) {
        agentos_mutex_unlock(&fw->lock);
        return -3;
    }
    c->voter_ids = new_voter_ids;
    c->votes[c->vote_count] = strdup(vote_json);
    c->voter_ids[c->vote_count] = strdup(agent_id);
    c->vote_count = new_count;

    agentos_mutex_unlock(&fw->lock);
    return 0;
}

static bool consensus_evaluate_majority(mac_consensus_t *c, mac_framework_t *fw)
{
    if (c->vote_count == 0)
        return false;
    size_t approve_count = 0;
    size_t total_members = 0;

    if (c->group_id[0]) {
        ssize_t gidx = mac_hash_find_group(fw, c->group_id);
        if (gidx >= 0)
            total_members = fw->groups[gidx].member_count;
    }
    if (total_members == 0)
        total_members = fw->agent_count;
    if (total_members == 0)
        total_members = 1;

    for (size_t v = 0; v < c->vote_count; v++) {
        if (is_approval_vote(c->votes[v])) {
            approve_count++;
        }
    }

    return approve_count > total_members / 2;
}

static bool consensus_evaluate_unanimous(mac_consensus_t *c, mac_framework_t *fw)
{
    if (c->vote_count == 0)
        return false;
    size_t total_members = 0;
    if (c->group_id[0]) {
        ssize_t gidx = mac_hash_find_group(fw, c->group_id);
        if (gidx >= 0)
            total_members = fw->groups[gidx].member_count;
    }
    if (total_members == 0)
        total_members = fw->agent_count;
    if (total_members == 0)
        total_members = 1;

    size_t approve_count = 0;
    for (size_t v = 0; v < c->vote_count; v++) {
        if (is_rejection_vote(c->votes[v])) {
            return false;
        }
        if (is_approval_vote(c->votes[v])) {
            approve_count++;
        }
    }

    return approve_count >= total_members && (c->vote_count >= total_members);
}

static bool consensus_evaluate_weighted(mac_consensus_t *c, mac_framework_t *fw)
{
    if (c->vote_count == 0)
        return false;

    double weight_sum = 0.0;
    double approve_weight = 0.0;

    for (size_t v = 0; v < c->vote_count; v++) {
        if (!c->votes[v])
            continue;

        double voter_weight = 1.0;
        const char *voter_id = (c->voter_ids && c->voter_ids[v]) ? c->voter_ids[v] : NULL;
        if (voter_id) {
            ssize_t aidx = mac_hash_find_agent(fw, voter_id);
            if (aidx >= 0) {
                voter_weight =
                    fw->agents[aidx].reliability_score * (1.0 + fw->agents[aidx].performance_score);
            }
        } else {
            ssize_t aidx = mac_hash_find_agent(fw, c->votes[v]);
            if (aidx >= 0) {
                voter_weight =
                    fw->agents[aidx].reliability_score * (1.0 + fw->agents[aidx].performance_score);
            }
        }

        weight_sum += voter_weight;
        if (is_approval_vote(c->votes[v]))
            approve_weight += voter_weight;
    }

    if (weight_sum <= 0.0)
        return false;
    return (approve_weight / weight_sum) > 0.5;
}

static bool consensus_evaluate_leader(mac_consensus_t *c, mac_framework_t *fw)
{
    if (c->vote_count == 0)
        return false;

    if (c->group_id[0]) {
        ssize_t gidx = mac_hash_find_group(fw, c->group_id);
        if (gidx >= 0) {
            const char *leader_id = fw->groups[gidx].leader_id;
            if (!leader_id || leader_id[0] == '\0')
                return true;

            for (size_t v = 0; v < c->vote_count; v++) {
                const char *voter_id = (c->voter_ids && c->voter_ids[v]) ? c->voter_ids[v] : NULL;
                if ((voter_id && strcmp(voter_id, leader_id) == 0) ||
                    (c->votes[v] && strstr(c->votes[v], leader_id))) {
                    return is_approval_vote(c->votes[v]);
                }
            }
        }
    }
    return false;
}

int mac_framework_resolve_consensus(mac_framework_t *fw, const char *consensus_id,
                                    char **result_json)
{
    if (!fw || !consensus_id || !result_json)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    ssize_t cidx = mac_hash_find_consensus(fw, consensus_id);
    if (cidx < 0) {
        agentos_mutex_unlock(&fw->lock);
        return -2;
    }

    mac_consensus_t *c = &fw->consensuses[cidx];

    if (c->resolved) {
        *result_json = c->result_json ? strdup(c->result_json) : strdup("{}");
        agentos_mutex_unlock(&fw->lock);
        return 0;
    }

    bool approved = false;
    switch (c->strategy) {
    case MAC_CONSENSUS_MAJORITY:
        approved = consensus_evaluate_majority(c, fw);
        break;
    case MAC_CONSENSUS_UNANIMOUS:
        approved = consensus_evaluate_unanimous(c, fw);
        break;
    case MAC_CONSENSUS_WEIGHTED:
        approved = consensus_evaluate_weighted(c, fw);
        break;
    case MAC_CONSENSUS_LEADER:
        approved = consensus_evaluate_leader(c, fw);
        break;
    default:
        approved = consensus_evaluate_majority(c, fw);
        break;
    }

    c->resolved = true;
    if (approved) {
        c->result_json = strdup(c->proposal_json ? c->proposal_json : "{}");
    } else {
        c->result_json = strdup("{\"rejected\":true,\"reason\":\"consensus_not_reached\"}");
    }
    *result_json = strdup(c->result_json);

    agentos_mutex_unlock(&fw->lock);
    return 0;
}

int mac_framework_set_delegate_fn(mac_framework_t *fw, mac_task_delegate_fn fn, void *user_data)
{
    if (!fw)
        return -1;
    fw->delegate_fn = fn;
    fw->delegate_user_data = user_data;
    return 0;
}

int mac_framework_set_aggregate_fn(mac_framework_t *fw, mac_result_aggregate_fn fn, void *user_data)
{
    if (!fw)
        return -1;
    fw->aggregate_fn = fn;
    fw->aggregate_user_data = user_data;
    return 0;
}

size_t mac_framework_get_agent_count(mac_framework_t *fw)
{
    if (!fw)
        return 0;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);
    size_t n = fw->agent_count;
    agentos_mutex_unlock(&fw->lock);
    return n;
}

size_t mac_framework_get_group_count(mac_framework_t *fw)
{
    if (!fw)
        return 0;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);
    size_t n = fw->group_count;
    agentos_mutex_unlock(&fw->lock);
    return n;
}

int mac_framework_register_agents_batch(mac_framework_t *fw, const mac_agent_info_t *agents,
                                         size_t count, size_t *registered_count)
{
    if (!fw || !agents || count == 0)
        return -1;
    ensure_lock(fw);
    agentos_mutex_lock(&fw->lock);

    size_t ok = 0;
    int last_err = 0;
    for (size_t i = 0; i < count; i++) {
        if (fw->agent_count >= MAC_MAX_AGENTS) {
            last_err = -2;
            break;
        }
        if (!agents[i].id[0])
            continue;
        if (mac_hash_find_agent(fw, agents[i].id) >= 0) {
            last_err = -3;
            continue;
        }
        mac_agent_info_t *slot = &fw->agents[fw->agent_count];
        memcpy(slot, &agents[i], sizeof(mac_agent_info_t));
        slot->capabilities_json =
            agents[i].capabilities_json ? strdup(agents[i].capabilities_json) : NULL;
        slot->current_tasks = 0;
        slot->available = true;
        mac_hash_insert(fw->agent_hash, agents[i].id, fw->agent_count);
        fw->agent_count++;
        ok++;
    }

    if (registered_count)
        *registered_count = ok;
    agentos_mutex_unlock(&fw->lock);
    return (ok > 0) ? 0 : last_err;
}
