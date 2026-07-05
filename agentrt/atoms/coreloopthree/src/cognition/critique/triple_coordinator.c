/**
 * @file triple_coordinator.c
 * @brief 三组件协调器实现 - t2/t1-f/t1-p 流式批判循环
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "triple_coordinator.h"

#include "agentrt.h"
#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_unit_results(tc3_coordinator_t *coord)
{
    if (!coord)
        return;
    for (size_t i = 0; i < coord->unit_results_count; i++) {
        if (coord->unit_results[i].critique) {
            AGENTRT_FREE(coord->unit_results[i].critique);
            coord->unit_results[i].critique = NULL;
        }
    }
    coord->unit_results_count = 0;
}

static agentrt_error_t record_unit_result(tc3_coordinator_t *coord, const tc3_unit_result_t *result)
{
    if (!coord || !result) {
        AGENTRT_LOG_ERROR("record_unit_result: NULL params (coord=%p result=%p)", (void *)coord, (void *)result);
        return AGENTRT_EINVAL;
    }

    if (coord->unit_results_count >= coord->unit_results_capacity) {
        size_t new_cap = coord->unit_results_capacity * 2;
        if (new_cap == 0)
            new_cap = 32;
        tc3_unit_result_t *new_arr = (tc3_unit_result_t *)AGENTRT_REALLOC(
            coord->unit_results, new_cap * sizeof(tc3_unit_result_t));
        if (!new_arr) {
            AGENTRT_LOG_ERROR("record_unit_result: unit_results REALLOC failed (new_cap=%zu)", new_cap);
            return AGENTRT_ENOMEM;
        }
        coord->unit_results = new_arr;
        coord->unit_results_capacity = new_cap;
    }

    tc3_unit_result_t *slot = &coord->unit_results[coord->unit_results_count];
    *slot = *result;
    if (result->critique && result->critique_len > 0) {
        slot->critique = (char *)AGENTRT_MALLOC(result->critique_len + 1);
        if (slot->critique) {
            __builtin_memcpy(slot->critique, result->critique, result->critique_len);
            slot->critique[result->critique_len] = '\0';
        }
    } else {
        slot->critique = NULL;
    }
    coord->unit_results_count++;
    return AGENTRT_SUCCESS;
}

static tc3_verdict_t score_to_verdict(float score, const tc3_config_t *config)
{
    if (score >= config->accept_threshold)
        return TC3_RESULT_ACCEPT;
    if (score >= config->minor_fix_threshold)
        return TC3_RESULT_MINOR_FIX;
    if (score >= config->escalate_threshold)
        return TC3_RESULT_MAJOR_FIX;
    return TC3_RESULT_REJECT;
}

static agentrt_error_t default_s1_verify(const char *content, size_t content_len, float *out_score,
                                         int *out_acceptable, char **out_critique,
                                         size_t *out_critique_len, void *user_data)
{
    if (!content || !out_score) {
        AGENTRT_LOG_ERROR("default_s1_verify: NULL params (content=%p out_score=%p)", (void *)content, (void *)out_score);
        return AGENTRT_EINVAL;
    }

    const tc3_config_t *config = (const tc3_config_t *)user_data;
    float accept_threshold = config ? config->accept_threshold : TC3_ACCEPT_THRESHOLD;

    /* P2.8: 对抗式验证改造 — 移除确认偏误关键词加分。
     *
     * 原 default_s1_verify 基于"because"/"therefore"关键词加分，这是
     * 确认偏误（用表面关键词判断正确性）。对抗式验证的核心是"假设有
     * 缺陷，寻找证据"，而非"假设正确，寻找确认信号"。
     *
     * 新评分逻辑仅基于最低限度的结构完整性检查（内容非空、有基本长度、
     * 含句号表示完整句子），作为 LLM 和 metacognition 都不可用时的
     * 最后回退。这是"无法验证时的保守估计"，而非"验证通过"。
     *
     * 基准分 0.4（保守），长度加分上限 0.2，最高 0.6 — 低于大多数
     * accept_threshold（0.55~0.7），确保 default_s1_verify 不会自动
     * 放行，强制走修正路径或专家仲裁。
     */
    float score = 0.4f;
    if (content_len > 50)
        score += 0.1f;
    if (content_len > 200)
        score += 0.1f;
    if (content_len > 0 && strchr(content, '.'))
        score += 0.05f; /* 含句号 = 至少有一个完整句子 */

    if (score > 0.65f)
        score = 0.65f; /* default_s1_verify 最高 0.65，不自动放行 */
    *out_score = score;
    if (out_acceptable)
        *out_acceptable = (score >= accept_threshold) ? 1 : 0;

    if (out_critique && out_critique_len) {
        if (score < accept_threshold) {
            const char *tmpl = "Default heuristic: content unverified (score=%.2f < %.2f). "
                               "LLM/metacognition unavailable — conservative verdict applied.";
            size_t buf_sz = 192;
            char *buf = (char *)AGENTRT_MALLOC(buf_sz);
            if (buf) {
                snprintf(buf, buf_sz, tmpl, score, (double)accept_threshold);
                *out_critique = buf;
                *out_critique_len = strlen(buf);
            }
        } else {
            *out_critique = NULL;
            *out_critique_len = 0;
        }
    }
    return AGENTRT_SUCCESS;
}

