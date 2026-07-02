/**
 * @file tc3_llm_callbacks.c
 * @brief ThinkDual (双思考系统) LLM 驱动的 tc3 回调实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 triple_coordinator 的三个 LLM 驱动回调，使 ThinkDual 的
 * t2/t1-f/t1-p 流式批判循环真正生效：
 *
 *   s2_generate → T2 主思考：首轮复用 engine seed，后续轮 LLM 独立生成
 *   s1_verify   → T1-F 验证：LLM 评估逻辑正确性 + 任务对齐度（不偏离）
 *   s1_expert   → T1-P 仲裁：LLM 扮演领域专家给出最终判决
 *
 * 详见 tc3_llm_callbacks.h 的架构说明。
 */

#include "tc3_llm_callbacks.h"

#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h> /* strncasecmp / strcasecmp (POSIX) */
#endif

#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* ==================== 内部常量 ==================== */

/* Prompt 模板 — 精心设计以保证"逻辑正确 + 不偏离任务" */
static const char S2_SYSTEM_PROMPT[] =
    "You are a cognitive reasoning assistant in a dual-thinking system. "
    "Generate a thorough, logically rigorous response to the user's request. "
    "Ensure your reasoning is correct, evidence-based, and stays on topic. "
    "If given a correction instruction, improve your previous response accordingly.";

static const char S1_VERIFY_SYSTEM_PROMPT[] =
    "You are an adversarial critic in a dual-thinking system. Your sole job is to find bugs, "
    "flaws, and deviations in the response — assume it is defective until proven otherwise.\n"
    "Hunt aggressively for problems across four dimensions:\n"
    "1. Logical flaws: Find fallacies, unsupported claims, contradictions, or faulty reasoning. "
    "Do NOT assume the reasoning is sound — probe it.\n"
    "2. Task deviation: Identify ANY drift from the original task. Flag tangential, missing, or "
    "over-scoped content. Even subtle misalignment is a bug.\n"
    "3. Gaps: Find what is incomplete, hand-waved, or left unanswered. Vague answers are defects.\n"
    "4. Errors: Hunt for factual errors, incorrect assumptions, and broken logic chains.\n"
    "Be skeptical and rigorous. A long or well-formatted response is NOT automatically correct — "
    "length and clarity do not excuse logical defects. Only assign a high score if you genuinely "
    "cannot find any bugs after thorough scrutiny.\n"
    "Reply with ONLY a JSON object (no markdown fences, no extra text):\n"
    "{\"score\": <float 0.0-1.0>, \"acceptable\": <true|false>, \"critique\": \"<list every bug found, "
    "or empty string only if flawless>\"}\n"
    "Score guide: 0.9-1.0=no bugs found after rigorous scrutiny, 0.7-0.89=minor issues only, "
    "0.5-0.69=several bugs or one significant flaw, 0.3-0.49=major flaws present, "
    "below 0.3=critical defects, reject.";

static const char S1_EXPERT_SYSTEM_PROMPT[] =
    "You are a domain expert arbitrator in a dual-thinking system. A fast verifier has "
    "flagged issues with a response. Review the content and critique, then give your "
    "authoritative final verdict.\n"
    "Reply with ONLY a JSON object (no markdown fences, no extra text):\n"
    "{\"verdict\": \"<accept|minor_fix|major_fix|reject>\", \"score\": <float 0.0-1.0>, "
    "\"opinion\": \"<your expert reasoning>\"}\n"
    "Use 'accept' only if the response is correct despite the critique. Use 'reject' only "
    "if the response is fundamentally wrong and cannot be salvaged.";

/* LLM 响应缓冲区上限（防御性） */
#define TC3_LLM_MAX_RESPONSE_LEN  (256 * 1024)
/* prompt 缓冲区上限 */
#define TC3_LLM_MAX_PROMPT_LEN    (64 * 1024)

/* ==================== 内部辅助：LLM 调用 ==================== */

/**
 * @brief 统一的 LLM 调用辅助函数
 *
 * 线程安全地读取 LLM 句柄（加锁），然后在锁外调用 LLM（避免长时间持锁）。
 * 优先 IPC adapter，回退直接 service。
 *
 * @param ctx       回调上下文
 * @param system    system prompt
 * @param user      user prompt
 * @param model     P2.7: 指定模型名（NULL = provider 默认模型）
 * @param out_text  [out] 响应文本（AGENTOS_MALLOC 分配，调用方释放）
 * @param out_len   [out] 响应文本长度
 * @return AGENTOS_SUCCESS 或错误码
 */
