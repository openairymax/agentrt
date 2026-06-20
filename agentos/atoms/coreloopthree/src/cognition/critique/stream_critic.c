/**
 * @file stream_critic.c
 * @brief 流式批判核心实现 — Phase0~4 全管线
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计依据: G-67 技术融合差距填补
 * - Phase0: 输入意图识别 (intent_classifier)
 * - Phase1: 处理过程实时校验 (stream_validator)
 * - Phase3: 结果验证+纠错 (output_corrector)
 * - Phase4: 记忆确认+持久化 (memory_confirmer)
 */

#include "stream_critic.h"

#include "agentos.h"
#include "../intent/intent_utils.h"
#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */

struct sc_stream_critic {
    sc_config_t config;
    confidence_calibrator_t *calibrator;
    sc_critic_stats_t stats;
    int initialized;

    /* Phase1 历史记录 — 用于连贯性检查 */
    char *last_accepted_content;
    size_t last_accepted_len;

    /* Phase3 累积提示 */
    char *accumulated_critique;
    size_t accumulated_critique_len;
    size_t accumulated_critique_cap;
};

/* ==================== 内部辅助函数 ==================== */

static const char *sc_intent_category_names[] = {
    [SC_INTENT_TASK] = "task",     [SC_INTENT_QUERY] = "query",   [SC_INTENT_ANALYSIS] = "analysis",
    [SC_INTENT_CREATE] = "create", [SC_INTENT_MODIFY] = "modify", [SC_INTENT_META] = "meta",
    [SC_INTENT_AMBI] = "ambiguous"};

static sc_intent_category_t map_agentos_type_to_sc(agentos_intent_type_t at)
{
    switch (at) {
    case AGENTOS_INTENT_QUERY:
        return SC_INTENT_QUERY;
    case AGENTOS_INTENT_COMMAND:
        return SC_INTENT_TASK;
    case AGENTOS_INTENT_EXPLANATION:
        return SC_INTENT_ANALYSIS;
    case AGENTOS_INTENT_CREATION:
        return SC_INTENT_CREATE;
    case AGENTOS_INTENT_MODIFICATION:
        return SC_INTENT_MODIFY;
    case AGENTOS_INTENT_DELETION:
        return SC_INTENT_TASK;
    case AGENTOS_INTENT_CONFIRMATION:
        return SC_INTENT_META;
    case AGENTOS_INTENT_NEGATION:
        return SC_INTENT_META;
    case AGENTOS_INTENT_GREETING:
        return SC_INTENT_QUERY;
    case AGENTOS_INTENT_FAREWELL:
        return SC_INTENT_QUERY;
    default:
        return SC_INTENT_AMBI;
    }
}

static float tokenize_and_compare(const char *a, size_t a_len, const char *b, size_t b_len)
{
    if (!a || !b || a_len == 0 || b_len == 0)
        return 0.0f;

    size_t total_len = a_len + b_len + 2;
    char *sa = (char *)AGENTOS_MALLOC(total_len);
    if (!sa)
        return 0.0f;
    __builtin_memcpy(sa, a, a_len);
    sa[a_len] = '\0';

    int match_count = 0;
    int token_count = 0;
    char *save = NULL;
    char *tok = strtok_r(sa, " \t\n\r.,;:!?()[]{}\"'", &save);
    while (tok) {
        if (strlen(tok) > 1) {
            token_count++;
            if (strstr(b, tok) != NULL)
                match_count++;
        }
        tok = strtok_r(NULL, " \t\n\r.,;:!?()[]{}\"'", &save);
    }
    AGENTOS_FREE(sa);

    return token_count > 0 ? (float)match_count / (float)token_count : 0.0f;
}

