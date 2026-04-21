/**
 * @file metacognition.c
 * @brief 元认知模块完整实现 - DS-002
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 实现双思考系统S1验证角色的核心逻辑：
 * - 5维度评估（相关性/准确性/完整性/一致性/清晰度）
 * - 置信度校准（历史偏差跟踪）
 * - 纠错策略选择与执行
 * - 自我纠错模式检测
 */

#include "metacognition.h"
#include "../../../atoms/corekern/include/agentos.h"
#include "include/memory_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

/* ============================================================================
 * 内部辅助
 * ============================================================================ */

static uint64_t mc_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static const char* dim_name(mc_dimension_t d) {
    static const char* names[] = {"relevance", "accuracy", "completeness", "consistency", "clarity"};
    return names[(int)d < MC_DIM_COUNT ? (int)d : 0];
}

/* ============================================================================
 * 创建/销毁
 * ============================================================================ */

agentos_error_t agentos_mc_create(agentos_metacognition_t** out_mc) {
    if (!out_mc) return AGENTOS_EINVAL;

    agentos_metacognition_t* mc = (agentos_metacognition_t*)AGENTOS_CALLOC(1, sizeof(agentos_metacognition_t));
    if (!mc) return AGENTOS_ENOMEM;

    mc->acceptance_threshold = 0.7f;
    mc->auto_correct_threshold = 0.5f;
    mc->enable_confidence_calibration = 1;
    mc->enable_learning = 1;

    mc->record_capacity = MC_MAX_HISTORY_RECORDS;
    mc->records = (mc_evaluation_record_t*)AGENTOS_CALLOC(mc->record_capacity, sizeof(mc_evaluation_record_t));
    if (!mc->records) { AGENTOS_FREE(mc); return AGENTOS_ENOMEM; }
    mc->record_count = 0;
    mc->record_head = 0;

    memset(&mc->calibrator, 0, sizeof(mc_calibrator_t));

    mc->total_evaluations = 0;
    mc->total_corrections = 0;
    mc->total_rejections = 0;
    mc->total_auto_fixes = 0;
    mc->total_rerun_successes = 0;
    mc->chain = NULL;

    /* DS-005: 初始化学习系统 */
    memset(mc->patterns, 0, sizeof(mc->patterns));
    mc->pattern_count = 0;
    mc->adaptive_acceptance_threshold = mc->acceptance_threshold;
    mc->consecutive_accepts = 0;
    mc->consecutive_rejects = 0;
    mc->patterns_detected = 0;
    mc->preemptive_corrections = 0;
    mc->learning_effectiveness = 0.0f;

    *out_mc = mc;
    return AGENTOS_SUCCESS;
}

void agentos_mc_destroy(agentos_metacognition_t* mc) {
    if (!mc) return;
    for (size_t i = 0; i < mc->record_count; i++) {
        if (mc->records[i].result.critique_text)
            AGENTOS_FREE((void*)mc->records[i].result.critique_text);
    }
    AGENTOS_FREE(mc->records);
    AGENTOS_FREE(mc);
}

void agentos_mc_set_chain(agentos_metacognition_t* mc, agentos_thinking_chain_t* chain) {
    if (!mc) return;
    mc->chain = chain;
}

/* ============================================================================
 * 核心评估逻辑
 * ============================================================================ */

static float score_relevance(const char* input, size_t in_len,
                              const char* output, size_t out_len) {
    if (!input || !output || in_len == 0 || out_len == 0) return 0.3f;

    int match_count = 0;
    int check_words = 0;

    for (size_t i = 0; i < in_len && check_words < 10; i++) {
        if (input[i] == ' ' || i == in_len - 1) {
            check_words++;
            size_t word_start = i;
            while (word_start > 0 && input[word_start - 1] != ' ') word_start--;
            size_t word_len = i - word_start + (i == in_len - 1 && input[i] != ' ' ? 1 : 0);

            if (word_len >= 3 && word_len <= 30) {
                char found = 0;
                for (size_t j = 0; j + word_len <= out_len && !found; j++) {
                    if (strncmp(output + j, input + word_start, word_len) == 0 &&
                        (j == 0 || output[j-1] == ' ') &&
                        (j + word_len >= out_len || output[j+word_len] == ' ' ||
                         output[j+word_len] == '.' || output[j+word_len] == ',')) {
                        match_count++;
                        found = 1;
                    }
                }
            }
        }
    }

    if (check_words == 0) return 0.8f;
    return clampf((float)match_count / (float)(check_words > 0 ? check_words : 1), 0.05f, 1.0f);
}