static agentos_error_t tc3_llm_call(const tc3_llm_ctx_t *ctx, const char *system,
                                    const char *user, const char *model,
                                    char **out_text, size_t *out_len)
{
    if (!ctx || !system || !user || !out_text || !out_len) {
        return AGENTOS_EINVAL;
    }

    *out_text = NULL;
    *out_len = 0;

    /* 加锁读取 LLM 句柄（borrowed，不持有所有权） */
    llm_svc_adapter_t *adapter = NULL;
    llm_service_t *svc = NULL;
    if (ctx->lock) {
        agentos_mutex_lock(ctx->lock);
    }
    adapter = ctx->llm_adapter;
    svc = ctx->llm_svc;
    if (ctx->lock) {
        agentos_mutex_unlock(ctx->lock);
    }

    if (!adapter && !svc) {
        AGENTOS_LOG_DEBUG("TC3-LLM: no LLM handle available (adapter=%p svc=%p)",
                          (void *)adapter, (void *)svc);
        return AGENTOS_ESERVICE;
    }

    /* 构造请求 */
    llm_message_t msgs[2];
    msgs[0].role = "system";
    msgs[0].content = system;
    msgs[1].role = "user";
    msgs[1].content = user;

    llm_request_config_t llm_cfg;
    __builtin_memset(&llm_cfg, 0, sizeof(llm_cfg));
    /* P2.7: 使用调用方指定的模型（支持 S2/S1-F/S1-P 三独立模型），
     * NULL 时回退到 provider 默认模型（向后兼容）。 */
    llm_cfg.model = model;
    llm_cfg.messages = msgs;
    llm_cfg.message_count = 2;
    llm_cfg.temperature = 0.3f; /* 验证/仲裁需要确定性，低 temperature */
    llm_cfg.top_p = 1.0f;
    llm_cfg.max_tokens = (int)ctx->max_tokens;
    if (llm_cfg.max_tokens == 0) {
        llm_cfg.max_tokens = 4096;
    }
    llm_cfg.stream = 0;

    llm_response_t *resp = NULL;
    int ret = -1;

    if (adapter) {
        ret = llm_svc_adapter_complete(adapter, &llm_cfg, &resp);
        AGENTOS_LOG_DEBUG("TC3-LLM: IPC adapter complete: ret=%d", ret);
    } else if (svc) {
        ret = llm_service_complete(svc, &llm_cfg, &resp);
        AGENTOS_LOG_DEBUG("TC3-LLM: direct service complete: ret=%d", ret);
    }

    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0) {
        AGENTOS_LOG_WARN("TC3-LLM: LLM call failed (ret=%d resp=%p choices=%p count=%u)",
                         ret, (void *)resp,
                         resp ? (void *)resp->choices : NULL,
                         resp ? resp->choice_count : 0u);
        if (resp) {
            llm_response_free(resp);
        }
        return AGENTOS_ESERVICE;
    }

    const char *content = resp->choices[0].content;
    if (!content || content[0] == '\0') {
        AGENTOS_LOG_WARN("TC3-LLM: LLM returned empty content");
        llm_response_free(resp);
        return AGENTOS_ESERVICE;
    }

    size_t content_len = strlen(content);
    if (content_len > TC3_LLM_MAX_RESPONSE_LEN) {
        content_len = TC3_LLM_MAX_RESPONSE_LEN;
        AGENTOS_LOG_WARN("TC3-LLM: LLM response truncated (%zu -> %d)",
                         strlen(content), TC3_LLM_MAX_RESPONSE_LEN);
    }

    /* 复制响应文本（调用方负责释放） */
    char *text = (char *)AGENTOS_MALLOC(content_len + 1);
    if (!text) {
        AGENTOS_LOG_ERROR("TC3-LLM: OOM copying response (%zu bytes)", content_len + 1);
        llm_response_free(resp);
        return AGENTOS_ENOMEM;
    }
    __builtin_memcpy(text, content, content_len);
    text[content_len] = '\0';

    *out_text = text;
    *out_len = content_len;

    AGENTOS_LOG_DEBUG("TC3-LLM: LLM call success (%zu bytes, model=%s tokens=%u)",
                     content_len, resp->model ? resp->model : "?", resp->total_tokens);

    llm_response_free(resp);
    return AGENTOS_SUCCESS;
}

