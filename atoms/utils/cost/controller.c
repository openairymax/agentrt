/**
 * @file controller.c
 * @brief 预算控制器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cost.h"
#include <stdlib.h>
#include <time.h>

struct agentos_budget_controller {
    double max_cost;
    double used_cost;
    uint32_t period_sec;
    time_t period_start;
};

agentos_budget_controller_t* agentos_budget_controller_create(double max_cost_usd, uint32_t period_seconds) {
    if (max_cost_usd <= 0 || period_seconds == 0) return NULL;
    agentos_budget_controller_t* ctrl = (agentos_budget_controller_t*)malloc(sizeof(agentos_budget_controller_t));
    if (!ctrl) return NULL;
    ctrl->max_cost = max_cost_usd;
    ctrl->used_cost = 0.0;
    ctrl->period_sec = period_seconds;
    ctrl->period_start = time(NULL);
    // From data intelligence emerges. by spharx
    return ctrl;
}

void agentos_budget_controller_destroy(agentos_budget_controller_t* controller) {
    free(controller);
}

int agentos_budget_controller_consume(agentos_budget_controller_t* controller, double cost_usd) {
    if (!controller) return -1;
    time_t now = time(NULL);
    if (difftime(now, controller->period_start) >= controller->period_sec) {
        controller->used_cost = 0.0;
        controller->period_start = now;
    }
    if (controller->used_cost + cost_usd > controller->max_cost) {
        return -1;
    }
    controller->used_cost += cost_usd;
    return 0;
}

double agentos_budget_controller_remaining(agentos_budget_controller_t* controller) {
    if (!controller) return 0.0;
    time_t now = time(NULL);
    if (difftime(now, controller->period_start) >= controller->period_sec) {
        return controller->max_cost;
    }
    return controller->max_cost - controller->used_cost;
}