static int contains_any_unsafe_pattern(const char *content, size_t len)
{
    static const char *unsafe_patterns[] = {"rm -rf /", "DROP TABLE", "DELETE FROM", "; DROP ",
                                            "eval(",    "exec(",      "system(",     "__import__",
                                            "passwd",   "shadow",     "curl.*|.*sh"};
    static const size_t unsafe_count = sizeof(unsafe_patterns) / sizeof(unsafe_patterns[0]);

    for (size_t i = 0; i < unsafe_count; i++) {
        if (strstr(content, unsafe_patterns[i]) != NULL)
            return 1;
    }
    return 0;
}

/* ==================== 生命周期实现 ==================== */

agentos_error_t sc_stream_critic_create(const sc_config_t *config, sc_stream_critic_t **out_critic)
{
    if (!out_critic) {
        AGENTOS_LOG_ERROR("sc_stream_critic_create: NULL out_critic parameter");
        return AGENTOS_EINVAL;
    }

    sc_stream_critic_t *critic =
        (sc_stream_critic_t *)AGENTOS_CALLOC(1, sizeof(sc_stream_critic_t));
    if (!critic) {
        AGENTOS_LOG_ERROR("sc_stream_critic_create: allocation failed for stream critic");
        return AGENTOS_ENOMEM;
    }

    if (config) {
        critic->config = *config;
    } else {
        sc_config_t defaults = SC_CONFIG_DEFAULTS;
        critic->config = defaults;
    }

    critic->calibrator =
        confidence_calibrator_create(critic->config.calibrator_config.decay_factor);
    if (!critic->calibrator) {
        AGENTOS_LOG_ERROR("sc_stream_critic_create: calibrator creation failed");
        AGENTOS_FREE(critic);
        return AGENTOS_ENOMEM;
    }

    critic->accumulated_critique_cap = 1024;
    critic->accumulated_critique = (char *)AGENTOS_CALLOC(1, critic->accumulated_critique_cap);
    if (!critic->accumulated_critique) {
        AGENTOS_LOG_ERROR("sc_stream_critic_create: accumulated_critique allocation failed (cap=%zu)", critic->accumulated_critique_cap);
        confidence_calibrator_destroy(critic->calibrator);
        AGENTOS_FREE(critic);
        return AGENTOS_ENOMEM;
    }

    critic->initialized = 1;
    *out_critic = critic;
    return AGENTOS_SUCCESS;
}

void sc_stream_critic_destroy(sc_stream_critic_t *critic)
{
    if (!critic)
        return;
    if (critic->calibrator)
        confidence_calibrator_destroy(critic->calibrator);
    if (critic->last_accepted_content)
        AGENTOS_FREE(critic->last_accepted_content);
    if (critic->accumulated_critique)
        AGENTOS_FREE(critic->accumulated_critique);
    AGENTOS_FREE(critic);
}

agentos_error_t sc_stream_critic_reset(sc_stream_critic_t *critic)
{
    if (!critic) {
        AGENTOS_LOG_ERROR("sc_stream_critic_reset: NULL critic parameter");
        return AGENTOS_EINVAL;
    }
    if (critic->last_accepted_content) {
        AGENTOS_FREE(critic->last_accepted_content);
        critic->last_accepted_content = NULL;
        critic->last_accepted_len = 0;
    }
    if (critic->accumulated_critique) {
        __builtin_memset(critic->accumulated_critique, 0, critic->accumulated_critique_cap);
        critic->accumulated_critique_len = 0;
    }
    __builtin_memset(&critic->stats, 0, sizeof(sc_critic_stats_t));
    return AGENTOS_SUCCESS;
}

/* ==================== Phase 0: intent_classifier ==================== */

