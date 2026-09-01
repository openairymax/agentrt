// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_finalize.c
 * @brief 聊天域回复最终化（域拆分自 cli_chat.c，2026-08-27）。
 *
 * cli_chat_reply 的收尾阶段：语言网关输出后处理、最终回复渲染
 * （--json / -p / 交互流式折叠 / TUI 非流式）、历史写入与思考链持久化。
 * 共享声明见 cli_chat_internal.h。
 */

#include "cli_chat_internal.h"
#include "cli_gw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* 最终回复渲染与历史写入（cli_chat_reply 的收尾阶段）：
 *   --json  结构化 JSON（Codex exec 约定）
 *   -p      纯文本（Claude Code -p / Codex exec 约定；流式已直出）
 *   交互    TTY 流式：正文已直出即终态，收尾只补思考链折叠
 *           （0.1.7 弃用「擦除预览→重绘最终形态」三段式）；
 *           TUI/非流式：markdown 渲染 + 折叠区（浏览展开）
 * 不释放 final_resp（归调用方）。 */
void cli_chat_reply_finalize(llm_response_t *final_resp, const char *input,
                             int stream_mode, int tool_rounds)
{
    const char *final_content = (final_resp->choices && final_resp->choice_count > 0 &&
                                 final_resp->choices[0].content)
                                    ? final_resp->choices[0].content
                                    : "";

    /* 1.3 推理语言网关服务面化（M1-1c）：输出后处理（语言漂移检测 +
     * 术语一致性 + 润色）经 gateway → think.lang_postprocess（think_d
     * 承载）。期望输出语言取路由决策 output_lang（wire 值）；仅作用于
     * 渲染与历史写入，不修改 llm_d 原始响应（推理链条证据保留）。
     * 失败时保留原始正文，不阻断渲染。 */
    char *lg_final = NULL;
    const char *render_content = final_content;
    if (final_content[0]) {
#ifdef AIRY_HAS_CJSON
        cJSON *lp = cJSON_CreateObject();
        cJSON *lt = cJSON_CreateString(final_content);
        if (lp && lt)
            cJSON_AddItemToObject(lp, "text", lt);
        else
            cJSON_Delete(lt);
        if (g_cli_lang_output > 0) {
            cJSON *el = cJSON_CreateNumber(g_cli_lang_output);
            if (lp && el)
                cJSON_AddItemToObject(lp, "expected_lang", el);
            else
                cJSON_Delete(el);
        }
        char *lp_json = lp ? cJSON_PrintUnformatted(lp) : NULL;
        cJSON_Delete(lp);
        char *lp_res = NULL;
        if (lp_json && cli_gw_call("think.lang_postprocess", lp_json, 6000, &lp_res) == 0 &&
            lp_res) {
            cJSON *lr = cJSON_Parse(lp_res);
            if (lr) {
                cJSON *tx = cJSON_GetObjectItem(lr, "text");
                if (cJSON_IsString(tx) && tx->valuestring && tx->valuestring[0]) {
                    AIRY_FREE(lg_final);
                    lg_final = AIRY_STRDUP(tx->valuestring);
                    if (lg_final)
                        render_content = lg_final;
                }
                cJSON_Delete(lr);
            }
            AIRY_FREE(lp_res);
        } else {
            AIRY_FREE(lp_res);
        }
        AIRY_FREE(lp_json);
#endif /* AIRY_HAS_CJSON */
    }

    if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "role", "super_agent");
        cJSON_AddStringToObject(root, "type", "chat");
        if (final_content[0])
            cJSON_AddStringToObject(root, "content", render_content);
        else
            cJSON_AddStringToObject(root, "error", "reply failed");
        /* 2.1.1.6：--json 结构化输出携带思考链（TUI RunResponse.thinking
         * 已有先例），思考 token 不随 JSON 输出丢失。 */
        const char *json_reasoning = (final_resp->choices && final_resp->choice_count > 0 &&
                                      final_resp->choices[0].reasoning_content)
                                         ? final_resp->choices[0].reasoning_content
                                         : (cli_chat_reasoning_peek() ? cli_chat_reasoning_peek() : "");
        if (json_reasoning && json_reasoning[0])
            cJSON_AddStringToObject(root, "reasoning", json_reasoning);
        if (final_resp) {
            cJSON *usage = cJSON_CreateObject();
            cJSON_AddNumberToObject(usage, "prompt_tokens", final_resp->prompt_tokens);
            cJSON_AddNumberToObject(usage, "completion_tokens", final_resp->completion_tokens);
            cJSON_AddNumberToObject(usage, "total_tokens", final_resp->total_tokens);
            cJSON_AddNumberToObject(usage, "reasoning_tokens", final_resp->reasoning_tokens);
            cJSON_AddNumberToObject(usage, "cost_usd", final_resp->cost_usd);
            cJSON_AddItemToObject(root, "usage", usage);
        }
        char *js = cJSON_PrintUnformatted(root);
        if (js) {
            cli_outf("%s\n", js);
            cJSON_free(js);
        }
        cJSON_Delete(root);
