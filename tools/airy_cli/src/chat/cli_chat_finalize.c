// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_finalize.c
 * @brief 聊天域回复最终化（域拆分自 cli_chat.c，2026-08-27）。
 *
 * cli_chat_reply 的收尾阶段：语言网关输出后处理、最终回复渲染
 * （--json / -p / 交互流式折叠 / 非流式折叠）、历史写入与思考链持久化。
 * 共享声明见 cli_chat_internal.h。
 *
 * 2026-10-21 阶段化：finalize 只保留「语言后处理 → 按形态渲染 →
 * 历史/诊断收尾」骨架，四种输出形态（json / -p / 交互流式 / 非流式）
 * 各为一个 static 阶段函数，正文/思考链取用合并为单点判定。
 */

#include "cli_chat_internal.h"
#include "cli_gw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* 一轮回复的渲染上下文：四种输出形态共用的只读视图。 */
typedef struct {
    llm_response_t *resp;
    const char *content;
    const char *render;
    const char *reason;
    const char *think_hdr;
    int stream;
    int truncated;
} chat_fin_t;

/* 首选项正文；缺 choices 或空正文时返回空串（渲染层统一判空）。 */
static const char *chat_text_of(const llm_response_t *r)
{
    if (r->choices && r->choice_count > 0 && r->choices[0].content)
        return r->choices[0].content;
    return "";
}

/* 本轮思考链：响应自带优先，其次取流式累积；两者皆无返回空串。 */
static const char *chat_think_of(const llm_response_t *r)
{
    if (r->choices && r->choice_count > 0 && r->choices[0].reasoning_content)
        return r->choices[0].reasoning_content;
    return cli_chat_reasoning_peek() ? cli_chat_reasoning_peek() : "";
}

/* 响应是否携带非空思考链（交互形态据此补隐藏提示）。 */
static int chat_has_think(const llm_response_t *r)
{
    return r->choices && r->choice_count > 0 && r->choices[0].reasoning_content &&
           r->choices[0].reasoning_content[0];
}

/* 语言后处理请求体：{text, expected_lang?}。lang<=0 时不带期望语言。 */
#ifdef AIRY_HAS_CJSON
static char *chat_lp_build(const char *content, int lang)
{
    cJSON *lp = cJSON_CreateObject();
    cJSON *lt = cJSON_CreateString(content);
    char *js;

    if (lp && lt)
        cJSON_AddItemToObject(lp, "text", lt);
    else
        cJSON_Delete(lt);
    if (lang > 0) {
        cJSON *el = cJSON_CreateNumber(lang);

        if (lp && el)
            cJSON_AddItemToObject(lp, "expected_lang", el);
        else
            cJSON_Delete(el);
    }
    js = lp ? cJSON_PrintUnformatted(lp) : NULL;
    cJSON_Delete(lp);
    return js;
}
#endif /* AIRY_HAS_CJSON */

/* 1.3 推理语言网关服务面化（M1-1c）：输出后处理（语言漂移检测 +
 * 术语一致性 + 润色）经 gateway → think.lang_postprocess（think_d 承载）。
 * 期望输出语言取路由决策 output_lang（wire 值）；仅作用于渲染与历史
 * 写入，不修改 llm_d 原始响应（推理链条证据保留）。失败时保留原始正文，
 * 不阻断渲染。返回渲染用正文，*owned 为需要释放的替换文本。 */
static const char *chat_lang_out(const char *content, int lang, char **owned)
{
#ifdef AIRY_HAS_CJSON
    char *lp_json;
    char *lp_res = NULL;
    cJSON *lr;
    cJSON *tx;

    if (!content[0])
        return content;
    lp_json = chat_lp_build(content, lang);
    if (!lp_json || cli_gw_call("think.lang_postprocess", lp_json, 6000,
                                &lp_res) != 0 || !lp_res) {
        AIRY_FREE(lp_res);
        AIRY_FREE(lp_json);
        return content;
    }
    lr = cJSON_Parse(lp_res);
    if (lr) {
        tx = cJSON_GetObjectItem(lr, "text");
        if (cJSON_IsString(tx) && tx->valuestring && tx->valuestring[0]) {
            AIRY_FREE(*owned);
            *owned = AIRY_STRDUP(tx->valuestring);
            if (*owned)
                content = *owned;
        }
        cJSON_Delete(lr);
    }
    AIRY_FREE(lp_res);
    AIRY_FREE(lp_json);
#else
    (void)lang;
    (void)owned;
#endif /* AIRY_HAS_CJSON */
    return content;
}

/* --json 结构化回执（Codex exec 约定）：正文/错误、思考链、用量与
 * finish_reason。思考链属显式请求的结构化面（落 stdout 供脚本消费），
 * 与「默认不上屏」不冲突，故保留。 */
static void chat_json_out(const chat_fin_t *f)
{
#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_CreateObject();
    const char *reasoning;
    char *js;

    cJSON_AddStringToObject(root, "role", "super_agent");
    cJSON_AddStringToObject(root, "type", "chat");
    if (f->content[0])
        cJSON_AddStringToObject(root, "content", f->render);
    else
        cJSON_AddStringToObject(root, "error", "reply failed");
    reasoning = chat_think_of(f->resp);
    if (reasoning && reasoning[0])
        cJSON_AddStringToObject(root, "reasoning", reasoning);
    {
        cJSON *usage = cJSON_CreateObject();

        cJSON_AddNumberToObject(usage, "prompt_tokens", f->resp->prompt_tokens);
        cJSON_AddNumberToObject(usage, "completion_tokens", f->resp->completion_tokens);
        cJSON_AddNumberToObject(usage, "total_tokens", f->resp->total_tokens);
        cJSON_AddNumberToObject(usage, "reasoning_tokens", f->resp->reasoning_tokens);
        cJSON_AddNumberToObject(usage, "cost_usd", f->resp->cost_usd);
        cJSON_AddItemToObject(root, "usage", usage);
    }
    cJSON_AddStringToObject(root, "finish_reason", f->reason);
    js = cJSON_PrintUnformatted(root);
    if (js) {
        cli_outf("%s\n", js);
        cJSON_free(js);
    }
    cJSON_Delete(root);
#else
    cli_outf("{\"role\":\"super_agent\",\"type\":\"chat\"");
    if (f->content[0])
        cli_outf(",\"content\":\"%s\"", f->render);
    else
        cli_outf(",\"error\":\"reply failed\"");
    cli_outf(",\"finish_reason\":\"%s\"}\n", f->reason);
#endif /* AIRY_HAS_CJSON */
}

