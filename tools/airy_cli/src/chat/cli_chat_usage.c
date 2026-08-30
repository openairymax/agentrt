// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_usage.c
 * @brief airy_cli chat usage/cost accounting sub-module.
 *
 * 2.1.1.5/2.1.1.6：本轮对话真实 token/费用统计与思考链保留。
 *
 * llm_d 在响应的 usage/top-level 回填 total_tokens 与 cost_usd（含思考
 * token，DeepSeek/OpenAI 的 completion_tokens 已包含 reasoning_tokens），
 * 此处按轮累加；回合结束由 main.c 经 cli_chat_usage_get 读取展示，并在
 * 下一轮开始前清零（cli_chat_usage_reset）。reasoning_content 全量累积后
 * 写日志（折叠展示在对话内，完整文本保留在日志，思考 token 不丢失）。
 */

#include "cli_internal.h"

#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

static uint64_t g_chat_tokens_total = 0;
static double g_chat_cost_total = 0.0;
static char *g_chat_reasoning_acc = NULL;

/* 按轮累加本轮对话真实 token/费用（工具轮与最终轮都计入；含思考 token）。 */
void cli_chat_usage_add(const llm_response_t *resp)
{
    if (!resp)
        return;
    g_chat_tokens_total += resp->total_tokens;
    g_chat_cost_total += resp->cost_usd;
}

/* 思考链增量累积：跨工具轮与最终轮全量保留（对话内折叠展示，完整文本
 * 由 cli_chat_reasoning_persist 落日志，思考 token 不丢失）。 */
void cli_chat_reasoning_add(const char *reasoning)
{
    if (!reasoning || !reasoning[0])
        return;
    size_t old = g_chat_reasoning_acc ? strlen(g_chat_reasoning_acc) : 0;
    size_t add = strlen(reasoning);
    char *np = (char *)AIRY_REALLOC(g_chat_reasoning_acc, old + add + 2);
    if (!np)
        return;
    g_chat_reasoning_acc = np;
    if (old > 0)
        g_chat_reasoning_acc[old++] = '\n';
    __builtin_memcpy(g_chat_reasoning_acc + old, reasoning, add);
    g_chat_reasoning_acc[old + add] = '\0';
}

/* 读取本轮已累计的思考链全文（可能为 NULL；调用方不得持有跨轮引用，
 * 下一轮 cli_chat_usage_reset 会释放该指针）。 */
const char *cli_chat_reasoning_peek(void)
{
    return g_chat_reasoning_acc;
}

/* 供 main.c 在回合结束时读取本轮消耗统计（随后由 cli_chat_usage_reset
 * 在下一轮开始时清零）。 */
void cli_chat_usage_get(uint64_t *tokens, double *cost)
{
    if (tokens)
        *tokens = g_chat_tokens_total;
    if (cost)
        *cost = g_chat_cost_total;
}

/* 新一轮对话开始清零：本轮统计归零 + 释放思考链累积（安全网，覆盖
 * 上轮异常中断未走到收尾清理的路径）。 */
void cli_chat_usage_reset(void)
{
    g_chat_tokens_total = 0;
    g_chat_cost_total = 0.0;
    if (g_chat_reasoning_acc) {
        AIRY_FREE(g_chat_reasoning_acc);
        g_chat_reasoning_acc = NULL;
    }
}

/* 1.7 真实消耗会话差值：llm_d cost_tracker 是所有 LLM 请求（chat + task
 * 双思考路径）的持久化真相源。会话首读记录起点快照，此后每次返回与起点
 * 的差值 = 本会话真实消耗（含思考 token，completion_tokens 已含 reasoning）。
 * llm_d 不可用时返回 0（调用方回退 chat 累计 g_chat_tokens_total）。 */
static uint64_t g_llm_base_prompt = 0;
static uint64_t g_llm_base_completion = 0;
static double g_llm_base_cost = 0.0;
static int g_llm_base_set = 0;

static int cli_llm_usage_snap(uint64_t *out_prompt, uint64_t *out_completion,
                                    double *out_cost)
{
    /* 架构约束（2026-08-25）：统一经 gateway 派发（llm.get_stats →
     * gateway → SYS_SVC_CALL → llm_d），禁止直连 llm.sock。 */
    char *result = NULL;
    int rc = cli_gw_call("llm.get_stats", "{}", 6000, &result);
    if (rc != 0 || !result)
        return -1;

    uint64_t prompt = 0, comp = 0;
    double cost = 0.0;

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    AIRY_FREE(result);
    if (!root)
        return -1;
    cJSON *costj = cJSON_GetObjectItemCaseSensitive(root, "cost");
    cJSON *arr = costj ? cJSON_GetObjectItemCaseSensitive(costj, "models") : NULL;
    if (cJSON_IsArray(arr)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, arr) {
            cJSON *pt = cJSON_GetObjectItemCaseSensitive(item, "prompt_tokens");
            cJSON *ct = cJSON_GetObjectItemCaseSensitive(item, "completion_tokens");
            cJSON *cu = cJSON_GetObjectItemCaseSensitive(item, "cost_usd");
            if (cJSON_IsNumber(pt))
                prompt += (uint64_t)pt->valuedouble;
            if (cJSON_IsNumber(ct))
                comp += (uint64_t)ct->valuedouble;
            if (cJSON_IsNumber(cu))
                cost += cu->valuedouble;
        }
    }
    cJSON_Delete(root);
#else
    AIRY_FREE(result);
    return -1;
#endif

    if (out_prompt)
        *out_prompt = prompt;
    if (out_completion)
        *out_completion = comp;
    if (out_cost)
        *out_cost = cost;
    return 0;
}

/* 1.7：全链路真实消耗（会话差值）；llm_d 离线回退 chat 累计。 */
void cli_chat_usage_get_session(uint64_t *tokens, double *cost)
{
    uint64_t prompt = 0, comp = 0;
    double c = 0.0;
    if (cli_llm_usage_snap(&prompt, &comp, &c) != 0) {
        cli_chat_usage_get(tokens, cost);
        return;
    }

    if (!g_llm_base_set) {
        g_llm_base_prompt = prompt;
        g_llm_base_completion = comp;
        g_llm_base_cost = c;
        g_llm_base_set = 1;
        if (tokens)
            *tokens = 0;
        if (cost)
            *cost = 0.0;
        return;
    }

    if (tokens)
        *tokens = (prompt - g_llm_base_prompt) + (comp - g_llm_base_completion);
    if (cost)
        *cost = c - g_llm_base_cost;
}