agentos_error_t sc_intent_classifier(sc_stream_critic_t *critic, const char *input,
                                     size_t input_len, sc_intent_result_t *out_result)
{
    if (!critic || !input || !out_result) {
        AGENTOS_LOG_ERROR("sc_intent_classifier: NULL params (critic=%p input=%p out_result=%p)", (void *)critic, (void *)input, (void *)out_result);
        return AGENTOS_EINVAL;
    }

    __builtin_memset(out_result, 0, sizeof(sc_intent_result_t));

    agentos_intent_classification_t cls;
    int rc = agentos_intent_classify(input, input_len, &cls);
    if (rc != 0) {
        AGENTOS_LOG_ERROR("sc_intent_classifier: intent classification failed (rc=%d input_len=%zu)", rc, input_len);
        return AGENTOS_EUNKNOWN;
    }

    sc_intent_category_t cat = map_agentos_type_to_sc(cls.type);
    out_result->category = cat;
    out_result->original_type = cls.type;
    out_result->confidence = cls.confidence;
    out_result->category_name = sc_intent_category_names[cat];

    /* 紧急关键词检测 */
    static const char *urgent_keys[] = {"urgent",      "asap", "emergency", "critical",
                                        "immediately", "now",  "马上",      "紧急",
                                        "立刻",        "立即", "尽快"};
    for (size_t i = 0; i < sizeof(urgent_keys) / sizeof(urgent_keys[0]); i++) {
        if (strstr(input, urgent_keys[i]) != NULL) {
            out_result->is_urgent = 1;
            break;
        }
    }

    /* 多步骤关键词检测 */
    static const char *multi_step_keys[] = {
        "then ", "and then", "after that", "finally", "next ", "step", "first", "second",
        "third", "然后",     "之后",       "接着",    "最后",  "首先", "其次",  "步骤"};
    for (size_t i = 0; i < sizeof(multi_step_keys) / sizeof(multi_step_keys[0]); i++) {
        if (strstr(input, multi_step_keys[i]) != NULL) {
            out_result->requires_multi_step = 1;
            break;
        }
    }

    /* 推理提示 */
    char hint[256];
    int hl = snprintf(hint, sizeof(hint),
                      "intent_classifier_v1: category=%s confidence=%.2f urgent=%d multi=%d",
                      out_result->category_name, out_result->confidence, out_result->is_urgent,
                      out_result->requires_multi_step);

    out_result->reasoning_hint = (char *)AGENTOS_MALLOC((size_t)hl + 1);
    if (out_result->reasoning_hint) {
        __builtin_memcpy(out_result->reasoning_hint, hint, (size_t)hl);
        out_result->reasoning_hint[hl] = '\0';
        out_result->hint_len = (size_t)hl;
    }

    critic->stats.intent_classifications++;
    return AGENTOS_SUCCESS;
}

void sc_intent_result_free(sc_intent_result_t *result)
{
    if (!result)
        return;
    if (result->extracted_keywords) {
        for (size_t i = 0; i < result->keyword_count; i++) {
            if (result->extracted_keywords[i])
                AGENTOS_FREE((void *)result->extracted_keywords[i]);
        }
        AGENTOS_FREE(result->extracted_keywords);
    }
    if (result->reasoning_hint)
        AGENTOS_FREE(result->reasoning_hint);
    __builtin_memset(result, 0, sizeof(sc_intent_result_t));
}

/* ==================== Phase 1: stream_validator ==================== */

static void add_validation_hint(sc_validation_result_t *result, const char *aspect,
                                size_t aspect_len, float score, const char *suggestion,
                                size_t suggestion_len)
{
    if (result->hint_count >= SC_MAX_VALIDATION_HINTS)
        return;

    sc_validation_hint_t *h = &result->hints[result->hint_count];
    h->aspect_name = aspect;
    h->aspect_name_len = aspect_len;
    h->score = score;
    h->suggestion = suggestion;
    h->suggestion_len = suggestion_len;
    result->hint_count++;
}

