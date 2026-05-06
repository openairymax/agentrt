// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef MAC_FRAMEWORK_H
#define MAC_FRAMEWORK_H

#include "agentos.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAC_MODE_INDEPENDENT = 0,
    MAC_MODE_COLLABORATIVE = 1,
    MAC_MODE_CONSENSUS = 2,
    MAC_MODE_DELEGATED = 3
} mac_collab_mode_t;

typedef struct {
    char id[128];
    char name[128];
    double performance_score;
    double reliability_score;
    int max_concurrent_tasks;
    int current_tasks;
    bool available;
    char *capabilities_json;
} mac_agent_info_t;

typedef struct {
    char id[64];
    char name[128];
    mac_collab_mode_t mode;
    uint64_t created_at;
    size_t member_count;
    mac_agent_info_t *members;
    char leader_id[128];
    char *shared_context_json;
} mac_group_t;

typedef enum {
    MAC_TASK_STATUS_PENDING = 0,
    MAC_TASK_STATUS_ASSIGNED = 1,
    MAC_TASK_STATUS_RUNNING = 2,
    MAC_TASK_STATUS_COMPLETED = 3,
    MAC_TASK_STATUS_FAILED = 4
} mac_task_status_t;

typedef struct {
    char id[128];
    char group_id[64];
    char assigned_agent_id[128];
    mac_task_status_t status;
    char *input_json;
    char *output_json;
    uint64_t created_at;
    uint64_t completed_at;
} mac_collab_task_t;

typedef enum {
    MAC_CONSENSUS_MAJORITY = 0,
    MAC_CONSENSUS_UNANIMOUS = 1,
    MAC_CONSENSUS_WEIGHTED = 2,
    MAC_CONSENSUS_LEADER = 3
} mac_consensus_strategy_t;

typedef struct {
    char id[64];
    char group_id[64];
    mac_consensus_strategy_t strategy;
    char *proposal_json;
    char **votes;
    char **voter_ids;
    size_t vote_count;
    bool resolved;
    char *result_json;
} mac_consensus_t;

struct mac_framework_s;

typedef int (*mac_task_delegate_fn)(struct mac_framework_s *fw, const mac_collab_task_t *task,
                                    const mac_agent_info_t *agent, void *user_data);
typedef int (*mac_result_aggregate_fn)(struct mac_framework_s *fw, const char *group_id,
                                       const char *task_id, const char **results,
                                       size_t result_count, char **aggregated_result,
                                       void *user_data);

typedef struct mac_framework_s mac_framework_t;

AGENTOS_API mac_framework_t *mac_framework_create(mac_collab_mode_t default_mode);
AGENTOS_API void mac_framework_destroy(mac_framework_t *fw);

AGENTOS_API int mac_framework_register_agent(mac_framework_t *fw, const mac_agent_info_t *agent);
AGENTOS_API int mac_framework_unregister_agent(mac_framework_t *fw, const char *agent_id);
AGENTOS_API int mac_framework_create_group(mac_framework_t *fw, const char *name,
                                           mac_collab_mode_t mode, const char **agent_ids,
                                           size_t agent_count, char **group_id);
AGENTOS_API int mac_framework_disband_group(mac_framework_t *fw, const char *group_id);
AGENTOS_API int mac_framework_delegate_task(mac_framework_t *fw, const char *group_id,
                                            const mac_collab_task_t *task,
                                            char **assigned_agent_id);
AGENTOS_API int mac_framework_collect_results(mac_framework_t *fw, const char *group_id,
                                              const char *task_id, char ***results,
                                              size_t *result_count);
AGENTOS_API int mac_framework_start_consensus(mac_framework_t *fw, const char *group_id,
                                              const char *proposal_json,
                                              mac_consensus_strategy_t strategy,
                                              char **consensus_id);
AGENTOS_API int mac_framework_vote(mac_framework_t *fw, const char *consensus_id,
                                   const char *agent_id, const char *vote_json);
AGENTOS_API int mac_framework_resolve_consensus(mac_framework_t *fw, const char *consensus_id,
                                                char **result_json);
AGENTOS_API int mac_framework_set_delegate_fn(mac_framework_t *fw, mac_task_delegate_fn fn,
                                              void *user_data);
AGENTOS_API int mac_framework_set_aggregate_fn(mac_framework_t *fw, mac_result_aggregate_fn fn,
                                               void *user_data);
AGENTOS_API size_t mac_framework_get_agent_count(mac_framework_t *fw);
AGENTOS_API size_t mac_framework_get_group_count(mac_framework_t *fw);
AGENTOS_API int mac_framework_register_agents_batch(mac_framework_t *fw,
                                                     const mac_agent_info_t *agents,
                                                     size_t count, size_t *registered_count);

#ifdef __cplusplus
}
#endif

#endif