static float score_accuracy(const char* content, size_t len) {
    if (!content || len == 0) return 0.3f;

    int suspicious_patterns = 0;
    int total_checks = 4;

    const char* patterns[] = {
        "I think maybe possibly",
        "I am not sure but",
        "it could be that perhaps",
        "I guess"
    };

    for (int p = 0; p < total_checks; p++) {
        if (strstr(content, patterns[p])) suspicious_patterns++;
    }

    float base_score = 0.9f - (float)suspicious_patterns * 0.15f;

    int has_evidence = strstr(content, "because") || strstr(content, "since") ||
                      strstr(content, "according to") || strstr(content, "evidence");
    if (has_evidence) base_score += 0.05f;

    return clampf(base_score, 0.1f, 1.0f);
}

static float score_completeness(const char* content, size_t len) {
    if (!content || len == 0) return 0.2f;

    int aspects = 0;
    if (strstr(content, "first") || strstr(content, "initially")) aspects++;
    if (strstr(content, "second") || strstr(content, "then")) aspects++;
    if (strstr(content, "finally") || strstr(content, "conclusion") ||
        strstr(content, "in summary")) aspects++;
    if (len > 100) aspects++;

    return clampf(0.2f + (float)aspects * 0.2f, 0.2f, 1.0f);
}

static float score_consistency(const char* context, size_t ctx_len,
                                const char* content, size_t cnt_len) {
    if (!context || !content || ctx_len == 0 || cnt_len == 0) return 0.7f;

    int contradictions = 0;
    const char* contra_patterns[] = {"however", "but on the other hand", "contrary to", "despite this"};

    for (int p = 0; p < 4; p++) {
        if (strstr(content, contra_patterns[p])) contradictions++;
    }

    float score = 0.85f - (float)contradictions * 0.15f;

    if (ctx_len > 20 && cnt_len > 20) {
        size_t min_len = ctx_len < cnt_len ? ctx_len : cnt_len;
        int matches = 0;
        for (size_t i = 0; i + 5 < min_len; i += 6) {
            if (context[i] == content[i]) matches++;
        }
        if (matches > min_len / 12) score += 0.05f;
    }

    return clampf(score, 0.2f, 1.0f);
}

static float score_clarity(const char* content, size_t len) {
    if (!content || len == 0) return 0.3f;

    int sentences = 0;
    int long_sentences = 0;
    int current_sentence_len = 0;

    for (size_t i = 0; i < len; i++) {
        current_sentence_len++;
        if (content[i] == '.' || content[i] == '!' || content[i] == '?') {
            sentences++;
            if (current_sentence_len > 50) long_sentences++;
            current_sentence_len = 0;
        }
    }

    if (sentences == 0) return 0.4f;

    float clarity = 1.0f - (float)long_sentences / (float)sentences * 0.3f;

    int has_structure = (strstr(content, "First") || strstr(content, "1.")) &&
                       (strstr(content, "Second") || strstr(content, "2."));
    if (has_structure) clarity += 0.1f;

    return clampf(clarity, 0.2f, 1.0f);
}