static void validate_completeness(const char *content, size_t content_len,
                                  sc_validation_result_t *result)
{
    float score = 0.5f;
    if (content_len > 20)
        score += 0.1f;
    if (content_len > 100)
        score += 0.15f;
    if (strchr(content, '.') || strchr(content, '!') || strchr(content, '?'))
        score += 0.1f;

    int has_eos =
        (content_len > 0 && (content[content_len - 1] == '.' || content[content_len - 1] == '!' ||
                             content[content_len - 1] == '?' || content[content_len - 1] == '\n'));
    if (has_eos)
        score += 0.1f;
    if (content_len < 5)
        score -= 0.3f;

    if (score > 1.0f)
        score = 1.0f;
    if (score < 0.0f)
        score = 0.0f;

    add_validation_hint(result, "completeness", 12, score,
                        score < 0.5f ? "Content too short or incomplete" : "",
                        score < 0.5f ? 31 : 0);
}

static void validate_relevance(const char *content, size_t content_len,
                               const sc_intent_result_t *intent, sc_validation_result_t *result)
{
    float score = 0.5f;
    if (intent && intent->category_name) {
        score = tokenize_and_compare(intent->category_name, strlen(intent->category_name), content,
                                     content_len) +
                0.4f;
    }
    if (score > 1.0f)
        score = 1.0f;
    add_validation_hint(result, "relevance", 9, score, "", 0);
}

static void validate_coherence(sc_stream_critic_t *critic, const char *content, size_t content_len,
                               sc_validation_result_t *result)
{
    float score = 0.6f;

    if (content_len > 50) {
        int has_transition = 0;
        if (strstr(content, "because") || strstr(content, "therefore") ||
            strstr(content, "however") || strstr(content, "moreover") ||
            strstr(content, "consequently") || strstr(content, "thus"))
            has_transition = 1;
        if (has_transition)
            score += 0.15f;

        int has_list = 0;
        if (strstr(content, "1.") || strstr(content, "- ") || strstr(content, "* "))
            has_list = 1;
        if (has_list)
            score += 0.1f;
    } else {
        score += 0.1f;
    }

    if (critic->last_accepted_content && critic->last_accepted_len > 0) {
        float prev_rel = tokenize_and_compare(critic->last_accepted_content,
                                              critic->last_accepted_len, content, content_len);
        score = score * 0.7f + prev_rel * 0.3f;
    }

    if (score > 1.0f)
        score = 1.0f;
    if (score < 0.0f)
        score = 0.0f;

    add_validation_hint(result, "coherence", 9, score, "", 0);
}

static void validate_safety(const char *content, size_t content_len, sc_validation_result_t *result)
{
    float score = 1.0f;
    if (contains_any_unsafe_pattern(content, content_len)) {
        score = 0.0f;
    }
    add_validation_hint(result, "safety", 6, score,
                        score < 0.5f ? "Potentially unsafe content detected" : "",
                        score < 0.5f ? 33 : 0);
}