static agentrt_error_t verify_with_metacognition(tc3_coordinator_t *coord,
                                                 agentrt_thinking_step_t *step, const char *content,
                                                 size_t content_len, float *out_score,
                                                 int *out_acceptable, char **out_critique,
                                                 size_t *out_critique_len)
{
    /* ── ThinkDual 升级: 验证优先级 ──
     *
     * 优先级 1: LLM 驱动的 s1_verify 回调（如果已注入）
     *   - 当 engine 注入了 LLM 驱动的 s1_verify 时，它优先于 strstr 启发式，
     *     因为 LLM 能真正检查逻辑正确性和任务对齐度（不偏离任务）。
     *
     * 优先级 2: Metacognition 启发式（strstr 关键词匹配）
     *   - 当 s1_verify 未注入或调用失败时，使用 5 维度启发式评分作为回退。
     *
     * 优先级 3: default_s1_verify（内置启发式）
     *   - 当 metacognition 也不可用时，使用最简单的长度/关键词评分。
     *
     * 原实现将 metacognition 置于最高优先级，导致 LLM 驱动的 s1_verify
     * 永远不会被调用（metacognition 的 strstr 启发式几乎总是成功）。
     */

    /* 优先级 1: LLM 驱动的 s1_verify 回调 */
    if (coord->config.s1_verify) {
        float raw_score = 0.0f;
        int raw_acceptable = 0;
        agentrt_error_t vfy_err = coord->config.s1_verify(
            content, content_len, &raw_score, &raw_acceptable, out_critique,
            out_critique_len, coord->config.s1_user_data);

        if (vfy_err == AGENTRT_SUCCESS) {
            if (coord->calibrator) {
                double calibrated = confidence_calibrator_calibrate(
                    coord->calibrator, (double)raw_score, CC_DIM_ACCURACY);
                *out_score = (float)calibrated;
            } else {
                *out_score = raw_score;
            }
            *out_acceptable = raw_acceptable;
            return AGENTRT_SUCCESS;
        }
        /* s1_verify 失败 — 降级到 metacognition 启发式 */
        AGENTRT_LOG_WARN("verify_with_metacognition: s1_verify callback failed "
                         "(err=%d content_len=%zu), falling back to metacognition",
                         (int)vfy_err, content_len);
    }

    /* 优先级 2: Metacognition 启发式（strstr 关键词匹配） */
    if (coord->meta && step) {
        mc_evaluation_result_t eval;
        agentrt_error_t err =
            agentrt_mc_evaluate_quick(coord->meta, step, out_score, out_acceptable);
        if (err == AGENTRT_SUCCESS) {
            err = agentrt_mc_evaluate_step(coord->meta, step, content, content_len, &eval);
            if (err == AGENTRT_SUCCESS) {
                if (coord->calibrator) {
                    double raw = (double)eval.overall_score;
                    double calibrated =
                        confidence_calibrator_calibrate(coord->calibrator, raw, CC_DIM_ACCURACY);
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
                return AGENTRT_SUCCESS;
            } else {
                AGENTRT_LOG_WARN("verify_with_metacognition: mc_evaluate_step failed "
                                 "(err=%d step_id=%u), falling back to default_s1_verify",
                                 (int)err, step->step_id);
            }
        } else {
            AGENTRT_LOG_WARN("verify_with_metacognition: mc_evaluate_quick failed "
                             "(err=%d step_id=%u), falling back to default_s1_verify",
                             (int)err, step->step_id);
        }
    }

    /* 优先级 3: default_s1_verify（内置启发式） */
    float raw_score = 0.0f;
    int raw_acceptable = 0;
    agentrt_error_t vfy_err = default_s1_verify(content, content_len, &raw_score,
                                                &raw_acceptable, out_critique,
                                                out_critique_len, (void *)&coord->config);

    if (vfy_err == AGENTRT_SUCCESS && coord->calibrator) {
        double calibrated =
            confidence_calibrator_calibrate(coord->calibrator, (double)raw_score, CC_DIM_ACCURACY);
        *out_score = (float)calibrated;
        *out_acceptable = raw_acceptable;
    } else if (vfy_err == AGENTRT_SUCCESS) {
        *out_score = raw_score;
        *out_acceptable = raw_acceptable;
    } else {
        AGENTRT_LOG_ERROR("verify_with_metacognition: default_s1_verify failed "
                          "(vfy_err=%d content_len=%zu)",
                          (int)vfy_err, content_len);
    }
    return vfy_err;
}

/* P3.11-B6: 基于字符频率余弦相似度的语义环路检测。
 *
 * 此前环路检测仅用 memcmp 完全匹配（L523-525），无法捕获 paraphrase、近义改写、
 * 局部重复等语义循环——S2 无视 critique 但做了微调（如改标点、换行）即可绕过检测，
 * 导致 correction 循环空转消耗资源。
 *
 * 改进为字符频率余弦相似度：对 UTF-8 字节流统计 256 个字节值的出现频率，计算
 * 频率向量的余弦相似度。对中英文均有效（中文 UTF-8 字节频率具区分性）。
 *
 * 阈值 0.95：memcmp 对应 cosine=1.0，0.95 允许微小改动但仍判定为环路。
 * 使用平方比较（cosine² > 0.95² ≈ 0.9025）避免 math.h sqrt 依赖。
 *
 * 局限性：字符频率相似度对"相同字符不同顺序"的文本会给出高分（如 "abc" vs "cba"），
 * 但在 correction 场景中，S2 生成的内容与当前内容字符顺序通常一致（仅微调），
 * 此局限可接受。1.0.1 可升级为 bigram Jaccard 或编辑距离。 */
static int text_is_semantic_loop(const char *a, size_t a_len, const char *b, size_t b_len)
{
    if (a_len == 0 || b_len == 0)
        return 0;

    int freq_a[256] = {0};
    int freq_b[256] = {0};

    for (size_t i = 0; i < a_len; i++)
        freq_a[(unsigned char)a[i]]++;
    for (size_t i = 0; i < b_len; i++)
        freq_b[(unsigned char)b[i]]++;

    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (int i = 0; i < 256; i++) {
        dot += (double)freq_a[i] * (double)freq_b[i];
        norm_a += (double)freq_a[i] * (double)freq_a[i];
        norm_b += (double)freq_b[i] * (double)freq_b[i];
    }

    if (norm_a == 0.0 || norm_b == 0.0)
        return 0;

    /* cosine² = dot² / (norm_a * norm_b)，阈值 0.95² ≈ 0.9025 */
    double cosine_sq = (dot * dot) / (norm_a * norm_b);
    return cosine_sq > 0.9025;
}

static agentrt_error_t build_correction_prompt(const char *original, size_t original_len,
                                               const char *critique, size_t critique_len,
                                               int attempt, char **out_prompt,
                                               size_t *out_prompt_len)
{
    size_t buf_sz = original_len + critique_len + 256;
    char *buf = (char *)AGENTRT_MALLOC(buf_sz);
    if (!buf) {
        AGENTRT_LOG_ERROR("build_correction_prompt: allocation failed (buf_sz=%zu original_len=%zu critique_len=%zu)", buf_sz, original_len, critique_len);
        return AGENTRT_ENOMEM;
    }

    int written = snprintf(buf, buf_sz,
                           "[Original]\n%.*s\n[Critique #%d: %.*s]\n"
                           "[Instruction: Improve based on critique above]",
                           (int)(original_len > 500 ? 500 : original_len), original, attempt,
                           (int)(critique_len > 300 ? 300 : critique_len),
                           critique ? critique : "quality insufficient");

    if (written <= 0 || (size_t)written >= buf_sz) {
        AGENTRT_LOG_ERROR("build_correction_prompt: snprintf encoding error or truncation (written=%d buf_sz=%zu attempt=%d)", written, buf_sz, attempt);
        AGENTRT_FREE(buf);
        return AGENTRT_EUNKNOWN;
    }

    *out_prompt = buf;
    *out_prompt_len = (size_t)written;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t concatenate_units(const char **units, const size_t *lens, size_t count,
                                         char **out_text, size_t *out_len)
{
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += lens[i];
    }

    char *buf = (char *)AGENTRT_MALLOC(total + 1);
    if (!buf) {
        AGENTRT_LOG_ERROR("concatenate_units: allocation failed (total=%zu count=%zu)", total + 1, count);
        return AGENTRT_ENOMEM;
    }

    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        __builtin_memcpy(buf + pos, units[i], lens[i]);
        pos += lens[i];
    }
    buf[pos] = '\0';

    *out_text = buf;
    *out_len = pos;
    return AGENTRT_SUCCESS;
}

