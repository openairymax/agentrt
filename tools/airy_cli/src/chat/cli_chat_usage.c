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
 * 此处按轮累加；回合结束由 cli_chat_usage_metrics 统一组装展示口径
 * （chat / task / 蓝图快速路径共用），并在下一轮开始前清零
 * （cli_chat_usage_reset）。reasoning_content 按回合累积
 * （封顶 CLI_CHAT_REASONING_MAX_BYTES，防异常长推理拖爆内存）后写日志
 * （折叠展示在对话内，完整文本保留在日志，思考 token 不丢失）。
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
static uint64_t g_chat_prompt_total = 0;
static uint64_t g_chat_comp_total = 0;
static double g_chat_cost_total = 0.0;
static char *g_chat_reasoning_acc = NULL;
static int g_chat_reasoning_truncated = 0; /* S-04：截断标记只落一次 */

/* 按轮累加本轮对话真实 token/费用（工具轮与最终轮都计入；含思考 token）。
 * prompt/completion 与 total 分别累计：厂商 usage 三者齐全时按细目展示，
 * 缺失细目时只剩 total 可用。 */
void cli_chat_usage_add(const llm_response_t *resp)
{
    if (!resp)
        return;
    g_chat_tokens_total += resp->total_tokens;
    g_chat_prompt_total += resp->prompt_tokens;
    g_chat_comp_total += resp->completion_tokens;
    g_chat_cost_total += resp->cost_usd;
}

/* 思考链增量累积：跨工具轮与最终轮保留（对话内折叠展示，文本由
 * cli_chat_reasoning_persist 落日志，思考 token 不丢失）。S-04：单回合
 * 封顶 CLI_CHAT_REASONING_MAX_BYTES——异常长推理不再把内存拖到无界；
 * 达上限后整条丢弃增量并一次性落截断标记，日志回溯者可知尾部缺失。 */
void cli_chat_reasoning_add(const char *reasoning)
{
    if (!reasoning || !reasoning[0])
        return;
    size_t old = g_chat_reasoning_acc ? strlen(g_chat_reasoning_acc) : 0;
    if (old >= CLI_CHAT_REASONING_MAX_BYTES) {
        if (!g_chat_reasoning_truncated) {
            static const char mark[] = "\n[思考链已截断：超出本回合上限]";
            size_t mlen = sizeof(mark) - 1;
            char *np = (char *)AIRY_REALLOC(g_chat_reasoning_acc, old + mlen + 1);
            if (np) {
                g_chat_reasoning_acc = np;
                __builtin_memcpy(g_chat_reasoning_acc + old, mark, mlen + 1);
            }
            g_chat_reasoning_truncated = 1;
        }
        return;
    }
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

/* 新一轮对话开始清零：本轮统计归零 + 释放思考链累积（安全网，覆盖
 * 上轮异常中断未走到收尾清理的路径）。 */
void cli_chat_usage_reset(void)
{
    g_chat_tokens_total = 0;
    g_chat_prompt_total = 0;
    g_chat_comp_total = 0;
    g_chat_cost_total = 0.0;
    g_chat_reasoning_truncated = 0;
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

/* 1.7：全链路真实消耗细目（会话差值）。B-2 口径 SSoT：in/out 直接对应厂商
 * usage 的 prompt_tokens / completion_tokens，cost 取 llm_d cost_tracker
 * 差值（按厂商 usage × pricing_rule 计算并落盘），因此展示值可与厂商账单
 * 逐字段对照，而非本地估算。llm_d 离线时回退本轮 chat 累计（此路径
 * total_tokens 与 in+out 未必相等，故 total 单独回传）。 */
static void cli_usage_detail(uint64_t *in, uint64_t *out, uint64_t *total,
                             double *cost)
{
    uint64_t prompt = 0, comp = 0;
    double c = 0.0;
    if (cli_llm_usage_snap(&prompt, &comp, &c) != 0) {
        if (in)
            *in = g_chat_prompt_total;
        if (out)
            *out = g_chat_comp_total;
        if (total)
            *total = g_chat_tokens_total;
        if (cost)
            *cost = g_chat_cost_total;
        return;
    }

    if (!g_llm_base_set) {
        g_llm_base_prompt = prompt;
        g_llm_base_completion = comp;
        g_llm_base_cost = c;
        g_llm_base_set = 1;
        prompt = comp = 0;
        c = 0.0;
    } else {
        prompt -= g_llm_base_prompt;
        comp -= g_llm_base_completion;
        c -= g_llm_base_cost;
    }

    if (in)
        *in = prompt;
    if (out)
        *out = comp;
    if (total)
        *total = prompt + comp;
    if (cost)
        *cost = c;
}

/* 计费展示口径 SSoT：所有回合出口（chat / task / 蓝图快速路径）共用本函数
 * 组装同一串指标，避免各调用点各写一套口径。返回 1 = buf 已写入非空指标，
 * 返回 0 = 本回合无消耗（buf 置空串，调用方不展示计费段）。 */
int cli_chat_usage_metrics(char *buf, size_t n)
{
    if (!buf || n == 0)
        return 0;
    buf[0] = '\0';

    uint64_t in = 0, out = 0, total = 0;
    double cost = 0.0;
    cli_usage_detail(&in, &out, &total, &cost);
    if (total == 0 && cost <= 0.0)
        return 0;

    if (in > 0 || out > 0)
        snprintf(buf, n,
                 "Tokens: %llu (prompt %llu · completion %llu) · Cost: $%.6f",
                 (unsigned long long)total, (unsigned long long)in,
                 (unsigned long long)out, cost);
    else
        snprintf(buf, n, "Tokens: %llu · Cost: $%.6f",
                 (unsigned long long)total, cost);
    return 1;
}
