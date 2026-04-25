/**
 * @file triple_coordinator.h
 * @brief 三组件协调器 - t2/t1-f/t1-p 流式批判循环核心
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计依据: AgentOS双思维模型详细落地方案
 * - t2(Think2): 主思考，深度推理，持续输出token流
 * - t1-f(Fast-Think1): 快思考，流式验证，以语义单元为检查块
 * - t1-p(Pro-Think1): 专业思考，仲裁/专业领域审计
 *
 * 流式批判循环:
 * 1. t2生成语义单元 → 2. t1-f实时验证 →
 * 3. 若不通过: t2修正 → 4. 若多次不通过: t1-p仲裁
 */

#ifndef AGENTOS_TRIPLE_COORDINATOR_H
#define AGENTOS_TRIPLE_COORDINATOR_H

#include "agentos.h"
#include "thinking_chain.h"
#include "metacognition.h"
#include "semantic_unit.h"
#include "confidence_calibrator.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TC3_MAX_VERIFY_ROUNDS     3
#define TC3_MAX_ESCALATIONS       2
#define TC3_ACCEPT_THRESHOLD      0.7f
#define TC3_MINOR_FIX_THRESHOLD   0.5f

typedef enum {
    TC3_ROLE_T2 = 0,
    TC3_ROLE_T1F = 1,
    TC3_ROLE_T1P = 2
} tc3_role_t;

typedef enum {
    TC3_RESULT_ACCEPT = 0,
    TC3_RESULT_MINOR_FIX = 1,
    TC3_RESULT_MAJOR_FIX = 2,
    TC3_RESULT_ESCALATE = 3,
    TC3_RESULT_REJECT = 4
} tc3_verdict_t;

typedef struct {
    uint32_t unit_index;
    tc3_verdict_t verdict;
    float score;
    char* critique;
    size_t critique_len;
    tc3_role_t verified_by;
    uint32_t correction_attempts;
} tc3_unit_result_t;

typedef struct {
    char* corrected_text;
    size_t corrected_len;
    float new_confidence;
    tc3_role_t corrected_by;
} tc3_correction_t;

typedef agentos_error_t (*tc3_s2_generate_fn)(
    const char* input, size_t input_len,
    char** output, size_t* output_len,
    void* user_data);

typedef agentos_error_t (*tc3_s1_verify_fn)(
    const char* content, size_t content_len,
    float* out_score, int* out_acceptable,
    char** out_critique, size_t* out_critique_len,
    void* user_data);

typedef agentos_error_t (*tc3_s1_expert_fn)(
    const char* content, size_t content_len,
    const char* critique, size_t critique_len,
    float* out_score, tc3_verdict_t* out_verdict,
    char** out_expert_opinion, size_t* out_opinion_len,
    void* user_data);

typedef struct {
    uint32_t max_verify_rounds;
    uint32_t max_escalations;
    float accept_threshold;
    float minor_fix_threshold;
    float escalate_threshold;
    su_config_t stream_config;
    tc3_s2_generate_fn s2_generate;
    tc3_s1_verify_fn s1_verify;
    tc3_s1_expert_fn s1_expert;
    void* s2_user_data;
    void* s1_user_data;
    void* s1_expert_user_data;
} tc3_config_t;

#define TC3_CONFIG_DEFAULTS { \
    .max_verify_rounds = TC3_MAX_VERIFY_ROUNDS, \
    .max_escalations = TC3_MAX_ESCALATIONS, \
    .accept_threshold = TC3_ACCEPT_THRESHOLD, \
    .minor_fix_threshold = TC3_MINOR_FIX_THRESHOLD, \
    .escalate_threshold = 0.3f, \
    .stream_config = SU_CONFIG_DEFAULTS, \
    .s2_generate = NULL, \
    .s1_verify = NULL, \
    .s1_expert = NULL, \
    .s2_user_data = NULL, \
    .s1_user_data = NULL, \
    .s1_expert_user_data = NULL }

typedef struct {
    uint32_t total_units;
    uint32_t accepted_units;
    uint32_t minor_fix_units;
    uint32_t major_fix_units;
    uint32_t escalated_units;
    uint32_t rejected_units;
    uint32_t total_corrections;
    float avg_score;
    uint64_t total_time_ns;
} tc3_stats_t;

typedef struct tc3_coordinator tc3_coordinator_t;

struct tc3_coordinator {
    tc3_config_t config;
    su_stream_detector_t* detector;
    agentos_thinking_chain_t* chain;
    agentos_metacognition_t* meta;
    confidence_calibrator_t* calibrator;
    tc3_stats_t stats;
    tc3_unit_result_t* unit_results;
    size_t unit_results_capacity;
    size_t unit_results_count;
    int active;
};

AGENTOS_API agentos_error_t tc3_coordinator_create(
    const tc3_config_t* config,
    agentos_thinking_chain_t* chain,
    agentos_metacognition_t* meta,
    tc3_coordinator_t** out_coord);

AGENTOS_API void tc3_coordinator_destroy(tc3_coordinator_t* coord);

AGENTOS_API agentos_error_t tc3_coordinator_execute(
    tc3_coordinator_t* coord,
    const char* input,
    size_t input_len,
    char** out_output,
    size_t* out_output_len);

AGENTOS_API agentos_error_t tc3_coordinator_execute_streaming(
    tc3_coordinator_t* coord,
    const char* input,
    size_t input_len,
    char** out_output,
    size_t* out_output_len);

AGENTOS_API agentos_error_t tc3_coordinator_get_stats(
    const tc3_coordinator_t* coord,
    tc3_stats_t* out_stats);

AGENTOS_API agentos_error_t tc3_coordinator_reset(tc3_coordinator_t* coord);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TRIPLE_COORDINATOR_H */