/* -p 纯文本（Claude Code -p / Codex exec 约定）。流式正文已随块直出，
 * 不再重复打印；空返回与截断只看 stderr，stdout 保持可解析。 */
static void chat_print_out(const chat_fin_t *f)
{
    if (!f->stream)
        cli_outf("%s\n", f->render);
    else if (f->content[0] == '\0')
        fprintf(stderr,
                "[chat] warning: empty reply (model returned no text; "
                "reasoning-only or provider error)\n");
    if (f->truncated)
        fprintf(stderr, "[chat] warning: reply truncated at the output token cap; "
                        "the text above is incomplete\n");
}

/* 交互 TTY 流式（0.1.7 终态直出）：正文已随流逐块上屏，收尾只补空回复
 * 兜底、思考链隐藏提示（0.1.18 B4）与截断提示。 */
static void chat_stream_out(const chat_fin_t *f)
{
    if (f->content[0] == '\0') {
        cli_print_line("Super Agent", CLI_REPLY_EMPTY_HINT);
    } else if (chat_has_think(f->resp)) {
        /* 正文末尾补空行分层，思考链仅提示不展示原文 */
        cli_outc('\n');
        cli_print_line(f->think_hdr, CLI_REPLY_THINK_HIDDEN_HINT);
    }
    if (f->truncated)
        cli_print_line("Super Agent", CLI_REPLY_TRUNCATED_HINT);
}

/* TUI / 非流式交互：思考链仅提示；结果完整渲染进历史（用户要求结果
 * 不折叠，长结果经视口滚动浏览）。 */
static void chat_plain_out(const chat_fin_t *f)
{
    if (chat_has_think(f->resp))
        cli_print_line(f->think_hdr, CLI_REPLY_THINK_HIDDEN_HINT);
    if (f->content[0] != '\0')
        cli_print_line("Super Agent", f->render);
    else
        cli_print_line("Super Agent", CLI_REPLY_EMPTY_HINT);
    if (f->truncated)
        cli_print_line("Super Agent", CLI_REPLY_TRUNCATED_HINT);
}

/* 输出形态分派：结构化 / 单次执行 / 交互（流式或整段）。 */
static void chat_emit(const chat_fin_t *f)
{
    if (g_cli_json_mode)
        chat_json_out(f);
    else if (g_cli_print_mode)
        chat_print_out(f);
    else if (f->stream)
        chat_stream_out(f);
    else
        chat_plain_out(f);
}

/* 历史写入与诊断收尾：思考链跨轮回传 + 独立日志落盘（所有模式）。
 * 0.1.18 B4 边界：仅为会话内回传与诊断通道，不得写入长期记忆，
 * 也不得上屏。 */
static void chat_hist_flush(llm_response_t *resp, const char *input,
                            const char *render, int tool_rounds,
                            size_t content_len)
{
    const char *reasoning = chat_think_of(resp);

    cli_history_add("user", input, NULL);
    cli_history_add("assistant", render, reasoning);
    cli_chat_reasoning_persist(reasoning);
    if (reasoning[0])
        cli_trace("reasoning", "%s", reasoning);
    cli_chat_usage_reset();
    cli_trace("chat", "%s done rounds=%d bytes=%zu", CLI_ICON_CHECK,
              tool_rounds, content_len);
}

/* 最终回复渲染与历史写入（cli_chat_reply 的收尾阶段）。
 * 思考链默认不上屏（0.1.18 B4），完整文本走诊断通道。
 * 不释放 final_resp（归调用方）。 */
void cli_chat_reply_finalize(llm_response_t *final_resp, const char *input,
                             int stream_mode, int tool_rounds)
{
    const char *final_content = chat_text_of(final_resp);
    /* 思考提示行头（0.1.19 直印化）：名称直出（t1-f → Dual Fast Think /
     * 未配置回落 Dual Think），不再经体验段枚举间接。 */
    char think_hdr[48];
    /* finish_reason 走全栈 canonical 词汇表（llm_service_types.h SSoT）：
     * 截断（length）时收到的正文只是模型答案的前缀，必须在用户面可判读。 */
    const char *final_reason =
        final_resp->finish_reason ? final_resp->finish_reason : LLM_FINISH_STOP;
    char *lg_final = NULL;
    chat_fin_t fin;

    snprintf(think_hdr, sizeof(think_hdr), "%s:思考", cli_chat_think_name());
    fin.resp = final_resp;
    fin.content = final_content;
    fin.render = chat_lang_out(final_content, g_cli_lang_output, &lg_final);
    fin.reason = final_reason;
    fin.think_hdr = think_hdr;
    fin.stream = stream_mode;
    fin.truncated = llm_finish_is_truncated(final_reason);

    chat_emit(&fin);
    chat_hist_flush(final_resp, input, fin.render, tool_rounds,
                    strlen(final_content));
    AIRY_FREE(lg_final);
}