/* ==================== 内部辅助：JSON 字段提取 ==================== */

/**
 * @brief 从 LLM 响应中提取 JSON 对象子串
 *
 * LLM 可能返回带 markdown 围栏 (```json ... ```) 或前后有多余文本的响应。
 * 本函数找到第一个 '{' 和最后一个 '}'，提取中间内容。
 *
 * @param raw     原始 LLM 响应
 * @param raw_len 原始长度
 * @param out_json [out] 指向 raw 内第一个 '{' 的指针
 * @param out_len  [out] JSON 子串长度（含括号）
 * @return 找到 JSON 返回 1，否则返回 0
 */
static int extract_json_object(const char *raw, size_t raw_len,
                               const char **out_json, size_t *out_len)
{
    if (!raw || raw_len == 0) return 0;

    /* 跳过 markdown 围栏 ```json 或 ``` */
    const char *start = raw;
    const char *end = raw + raw_len;

    /* 找第一个 '{' */
    const char *brace_start = NULL;
    for (const char *p = start; p < end; p++) {
        if (*p == '{') {
            brace_start = p;
            break;
        }
    }
    if (!brace_start) return 0;

    /* 找最后一个 '}' */
    const char *brace_end = NULL;
    for (const char *p = end - 1; p >= brace_start; p--) {
        if (*p == '}') {
            brace_end = p;
            break;
        }
    }
    if (!brace_end) return 0;

    *out_json = brace_start;
    *out_len = (size_t)(brace_end - brace_start + 1);
    return 1;
}

/**
 * @brief 手动提取 JSON 中的浮点字段值（cJSON 不可用时的回退）
 *
 * 搜索 "field_name" : <number> 模式，返回浮点值。
 * 支持字段名前后有空格、冒号前后有空格。
 *
 * @return 找到返回 1 并设置 *out_val，否则返回 0
 */
static int extract_float_field(const char *json, size_t json_len,
                               const char *field, float *out_val)
{
    /* 构造搜索模式 "field" */
    char pattern[128];
    int pl = snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    if (pl <= 0 || (size_t)pl >= sizeof(pattern)) return 0;

    /* 在 json 中查找 pattern */
    if (json_len < (size_t)pl) return 0;
    const char *found = NULL;
    for (const char *p = json; p <= json + json_len - pl; p++) {
        if (__builtin_memcmp(p, pattern, pl) == 0) {
            found = p;
            break;
        }
    }
    if (!found) return 0;

    /* 跳过 pattern，找冒号，跳过空格 */
    const char *q = found + pl;
    const char *end = json + json_len;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end || *q != ':') return 0;
    q++;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end) return 0;

    /* 解析浮点数（支持负号和小数） */
    char *endptr = NULL;
    float val = strtof(q, &endptr);
    if (endptr == q) return 0; /* 没有解析到数字 */

    *out_val = val;
    return 1;
}

/**
 * @brief 手动提取 JSON 中的布尔字段值
 * @return 找到返回 1 并设置 *out_val (0 or 1)，否则返回 0
 */
