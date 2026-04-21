/**
 * @file reactive.c
 * @brief 反应式规划策略：基于意图快速生成动态计划
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 修复: 硬编码单节点 → 基于意图内容动态生成多节点计划
 * 支持LLM调用（可用时）+ 规则降级（不可用时）
 */

#include "cognition.h"
#include "llm_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "include/memory_compat.h"
#include "string_compat.h"

typedef struct reactive_data {
    agentos_llm_service_t* llm;
    char* model_name;
    agentos_mutex_t* lock;
    uint64_t plan_counter;
} reactive_data_t;

typedef struct {
    const char* keyword;
    const char* role;
    const char* action;
    int priority;
    int timeout_ms;
} reactive_rule_t;

static const reactive_rule_t REACTIVE_RULES[] = {
    {"query", "retriever", "retrieve_and_answer", 200, 15000},
    {"search", "retriever", "search_and_rank", 200, 20000},
    {"find", "retriever", "find_and_present", 200, 15000},
    {"create", "creator", "create_artifact", 180, 30000},
    {"generate", "creator", "generate_content", 180, 30000},
    {"write", "creator", "write_output", 180, 25000},
    {"analyze", "analyst", "analyze_and_report", 190, 25000},
    {"compare", "analyst", "compare_and_contrast", 190, 20000},
    {"evaluate", "analyst", "evaluate_and_score", 190, 20000},
    {"execute", "executor", "execute_action", 220, 15000},
    {"run", "executor", "run_command", 220, 10000},
    {"delete", "executor", "delete_resource", 230, 10000},
    {"update", "executor", "update_resource", 220, 15000},
    {"translate", "translator", "translate_content", 170, 20000},
    {"summarize", "summarizer", "summarize_content", 170, 15000},
    {"explain", "explainer", "explain_concept", 170, 20000},
};

#define REACTIVE_RULE_COUNT (sizeof(REACTIVE_RULES) / sizeof(REACTIVE_RULES[0]))

static int match_rules(const char* goal, size_t goal_len,
                       int* out_indices, int max_matches) {
    if (!goal || goal_len == 0) return 0;
    int count = 0;
    for (size_t r = 0; r < REACTIVE_RULE_COUNT && count < max_matches; r++) {
        if (strstr(goal, REACTIVE_RULES[r].keyword) != NULL) {
            out_indices[count++] = (int)r;
        }
    }
    return count;
}

static void reactive_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    reactive_data_t* data = (reactive_data_t*)strategy->data;
    if (data) {
        if (data->model_name) AGENTOS_FREE(data->model_name);
        if (data->lock) agentos_mutex_destroy(data->lock);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(strategy);
}