agentos_error_t agentos_mc_evaluate_step(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    const char* context,
    size_t context_len,
    mc_evaluation_result_t* out_result) {

    if (!mc || !step || !step->content || !out_result) return AGENTOS_EINVAL;

    memset(out_result, 0, sizeof(mc_evaluation_result_t));
    mc->total_evaluations++;

    const char* input = step->raw_input ? step->raw_input : "";
    size_t in_len = step->raw_input_len;
    const char* content = step->content;
    size_t cnt_len = step->content_len;

    /* 五维度评估 */
    out_result->dimensions[MC_DIM_RELEVANCE].dimension = MC_DIM_RELEVANCE;
    out_result->dimensions[MC_DIM_RELEVANCE].score = score_relevance(input, in_len, content, cnt_len);

    out_result->dimensions[MC_DIM_ACCURACY].dimension = MC_DIM_ACCURACY;
    out_result->dimensions[MC_DIM_ACCURACY].score = score_accuracy(content, cnt_len);

    out_result->dimensions[MC_DIM_COMPLETENESS].dimension = MC_DIM_COMPLETENESS;
    out_result->dimensions[MC_DIM_COMPLETENESS].score = score_completeness(content, cnt_len);

    out_result->dimensions[MC_DIM_CONSISTENCY].dimension = MC_DIM_CONSISTENCY;
    out_result->dimensions[MC_DIM_CONSISTENCY].score = score_consistency(context, context_len, content, cnt_len);

    out_result->dimensions[MC_DIM_CLARITY].dimension = MC_DIM_CLARITY;
    out_result->dimensions[MC_DIM_CLARITY].score = score_clarity(content, cnt_len);

    /* 综合评分（加权平均） */
    float weights[MC_DIM_COUNT] = {0.25f, 0.25f, 0.15f, 0.2f, 0.15f};
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;
    for (int d = 0; d < MC_DIM_COUNT; d++) {
        weighted_sum += out_result->dimensions[d].score * weights[d];
        weight_total += weights[d];
    }
    out_result->overall_score = (weight_total > 0.0f) ? weighted_sum / weight_total : 0.0f;

    /* 应用置信度校准 */
    out_result->calibrated_confidence = agentos_mc_calibrate_confidence(mc, step->confidence);

    /* 决定可接受性和纠正策略 */
    out_result->is_acceptable = (out_result->overall_score >= mc->acceptance_threshold) ? 1 : 0;

    /* DS-005: 跟踪自适应阈值统计 */
    if (out_result->is_acceptable) {
        mc->consecutive_accepts++;
        mc->consecutive_rejects = 0;
    } else {
        mc->consecutive_rejects++;
        mc->consecutive_accepts = 0;
    }

    if (out_result->overall_score >= mc->acceptance_threshold) {
        out_result->strategy = MC_CORRECT_NONE;
        out_result->severity = MC_SEV_INFO;
    } else if (out_result->overall_score >= mc->auto_correct_threshold) {
        out_result->strategy = MC_CORRECT_AUTO;
        out_result->severity = MC_SEV_WARNING;
    } else if (out_result->overall_score >= 0.3f) {
        out_result->strategy = MC_CORRECT_RERUN;
        out_result->severity = MC_SEV_ERROR;
    } else {
        out_result->strategy = MC_CORRECT_ESCALATE;
        out_result->severity = MC_SEV_CRITICAL;
    }

    /* 生成批判意见文本 */
    char buf[MC_MAX_CRITIQUE_LEN];
    int pos = snprintf(buf, sizeof(buf), "[S1 Evaluation] step#%u: ", step->step_id);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "overall=%.2f [", out_result->overall_score);
    for (int d = 0; d < MC_DIM_COUNT; d++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s=%.2f%s",
                        dim_name((mc_dimension_t)d),
                        out_result->dimensions[d].score,
                        (d < MC_DIM_COUNT - 1) ? "," : "]");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, " strategy=%d severity=%d",
                    out_result->strategy, out_result->severity);

    out_result->critique_text = (char*)AGENTOS_MALLOC(pos + 1);
    if (out_result->critique_text) {
        memcpy(out_result->critique_text, buf, pos + 1);
        out_result->critique_len = (size_t)pos;
    }

    /* 记录到历史 */
    if (mc->record_capacity > 0) {
        mc_evaluation_record_t* rec = &mc->records[mc->record_head % mc->record_capacity];
        if (rec->result.critique_text)
            AGENTOS_FREE((void*)rec->result.critique_text);
        rec->step_id = step->step_id;
        rec->timestamp_ns = mc_time_now();
        memcpy(&rec->result, out_result, sizeof(mc_evaluation_result_t));
        rec->original_content = content;
        rec->corrected_content = NULL;

        if (mc->record_count < mc->record_capacity)
            mc->record_count++;
        mc->record_head++;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_mc_evaluate_quick(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    float* out_score,
    int* out_acceptable) {

    if (!mc || !step || !out_score || !out_acceptable) return AGENTOS_EINVAL;

    mc_evaluation_result_t result;
    agentos_error_t err = agentos_mc_evaluate_step(mc, step, NULL, 0, &result);
    if (err != AGENTOS_SUCCESS) return err;

    *out_score = result.overall_score;
    *out_acceptable = result.is_acceptable;

    if (result.critique_text) AGENTOS_FREE(result.critique_text);
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 纠错执行
 * ============================================================================ */

agentos_error_t agentos_mc_apply_correction(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    const mc_evaluation_result_t* eval,
    agentos_error_t (*corrector_fn)(const char*, size_t, char**, size_t*, void*),
    void* user_data) {

    if (!mc || !step || !eval) return AGENTOS_EINVAL;

    switch (eval->strategy) {
    case MC_CORRECT_NONE:
        return AGENTOS_SUCCESS;

    case MC_CORRECT_AUTO: {
        if (!corrector_fn) return AGENTOS_EINVAL;
        char* corrected = NULL;
        size_t corr_len = 0;
        agentos_error_t err = corrector_fn(step->raw_input, step->raw_input_len,
                                           &corrected, &corr_len, user_data);
        if (err == AGENTOS_SUCCESS && corrected && corr_len > 0) {
            agentos_tc_step_correct(step, corrected, corr_len);
            AGENTOS_FREE(corrected);
            mc->total_auto_fixes++;
            mc->total_corrections++;

            if (mc->chain && mc->chain->on_correction) {
                mc->chain->on_correction(step, eval->critique_text, mc->chain->callback_user_data);
            }
        }
        return err;
    }

    case MC_CORRECT_RERUN: {
        mc->total_corrections++;
        if (!corrector_fn) { step->status = TC_STATUS_FAILED; mc->total_rejections++; return AGENTOS_EPERM; }

        const int max_retries = 3;
        agentos_error_t last_err = AGENTOS_EPERM;
        for (int attempt = 0; attempt < max_retries; attempt++) {
            char* corrected = NULL;
            size_t corr_len = 0;

            char* enhanced_input = NULL;
            size_t enhanced_len = 0;
            if (attempt > 0 && eval->critique_text) {
                enhanced_len = step->raw_input_len + strlen(eval->critique_text) + 64;
                enhanced_input = (char*)AGENTOS_CALLOC(1, enhanced_len);
                if (enhanced_input) {
                    snprintf(enhanced_input, enhanced_len,
                             "[Original]\n%s\n[Critique #%d: %s]\n[Instruction: Improve based on critique above]",
                             step->raw_input, attempt, eval->critique_text);
                }
            }
            const char* input_data = enhanced_input ? enhanced_input : step->raw_input;
            size_t input_len = enhanced_input ? strlen(enhanced_input) : step->raw_input_len;

            last_err = corrector_fn(input_data, input_len, &corrected, &corr_len, user_data);
            if (enhanced_input) AGENTOS_FREE(enhanced_input);

            if (last_err == AGENTOS_SUCCESS && corrected && corr_len > 0) {
                agentos_tc_step_correct(step, corrected, corr_len);
                AGENTOS_FREE(corrected);
                if (mc->chain && mc->chain->on_correction) {
                    mc->chain->on_correction(step, eval->critique_text, mc->chain->callback_user_data);
                }
                mc->total_rerun_successes++;
                return AGENTOS_SUCCESS;
            }
            if (corrected) AGENTOS_FREE(corrected);

            if (attempt < max_retries - 1) {
                struct timespec ts = { .tv_sec = 1 << attempt, .tv_nsec = 0 };
                nanosleep(&ts, NULL);
            }
        }

        step->status = TC_STATUS_FAILED;
        mc->total_rejections++;
        return last_err;
    }

    case MC_CORRECT_ESCALATE:
        mc->total_rejections++;
        step->status = TC_STATUS_FAILED;
        AGENTOS_LOG_ERROR("MC_ESCALATE: step=%p strategy=ESCALATE overall=%.2f conf=%.2f acceptable=%d",
                          (void*)step,
                          eval ? eval->overall_score : 0.0f,
                          eval ? eval->calibrated_confidence : 0.0f,
                          eval ? eval->is_acceptable : 0);
        return AGENTOS_EPERM;

    default:
        return AGENTOS_EINVAL;
    }
}

int agentos_mc_should_self_correct(agentos_metacognition_t* mc, tc_step_type_t step_type) {
    if (!mc || mc->record_count < 3) return 0;

    size_t recent_failures = 0;
    size_t check_count = (mc->record_count > 10) ? 10 : mc->record_count;

    for (size_t i = 0; i < check_count; i++) {
        size_t idx = (mc->record_head - 1 - i + mc->record_capacity) % mc->record_capacity;
        if (idx >= mc->record_count) break;
        if (mc->records[idx].result.strategy != MC_CORRECT_NONE)
            recent_failures++;
    }

    return (recent_failures >= check_count / 2) ? 1 : 0;
}

/* ============================================================================
 * 置信度校准
 * ============================================================================ */

float agentos_mc_calibrate_confidence(agentos_metacognition_t* mc, float raw_confidence) {
    if (!mc || !mc->enable_confidence_calibration) return raw_confidence;

    raw_confidence = clampf(raw_confidence, 0.0f, 1.0f);

    if (mc->calibrator.calibration_count < 5) return raw_confidence;

    float bias = (mc->calibrator.calibration_count > 0)
                 ? mc->calibrator.calibration_sum / (float)mc->calibrator.calibration_count
                 : 0.0f;

    float calibrated = raw_confidence - bias * 0.5f;
    calibrated = clampf(calibrated, 0.05f, 0.99f);

    if (calibrated < 0.3f && raw_confidence > 0.7f) {
        mc->calibrator.overconfidence_rate += (1.0f / (float)(mc->calibrator.calibration_count + 1));
    } else if (calibrated > 0.7f && raw_confidence < 0.3f) {
        mc->calibrator.underconfidence_rate += (1.0f / (float)(mc->calibrator.calibration_count + 1));
    }

    return calibrated;
}

agentos_error_t agentos_mc_feedback(
    agentos_metacognition_t* mc,
    float predicted_confidence,
    int was_correct) {

    if (!mc) return AGENTOS_EINVAL;
    if (!mc->enable_confidence_calibration) return AGENTOS_SUCCESS;

    float actual = was_correct ? 1.0f : 0.0f;
    float error = predicted_confidence - actual;

    mc->calibrator.calibration_sum += error;
    mc->calibrator.calibration_count++;
    mc->calibrator.last_calibration_error = error;

    size_t idx = mc->calibrator.history_index % MC_CALIBRATION_WINDOW;
    mc->calibrator.history[idx].predicted = predicted_confidence;
    mc->calibrator.history[idx].actual = actual;
    mc->calibrator.history_index++;

    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 统计与诊断
 * ============================================================================ */

agentos_error_t agentos_mc_stats(agentos_metacognition_t* mc, char** out_json) {
    if (!mc || !out_json) return AGENTOS_EINVAL;

    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "{\"evaluations\":%llu,"
        "\"corrections\":%llu,"
        "\"rejections\":%llu,"
        "\"auto_fixes\":%llu,"
        "\"calibration\":{\"samples\":%zu,\"bias\":%.4f,"
        "\"overconf_rate\":%.4f,\"underconf_rate\":%.4f},"
        "\"records\":%zu}",
        (unsigned long long)mc->total_evaluations,
        (unsigned long long)mc->total_corrections,
        (unsigned long long)mc->total_rejections,
        (unsigned long long)mc->total_auto_fixes,
        mc->calibrator.calibration_count,
        mc->calibrator.calibration_count > 0 ?
            mc->calibrator.calibration_sum / (float)mc->calibrator.calibration_count : 0.0f,
        mc->calibrator.overconfidence_rate,
        mc->calibrator.underconfidence_rate,
        mc->record_count);

    char* result = AGENTOS_STRDUP(buf);
    if (!result) return AGENTOS_ENOMEM;
    *out_json = result;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_mc_get_history(
    agentos_metacognition_t* mc,
    size_t count,
    mc_evaluation_record_t** out_records,
    size_t* out_count) {

    if (!mc || !out_records || !out_count) return AGENTOS_EINVAL;

    size_t avail = (count < mc->record_count) ? count : mc->record_count;
    *out_records = &mc->records[(mc->record_head - avail + mc->record_capacity) % mc->record_capacity];
    *out_count = avail;
    return AGENTOS_SUCCESS;
}

void agentos_mc_reset(agentos_metacognition_t* mc) {
    if (!mc) return;
    for (size_t i = 0; i < mc->record_count; i++) {
        if (mc->records[i].result.critique_text)
            AGENTOS_FREE((void*)mc->records[i].result.critique_text);
    }
    mc->record_count = 0;
    mc->record_head = 0;
    memset(&mc->calibrator, 0, sizeof(mc_calibrator_t));
    mc->total_evaluations = 0;
    mc->total_corrections = 0;
    mc->total_rejections = 0;
    mc->total_auto_fixes = 0;
    mc->total_rerun_successes = 0;

    /* DS-005: 重置学习状态 */
    memset(mc->patterns, 0, sizeof(mc->patterns));
    mc->pattern_count = 0;
    mc->adaptive_acceptance_threshold = mc->acceptance_threshold;
    mc->consecutive_accepts = 0;
    mc->consecutive_rejects = 0;
    mc->patterns_detected = 0;
    mc->preemptive_corrections = 0;
    mc->learning_effectiveness = 0.0f;
}

/* ============================================================================
 * DS-005: 持久学习能力实现
 * ============================================================================ */

static const char* step_type_name(tc_step_type_t t) {
    static const char* names[] = {
        "decomposition", "planning", "generation",
        "verification", "correction", "audit", "alignment"
    };
    int idx = (int)t;
    return (idx >= 0 && idx < 7) ? names[idx] : "unknown";
}

static const char* dim_short_name(mc_dimension_t d) {
    static const char* names[] = {"rel", "acc", "cmp", "con", "clr"};
    return names[(int)d < MC_DIM_COUNT ? (int)d : 0];
}

static size_t find_or_create_pattern(agentos_metacognition_t* mc, const char* key) {
    for (size_t i = 0; i < mc->pattern_count; i++) {
        if (strncmp(mc->patterns[i].pattern_key, key, 127) == 0)
            return i;
    }
    if (mc->pattern_count >= MC_MAX_PATTERNS) return MC_MAX_PATTERNS;
    size_t idx = mc->pattern_count++;
    memset(&mc->patterns[idx], 0, sizeof(mc_error_pattern_t));
    snprintf(mc->patterns[idx].pattern_key, sizeof(mc->patterns[idx].pattern_key), "%s", key);
    return idx;
}

agentos_error_t agentos_mc_detect_patterns(
    agentos_metacognition_t* mc,
    mc_error_pattern_t** out_patterns,
    size_t* out_count)
{
    if (!mc || !out_patterns || !out_count) return AGENTOS_EINVAL;
    if (mc->record_count < 3) {
        *out_patterns = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }

    /* 按步骤类型+最低分维度分组统计 */
    typedef struct { char key[96]; uint64_t total; uint64_t fail; mc_dimension_t worst_dim; } pattern_acc_t;
    pattern_acc_t acc[MC_MAX_PATTERNS] = {{{0}}};
    size_t acc_count = 0;

    size_t check_n = (mc->record_count > 20) ? 20 : mc->record_count;
    for (size_t i = 0; i < check_n; i++) {
        size_t idx = (mc->record_head - 1 - i + mc->record_capacity) % mc->record_capacity;
        if (idx >= mc->record_count) continue;
        mc_evaluation_record_t* rec = &mc->records[idx];

        /* 构建模式键: "type_dim" */
        char pkey[96];
        int pklen = snprintf(pkey, sizeof(pkey), "%s_",
                             step_type_name((tc_step_type_t)(rec->step_id % 7)));

        float worst_score = 1.0f;
        mc_dimension_t worst_d = MC_DIM_RELEVANCE;
        for (int d = 0; d < MC_DIM_COUNT; d++) {
            if (rec->result.dimensions[d].score < worst_score) {
                worst_score = rec->result.dimensions[d].score;
                worst_d = (mc_dimension_t)d;
            }
        }
        pklen += snprintf(pkey + pklen, sizeof(pkey) - pklen, "%s_%.1f",
                          dim_short_name(worst_d), worst_score);

        /* 查找或创建累积器 */
        size_t aidx = acc_count;
        for (size_t j = 0; j < acc_count; j++) {
            if (strncmp(acc[j].key, pkey, 95) == 0) { aidx = j; break; }
        }
        if (aidx == acc_count && acc_count < MC_MAX_PATTERNS) {
            snprintf(acc[aidx].key, sizeof(acc[aidx].key), "%s", pkey);
            acc_count++;
        }
        if (aidx < MC_MAX_PATTERNS) {
            acc[aidx].total++;
            acc[aidx].worst_dim = worst_d;
            if (rec->result.strategy != MC_CORRECT_NONE) acc[aidx].fail++;
        }
    }

    /* 将高频失败模式写入patterns数组 */
    size_t detected = 0;
    for (size_t i = 0; i < acc_count && detected < MC_MAX_PATTERNS; i++) {
        if (acc[i].total >= 2 && acc[i].fail >= acc[i].total / 3) {
            const char* pkey = acc[i].key;
            size_t pidx = find_or_create_pattern(mc, pkey);
            if (pidx < MC_MAX_PATTERNS) {
                mc->patterns[pidx].occurrence_count += acc[i].total;
                mc->patterns[pidx].failure_count += acc[i].fail;
                mc->patterns[pidx].failure_rate =
                    (float)mc->patterns[pidx].failure_count /
                    (float)(mc->patterns[pidx].occurrence_count > 0 ?
                     mc->patterns[pidx].occurrence_count : 1);
                mc->patterns[pidx].last_seen_ns = mc_time_now();
                mc->patterns[pidx].is_active = 1;
                detected++;
                mc->patterns_detected++;
            }
        }
    }

    *out_patterns = (mc->pattern_count > 0) ? mc->patterns : NULL;
    *out_count = mc->pattern_count;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_mc_learn_best_strategy(
    agentos_metacognition_t* mc,
    const char* pattern_key,
    mc_correction_strategy_t* out_strategy)
{
    if (!mc || !pattern_key || !out_strategy) return AGENTOS_EINVAL;

    *out_strategy = MC_CORRECT_RERUN;

    size_t pidx = MC_MAX_PATTERNS;
    for (size_t i = 0; i < mc->pattern_count; i++) {
        if (strncmp(mc->patterns[i].pattern_key, pattern_key, 127) == 0) {
            pidx = i;
            break;
        }
    }

    if (pidx < MC_MAX_PATTERNS && mc->patterns[pidx].strategy_success_rate > 0.3f) {
        *out_strategy = mc->patterns[pidx].best_strategy;
        return AGENTOS_SUCCESS;
    }

    /* 无历史数据时基于失败率选择策略 */
    if (pidx < MC_MAX_PATTERNS && mc->patterns[pidx].failure_rate > 0.8f) {
        *out_strategy = MC_CORRECT_ESCALATE;
    } else if (pidx < MC_MAX_PATTERNS && mc->patterns[pidx].failure_rate > 0.5f) {
        *out_strategy = MC_CORRECT_RERUN;
    } else {
        *out_strategy = MC_CORRECT_AUTO;
    }

    return AGENTOS_SUCCESS;
}

int agentos_mc_preemptive_check(
    agentos_metacognition_t* mc,
    tc_step_type_t step_type,
    const char* input,
    size_t input_len,
    char** out_preemptive_hint,
    size_t* out_hint_len)
{
    if (!mc || !input || !out_preemptive_hint || !out_hint_len) return -1;
    *out_preemptive_hint = NULL;
    *out_hint_len = 0;

    if (mc->pattern_count == 0 || !mc->enable_learning) return 0;

    /* 检查输入是否匹配已知失败模式 */
    for (size_t i = 0; i < mc->pattern_count; i++) {
        mc_error_pattern_t* pat = &mc->patterns[i];
        if (!pat->is_active || pat->failure_rate < 0.4f) continue;

        /* 检查模式键中是否包含当前步骤类型 */
        char type_prefix[32];
        snprintf(type_prefix, sizeof(type_prefix), "%s_", step_type_name(step_type));

        if (strstr(pat->pattern_key, type_prefix) == NULL) continue;

        /* 匹配成功 → 生成预防性提示 */
        size_t hlen = 384 + strlen(pat->pattern_key) + 64;
        char* hint = (char*)AGENTOS_MALLOC(hlen);
        if (!hint) return -1;

        int written = snprintf(hint, hlen,
            "[PREEMPTIVE GUIDANCE] Detected known failure pattern '%s' "
            "(failure_rate=%.0f%%, occurrences=%llu). "
            "Precautionary instructions:\n"
            "- Pay extra attention to %s\n"
            "- Verify your output against the original request before finalizing\n"
            "- If unsure about any fact, explicitly state uncertainty\n"
            "- Structure your response clearly with numbered points",
            pat->pattern_key, pat->failure_rate * 100.0f,
            (unsigned long long)pat->occurrence_count,
            pat->pattern_key);

        if (written <= 0 || (size_t)written >= hlen) {
            AGENTOS_FREE(hint);
            return -1;
        }

        *out_preemptive_hint = hint;
        *out_hint_len = (size_t)written;
        mc->preemptive_corrections++;

        AGENTOS_LOG_INFO("MC preemptive: matched pattern '%s' (rate=%.2f)",
                        pat->pattern_key, pat->failure_rate);
        return 1;
    }

    return 0;
}

agentos_error_t agentos_mc_record_strategy_result(
    agentos_metacognition_t* mc,
    const char* pattern_key,
    mc_correction_strategy_t strategy,
    int success)
{
    if (!mc || !pattern_key) return AGENTOS_EINVAL;

    size_t pidx = find_or_create_pattern(mc, pattern_key);
    if (pidx >= MC_MAX_PATTERNS) return AGENTOS_ENOMEM;

    mc_error_pattern_t* pat = &mc->patterns[pidx];
    pat->occurrence_count++;
    pat->last_seen_ns = mc_time_now();

    if (!success) {
        pat->failure_count++;
        pat->failure_rate = (float)pat->failure_count / (float)pat->occurrence_count;
    }

    /* 更新策略成功率（指数移动平均） */
    float alpha = 0.3f;
    if (success) {
        pat->strategy_success_rate = alpha * 1.0f +
                                     (1.0f - alpha) * pat->strategy_success_rate;
        pat->best_strategy = strategy;
    } else {
        pat->strategy_success_rate = alpha * 0.0f +
                                     (1.0f - alpha) * pat->strategy_success_rate;
    }

    /* 更新学习效果指标 */
    if (pat->occurrence_count > 5) {
        mc->learning_effectiveness = 1.0f - pat->failure_rate;
    }

    return AGENTOS_SUCCESS;
}

float agentos_mc_adapt_threshold(agentos_metacognition_t* mc) {
    if (!mc) return 0.7f;

    /* 初始化自适应阈值 */
    if (mc->adaptive_acceptance_threshold <= 0.0f)
        mc->adaptive_acceptance_threshold = mc->acceptance_threshold;

    /* 连续通过 → 放宽阈值 (提高效率) */
    if (mc->consecutive_accepts >= 5) {
        mc->adaptive_acceptance_threshold -= 0.02f;
        mc->consecutive_accepts = 0;
        if (mc->adaptive_acceptance_threshold < 0.55f)
            mc->adaptive_acceptance_threshold = 0.55f;
    }

    /* 连续拒绝 → 收紧阈值 (提高质量) */
    if (mc->consecutive_rejects >= 3) {
        mc->adaptive_acceptance_threshold += 0.03f;
        mc->consecutive_rejects = 0;
        if (mc->adaptive_acceptance_threshold > 0.90f)
            mc->adaptive_acceptance_threshold = 0.90f;
    }

    return mc->adaptive_acceptance_threshold;
}