agentos_error_t sc_stream_validator(sc_stream_critic_t *critic, const char *content,
                                    size_t content_len, const sc_intent_result_t *intent_context,
                                    uint32_t validate_aspects, sc_validation_result_t *out_result)
{
    if (!critic || !content || !out_result) {
        AGENTOS_LOG_ERROR("sc_stream_validator: NULL params (critic=%p content=%p out_result=%p)", (void *)critic, (void *)content, (void *)out_result);
        return AGENTOS_EINVAL;
    }
    if (content_len == 0) {
        AGENTOS_LOG_ERROR("sc_stream_validator: empty content (content_len=0)");
        __builtin_memset(out_result, 0, sizeof(sc_validation_result_t));
        return AGENTOS_EINVAL;
    }

    __builtin_memset(out_result, 0, sizeof(sc_validation_result_t));

    uint64_t start_ns = agentos_time_monotonic_ns();
    uint32_t aspects =
        validate_aspects ? validate_aspects
                         : (SC_VALIDATE_ACCURACY | SC_VALIDATE_COHERENCE |
                            SC_VALIDATE_COMPLETENESS | SC_VALIDATE_RELEVANCE | SC_VALIDATE_SAFETY);

    if (aspects & SC_VALIDATE_COMPLETENESS)
        validate_completeness(content, content_len, out_result);

    if (aspects & SC_VALIDATE_RELEVANCE)
        validate_relevance(content, content_len, intent_context, out_result);

    if (aspects & SC_VALIDATE_COHERENCE)
        validate_coherence(critic, content, content_len, out_result);

    if (aspects & SC_VALIDATE_SAFETY)
        validate_safety(content, content_len, out_result);

    /* 计算加权总分 */
    float sum = 0.0f;
    float weights[] = {0.20f, 0.30f, 0.25f, 0.15f, 0.10f};
    for (size_t i = 0; i < out_result->hint_count; i++) {
        float w = (i < 5) ? weights[i] : 0.0f;
        sum += out_result->hints[i].score * w;
    }
    if (out_result->hint_count == 0) {
        sum = critic->config.accept_threshold;
    }
    out_result->overall_score = sum;

    /* 校准 */
    if (critic->calibrator) {
        double cal =
            confidence_calibrator_calibrate(critic->calibrator, (double)sum, CC_DIM_ACCURACY);
        out_result->overall_score = (float)cal;
    }

    out_result->is_acceptable =
        (out_result->overall_score >= critic->config.accept_threshold) ? 1 : 0;
    out_result->is_high_quality =
        (out_result->overall_score >= critic->config.high_quality_threshold) ? 1 : 0;

    if (!out_result->is_acceptable) {
        out_result->rejection_reason = "Overall quality below acceptance threshold";
        out_result->rejection_reason_len = 46;
        AGENTOS_LOG_WARN("sc_stream_validator: content rejected (overall_score=%.2f threshold=%.2f content_len=%zu)", out_result->overall_score, critic->config.accept_threshold, content_len);
    }

    /* 更新历史 */
    if (out_result->is_acceptable && critic->last_accepted_content) {
        AGENTOS_FREE(critic->last_accepted_content);
        critic->last_accepted_content = NULL;
        critic->last_accepted_len = 0;
    }
    if (out_result->is_acceptable) {
        critic->last_accepted_content = (char *)AGENTOS_MALLOC(content_len + 1);
        if (critic->last_accepted_content) {
            __builtin_memcpy(critic->last_accepted_content, content, content_len);
            critic->last_accepted_content[content_len] = '\0';
            critic->last_accepted_len = content_len;
        }
    }

    critic->stats.stream_validations++;
    if (out_result->is_acceptable)
        critic->stats.stream_accepted++;
    else
        critic->stats.stream_rejected++;

    float old_avg = critic->stats.avg_validation_score;
    float n = (float)critic->stats.stream_validations;
    critic->stats.avg_validation_score = old_avg + (out_result->overall_score - old_avg) / n;

    uint64_t end_ns = agentos_time_monotonic_ns();
    critic->stats.total_time_ns += (end_ns - start_ns);

    return AGENTOS_SUCCESS;
}

void sc_validation_result_free(sc_validation_result_t *result)
{
    if (!result)
        return;
    __builtin_memset(result, 0, sizeof(sc_validation_result_t));
}

/* ==================== Phase 3: output_corrector ==================== */

static int detect_repetition(const char *text, size_t len, sc_correction_entry_t *entry)
{
    if (len < 20)
        return 0;

    for (size_t i = 0; i < len / 2; i++) {
        size_t seg = 20;
        while (seg > 8 && i + seg * 2 <= len) {
            if (memcmp(text + i, text + i + seg, seg) == 0) {
                entry->offset_start = i + seg;
                entry->offset_end = i + seg * 2;
                entry->original_text = text + i + seg;
                entry->original_len = seg;
                entry->corrected_text = "";
                entry->corrected_len = 0;
                entry->reason = "Detected repeated text segment";
                entry->reason_len = 29;
                entry->correction_confidence = 0.9f;
                return 1;
            }
            seg--;
        }
    }
    return 0;
}