static agentos_error_t reactive_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    reactive_data_t* data = (reactive_data_t*)context;
    if (!intent || !out_plan) return AGENTOS_EINVAL;

    const char* goal = intent->intent_goal ? (const char*)intent->intent_goal : "";
    size_t goal_len = intent->intent_goal_len;
    int complexity = (int)(intent->intent_flags & 0x07);

    /* 匹配意图规则 */
    int matched[8];
    int match_count = match_rules(goal, goal_len, matched, 8);

    /* 尝试LLM快速规划 */
    char* llm_plan = NULL;
    if (data && data->llm && agentos_llm_service_is_available(data->llm)) {
        char prompt[1024];
        snprintf(prompt, sizeof(prompt),
            "Generate a brief action plan (max 5 steps) for: %s\n"
            "Format: one action per line, no numbering.",
            goal_len > 500 ? "(long input)" : goal);
        agentos_error_t llm_err = agentos_llm_service_call(data->llm, prompt, &llm_plan);
        if (llm_err != AGENTOS_SUCCESS || !llm_plan) {
            llm_plan = NULL;
        }
    }

    /* 确定节点数量 */
    size_t node_count = 0;
    if (llm_plan) {
        for (size_t i = 0; llm_plan[i]; i++) {
            if (llm_plan[i] == '\n') node_count++;
        }
        if (node_count == 0 && strlen(llm_plan) > 0) node_count = 1;
        if (node_count > 8) node_count = 8;
    }

    if (node_count == 0) {
        if (match_count > 0) {
            node_count = (size_t)match_count;
            if (complexity >= 4) node_count += 1;
        } else {
            node_count = (complexity >= 3) ? 3 : 1;
        }
    }

    /* 构建计划 */
    agentos_task_plan_t* plan = (agentos_task_plan_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_plan_t));
    if (!plan) { if (llm_plan) AGENTOS_FREE(llm_plan); return AGENTOS_ENOMEM; }

    uint64_t counter = 0;
    if (data) {
        agentos_mutex_lock(data->lock);
        data->plan_counter++;
        counter = data->plan_counter;
        agentos_mutex_unlock(data->lock);
    }

    char plan_id[64];
    snprintf(plan_id, sizeof(plan_id), "reactive_%llu", (unsigned long long)counter);
    plan->task_plan_id = AGENTOS_STRDUP(plan_id);

    plan->task_plan_nodes = (agentos_task_node_t**)AGENTOS_CALLOC(node_count, sizeof(agentos_task_node_t*));
    if (!plan->task_plan_nodes && node_count > 0) {
        AGENTOS_FREE(plan->task_plan_id); AGENTOS_FREE(plan);
        if (llm_plan) AGENTOS_FREE(llm_plan);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < node_count; i++) {
        agentos_task_node_t* node = (agentos_task_node_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_node_t));
        if (!node) goto cleanup;

        char nid[128];
        snprintf(nid, sizeof(nid), "%s_step%zu", plan_id, i + 1);
        node->task_node_id = AGENTOS_STRDUP(nid);

        if (llm_plan && i < node_count) {
            node->task_node_agent_role = AGENTOS_STRDUP("reactive-agent");
            node->task_node_timeout_ms = 20000;
            node->task_node_priority = 180 - (int)i * 10;
        } else if (i < (size_t)match_count) {
            const reactive_rule_t* rule = &REACTIVE_RULES[matched[i]];
            node->task_node_agent_role = AGENTOS_STRDUP(rule->role);
            node->task_node_timeout_ms = rule->timeout_ms;
            node->task_node_priority = rule->priority;
        } else {
            const char* roles[] = {"processor", "validator", "formatter"};
            node->task_node_agent_role = AGENTOS_STRDUP(roles[i % 3]);
            node->task_node_timeout_ms = 15000;
            node->task_node_priority = 150 - (int)i * 10;
        }

        if (i > 0) {
            node->task_node_depends_on = (char**)AGENTOS_MALLOC(sizeof(char*));
            if (node->task_node_depends_on) {
                node->task_node_depends_count = 1;
                node->task_node_depends_on[0] = AGENTOS_STRDUP(plan->task_plan_nodes[i - 1]->task_node_id);
            }
        }

        plan->task_plan_nodes[i] = node;
        plan->task_plan_node_count++;
    }

    plan->task_plan_entry_points = (char**)AGENTOS_MALLOC(sizeof(char*));
    if (plan->task_plan_entry_points && plan->task_plan_node_count > 0) {
        plan->task_plan_entry_count = 1;
        plan->task_plan_entry_points[0] = AGENTOS_STRDUP(plan->task_plan_nodes[0]->task_node_id);
    }

    if (llm_plan) AGENTOS_FREE(llm_plan);
    *out_plan = plan;
    return AGENTOS_SUCCESS;

cleanup:
    for (size_t n = 0; n < plan->task_plan_node_count; n++) {
        if (plan->task_plan_nodes[n]) {
            AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_id);
            AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_agent_role);
            if (plan->task_plan_nodes[n]->task_node_depends_on) {
                for (size_t d = 0; d < plan->task_plan_nodes[n]->task_node_depends_count; d++)
                    AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_depends_on[d]);
                AGENTOS_FREE(plan->task_plan_nodes[n]->task_node_depends_on);
            }
            AGENTOS_FREE(plan->task_plan_nodes[n]);
        }
    }
    AGENTOS_FREE(plan->task_plan_nodes);
    AGENTOS_FREE(plan->task_plan_id);
    AGENTOS_FREE(plan);
    if (llm_plan) AGENTOS_FREE(llm_plan);
    return AGENTOS_ENOMEM;
}

agentos_plan_strategy_t* agentos_plan_reactive_create(agentos_llm_service_t* llm) {
    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)AGENTOS_CALLOC(1, sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    reactive_data_t* rdata = (reactive_data_t*)AGENTOS_CALLOC(1, sizeof(reactive_data_t));
    if (!rdata) { AGENTOS_FREE(strat); return NULL; }

    rdata->llm = llm;
    rdata->lock = agentos_mutex_create();
    if (!rdata->lock) { AGENTOS_FREE(rdata); AGENTOS_FREE(strat); return NULL; }

    strat->plan = reactive_plan;
    strat->destroy = reactive_destroy;
    strat->data = rdata;

    return strat;
}