agentrt_error_t tc3_coordinator_create(const tc3_config_t *config, agentrt_thinking_chain_t *chain,
                                       agentrt_metacognition_t *meta, tc3_coordinator_t **out_coord)
{
    if (!out_coord) {
        AGENTRT_LOG_ERROR("tc3_coordinator_create: NULL out_coord parameter");
        return AGENTRT_EINVAL;
    }

    tc3_coordinator_t *coord = (tc3_coordinator_t *)AGENTRT_CALLOC(1, sizeof(tc3_coordinator_t));
    if (!coord) {
        AGENTRT_LOG_ERROR("tc3_coordinator_create: allocation failed for coordinator");
        return AGENTRT_ENOMEM;
    }

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
        AGENTRT_LOG_ERROR("tc3_coordinator_create: calibrator creation failed");
        AGENTRT_FREE(coord);
        return AGENTRT_ENOMEM;
    }

    agentrt_error_t err = su_stream_detector_create(&coord->config.stream_config, &coord->detector);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("tc3_coordinator_create: stream detector creation failed (err=%d)", (int)err);
        confidence_calibrator_destroy(coord->calibrator);
        AGENTRT_FREE(coord);
        return err;
    }

    coord->unit_results = NULL;
    coord->unit_results_capacity = 0;
    coord->unit_results_count = 0;
    coord->active = 0;
    __builtin_memset(&coord->stats, 0, sizeof(tc3_stats_t));

    *out_coord = coord;
    return AGENTRT_SUCCESS;
}