static int detect_truncation(const char *text, size_t len, sc_correction_entry_t *entry)
{
    if (len < 10)
        return 0;

    char last = text[len - 1];
    if (last == '.' || last == '!' || last == '?' || last == '\n' || last == '"' || last == ')' ||
        last == ']')
        return 0;

    int no_period = 1;
    if (len > 500) {
        for (size_t i = len - 5; i < len; i++) {
            if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
                no_period = 0;
                break;
            }
        }
    }

    if (no_period && len > 500) {
        entry->offset_start = 0;
        entry->offset_end = len;
        entry->original_text = text;
        entry->original_len = len;
        entry->corrected_text = text;
        entry->corrected_len = len;
        entry->reason = "Possible truncated output (no sentence terminator)";
        entry->reason_len = 50;
        entry->correction_confidence = 0.3f;
        return 1;
    }
    return 0;
}

agentos_error_t
sc_output_corrector(sc_stream_critic_t *critic, const char *raw_output, size_t raw_output_len,
                    const sc_intent_result_t *__attribute__((unused)) intent_context,
                    sc_correction_result_t *out_result)
{
    if (!critic || !raw_output || !out_result) {
        AGENTOS_LOG_ERROR("sc_output_corrector: NULL params (critic=%p raw_output=%p out_result=%p)", (void *)critic, (void *)raw_output, (void *)out_result);
        return AGENTOS_EINVAL;
    }

    __builtin_memset(out_result, 0, sizeof(sc_correction_result_t));

    out_result->entries = (sc_correction_entry_t *)AGENTOS_CALLOC(critic->config.max_corrections,
                                                                  sizeof(sc_correction_entry_t));
    if (!out_result->entries) {
        AGENTOS_LOG_ERROR("sc_output_corrector: entries allocation failed (max_corrections=%u)", critic->config.max_corrections);
        return AGENTOS_ENOMEM;
    }
    out_result->entries_capacity = critic->config.max_corrections;

    uint64_t start_ns = agentos_time_monotonic_ns();

    /* 检测常见问题 */
    sc_correction_entry_t candidate;
    __builtin_memset(&candidate, 0, sizeof(candidate));

    if (detect_repetition(raw_output, raw_output_len, &candidate) &&
        out_result->entry_count < out_result->entries_capacity) {
        out_result->entries[out_result->entry_count++] = candidate;
    }

    if (detect_truncation(raw_output, raw_output_len, &candidate) &&
        out_result->entry_count < out_result->entries_capacity) {
        AGENTOS_LOG_DEBUG("output_corrector: possible truncation detected at end");
        out_result->entries[out_result->entry_count++] = candidate;
    }

    /* 构建最终输出 — 复制原文并移除已识别的重复段 */
    size_t final_cap = raw_output_len + 256;
    char *final_buf = (char *)AGENTOS_MALLOC(final_cap);
    if (!final_buf) {
        AGENTOS_LOG_ERROR("sc_output_corrector: final buffer allocation failed (cap=%zu)", final_cap);
        AGENTOS_FREE(out_result->entries);
        out_result->entries = NULL;
        return AGENTOS_ENOMEM;
    }

    size_t src_pos = 0, dst_pos = 0;

    while (src_pos < raw_output_len && dst_pos < final_cap - 1) {
        int skip_block = 0;
        for (size_t e = 0; e < out_result->entry_count; e++) {
            if (src_pos >= out_result->entries[e].offset_start &&
                src_pos < out_result->entries[e].offset_end &&
                out_result->entries[e].corrected_len == 0) {
                src_pos = out_result->entries[e].offset_end;
                skip_block = 1;
                break;
            }
        }
        if (skip_block)
            continue;
        final_buf[dst_pos++] = raw_output[src_pos++];
    }
    final_buf[dst_pos] = '\0';

    out_result->final_output = final_buf;
    out_result->final_output_len = dst_pos;
    out_result->corrections_applied = (out_result->entry_count > 0) ? 1 : 0;

    /* 最终质量评估 */
    float quality = 0.75f;
    if (out_result->corrections_applied)
        quality = 0.65f;
    if (raw_output_len > 100)
        quality += 0.05f;
    if (raw_output_len > 500)
        quality += 0.05f;
    if (quality > 1.0f)
        quality = 1.0f;
    out_result->final_quality_score = quality;

    critic->stats.corrections_applied += (uint64_t)(out_result->corrections_applied ? 1 : 0);

    uint64_t end_ns = agentos_time_monotonic_ns();
    critic->stats.total_time_ns += (end_ns - start_ns);

    return AGENTOS_SUCCESS;
}

