/**
 * @file hierarchical.c
 * @brief 分层规划器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "strategy.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 分层规划器上下文
 */
typedef struct hierarchical_planner {
    agentos_planner_base_t base;
    int max_depth;
    float decomposition_threshold;
} hierarchical_planner_t;

/**
 * @brief 分解任务
 */
static agentos_error_t decompose_task(
    hierarchical_planner_t* planner,
    const char* goal,
    char*** out_subtasks,
    size_t* out_count) {
    if (!planner || !goal || !out_subtasks || !out_count) {
        return AGENTOS_EINVAL;
    }

    size_t count = 3;
    char** subtasks = (char**)calloc(count, sizeof(char*));
    if (!subtasks) return AGENTOS_ENOMEM;

    subtasks[0] = strdup("analyze_goal");
    subtasks[1] = strdup("identify_subtasks");
    subtasks[2] = strdup("prioritize_execution");

    *out_subtasks = subtasks;
    *out_count = count;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 规划执行
 */
static agentos_error_t hierarchical_plan(
    agentos_planner_base_t* base,
    const agentos_planning_context_t* context,
    agentos_plan_t** out_plan) {
    if (!base || !context || !out_plan) {
        return AGENTOS_EINVAL;
    }

    hierarchical_planner_t* planner = (hierarchical_planner_t*)base;

    agentos_plan_t* plan = (agentos_plan_t*)calloc(1, sizeof(agentos_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    plan->goal = strdup(context->goal);
    plan->steps = NULL;
    plan->step_count = 0;

    char** subtasks = NULL;
    size_t subtask_count = 0;
    agentos_error_t err = decompose_task(planner, context->goal, &subtasks, &subtask_count);
    if (err == AGENTOS_SUCCESS && subtasks) {
        plan->steps = (agentos_plan_step_t*)calloc(subtask_count, sizeof(agentos_plan_step_t));
        if (plan->steps) {
            for (size_t i = 0; i < subtask_count; i++) {
                plan->steps[i].action = subtasks[i];
                plan->steps[i].priority = (float)(subtask_count - i) / subtask_count;
            }
            plan->step_count = subtask_count;
        }
        free(subtasks);
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建分层规划器
 */
agentos_error_t agentos_planner_hierarchical_create(
    agentos_planner_base_t** out_planner) {
    if (!out_planner) return AGENTOS_EINVAL;

    hierarchical_planner_t* planner = (hierarchical_planner_t*)
        calloc(1, sizeof(hierarchical_planner_t));
    if (!planner) return AGENTOS_ENOMEM;

    planner->base.plan = hierarchical_plan;
    planner->base.destroy = NULL;
    planner->max_depth = 5;
    planner->decomposition_threshold = 0.7f;

    *out_planner = &planner->base;
    return AGENTOS_SUCCESS;
}