static int extract_bool_field(const char *json, size_t json_len,
                              const char *field, int *out_val)
{
    char pattern[128];
    int pl = snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    if (pl <= 0 || (size_t)pl >= sizeof(pattern)) return 0;

    if (json_len < (size_t)pl) return 0;
    const char *found = NULL;
    for (const char *p = json; p <= json + json_len - pl; p++) {
        if (__builtin_memcmp(p, pattern, pl) == 0) {
            found = p;
            break;
        }
    }
    if (!found) return 0;

    const char *q = found + pl;
    const char *end = json + json_len;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end || *q != ':') return 0;
    q++;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end) return 0;

    /* 检查 true/false */
    if (q + 4 <= end && __builtin_memcmp(q, "true", 4) == 0) {
        *out_val = 1;
        return 1;
    }
    if (q + 5 <= end && __builtin_memcmp(q, "false", 5) == 0) {
        *out_val = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief 手动提取 JSON 中的字符串字段值
 *
 * 搜索 "field" : "value" 模式，返回堆分配的 value 副本。
 * 处理基本转义 (\" \\ \n \t)。
 *
 * @return 找到返回 1 并设置 *out_str (AGENTOS_MALLOC 分配)，否则返回 0
 */
static int extract_string_field(const char *json, size_t json_len,
                                const char *field, char **out_str)
{
    char pattern[128];
    int pl = snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    if (pl <= 0 || (size_t)pl >= sizeof(pattern)) return 0;

    if (json_len < (size_t)pl) return 0;
    const char *found = NULL;
    for (const char *p = json; p <= json + json_len - pl; p++) {
        if (__builtin_memcmp(p, pattern, pl) == 0) {
            found = p;
            break;
        }
    }
    if (!found) return 0;

    const char *q = found + pl;
    const char *end = json + json_len;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end || *q != ':') return 0;
    q++;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q >= end || *q != '"') return 0;
    q++; /* 跳过开引号 */

    /* 收集字符串内容，处理转义 */
    size_t buf_sz = 512;
    char *buf = (char *)AGENTOS_MALLOC(buf_sz);
    if (!buf) return 0;
    size_t pos = 0;

    while (q < end && *q != '"') {
        if (*q == '\\' && q + 1 < end) {
            char esc = *(q + 1);
            char ch = esc;
            if (esc == 'n') ch = '\n';
            else if (esc == 't') ch = '\t';
            else if (esc == 'r') ch = '\r';
            else if (esc == '"') ch = '"';
            else if (esc == '\\') ch = '\\';
            else ch = esc; /* 其他转义保留原字符 */
            q += 2;
            if (pos + 1 >= buf_sz) {
                buf_sz *= 2;
                char *nb = (char *)AGENTOS_REALLOC(buf, buf_sz);
                if (!nb) {
                    AGENTOS_FREE(buf);
                    return 0;
                }
                buf = nb;
            }
            buf[pos++] = ch;
        } else {
            if (pos + 1 >= buf_sz) {
                buf_sz *= 2;
                char *nb = (char *)AGENTOS_REALLOC(buf, buf_sz);
                if (!nb) {
                    AGENTOS_FREE(buf);
                    return 0;
                }
                buf = nb;
            }
            buf[pos++] = *q;
            q++;
        }
    }
    buf[pos] = '\0';
    *out_str = buf;
    return 1;
}

/* ==================== s2_generate 回调实现 ==================== */

agentos_error_t tc3_llm_s2_generate(const char *input, size_t input_len, char **output,
                                    size_t *output_len, void *user_data)
{
    if (!input || input_len == 0 || !output || !output_len) {
        AGENTOS_LOG_ERROR("TC3 s2_generate: invalid params (input=%p input_len=%zu "
                         "output=%p output_len=%p)",
                         (const void *)input, input_len, (void *)output, (void *)output_len);
        return AGENTOS_EINVAL;
    }

    *output = NULL;
    *output_len = 0;

    tc3_llm_ctx_t *ctx = (tc3_llm_ctx_t *)user_data;
    if (!ctx) {
        AGENTOS_LOG_ERROR("TC3 s2_generate: NULL user_data");
        return AGENTOS_EINVAL;
    }

    /* 首轮: 复用 seed text（避免重复 LLM 调用） */
    if (ctx->seed_text && ctx->seed_len > 0 && !ctx->seed_consumed) {
        char *seed_copy = (char *)AGENTOS_MALLOC(ctx->seed_len + 1);
        if (!seed_copy) {
            AGENTOS_LOG_ERROR("TC3 s2_generate: OOM copying seed (%zu bytes)", ctx->seed_len + 1);
            return AGENTOS_ENOMEM;
        }
        __builtin_memcpy(seed_copy, ctx->seed_text, ctx->seed_len);
        seed_copy[ctx->seed_len] = '\0';

        *output = seed_copy;
        *output_len = ctx->seed_len;
        ctx->seed_consumed = 1;

        AGENTOS_LOG_INFO("TC3 s2_generate: returned seed text (%zu bytes, first call)",
                        ctx->seed_len);
        return AGENTOS_SUCCESS;
    }

    /* 后续轮（修正）: 调用 LLM 独立生成 */
    size_t prompt_len = input_len > TC3_LLM_MAX_PROMPT_LEN ? TC3_LLM_MAX_PROMPT_LEN : input_len;

    agentos_error_t err = tc3_llm_call(ctx, S2_SYSTEM_PROMPT, input, ctx->s2_model,
                                        output, &prompt_len);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("TC3 s2_generate: LLM call failed (err=%d round=%d), "
                        "tc3 correction loop will abort this unit",
                        (int)err, ctx->seed_consumed ? 2 : 1);
        return err;
    }

    *output_len = prompt_len;
    AGENTOS_LOG_INFO("TC3 s2_generate: LLM generated %zu bytes (correction round)",
                    *output_len);
    return AGENTOS_SUCCESS;
}