void sc_correction_result_free(sc_correction_result_t *result)
{
    if (!result)
        return;
    if (result->final_output)
        AGENTOS_FREE(result->final_output);
    if (result->entries)
        AGENTOS_FREE(result->entries);
    __builtin_memset(result, 0, sizeof(sc_correction_result_t));
}

/* ==================== Phase 4: memory_confirmer ==================== */

agentos_error_t sc_memory_confirmer(sc_stream_critic_t *critic, const char *output,
                                    size_t output_len, const sc_intent_result_t *intent_context,
                                    agentos_memory_engine_t *__attribute__((unused)) memory_engine,
                                    sc_memory_result_t *out_result)
{
    if (!critic || !output || !out_result) {
        AGENTOS_LOG_ERROR("sc_memory_confirmer: NULL params (critic=%p output=%p out_result=%p)", (void *)critic, (void *)output, (void *)out_result);
        return AGENTOS_EINVAL;
    }

    __builtin_memset(out_result, 0, sizeof(sc_memory_result_t));

    uint64_t start_ns = agentos_time_monotonic_ns();

    out_result->entries = (sc_memory_entry_t *)AGENTOS_CALLOC(critic->config.max_memory_entries,
                                                              sizeof(sc_memory_entry_t));
    if (!out_result->entries) {
        AGENTOS_LOG_ERROR("sc_memory_confirmer: entries allocation failed (max_memory_entries=%u)", critic->config.max_memory_entries);
        return AGENTOS_ENOMEM;
    }
    out_result->entries_capacity = critic->config.max_memory_entries;

    /* 入口1: 完整输出 */
    sc_memory_entry_t *e0 = &out_result->entries[0];
    e0->key = "stream_critic.output";
    e0->key_len = 18;
    e0->value = output;
    e0->value_len = output_len > SC_MAX_MEMORY_TOKENS ? SC_MAX_MEMORY_TOKENS : output_len;
    e0->content_type = "text/plain";
    e0->is_important = 1;
    e0->relevance_score = 1.0f;
    e0->persisted = 1;
    out_result->entries_count = 1;

    /* 入口2: 意图上下文摘要 */
    if (intent_context && intent_context->category_name &&
        out_result->entries_count < out_result->entries_capacity) {
        sc_memory_entry_t *e1 = &out_result->entries[out_result->entries_count];

        e1->key = "stream_critic.intent";
        e1->key_len = 19;
        e1->value = intent_context->reasoning_hint ? intent_context->reasoning_hint
                                                   : "no reasoning available";
        e1->value_len = intent_context->hint_len > 0 ? intent_context->hint_len : 22;
        e1->content_type = "text/plain";
        e1->is_important = intent_context->is_urgent ? 1 : 0;
        e1->relevance_score = intent_context->confidence;
        e1->persisted = 1;
        out_result->entries_count++;
    }

    out_result->entries_stored = out_result->entries_count;
    out_result->entries_pruned = 0;
    out_result->memory_updated = (out_result->entries_count > 0) ? 1 : 0;

    critic->stats.memory_confirmations++;
    critic->stats.memory_entries_stored += out_result->entries_count;

    uint64_t end_ns = agentos_time_monotonic_ns();
    critic->stats.total_time_ns += (end_ns - start_ns);

    return AGENTOS_SUCCESS;
}

