// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dynamic_policy_engine.h
 * @brief Dynamic Policy Engine for AgentOS SafetyGuard
 *
 * 动态安全策略引擎，支持运行时策略加载、版本管理、
 * 冲突检测与解决、策略热更新和合规性验证。
 *
 * @since 2.0.0
 */

#ifndef AGENTOS_DYNAMIC_POLICY_ENGINE_H
#define AGENTOS_DYNAMIC_POLICY_ENGINE_H

#include "safety_guard.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPOLICY_MAX_RULES       512
#define DPOLICY_MAX_VERSIONS    32
#define DPOLICY_MAX_CONFLICTS   64

typedef enum {
    DPOLICY_EFFECT_ALLOW = 0,
    DPOLICY_EFFECT_DENY,
    DPOLICY_EFFECT_CONDITIONAL
} dpolicy_effect_t;

typedef enum {
    DPOLICY_CONFLICT_DENY_WINS = 0,
    DPOLICY_CONFLICT_ALLOW_WINS,
    DPOLICY_CONFLICT_HIGHEST_PRIORITY,
    DPOLICY_CONFLICT_MOST_RESTRICTIVE
} dpolicy_conflict_strategy_t;

typedef enum {
    DPOLICY_CHANGE_ADD = 0,
    DPOLICY_CHANGE_REMOVE,
    DPOLICY_CHANGE_UPDATE,
    DPOLICY_CHANGE_ROLLBACK
} dpolicy_change_type_t;

typedef struct {
    char id[64];
    char name[128];
    dpolicy_effect_t effect;
    char subject_pattern[256];
    char action_pattern[128];
    char resource_pattern[256];
    char condition_json[512];
    safety_priority_t priority;
    uint64_t valid_from;
    uint64_t valid_until;
    bool enabled;
} dpolicy_rule_t;

typedef struct {
    char version[32];
    dpolicy_rule_t* rules;
    size_t rule_count;
    uint64_t created_at;
    char* created_by;
    char* description;
} dpolicy_version_t;

typedef struct {
    char rule_a_id[64];
    char rule_b_id[64];
    dpolicy_conflict_strategy_t resolution;
    char reason[256];
} dpolicy_conflict_t;

typedef struct {
    dpolicy_change_type_t type;
    char rule_id[64];
    char* old_value_json;
    char* new_value_json;
    uint64_t timestamp;
    char* changed_by;
} dpolicy_change_record_t;

typedef struct dpolicy_engine_s dpolicy_engine_t;

typedef void (*dpolicy_change_callback_t)(const dpolicy_change_record_t* record,
                                            void* user_data);

dpolicy_engine_t* dpolicy_engine_create(dpolicy_conflict_strategy_t default_strategy);
void dpolicy_engine_destroy(dpolicy_engine_t* engine);

int dpolicy_engine_add_rule(dpolicy_engine_t* engine, const dpolicy_rule_t* rule);
int dpolicy_engine_remove_rule(dpolicy_engine_t* engine, const char* rule_id);
int dpolicy_engine_update_rule(dpolicy_engine_t* engine, const char* rule_id, const dpolicy_rule_t* new_rule);

dpolicy_effect_t dpolicy_engine_evaluate(dpolicy_engine_t* engine,
                                           const char* subject,
                                           const char* action,
                                           const char* resource,
                                           const char* context_json);

int dpolicy_engine_detect_conflicts(dpolicy_engine_t* engine,
                                      dpolicy_conflict_t** conflicts,
                                      size_t* conflict_count);

int dpolicy_engine_resolve_conflict(dpolicy_engine_t* engine,
                                      const dpolicy_conflict_t* conflict);

int dpolicy_engine_commit_version(dpolicy_engine_t* engine, const char* description);
int dpolicy_engine_rollback(dpolicy_engine_t* engine, const char* version);

int dpolicy_engine_load_policies_json(dpolicy_engine_t* engine, const char* json);
int dpolicy_engine_export_policies_json(dpolicy_engine_t* engine, char** json);

int dpolicy_engine_set_change_callback(dpolicy_engine_t* engine,
                                         dpolicy_change_callback_t callback,
                                         void* user_data);

int dpolicy_engine_validate_compliance(dpolicy_engine_t* engine,
                                         const char* standard,
                                         char** report_json);

size_t dpolicy_engine_get_rule_count(dpolicy_engine_t* engine);
size_t dpolicy_engine_get_version_count(dpolicy_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DYNAMIC_POLICY_ENGINE_H */