/* ==================== s1_verify 回调实现 ==================== */

agentos_error_t tc3_llm_s1_verify(const char *content, size_t content_len, float *out_score,
                                  int *out_acceptable, char **out_critique,
                                  size_t *out_critique_len, void *user_data)
{
    if (!content || content_len == 0 || !out_score) {
        AGENTOS_LOG_ERROR("TC3 s1_verify: invalid params (content=%p content_len=%zu "
                         "out_score=%p)",
                         (const void *)content, content_len, (void *)out_score);
        return AGENTOS_EINVAL;
    }

    /* 初始化出参为安全默认值 */
    *out_score = 0.5f;
    if (out_acceptable) *out_acceptable = 0;
    if (out_critique) *out_critique = NULL;
    if (out_critique_len) *out_critique_len = 0;

    tc3_llm_ctx_t *ctx = (tc3_llm_ctx_t *)user_data;
    if (!ctx) {
        AGENTOS_LOG_ERROR("TC3 s1_verify: NULL user_data");
        return AGENTOS_EINVAL;
    }

    /* 构建 user prompt: 原始任务 + 待验证内容 */
    size_t task_len = 0;
    const char *task = ctx->original_input ? ctx->original_input : "(not provided)";
    task_len = ctx->original_input_len;
    if (task_len == 0) task_len = strlen(task);

    /* 截断防止 prompt 超长 */
    size_t task_show = task_len > 2000 ? 2000 : task_len;
    size_t content_show = content_len > 4000 ? 4000 : content_len;

    /* prompt: "Original task: ...\n\nResponse to evaluate: ...\n\nReply with JSON..." */
    size_t prompt_sz = task_show + content_show + 512;
    char *prompt = (char *)AGENTOS_MALLOC(prompt_sz);
    if (!prompt) {
        AGENTOS_LOG_ERROR("TC3 s1_verify: OOM building prompt (%zu bytes)", prompt_sz);
        return AGENTOS_ENOMEM;
    }

    int written = snprintf(prompt, prompt_sz,
                           "Original task:\n%.*s\n\n"
                           "Response to evaluate:\n%.*s\n\n"
                           "Evaluate the response for logical correctness and task alignment. "
                           "Reply with ONLY a JSON object.",
                           (int)task_show, task,
                           (int)content_show, content);
    if (written <= 0 || (size_t)written >= prompt_sz) {
        AGENTOS_LOG_ERROR("TC3 s1_verify: prompt encoding error (written=%d buf=%zu)",
                         written, prompt_sz);
        AGENTOS_FREE(prompt);
        return AGENTOS_EUNKNOWN;
    }

    /* 调用 LLM */
    char *llm_resp = NULL;
    size_t llm_resp_len = 0;
    agentos_error_t err = tc3_llm_call(ctx, S1_VERIFY_SYSTEM_PROMPT, prompt, ctx->s1_verify_model,
                                       &llm_resp, &llm_resp_len);
    AGENTOS_FREE(prompt);

    if (err != AGENTOS_SUCCESS || !llm_resp || llm_resp_len == 0) {
        /* P2.8: LLM 不可用时不再字数加分放行（原逻辑会让长文本自动 acceptable）。
         *
         * 对抗式验证的核心原则：无法验证 = 无法通过。当 LLM 不可用时，
         * 返回保守低分（0.3）+ acceptable=0，强制 tc3 走修正路径或升级
         * 专家仲裁，而非自动放行。这避免了"长文即合格"的假验证。
         *
         * 返回 AGENTOS_SUCCESS 表示"验证已完成"（只是结果是不通过），
         * 调用方 verify_with_metacognition 会据此降级到 metacognition
         * 或 default_s1_verify 做进一步评估。 */
        AGENTOS_LOG_WARN("TC3 s1_verify: LLM unavailable (err=%d), applying conservative "
                         "fail-closed verdict (score=0.3, not acceptable)",
                        (int)err);

        *out_score = 0.3f;
        if (out_acceptable)
            *out_acceptable = 0;

        if (out_critique && out_critique_len) {
            const char *fb = "Adversarial verification unavailable (LLM offline). "
                             "Cannot confirm absence of bugs — fail-closed: content unverified.";
            size_t fb_len = strlen(fb);
            char *fb_copy = (char *)AGENTOS_MALLOC(fb_len + 1);
            if (fb_copy) {
                __builtin_memcpy(fb_copy, fb, fb_len);
                fb_copy[fb_len] = '\0';
                *out_critique = fb_copy;
                *out_critique_len = fb_len;
            }
        }
        return AGENTOS_SUCCESS;
    }

    /* 解析 LLM 响应 */
    float score = 0.5f;
    int acceptable = 0;
    char *critique = NULL;
    int parse_ok = 0;

    /* 提取 JSON 子串 */
    const char *json_str = NULL;
    size_t json_len = 0;
    if (extract_json_object(llm_resp, llm_resp_len, &json_str, &json_len)) {
#ifdef AGENTOS_HAS_CJSON
        /* 用 cJSON 解析（更健壮） */
        char *json_copy = (char *)AGENTOS_MALLOC(json_len + 1);
        if (json_copy) {
            __builtin_memcpy(json_copy, json_str, json_len);
            json_copy[json_len] = '\0';

            cJSON *root = cJSON_Parse(json_copy);
            if (root) {
                cJSON *j_score = cJSON_GetObjectItem(root, "score");
                cJSON *j_accept = cJSON_GetObjectItem(root, "acceptable");
                cJSON *j_critique = cJSON_GetObjectItem(root, "critique");

                if (j_score && cJSON_IsNumber(j_score)) {
                    score = (float)j_score->valuedouble;
                    parse_ok = 1;
                }
                if (j_accept && cJSON_IsBool(j_accept)) {
                    acceptable = cJSON_IsTrue(j_accept) ? 1 : 0;
                }
                if (j_critique && cJSON_IsString(j_critique) && j_critique->valuestring[0]) {
                    size_t cl = strlen(j_critique->valuestring);
                    critique = (char *)AGENTOS_MALLOC(cl + 1);
                    if (critique) {
                        __builtin_memcpy(critique, j_critique->valuestring, cl);
                        critique[cl] = '\0';
                    }
                }
                cJSON_Delete(root);
            }
            AGENTOS_FREE(json_copy);
        }
#endif
        /* cJSON 不可用或解析失败 — 手动提取 */
        if (!parse_ok) {
            float f_val;
            int b_val;
            if (extract_float_field(json_str, json_len, "score", &f_val)) {
                score = f_val;
                parse_ok = 1;
            }
            if (extract_bool_field(json_str, json_len, "acceptable", &b_val)) {
                acceptable = b_val;
            }
            if (!critique) {
                extract_string_field(json_str, json_len, "critique", &critique);
            }
        }
    }

    /* 钳制 score 到 [0, 1] */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    /* 如果 acceptable 未从响应中提取到，按 score 推导 */
    if (!parse_ok) {
        /* JSON 解析完全失败 — 从原始文本尝试提取数字，否则用启发式 */
        AGENTOS_LOG_WARN("TC3 s1_verify: JSON parse failed, raw response: %.200s", llm_resp);
        score = 0.5f; /* 保守回退 */
        acceptable = 0;
    } else {
        /* acceptable 若未提取到，按 score >= 0.7 推导 */
        if (acceptable == 0 && score >= 0.7f) {
            acceptable = 1;
        }
    }

    *out_score = score;
    if (out_acceptable) *out_acceptable = acceptable;

    if (out_critique && out_critique_len) {
        if (critique) {
            *out_critique = critique;
            *out_critique_len = strlen(critique);
        } else {
            /* 没有批判意见 — 生成简短摘要 */
            char fb[256];
            int fl = snprintf(fb, sizeof(fb),
                             "LLM verify: score=%.2f acceptable=%s (no detailed critique provided)",
                             score, acceptable ? "yes" : "no");
            if (fl > 0) {
                char *fb_copy = (char *)AGENTOS_MALLOC(fl + 1);
                if (fb_copy) {
                    __builtin_memcpy(fb_copy, fb, fl);
                    fb_copy[fl] = '\0';
                    *out_critique = fb_copy;
                    *out_critique_len = (size_t)fl;
                }
            }
        }
    }

    AGENTOS_LOG_INFO("TC3 s1_verify: score=%.2f acceptable=%d critique_len=%zu (parse_ok=%d)",
                    score, acceptable,
                    (out_critique && *out_critique) ? *out_critique_len : 0,
                    parse_ok);

    AGENTOS_FREE(llm_resp);
    return AGENTOS_SUCCESS;
}

