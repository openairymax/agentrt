/**
 * @file budget.c
 * @brief Token预算管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "token.h"
#include <stdlib.h>

struct agentos_token_budget {
    size_t max_tokens;
    size_t used_tokens;
};

agentos_token_budget_t* agentos_token_budget_create(size_t max_tokens) {
    if (max_tokens == 0) return NULL;
    agentos_token_budget_t* budget = (agentos_token_budget_t*)malloc(sizeof(agentos_token_budget_t));
    if (!budget) return NULL;
    budget->max_tokens = max_tokens;
    budget->used_tokens = 0;
    return budget;
}

void agentos_token_budget_destroy(agentos_token_budget_t* budget) {
    free(budget);
}

int agentos_token_budget_add(agentos_token_budget_t* budget, size_t input_tokens, size_t output_tokens) {
    if (!budget) return -1;
    if (budget->used_tokens + input_tokens + output_tokens > budget->max_tokens) {
        return -1;
    }
    budget->used_tokens += input_tokens + output_tokens;
    return 0;
}

size_t agentos_token_budget_remaining(agentos_token_budget_t* budget) {
    if (!budget) return 0;
    return budget->max_tokens - budget->used_tokens;
}

void agentos_token_budget_reset(agentos_token_budget_t* budget) {
    if (budget) budget->used_tokens = 0;
}