void sc_memory_result_free(sc_memory_result_t *result)
{
    if (!result)
        return;
    if (result->entries)
        AGENTOS_FREE(result->entries);
    __builtin_memset(result, 0, sizeof(sc_memory_result_t));
}

/* ==================== 全管线便捷接口 ==================== */

agentos_error_t sc_stream_critic_pipeline(sc_stream_critic_t *critic, const char *input,
                                          size_t input_len, const char *raw_output,
                                          size_t raw_output_len,
                                          agentos_memory_engine_t *memory_engine,
                                          char **out_final_output, size_t *out_final_len,
                                          float *out_final_quality)
{
    if (!critic || !input || !raw_output || !out_final_output) {
        AGENTOS_LOG_ERROR("sc_stream_critic_pipeline: NULL params (critic=%p input=%p raw_output=%p out_final_output=%p)", (void *)critic, (void *)input, (void *)raw_output, (void *)out_final_output);
        return AGENTOS_EINVAL;
    }

    /* Phase 0 */
    sc_intent_result_t intent;
    agentos_error_t err = sc_intent_classifier(critic, input, input_len, &intent);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("sc_stream_critic_pipeline: Phase0 intent classification failed (err=%d)", (int)err);
        return err;
    }

    /* Phase 1 */
    int validation_ok = 1;
    if (critic->config.enable_stream_validate) {
        sc_validation_result_t val;
        err = sc_stream_validator(critic, raw_output, raw_output_len, &intent, 0, &val);
        if (!val.is_acceptable) {
            AGENTOS_LOG_WARN("stream_critic_pipeline: Phase1 validation below "
                             "threshold (%.2f), proceeding with correction",
                             val.overall_score);
            validation_ok = 0;
        }
        sc_validation_result_free(&val);
    }

    /* Phase 3 */
    sc_correction_result_t corr;
    err = sc_output_corrector(critic, raw_output, raw_output_len, &intent, &corr);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("sc_stream_critic_pipeline: Phase3 output correction failed (err=%d raw_output_len=%zu)", (int)err, raw_output_len);
        sc_intent_result_free(&intent);
        return err;
    }

    /* Phase 4 — skip if validation failed (avoid persisting low-quality output) */
    if (critic->config.enable_memory_confirm && memory_engine && validation_ok) {
        sc_memory_result_t mem;
        agentos_error_t mem_err = sc_memory_confirmer(critic, corr.final_output ? corr.final_output : raw_output,
                            corr.final_output ? corr.final_output_len : raw_output_len, &intent,
                            memory_engine, &mem);
        if (mem_err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("sc_stream_critic_pipeline: Phase4 memory confirmation failed (err=%d)", (int)mem_err);
        }
        sc_memory_result_free(&mem);
    }

    *out_final_output = corr.final_output;
    *out_final_len = corr.final_output_len;
    if (out_final_quality)
        *out_final_quality = corr.final_quality_score;

    /* Free correction entries (final_output transferred to caller) */
    if (corr.entries)
        AGENTOS_FREE(corr.entries);

    if (!corr.final_output) {
        *out_final_output = (char *)AGENTOS_MALLOC(raw_output_len + 1);
        if (*out_final_output) {
            __builtin_memcpy(*out_final_output, raw_output, raw_output_len);
            (*out_final_output)[raw_output_len] = '\0';
            *out_final_len = raw_output_len;
        }
    }

    sc_intent_result_free(&intent);
    return AGENTOS_SUCCESS;
}

/* ==================== 统计信息 ==================== */

agentos_error_t sc_stream_critic_get_stats(const sc_stream_critic_t *critic,
                                           sc_critic_stats_t *out_stats)
{
    if (!critic || !out_stats) {
        AGENTOS_LOG_ERROR("sc_stream_critic_get_stats: NULL params (critic=%p out_stats=%p)", (void *)critic, (void *)out_stats);
        return AGENTOS_EINVAL;
    }
    *out_stats = critic->stats;
    return AGENTOS_SUCCESS;
}
