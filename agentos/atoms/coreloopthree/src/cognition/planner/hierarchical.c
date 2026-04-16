/**
 * @file hierarchical.c
 * @brief 分层规划器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "cognition.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <agentos/memory.h>
#include <agentos/string.h>

typedef struct {
    const char* keyword;
    const char* domain;
    const char* subtasks[4];
    size_t subtask_count;
} domain_rule_t;

static const domain_rule_t g_domain_rules[] = {
    {"code", "code", {"analyze_requirements", "design_structure", "implement_code", "test_verify"}, 4},
    {"data", "data", {"collect_data", "clean_data", "analyze_patterns", "generate_report"}, 4},
    {"analyze", "analysis", {"gather_context", "extract_features", "apply_model", "interpret_results"}, 4},
    {"file", "file", {"locate_file", "read_content", "process_content", "write_result"}, 4},
    {"search", "search", {"formulate_query", "execute_search", "rank_results", "summarize_findings"}, 4},
    {"write", "writing", {"research_topic", "outline_structure", "draft_content", "review_polish"}, 4},
};
static const size_t g_domain_rule_count = sizeof(g_domain_rules) / sizeof(g_domain_rules[0]);

static const char* g_default_subtasks[] = {
    "analyze_goal", "identify_subtasks", "plan_execution", "execute_primary", "verify_result"
};
static const size_t g_default_count = sizeof(g_default_subtasks) / sizeof(g_default_subtasks[0]);

typedef struct {
    int max_depth;
    float decomposition_threshold;
} hierarchical_data_t;

static int str_contains_i(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    size_t needle_len = strlen(needle);
    size_t hay_len = strlen(haystack);
    if (needle_len > hay_len) return 0;
    for (size_t i = 0; i <= hay_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if ((int)tolower((unsigned char)haystack[i + j]) != (int)tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

static const domain_rule_t* match_domain(const char* goal) {
    if (!goal) return NULL;
    for (size_t i = 0; i < g_domain_rule_count; i++) {
        if (str_contains_i(goal, g_domain_rules[i].keyword)) {
            return &g_domain_rules[i];
        }
    }
    return NULL;
}

static agentos_error_t hierarchical_plan_func(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {
    (void)intent;
    if (!context || !out_plan) return AGENTOS_EINVAL;

    hierarchical_data_t* data = (hierarchical_data_t*)context;

    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    const domain_rule_t* rule = match_domain(intent ? intent->intent_goal : NULL);
    const char* const* task_names = rule ? rule->subtasks : g_default_subtasks;
    size_t count = rule ? rule->subtask_count : g_default_count;

    if (count > 0) {
        plan->task_plan_nodes = (agentos_task_node_t**)AGENTOS_CALLOC(count, sizeof(agentos_task_node_t*));
        if (!plan->task_plan_nodes) {
            AGENTOS_FREE(plan);
            return AGENTOS_ENOMEM;
        }
        size_t actual_count = 0;
        for (size_t i = 0; i < count; i++) {
            agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
            if (node) {
                node->task_node_id = AGENTOS_STRDUP(task_names[i]);
                if (!node->task_node_id) {
                    AGENTOS_FREE(node);
                    continue;
                }
                plan->task_plan_nodes[actual_count] = node;
                actual_count++;
            }
        }
        plan->task_plan_node_count = actual_count;
    }

    (void)data;
    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

static void hierarchical_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    if (strategy->data) AGENTOS_FREE(strategy->data);
    AGENTOS_FREE(strategy);
}

agentos_plan_strategy_t* agentos_plan_hierarchical_create(
    agentos_llm_service_t* llm,
    int max_depth) {
    (void)llm;

    hierarchical_data_t* data = (hierarchical_data_t*)AGENTOS_CALLOC(1, sizeof(hierarchical_data_t));
    if (!data) return NULL;
    data->max_depth = max_depth > 0 ? max_depth : 5;
    data->decomposition_threshold = 0.7f;

    agentos_plan_strategy_t* strategy = (agentos_plan_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_plan_strategy_t));
    if (!strategy) {
        AGENTOS_FREE(data);
        return NULL;
    }

    strategy->plan = hierarchical_plan_func;
    strategy->destroy = hierarchical_destroy;
    strategy->data = data;

    return strategy;
}
