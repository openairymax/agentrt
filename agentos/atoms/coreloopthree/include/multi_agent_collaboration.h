// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file multi_agent_collaboration.h
 * @brief Multi-Agent Collaboration Framework for AgentOS
 *
 * 多智能体协作框架，支持智能体间的发现、协商、任务委派、
 * 结果聚合和冲突解决。基于A2A协议和MCIS理论设计。
 *
 * 协作模式:
 * 1. 主从模式 — 主智能体分配任务，从智能体执行
 * 2. 对等模式 — 智能体间平等协商与协作
 * 3. 层级模式 — 多层级智能体组织结构
 * 4. 市场模式 — 基于竞拍的任务分配
 * 5. 涌现模式 — 自组织协作行为
 *
 * @since 2.0.0
 */

#ifndef AGENTOS_MULTI_AGENT_COLLABORATION_H
#define AGENTOS_MULTI_AGENT_COLLABORATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAC_MAX_AGENTS          128
#define MAC_MAX_TASKS           2048
#define MAC_MAX_GROUPS          64
#define MAC_MAX_CONSENSUS       32

typedef enum {
    MAC_ROLE_LEADER = 0,
    MAC_ROLE_WORKER,
    MAC_ROLE_COORDINATOR,
    MAC_ROLE_OBSERVER,
    MAC_ROLE_PEER
} mac_role_t;

typedef enum {
    MAC_MODE_MASTER_WORKER = 0,
    MAC_MODE_PEER_TO_PEER,
    MAC_MODE_HIERARCHICAL,
    MAC_MODE_MARKET,
    MAC_MODE_EMERGENT
} mac_collab_mode_t;

typedef enum {
    MAC_CONSENSUS_MAJORITY = 0,
    MAC_CONSENSUS_UNANIMOUS,
    MAC_CONSENSUS_WEIGHTED,
    MAC_CONSENSUS_LEADER
} mac_consensus_strategy_t;

typedef struct {
    char id[64];
    char name[128];
    mac_role_t role;
    char* capabilities_json;
    double performance_score;
    double reliability_score;
    uint32_t max_concurrent_tasks;
    uint32_t current_tasks;
    bool available;
} mac_agent_info_t;

typedef struct {
    char id[64];
    char name[128];
    mac_collab_mode_t mode;
    mac_agent_info_t* members;
    size_t member_count;
    char leader_id[64];
    char* shared_context_json;
    uint64_t created_at;
} mac_group_t;

typedef struct {
    char id[64];
    char group_id[64];
    char description[256];
    char* input_json;
    char* output_json;
    char assigned_agent_id[64];
    mac_role_t required_role;
    int32_t priority;
    int32_t timeout_ms;
    uint64_t created_at;
    uint64_t deadline;
    bool completed;
} mac_collab_task_t;

typedef struct {
    char id[64];
    char group_id[64];
    mac_consensus_strategy_t strategy;
    char* proposal_json;
    char** votes;
    size_t vote_count;
    char* result_json;
    bool resolved;
} mac_consensus_t;

typedef struct mac_framework_s mac_framework_t;

typedef int (*mac_task_delegate_fn)(mac_framework_t* fw,
                                      const mac_collab_task_t* task,
                                      const mac_agent_info_t* agent,
                                      void* user_data);

typedef void (*mac_result_aggregate_fn)(mac_framework_t* fw,
                                          const char* group_id,
                                          const char* task_id,
                                          const char** results,
                                          size_t result_count,
                                          char** aggregated,
                                          void* user_data);

mac_framework_t* mac_framework_create(mac_collab_mode_t default_mode);
void mac_framework_destroy(mac_framework_t* fw);

int mac_framework_register_agent(mac_framework_t* fw, const mac_agent_info_t* agent);
int mac_framework_unregister_agent(mac_framework_t* fw, const char* agent_id);

int mac_framework_create_group(mac_framework_t* fw,
                                 const char* name,
                                 mac_collab_mode_t mode,
                                 const char** agent_ids,
                                 size_t agent_count,
                                 char** group_id);

int mac_framework_disband_group(mac_framework_t* fw, const char* group_id);

int mac_framework_delegate_task(mac_framework_t* fw,
                                  const char* group_id,
                                  const mac_collab_task_t* task,
                                  char** assigned_agent_id);

int mac_framework_collect_results(mac_framework_t* fw,
                                    const char* group_id,
                                    const char* task_id,
                                    char*** results,
                                    size_t* result_count);

int mac_framework_start_consensus(mac_framework_t* fw,
                                    const char* group_id,
                                    const char* proposal_json,
                                    mac_consensus_strategy_t strategy,
                                    char** consensus_id);

int mac_framework_vote(mac_framework_t* fw,
                         const char* consensus_id,
                         const char* agent_id,
                         const char* vote_json);

int mac_framework_resolve_consensus(mac_framework_t* fw,
                                      const char* consensus_id,
                                      char** result_json);

int mac_framework_set_delegate_fn(mac_framework_t* fw,
                                    mac_task_delegate_fn fn,
                                    void* user_data);

int mac_framework_set_aggregate_fn(mac_framework_t* fw,
                                     mac_result_aggregate_fn fn,
                                     void* user_data);

size_t mac_framework_get_agent_count(mac_framework_t* fw);
size_t mac_framework_get_group_count(mac_framework_t* fw);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MULTI_AGENT_COLLABORATION_H */