#else
        cli_outf("{\"role\":\"super_agent\",\"type\":\"chat\"");
        if (final_content[0])
            cli_outf(",\"content\":\"%s\"", render_content);
        else
            cli_outf(",\"error\":\"reply failed\"");
        cli_outf("}\n");
#endif /* AIRY_HAS_CJSON */
    } else if (g_cli_print_mode) {
        /* 流式模式：final_content 已随块实时直出，不再重复打印。
         * 空返回诊断（2026-08-17）：流式未输出任何字节且模型无文本
         * 回复（thinking 模型可能只产生 reasoning_content）→ stderr
         * 明确告警，stdout 保持空串可解析（脚本不被打断）。 */
        if (!stream_mode)
            cli_outf("%s\n", render_content);
        else if (final_content[0] == '\0')
            fprintf(stderr,
                    "[chat] warning: empty reply (model returned no text; "
                    "reasoning-only or provider error)\n");
    } else {
        cli_tui_t *r_tui = cli_tui_get_default();
        (void)r_tui;
        if (stream_mode) {
            /* 交互 TTY 流式（0.1.7 终态直出）：正文已随流式逐块上屏，
             * 不再「擦除预览 + 重绘最终形态」——原方案依赖 ANSI 光标
             * 上移计量（g_chat_fold_phys），跨终端/管道易重叠乱码。
             * 收尾仅两件事：①空回复兜底提示；②思考链折叠附加在
             * 回复之后（思考过程已隐藏，落定后补上，浏览/日志可见）。 */
            if (final_content[0] == '\0') {
                cli_render_super_agent(CLI_REPLY_EMPTY_HINT);
            } else if (final_resp->choices && final_resp->choice_count > 0 &&
                       final_resp->choices[0].reasoning_content &&
                       final_resp->choices[0].reasoning_content[0]) {
                /* 正文末尾补空行分层，思考链折叠展示 */
                cli_outc('\n');
                cli_render_role_line(CLI_ROLE_DUAL_THINK, cli_chat_think_actor(),
                                     "思考", NULL);
                cli_render_collapsed(final_resp->choices[0].reasoning_content,
                                     4, CLI_REPLY_FOLD_KEEP, 1);
            }
        } else {
            /* TUI / 非流式交互：思考链折叠展示（进历史）；结果完整渲染
             * 进历史（用户要求结果不折叠，长结果经视口滚动浏览）。 */
            if (final_resp->choices && final_resp->choice_count > 0 &&
                final_resp->choices[0].reasoning_content &&
                final_resp->choices[0].reasoning_content[0]) {
                cli_render_role_line(CLI_ROLE_DUAL_THINK, cli_chat_think_actor(),
                                     "思考", NULL);
                cli_render_collapsed(final_resp->choices[0].reasoning_content,
                                     4, CLI_REPLY_FOLD_KEEP, 1);
            }
            if (final_content[0] != '\0') {
                cli_render_super_agent(render_content);
            } else {
                cli_render_super_agent(CLI_REPLY_EMPTY_HINT);
            }
        }
    }

    /* 2.1.1.6：思考链全量保留——历史携带 reasoning（跨轮回传 DeepSeek
     * 续轮规范）+ 独立日志落盘（所有模式），折叠展示之外的完整文本不丢失。 */
    const char *round_reasoning = (final_resp->choices && final_resp->choice_count > 0 &&
                                   final_resp->choices[0].reasoning_content)
                                      ? final_resp->choices[0].reasoning_content
                                      : (cli_chat_reasoning_peek() ? cli_chat_reasoning_peek() : "");
    cli_history_add("user", input, NULL);
    cli_history_add("assistant", render_content, round_reasoning);
    cli_chat_reasoning_persist(round_reasoning);
    /* -p 模式保持既有 stderr trace 通道（供脚本消费进度）；随后清零本轮
     * 统计与思考链累积（cli_chat_usage.c 统一收口）。 */
    if (cli_chat_reasoning_peek() && cli_chat_reasoning_peek()[0])
        cli_trace("reasoning", "%s", cli_chat_reasoning_peek());
    cli_chat_usage_reset();
    cli_trace("chat", "%s done rounds=%d bytes=%zu", CLI_ICON_CHECK, tool_rounds,
              strlen(final_content));

    AIRY_FREE(lg_final);
}