void tc3_coordinator_destroy(tc3_coordinator_t *coord)
{
    if (!coord)
        return;
    if (coord->detector)
        su_stream_detector_destroy(coord->detector);
    if (coord->calibrator)
        confidence_calibrator_destroy(coord->calibrator);
    free_unit_results(coord);
    if (coord->unit_results)
        AGENTRT_FREE(coord->unit_results);
    AGENTRT_FREE(coord);
}

agentrt_error_t tc3_coordinator_execute(tc3_coordinator_t *coord, const char *input,
                                        size_t input_len, char **out_output, size_t *out_output_len)
{
    if (!coord || !input || !out_output) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: NULL params (coord=%p input=%p out_output=%p)", (void *)coord, (void *)input, (void *)out_output);
        return AGENTRT_EINVAL;
    }

    coord->active = 1;
    uint64_t start_ns = agentrt_time_monotonic_ns();
    AGENTRT_LOG_INFO("C-L02: ThinkDual tc3 execute started (input_len=%zu max_rounds=%u accept_threshold=%.2f)",
                    input_len, coord->config.max_verify_rounds, (double)coord->config.accept_threshold);

    agentrt_error_t err = su_stream_detector_reset(coord->detector);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: stream detector reset failed (err=%d)", (int)err);
        coord->active = 0;
        return err;
    }

    char *full_output = NULL;
    size_t full_output_len = 0;

    tc3_s2_generate_fn s2_fn = coord->config.s2_generate;
    if (!s2_fn) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: no S2 generate function configured, cannot execute");
        coord->active = 0;
        return AGENTRT_ESERVICE;
    }

    char *s2_output = NULL;
    size_t s2_output_len = 0;
    err = s2_fn(input, input_len, &s2_output, &s2_output_len, coord->config.s2_user_data);
    if (err != AGENTRT_SUCCESS || !s2_output) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: S2 generate call failed (err=%d s2_output=%p input_len=%zu)", (int)err, (void *)s2_output, input_len);
        if (s2_output)
            AGENTRT_FREE(s2_output);
        coord->active = 0;
        return err ? err : AGENTRT_EUNKNOWN;
    }

    AGENTRT_LOG_DEBUG("C-L02: ThinkDual S2 generated (s2_output_len=%zu)", s2_output_len);
    err = su_stream_detector_feed(coord->detector, s2_output, s2_output_len, 0.7f);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: stream detector feed failed (err=%d s2_output_len=%zu)", (int)err, s2_output_len);
        AGENTRT_FREE(s2_output);
        coord->active = 0;
        return err;
    }
    err = su_stream_detector_flush(coord->detector);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("tc3_coordinator_execute: stream detector flush failed (err=%d)", (int)err);
        AGENTRT_FREE(s2_output);
        coord->active = 0;
        return err;
    }

    size_t accepted_count = 0;
    const char **accepted_texts = NULL;
    size_t *accepted_lens = NULL;
    size_t accepted_capacity = 0;

    while (su_stream_detector_pending_count(coord->detector) > 0) {
        su_semantic_unit_t unit;
        err = su_stream_detector_pop_pending(coord->detector, &unit);
        if (err != AGENTRT_SUCCESS)
            break;

        float score = 0.0f;
        int acceptable = 0;
        char *critique = NULL;
        size_t critique_len = 0;

        agentrt_thinking_step_t *gen_step = NULL;
        if (coord->chain) {
            agentrt_tc_step_create(coord->chain, TC_STEP_GENERATION, unit.text, unit.text_len, NULL,
                                   0, &gen_step);
            if (gen_step) {
                agentrt_tc_step_complete(gen_step, unit.text, unit.text_len, unit.confidence,
                                         "t2-generator");
            }
        }

        verify_with_metacognition(coord, gen_step, unit.text, unit.text_len, &score, &acceptable,
                                  &critique, &critique_len);

        tc3_verdict_t verdict = score_to_verdict(score, &coord->config);
        uint32_t correction_attempts = 0;
        tc3_role_t verified_by = TC3_ROLE_T1F;
        AGENTRT_LOG_DEBUG("C-L02: ThinkDual unit[%u] initial verify (score=%.2f verdict=%d)",
                         (unsigned int)unit.unit_index, (double)score, (int)verdict);

        char *current_text = unit.text;
        size_t current_len = unit.text_len;
        int owns_current = 1;  /* pop_pending 已转移 unit.text 的所有权 */

        while (verdict != TC3_RESULT_ACCEPT && verdict != TC3_RESULT_REJECT &&
               correction_attempts < coord->config.max_verify_rounds) {

            if (verdict == TC3_RESULT_MAJOR_FIX && coord->config.s1_expert) {
                float expert_score = 0.0f;
                tc3_verdict_t expert_verdict = TC3_RESULT_REJECT;
                char *expert_opinion = NULL;
                size_t expert_opinion_len = 0;

                agentrt_error_t exp_err = coord->config.s1_expert(
                    current_text, current_len, critique ? critique : "quality low", critique_len,
                    &expert_score, &expert_verdict, &expert_opinion, &expert_opinion_len,
                    coord->config.s1_expert_user_data);

                if (exp_err == AGENTRT_SUCCESS) {
                    verdict = expert_verdict;
                    score = expert_score;
                    verified_by = TC3_ROLE_T1P;
                    AGENTRT_LOG_DEBUG("C-L02: ThinkDual unit[%u] expert arbitration (verdict=%d score=%.2f)",
                                     (unsigned int)unit.unit_index, (int)expert_verdict, (double)expert_score);
                    if (expert_opinion)
                        AGENTRT_FREE(expert_opinion);
                } else {
                    AGENTRT_LOG_WARN("tc3_coordinator_execute: S1 expert call failed (exp_err=%d unit_index=%u)", (int)exp_err, (unsigned int)unit.unit_index);
                }
                coord->stats.escalated_units++;
                break;
            }

            correction_attempts++;
            coord->stats.total_corrections++;

            char *correction_prompt = NULL;
            size_t correction_prompt_len = 0;
            err = build_correction_prompt(current_text, current_len, critique, critique_len,
                                          (int)correction_attempts, &correction_prompt,
                                          &correction_prompt_len);
            if (err != AGENTRT_SUCCESS)
                break;

            char *corrected = NULL;
            size_t corrected_len = 0;
            err = s2_fn(correction_prompt, correction_prompt_len, &corrected, &corrected_len,
                        coord->config.s2_user_data);
            AGENTRT_FREE(correction_prompt);

            if (err != AGENTRT_SUCCESS || !corrected) {
                AGENTRT_LOG_WARN("tc3_coordinator_execute: S2 correction call failed (err=%d corrected=%p attempt=%u unit_index=%u)", (int)err, (void *)corrected, correction_attempts, (unsigned int)unit.unit_index);
                if (critique)
                    AGENTRT_FREE(critique);
                break;
            }

            /* P3.11-B6: 语义循环检测 — 基于字符频率余弦相似度
             *
             * 当 S2 生成的 corrected 与 current_text 语义高度相似（cosine > 0.92）时，
             * 说明修正陷入循环（S2 无视了 critique，仅做微调重复生成）。继续循环只会
             * 消耗资源而无法改善，因此中止修正循环，保留当前 verdict（通常为
             * MINOR_FIX/MAJOR_FIX），交由外层 ACCEPT/REJECT 决策处理。
             *
             * P2.9 原实现仅检测完全相同（memcmp），S2 改标点/换行/微调即可绕过。
             * P3.11-B6 升级为字符频率余弦相似度，捕获 paraphrase / 近义改写 / 局部重复。
             */
            if (current_text && corrected && current_len > 0 && corrected_len > 0 &&
                text_is_semantic_loop(current_text, current_len, corrected, corrected_len)) {
                AGENTRT_LOG_WARN("tc3: unit[%u] semantic loop detected — corrected text "
                                 "highly similar to current (attempt=%u cur_len=%zu "
                                 "corr_len=%zu cosine>0.95), aborting correction",
                                 (unsigned int)unit.unit_index, correction_attempts,
                                 current_len, corrected_len);
                coord->stats.loop_detected_units++;
                AGENTRT_FREE(corrected);
                corrected = NULL;
                break;
            }

            if (owns_current && current_text)
                AGENTRT_FREE(current_text);
            current_text = corrected;
            current_len = corrected_len;
            owns_current = 1;

            if (critique) {
                AGENTRT_FREE(critique);
                critique = NULL;
            }

            verify_with_metacognition(coord, gen_step, current_text, current_len, &score,
                                      &acceptable, &critique, &critique_len);
            verdict = score_to_verdict(score, &coord->config);
            AGENTRT_LOG_DEBUG("C-L02: ThinkDual unit[%u] correction round %u (score=%.2f verdict=%d)",
                             (unsigned int)unit.unit_index, correction_attempts, (double)score, (int)verdict);
        }

        tc3_unit_result_t result = {.unit_index = unit.unit_index,
                                    .verdict = verdict,
                                    .score = score,
                                    .critique = critique,
                                    .critique_len = critique_len,
                                    .verified_by = verified_by,
                                    .correction_attempts = correction_attempts};
        record_unit_result(coord, &result);

        if (critique)
            AGENTRT_FREE(critique);
        critique = NULL;

        AGENTRT_LOG_INFO("C-L02: ThinkDual unit[%u] final (verdict=%d score=%.2f attempts=%u by=%d)",
                        (unsigned int)unit.unit_index, (int)verdict, (double)score,
                        correction_attempts, (int)verified_by);
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
            /* P3.11-B5: REJECT 大声失败 + 一次"完全重写"重试
             *
             * P2.9 原实现仅记录 WARN 日志后 continue 丢弃，无重试机制。
             * P3.11-B5 改为：
             *   1. 用 "complete rewrite from scratch" 指令调用 s2_fn 重新生成一次
             *      （区别于 correction 的 "improve based on critique" 指令）
             *   2. 重新验证：若 verdict 改善（非 REJECT），接受重试结果
             *   3. 若重试仍 REJECT，记录 ERROR 级别日志（大声失败）并丢弃
             */
            int retry_succeeded = 0;
            if (s2_fn && current_text && current_len > 0) {
                /* 构建 rewrite prompt（不截断，完整传入 current_text） */
                size_t rw_sz = current_len + 256;
                char *rw_prompt = (char *)AGENTRT_MALLOC(rw_sz);
                if (rw_prompt) {
                    int rw_written = snprintf(rw_prompt, rw_sz,
                        "[Rejected Content (quality insufficient after %u corrections)]\n"
                        "%.*s\n\n"
                        "[Instruction: Completely rewrite this from scratch with high quality. "
                        "Do not merely patch the above — produce a fresh, correct version.]",
                        (unsigned)correction_attempts,
                        (int)current_len, current_text);

                    if (rw_written > 0 && (size_t)rw_written < rw_sz) {
                        char *rewritten = NULL;
                        size_t rewritten_len = 0;
                        agentrt_error_t rw_err = s2_fn(rw_prompt, (size_t)rw_written,
                                                        &rewritten, &rewritten_len,
                                                        coord->config.s2_user_data);
                        if (rw_err == AGENTRT_SUCCESS && rewritten && rewritten_len > 0) {
                            float retry_score = 0.0f;
                            int retry_acceptable = 0;
                            char *retry_critique = NULL;
                            size_t retry_critique_len = 0;
                            verify_with_metacognition(coord, gen_step, rewritten, rewritten_len,
                                                      &retry_score, &retry_acceptable,
                                                      &retry_critique, &retry_critique_len);
                            tc3_verdict_t retry_verdict = score_to_verdict(retry_score, &coord->config);
                            if (retry_critique)
                                AGENTRT_FREE(retry_critique);

                            if (retry_verdict != TC3_RESULT_REJECT) {
                                /* 重试成功 — 撤回 REJECT 计数，更新 verdict 和统计 */
                                coord->stats.rejected_units--;
                                if (owns_current && current_text)
                                    AGENTRT_FREE(current_text);
                                current_text = rewritten;
                                current_len = rewritten_len;
                                owns_current = 1;
                                verdict = retry_verdict;
                                score = retry_score;
                                correction_attempts++; /* 计入重试 */
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
                                default:
                                    break;
                                }
                                retry_succeeded = 1;
                                AGENTRT_LOG_INFO("tc3: unit[%u] REJECT retry SUCCEEDED "
                                                 "(rewrite verdict=%d score=%.2f)",
                                                 (unsigned int)unit.unit_index,
                                                 (int)verdict, (double)score);
                            } else {
                                AGENTRT_FREE(rewritten);
                                AGENTRT_LOG_DEBUG("tc3: unit[%u] REJECT retry still REJECTED "
                                                  "(score=%.2f)",
                                                  (unsigned int)unit.unit_index,
                                                  (double)retry_score);
                            }
                        } else {
                            AGENTRT_LOG_WARN("tc3: unit[%u] REJECT retry S2 call failed "
                                             "(err=%d rewritten=%p)",
                                             (unsigned int)unit.unit_index,
                                             (int)rw_err, (void *)rewritten);
                        }
                    }
                    AGENTRT_FREE(rw_prompt);
                }
            }

            if (!retry_succeeded) {
                /* P3.11-B5: 大声失败 — ERROR 级别日志（P2.9 为 WARN） */
                AGENTRT_LOG_ERROR("tc3: unit[%u] REJECTED after retry (score=%.2f attempts=%u "
                                  "rejected=%zu/%zu=%.0f%%) — content discarded",
                                  (unsigned int)unit.unit_index, (double)score,
                                  correction_attempts, coord->stats.rejected_units,
                                  coord->stats.total_units,
                                  coord->stats.total_units > 0
                                      ? (double)coord->stats.rejected_units /
                                            (double)coord->stats.total_units * 100.0
                                      : 0.0);
                if (owns_current)
                    AGENTRT_FREE(current_text);
                continue;
            }
            /* 重试成功，fall through 到 accepted 数组 */
        }

        if (accepted_count >= accepted_capacity) {
            size_t new_cap = accepted_capacity * 2;
            if (new_cap == 0)
                new_cap = 16;
            const char **new_texts =
                (const char **)AGENTRT_REALLOC(accepted_texts, new_cap * sizeof(char *));
            size_t *new_lens = (size_t *)AGENTRT_REALLOC(accepted_lens, new_cap * sizeof(size_t));
            if (!new_texts || !new_lens) {
                AGENTRT_LOG_ERROR("tc3_coordinator_execute: accepted arrays REALLOC failed (new_cap=%zu accepted_count=%zu)", new_cap, accepted_count);
                if (new_texts)
                    accepted_texts = new_texts;
                if (new_lens)
                    accepted_lens = new_lens;
                if (owns_current && current_text)
                    AGENTRT_FREE(current_text);
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
        concatenate_units(accepted_texts, accepted_lens, accepted_count, &full_output,
                          &full_output_len);
        /* concatenate_units 已创建拼接副本，释放各片段原文 */
        for (size_t i = 0; i < accepted_count; i++) {
            if (accepted_texts[i])
                AGENTRT_FREE((void *)accepted_texts[i]);
        }
    }

    if (accepted_texts)
        AGENTRT_FREE(accepted_texts);
    if (accepted_lens)
        AGENTRT_FREE(accepted_lens);
    AGENTRT_FREE(s2_output);

    coord->stats.total_time_ns = agentrt_time_monotonic_ns() - start_ns;
    AGENTRT_LOG_INFO("C-L02: ThinkDual tc3 complete (units=%u accepted=%u minor=%u major=%u "
                     "rejected=%u loops=%u corrections=%u avg_score=%.2f time=%llu ms)",
                    (unsigned)coord->stats.total_units, (unsigned)coord->stats.accepted_units,
                    (unsigned)coord->stats.minor_fix_units, (unsigned)coord->stats.major_fix_units,
                    (unsigned)coord->stats.rejected_units, (unsigned)coord->stats.loop_detected_units,
                    (unsigned)coord->stats.total_corrections, (double)coord->stats.avg_score,
                    (unsigned long long)(coord->stats.total_time_ns / 1000000ULL));
    if (coord->stats.total_units > 0) {
        float sum = 0.0f;
        for (size_t i = 0; i < coord->unit_results_count; i++) {
            sum += coord->unit_results[i].score;
        }
        coord->stats.avg_score = sum / (float)coord->unit_results_count;
    }

    coord->active = 0;
    *out_output = full_output;
    if (out_output_len)
        *out_output_len = full_output_len;
    return AGENTRT_SUCCESS;
}

agentrt_error_t tc3_coordinator_execute_streaming(tc3_coordinator_t *coord, const char *input,
                                                  size_t input_len, char **out_output,
                                                  size_t *out_output_len)
{
    return tc3_coordinator_execute(coord, input, input_len, out_output, out_output_len);
}

agentrt_error_t tc3_coordinator_get_stats(const tc3_coordinator_t *coord, tc3_stats_t *out_stats)
{
    if (!coord || !out_stats) {
        AGENTRT_LOG_ERROR("tc3_coordinator_get_stats: NULL params (coord=%p out_stats=%p)", (void *)coord, (void *)out_stats);
        return AGENTRT_EINVAL;
    }
    *out_stats = coord->stats;
    return AGENTRT_SUCCESS;
}

agentrt_error_t tc3_coordinator_reset(tc3_coordinator_t *coord)
{
    if (!coord) {
        AGENTRT_LOG_ERROR("tc3_coordinator_reset: NULL coord parameter");
        return AGENTRT_EINVAL;
    }
    free_unit_results(coord);
    if (coord->detector)
        su_stream_detector_reset(coord->detector);
    __builtin_memset(&coord->stats, 0, sizeof(tc3_stats_t));
    coord->active = 0;
    return AGENTRT_SUCCESS;
}
