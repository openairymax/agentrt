/**
 * @file cost_tracker.h
 * @brief 成本跟踪接口（支持配置定价）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LLM_COST_TRACKER_H
#define AGENTOS_LLM_COST_TRACKER_H

#include <cjson/cJSON.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *model_pattern; /* 支持通配符或前缀匹配，如 "gpt-4*" */
    double input_price_per_k;
    double output_price_per_k;
} pricing_rule_t;

typedef struct cost_tracker cost_tracker_t;

cost_tracker_t *cost_tracker_create(const pricing_rule_t *rules, int rule_count);
void cost_tracker_destroy(cost_tracker_t *ct);
void cost_tracker_add(cost_tracker_t *ct, const char *model, uint32_t prompt_tokens,
                      uint32_t completion_tokens);
cJSON *cost_tracker_export(cost_tracker_t *ct);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LLM_COST_TRACKER_H */