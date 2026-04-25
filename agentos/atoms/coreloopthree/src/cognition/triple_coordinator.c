/**
 * @file triple_coordinator.c
 * @brief 三组件协调器实现 - t2/t1-f/t1-p 流式批判循环
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "triple_coordinator.h"
#include "agentos.h"
#include "memory_compat.h"
#include "string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void free_unit_results(tc3_coordinator_t* coord) {
    if (!coord) return;
    for (size_t i = 0; i < coord->unit_results_count; i++) {
        if (coord->unit_results[i].critique) {
            AGENTOS_FREE(coord->unit_results[i].critique);
        }
    }
    coord->unit_results_count = 0;
}

static agentos_error_t record_unit_result(
    tc3_coordinator_t* coord,
    const tc3_unit_result_t* result) {
    if (!coord || !result) return AGENTOS_EINVAL;

    if (coord->unit_results_count >= coord->unit_results_capacity) {
        size_t new_cap = coord->unit_results_capacity * 2;
        if (new_cap == 0) new_cap = 32;
        tc3_unit_result_t* new_arr = (tc3_unit_result_t*)AGENTOS_REALLOC(
            coord->unit_results, new_cap * sizeof(tc3_unit_result_t));
        if (!new_arr) return AGENTOS_ENOMEM;
        coord->unit_results = new_arr;
        coord->unit_results_capacity = new_cap;
    }

    tc3_unit_result_t* slot = &coord->unit_results[coord->unit_results_count];
    *slot = *result;
    if (result->critique && result->critique_len > 0) {
        slot->critique = (char*)AGENTOS_MALLOC(result->critique_len + 1);
        if (slot->critique) {
            memcpy(slot->critique, result->critique, result->critique_len);
            slot->critique[result->critique_len] = '\0';
        }
    } else {
        slot->critique = NULL;
    }
    coord->unit_results_count++;
    return AGENTOS_SUCCESS;
}

static tc3_verdict_t score_to_verdict(float score, const tc3_config_t* config) {
    if (score >= config->accept_threshold) return TC3_RESULT_ACCEPT;
    if (score >= config->minor_fix_threshold) return TC3_RESULT_MINOR_FIX;
    if (score >= config->escalate_threshold) return TC3_RESULT_MAJOR_FIX;
    return TC3_RESULT_REJECT;
}

static agentos_error_t default_s1_verify(
    const char* content, size_t content_len,
    float* out_score, int* out_acceptable,
    char** out_critique, size_t* out_critique_len,
    void* user_data) {
    (void)user_data;
    if (!content || !out_score) return AGENTOS_EINVAL;

    float score = 0.5f;
    if (content_len > 20) score += 0.1f;
    if (content_len > 50) score += 0.1f;
    if (strstr(content, "because") || strstr(content, "therefore")) score += 0.1f;
    if (content_len > 100 && strchr(content, '.')) score += 0.05f;

    if (score > 1.0f) score = 1.0f;
    *out_score = score;
    if (out_acceptable) *out_acceptable = (score >= TC3_ACCEPT_THRESHOLD) ? 1 : 0;

    if (out_critique && out_critique_len) {
        if (score < TC3_ACCEPT_THRESHOLD) {
            const char* tmpl = "Quality below threshold (%.2f < %.2f). Needs improvement.";
            size_t buf_sz = 128;
            char* buf = (char*)AGENTOS_MALLOC(buf_sz);
            if (buf) {
                snprintf(buf, buf_sz, tmpl, score, TC3_ACCEPT_THRESHOLD);
                *out_critique = buf;
                *out_critique_len = strlen(buf);
            }
        } else {
            *out_critique = NULL;
            *out_critique_len = 0;
        }
    }
    return AGENTOS_SUCCESS;
}

static agentos_error_t verify_with_metacognition(
    tc3_coordinator_t* coord,
    agentos_thinking_step_t* step,
    const char* content, size_t content_len,
    float* out_score, int* out_acceptable,
    char** out_critique, size_t* out_critique_len) {
    if (coord->meta && step) {
        mc_evaluation_result_t eval;
        agentos_error_t err = agentos_mc_evaluate_quick(
            coord->meta, step, out_score, out_acceptable);
        if (err == AGENTOS_SUCCESS) {
            err = agentos_mc_evaluate_step(
                coord->meta, step, content, content_len, &eval);
            if (err == AGENTOS_SUCCESS) {
                if (coord->calibrator) {
                    double raw = (double)eval.overall_score;
                    double calibrated = confidence_calibrator_calibrate(
                        coord->calibrator, raw, CC_DIM_ACCURACY);
                    eval.overall_score = (float)calibrated;
                    *out_score = (float)calibrated;
                } else {
                    *out_score = eval.overall_score;
                }
                *out_acceptable = eval.is_acceptable;
                if (eval.critique_text && eval.critique_len > 0) {
                    *out_critique = eval.critique_text;
                    *out_critique_len = eval.critique_len;
                } else {
                    *out_critique = NULL;
                    *out_critique_len = 0;
                }
                return AGENTOS_SUCCESS;
            }
        }
    }

    float fallback_score = 0.0f;
    int fallback_acceptable = 0;
    agentos_error_t fb_err = default_s1_verify(content, content_len,
                             &fallback_score, &fallback_acceptable,
                             out_critique, out_critique_len, NULL);
    if (fb_err == AGENTOS_SUCCESS && coord->calibrator) {
        double calibrated = confidence_calibrator_calibrate(
            coord->calibrator, (double)fallback_score, CC_DIM_ACCURACY);
        *out_score = (float)calibrated;
        *out_acceptable = fallback_acceptable;
    } else if (fb_err == AGENTOS_SUCCESS) {
        *out_score = fallback_score;
        *out_acceptable = fallback_acceptable;
    }
    return fb_err;
}

static agentos_error_t build_correction_prompt(
    const char* original, size_t original_len,
    const char* critique, size_t critique_len,
    int attempt,
    char** out_prompt, size_t* out_prompt_len) {
    size_t buf_sz = original_len + critique_len + 256;
    char* buf = (char*)AGENTOS_MALLOC(buf_sz);
    if (!buf) return AGENTOS_ENOMEM;

    int written = snprintf(buf, buf_sz,
        "[Original]\n%.*s\n[Critique #%d: %.*s]\n"
        "[Instruction: Improve based on critique above]",
        (int)(original_len > 500 ? 500 : original_len), original,
        attempt,
        (int)(critique_len > 300 ? 300 : critique_len),
        critique ? critique : "quality insufficient");

    if (written <= 0 || (size_t)written >= buf_sz) {
        AGENTOS_FREE(buf);
        return AGENTOS_EUNKNOWN;
    }

    *out_prompt = buf;
    *out_prompt_len = (size_t)written;
    return AGENTOS_SUCCESS;
}

static agentos_error_t concatenate_units(
    const char** units, const size_t* lens, size_t count,
    char** out_text, size_t* out_len) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += lens[i];
    }

    char* buf = (char*)AGENTOS_MALLOC(total + 1);
    if (!buf) return AGENTOS_ENOMEM;

    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        memcpy(buf + pos, units[i], lens[i]);
        pos += lens[i];
    }
    buf[pos] = '\0';

    *out_text = buf;
    *out_len = pos;
    return AGENTOS_SUCCESS;
}

agentos_error_t tc3_coordinator_create(
    const tc3_config_t* config,
    agentos_thinking_chain_t* chain,
    agentos_metacognition_t* meta,
    tc3_coordinator_t** out_coord) {
    if (!out_coord) return AGENTOS_EINVAL;

    tc3_coordinator_t* coord = (tc3_coordinator_t*)AGENTOS_CALLOC(
        1, sizeof(tc3_coordinator_t));
    if (!coord) return AGENTOS_ENOMEM;

    if (config) {
        coord->config = *config;
    } else {
        tc3_config_t defaults = TC3_CONFIG_DEFAULTS;
        coord->config = defaults;
    }

    coord->chain = chain;
    coord->meta = meta;
    coord->calibrator = confidence_calibrator_create(CC_DEFAULT_DECAY_FACTOR);
    if (!coord->calibrator) {
        AGENTOS_FREE(coord);
        return AGENTOS_ENOMEM;
    }

    agentos_error_t err = su_stream_detector_create(
        &coord->config.stream_config, &coord->detector);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(coord);
        return err;
    }

    coord->unit_results = NULL;
    coord->unit_results_capacity = 0;
    coord->unit_results_count = 0;
    coord->active = 0;
    memset(&coord->stats, 0, sizeof(tc3_stats_t));

    *out_coord = coord;
    return AGENTOS_SUCCESS;
}

void tc3_coordinator_destroy(tc3_coordinator_t* coord) {
    if (!coord) return;
    if (coord->detector) su_stream_detector_destroy(coord->detector);
    if (coord->calibrator) confidence_calibrator_destroy(coord->calibrator);
    free_unit_results(coord);
    if (coord->unit_results) AGENTOS_FREE(coord->unit_results);
    AGENTOS_FREE(coord);
}

agentos_error_t tc3_coordinator_execute(
    tc3_coordinator_t* coord,
    const char* input,
    size_t input_len,
    char** out_output,
    size_t* out_output_len) {
    if (!coord || !input || !out_output) return AGENTOS_EINVAL;

    coord->active = 1;
    uint64_t start_ns = agentos_time_monotonic_ns();

    agentos_error_t err = su_stream_detector_reset(coord->detector);
    if (err != AGENTOS_SUCCESS) return err;

    char* full_output = NULL;
    size_t full_output_len = 0;

    tc3_s2_generate_fn s2_fn = coord->config.s2_generate;
    if (!s2_fn) {
        coord->active = 0;
        return AGENTOS_ENOTSUP;
    }

    char* s2_output = NULL;
    size_t s2_output_len = 0;
    err = s2_fn(input, input_len, &s2_output, &s2_output_len, coord->config.s2_user_data);
    if (err != AGENTOS_SUCCESS || !s2_output) {
        coord->active = 0;
        return err ? err : AGENTOS_EUNKNOWN;
    }

    err = su_stream_detector_feed(coord->detector, s2_output, s2_output_len, 0.7f);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(s2_output);
        coord->active = 0;
        return err;
    }
    err = su_stream_detector_flush(coord->detector);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(s2_output);
        coord->active = 0;
        return err;
    }

    size_t accepted_count = 0;
    const char** accepted_texts = NULL;
    size_t* accepted_lens = NULL;
    size_t accepted_capacity = 0;

    while (su_stream_detector_pending_count(coord->detector) > 0) {
        su_semantic_unit_t unit;
        err = su_stream_detector_pop_pending(coord->detector, &unit);
        if (err != AGENTOS_SUCCESS) break;

        float score = 0.0f;
        int acceptable = 0;
        char* critique = NULL;
        size_t critique_len = 0;

        agentos_thinking_step_t* gen_step = NULL;
        if (coord->chain) {
            agentos_tc_step_create(coord->chain, TC_STEP_GENERATION,
                                   unit.text, unit.text_len, NULL, 0, &gen_step);
            if (gen_step) {
                agentos_tc_step_complete(gen_step, unit.text, unit.text_len,
                                         unit.confidence, "t2-generator");
            }
        }

        verify_with_metacognition(coord, gen_step,
                                  unit.text, unit.text_len,
                                  &score, &acceptable,
                                  &critique, &critique_len);

        tc3_verdict_t verdict = score_to_verdict(score, &coord->config);
        uint32_t correction_attempts = 0;

        char* current_text = unit.text;
        size_t current_len = unit.text_len;
        int owns_current = 0;

        while (verdict != TC3_RESULT_ACCEPT && verdict != TC3_RESULT_REJECT &&
               correction_attempts < coord->config.max_verify_rounds) {

            if (verdict == TC3_RESULT_ESCALATE && coord->config.s1_expert) {
                float expert_score = 0.0f;
                tc3_verdict_t expert_verdict = TC3_RESULT_REJECT;
                char* expert_opinion = NULL;
                size_t expert_opinion_len = 0;

                agentos_error_t exp_err = coord->config.s1_expert(
                    current_text, current_len,
                    critique ? critique : "quality low", critique_len,
                    &expert_score, &expert_verdict,
                    &expert_opinion, &expert_opinion_len,
                    coord->config.s1_expert_user_data);

                if (exp_err == AGENTOS_SUCCESS) {
                    verdict = expert_verdict;
                    score = expert_score;
                    if (expert_opinion) AGENTOS_FREE(expert_opinion);
                }
                coord->stats.escalated_units++;
                break;
            }

            correction_attempts++;
            coord->stats.total_corrections++;

            char* correction_prompt = NULL;
            size_t correction_prompt_len = 0;
            err = build_correction_prompt(
                current_text, current_len,
                critique, critique_len,
                (int)correction_attempts,
                &correction_prompt, &correction_prompt_len);
            if (err != AGENTOS_SUCCESS) break;

            char* corrected = NULL;
            size_t corrected_len = 0;
            err = s2_fn(correction_prompt, correction_prompt_len,
                        &corrected, &corrected_len, coord->config.s2_user_data);
            AGENTOS_FREE(correction_prompt);

            if (err != AGENTOS_SUCCESS || !corrected) {
                if (critique) AGENTOS_FREE(critique);
                break;
            }

            if (owns_current && current_text) AGENTOS_FREE(current_text);
            current_text = corrected;
            current_len = corrected_len;
            owns_current = 1;

            if (critique) { AGENTOS_FREE(critique); critique = NULL; }

            verify_with_metacognition(coord, gen_step,
                                      current_text, current_len,
                                      &score, &acceptable,
                                      &critique, &critique_len);
            verdict = score_to_verdict(score, &coord->config);
        }

        tc3_unit_result_t result = {
            .unit_index = unit.unit_index,
            .verdict = verdict,
            .score = score,
            .critique = critique,
            .critique_len = critique_len,
            .verified_by = (verdict == TC3_RESULT_ESCALATE) ? TC3_ROLE_T1P : TC3_ROLE_T1F,
            .correction_attempts = correction_attempts
        };
        record_unit_result(coord, &result);

        if (critique) AGENTOS_FREE(critique);

        switch (verdict) {
            case TC3_RESULT_ACCEPT:
                coord->stats.accepted_units++;
                break;
            case TC3_RESULT_MINOR_FIX:
                coord->stats.minor_fix_units++;
                break;
            case TC3_RESULT_MAJOR_FIX:
                coord->stats.major_fix_units++;
                break;
            case TC3_RESULT_ESCALATE:
                break;
            case TC3_RESULT_REJECT:
                coord->stats.rejected_units++;
                break;
        }
        coord->stats.total_units++;

        if (verdict == TC3_RESULT_REJECT) {
            if (owns_current) AGENTOS_FREE(current_text);
            continue;
        }

        if (accepted_count >= accepted_capacity) {
            size_t new_cap = accepted_capacity * 2;
            if (new_cap == 0) new_cap = 16;
            const char** new_texts = (const char**)AGENTOS_REALLOC(
                accepted_texts, new_cap * sizeof(char*));
            size_t* new_lens = (size_t*)AGENTOS_REALLOC(
                accepted_lens, new_cap * sizeof(size_t));
            if (!new_texts || !new_lens) {
                if (new_texts) accepted_texts = new_texts;
                if (new_lens) accepted_lens = new_lens;
                if (owns_current && current_text) AGENTOS_FREE(current_text);
                break;
            }
            accepted_texts = new_texts;
            accepted_lens = new_lens;
            accepted_capacity = new_cap;
        }

        accepted_texts[accepted_count] = current_text;
        accepted_lens[accepted_count] = current_len;
        accepted_count++;
    }

    if (accepted_count > 0) {
        concatenate_units(accepted_texts, accepted_lens, accepted_count,
                          &full_output, &full_output_len);
    }

    if (accepted_texts) AGENTOS_FREE(accepted_texts);
    if (accepted_lens) AGENTOS_FREE(accepted_lens);
    AGENTOS_FREE(s2_output);

    coord->stats.total_time_ns = agentos_time_monotonic_ns() - start_ns;
    if (coord->stats.total_units > 0) {
        float sum = 0.0f;
        for (size_t i = 0; i < coord->unit_results_count; i++) {
            sum += coord->unit_results[i].score;
        }
        coord->stats.avg_score = sum / (float)coord->unit_results_count;
    }

    coord->active = 0;
    *out_output = full_output;
    if (out_output_len) *out_output_len = full_output_len;
    return AGENTOS_SUCCESS;
}

agentos_error_t tc3_coordinator_execute_streaming(
    tc3_coordinator_t* coord,
    const char* input,
    size_t input_len,
    char** out_output,
    size_t* out_output_len) {
    return tc3_coordinator_execute(coord, input, input_len, out_output, out_output_len);
}

agentos_error_t tc3_coordinator_get_stats(
    const tc3_coordinator_t* coord,
    tc3_stats_t* out_stats) {
    if (!coord || !out_stats) return AGENTOS_EINVAL;
    *out_stats = coord->stats;
    return AGENTOS_SUCCESS;
}

agentos_error_t tc3_coordinator_reset(tc3_coordinator_t* coord) {
    if (!coord) return AGENTOS_EINVAL;
    free_unit_results(coord);
    if (coord->detector) su_stream_detector_reset(coord->detector);
    memset(&coord->stats, 0, sizeof(tc3_stats_t));
    coord->active = 0;
    return AGENTOS_SUCCESS;
}