/* ==================== s1_expert 回调实现 ==================== */

/**
 * @brief 将判决字符串映射为 tc3_verdict_t
 */
static tc3_verdict_t parse_verdict_string(const char *s)
{
    if (!s || !s[0]) return TC3_RESULT_MAJOR_FIX; /* 保守默认 */

    /* 不区分大小写比较前缀 */
    if (strncasecmp(s, "accept", 6) == 0) return TC3_RESULT_ACCEPT;
    if (strncasecmp(s, "minor", 5) == 0) return TC3_RESULT_MINOR_FIX;
    if (strncasecmp(s, "major", 5) == 0) return TC3_RESULT_MAJOR_FIX;
    if (strncasecmp(s, "reject", 6) == 0) return TC3_RESULT_REJECT;

    return TC3_RESULT_MAJOR_FIX; /* 未知值保守处理 */
}

agentos_error_t tc3_llm_s1_expert(const char *content, size_t content_len, const char *critique,
                                  size_t critique_len, float *out_score,
                                  tc3_verdict_t *out_verdict, char **out_expert_opinion,
                                  size_t *out_opinion_len, void *user_data)
{
    if (!content || content_len == 0 || !out_score || !out_verdict) {
        AGENTOS_LOG_ERROR("TC3 s1_expert: invalid params (content=%p content_len=%zu "
                         "out_score=%p out_verdict=%p)",
                         (const void *)content, content_len, (void *)out_score,
                         (void *)out_verdict);
        return AGENTOS_EINVAL;
    }

    /* 初始化出参为保守默认（MAJOR_FIX，保持问题状态） */
    *out_score = 0.4f;
    *out_verdict = TC3_RESULT_MAJOR_FIX;
    if (out_expert_opinion) *out_expert_opinion = NULL;
    if (out_opinion_len) *out_opinion_len = 0;

    tc3_llm_ctx_t *ctx = (tc3_llm_ctx_t *)user_data;
    if (!ctx) {
        AGENTOS_LOG_ERROR("TC3 s1_expert: NULL user_data");
        return AGENTOS_EINVAL;
    }

    /* 构建 user prompt: 内容 + 批判意见 */
    size_t content_show = content_len > 4000 ? 4000 : content_len;
    size_t critique_show = critique_len > 1000 ? 1000 : critique_len;
    const char *critique_safe = critique ? critique : "(no critique provided)";

    size_t prompt_sz = content_show + critique_show + 512;
    char *prompt = (char *)AGENTOS_MALLOC(prompt_sz);
    if (!prompt) {
        AGENTOS_LOG_ERROR("TC3 s1_expert: OOM building prompt (%zu bytes)", prompt_sz);
        return AGENTOS_ENOMEM;
    }

    int written = snprintf(prompt, prompt_sz,
                           "Content under review:\n%.*s\n\n"
                           "Fast verifier critique:\n%.*s\n\n"
                           "As a domain expert, review the content and critique. "
                           "Give your final verdict. Reply with ONLY a JSON object.",
                           (int)content_show, content,
                           (int)critique_show, critique_safe);
    if (written <= 0 || (size_t)written >= prompt_sz) {
        AGENTOS_LOG_ERROR("TC3 s1_expert: prompt encoding error (written=%d buf=%zu)",
                         written, prompt_sz);
        AGENTOS_FREE(prompt);
        return AGENTOS_EUNKNOWN;
    }

    /* 调用 LLM */
    char *llm_resp = NULL;
    size_t llm_resp_len = 0;
    agentos_error_t err = tc3_llm_call(ctx, S1_EXPERT_SYSTEM_PROMPT, prompt, ctx->s1_expert_model,
                                       &llm_resp, &llm_resp_len);
    AGENTOS_FREE(prompt);

    if (err != AGENTOS_SUCCESS || !llm_resp || llm_resp_len == 0) {
        /* LLM 不可用 — 回退到保守判决（MAJOR_FIX） */
        AGENTOS_LOG_WARN("TC3 s1_expert: LLM unavailable (err=%d), "
                        "returning conservative MAJOR_FIX verdict",
                        (int)err);

        if (out_expert_opinion && out_opinion_len) {
            const char *fb = "Expert arbitrator unavailable; conservative MAJOR_FIX verdict "
                             "applied. Content requires manual review or LLM-driven correction.";
            size_t fb_len = strlen(fb);
            char *fb_copy = (char *)AGENTOS_MALLOC(fb_len + 1);
            if (fb_copy) {
                __builtin_memcpy(fb_copy, fb, fb_len);
                fb_copy[fb_len] = '\0';
                *out_expert_opinion = fb_copy;
                *out_opinion_len = fb_len;
            }
        }
        return AGENTOS_SUCCESS; /* 回退成功 */
    }

    /* 解析 LLM 响应 */
    float score = 0.4f;
    tc3_verdict_t verdict = TC3_RESULT_MAJOR_FIX;
    char *opinion = NULL;
    int parse_ok = 0;

    const char *json_str = NULL;
    size_t json_len = 0;
    if (extract_json_object(llm_resp, llm_resp_len, &json_str, &json_len)) {
#ifdef AGENTOS_HAS_CJSON
        char *json_copy = (char *)AGENTOS_MALLOC(json_len + 1);
        if (json_copy) {
            __builtin_memcpy(json_copy, json_str, json_len);
            json_copy[json_len] = '\0';

            cJSON *root = cJSON_Parse(json_copy);
            if (root) {
                cJSON *j_verdict = cJSON_GetObjectItem(root, "verdict");
                cJSON *j_score = cJSON_GetObjectItem(root, "score");
                cJSON *j_opinion = cJSON_GetObjectItem(root, "opinion");

                if (j_verdict && cJSON_IsString(j_verdict)) {
                    verdict = parse_verdict_string(j_verdict->valuestring);
                    parse_ok = 1;
                }
                if (j_score && cJSON_IsNumber(j_score)) {
                    score = (float)j_score->valuedouble;
                    parse_ok = 1;
                }
                if (j_opinion && cJSON_IsString(j_opinion) && j_opinion->valuestring[0]) {
                    size_t ol = strlen(j_opinion->valuestring);
                    opinion = (char *)AGENTOS_MALLOC(ol + 1);
                    if (opinion) {
                        __builtin_memcpy(opinion, j_opinion->valuestring, ol);
                        opinion[ol] = '\0';
                    }
                }
                cJSON_Delete(root);
            }
            AGENTOS_FREE(json_copy);
        }
#endif
        if (!parse_ok) {
            /* 手动提取 */
            float f_val;
            char *s_verdict = NULL;
            if (extract_float_field(json_str, json_len, "score", &f_val)) {
                score = f_val;
                parse_ok = 1;
            }
            if (extract_string_field(json_str, json_len, "verdict", &s_verdict)) {
                verdict = parse_verdict_string(s_verdict);
                parse_ok = 1;
                AGENTOS_FREE(s_verdict);
            }
            if (!opinion) {
                extract_string_field(json_str, json_len, "opinion", &opinion);
            }
        }
    }

    /* 钳制 score */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    if (!parse_ok) {
        AGENTOS_LOG_WARN("TC3 s1_expert: JSON parse failed, raw response: %.200s", llm_resp);
        /* 保持保守默认 */
    }

    *out_score = score;
    *out_verdict = verdict;

    if (out_expert_opinion && out_opinion_len) {
        if (opinion) {
            *out_expert_opinion = opinion;
            *out_opinion_len = strlen(opinion);
        } else {
            char fb[256];
            int fl = snprintf(fb, sizeof(fb),
                             "Expert verdict: %s (score=%.2f). No detailed opinion provided.",
                             verdict == TC3_RESULT_ACCEPT ? "ACCEPT" :
                             verdict == TC3_RESULT_MINOR_FIX ? "MINOR_FIX" :
                             verdict == TC3_RESULT_MAJOR_FIX ? "MAJOR_FIX" : "REJECT",
                             score);
            if (fl > 0) {
                char *fb_copy = (char *)AGENTOS_MALLOC(fl + 1);
                if (fb_copy) {
                    __builtin_memcpy(fb_copy, fb, fl);
                    fb_copy[fl] = '\0';
                    *out_expert_opinion = fb_copy;
                    *out_opinion_len = (size_t)fl;
                }
            }
        }
    }

    AGENTOS_LOG_INFO("TC3 s1_expert: verdict=%d score=%.2f opinion_len=%zu (parse_ok=%d)",
                    (int)verdict, score,
                    (out_expert_opinion && *out_expert_opinion) ? *out_opinion_len : 0,
                    parse_ok);

    AGENTOS_FREE(llm_resp);
    return AGENTOS_SUCCESS;
}
