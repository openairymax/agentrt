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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* 最终回复渲染与历史写入（cli_chat_reply 的收尾阶段）：
 *   --json  结构化 JSON（Codex exec 约定）
 *   -p      纯文本（Claude Code -p / Codex exec 约定；流式已直出）
 *   交互    TTY 流式：擦除预览后 markdown 精修 / 长回复折叠；
 *           TUI/非流式：markdown 渲染 + 折叠区（浏览展开）
 * 不释放 final_resp（归调用方）。 */
void cli_chat_reply_finalize(llm_response_t *final_resp, const char *input,
                             int stream_mode, int tool_rounds)
{
    const char *final_content = (final_resp->choices && final_resp->choice_count > 0 &&
                                 final_resp->choices[0].content)
                                    ? final_resp->choices[0].content
                                    : "";

    /* 1.3 推理语言网关：输出后处理（语言漂移检测 + 术语一致性 + 润色）。
     * 期望输出语言取路由决策 output_lang；仅作用于渲染与历史写入，
     * 不修改 llm_d 原始响应（推理链条证据保留）。 */
    char *lg_final = NULL;
    const char *render_content = final_content;
    if (g_cli_lang_gateway && final_content[0]) {
        if (airy_lang_gateway_post_process(g_cli_lang_gateway, final_content,
                                           g_cli_lang_output,
                                           &lg_final) == AIRY_EOK &&
            lg_final && lg_final[0])
            render_content = lg_final;
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
        else if (final_content[0] == '\0' && g_chat_fold_phys == 0)
            fprintf(stderr,
                    "[chat] warning: empty reply (model returned no text; "
                    "reasoning-only or provider error)\n");
    } else {
        cli_tui_t *r_tui = cli_tui_get_default();
        (void)r_tui;
        if (stream_mode) {
            /* 交互 TTY 流式：擦除打字机预览，重绘最终形态。
             * 上移行数：末尾无换行（光标在最后一行行尾）时 = phys-1，
             * 否则 = phys；\r 回行首再 \033[J 清屏（CUU 只移行不移列，
             * 直接清会残留列尾内容）。擦除前强制 flush：预览/进度行
             * 全部落盘后再移动光标，避免 stdio 缓冲重排造成擦除错位。 */
            if (g_chat_fold_phys > 0 && cli_term_is_tty()) {
                fflush(stdout);
                size_t up = g_chat_fold_phys;
                if (g_chat_fold_tail_no_nl && up > 0)
                    up -= 1;
                /* CUU 参数 0 = 默认值 1（ANSI），up=0 时避免 `\033[0A`
                 * 误删上一行，改用回车 + 清当前行。 */
                char erase[32];
                int en;
                if (up > 0)
                    en = snprintf(erase, sizeof(erase), "\033[%zuA\r\033[J", up);
                else
                    en = snprintf(erase, sizeof(erase), "\r\033[2K");
                if (en > 0)
                    fwrite(erase, 1, (size_t)en, stdout);
                fflush(stdout);
            }
            /* 2.3.5/2.3.14：thinking 模型的思考过程以 [Dual Think] 折叠
             * 呈现（前几行 + 折叠尾），避免碎片刷屏，又让用户看到模型
             * 确实在思考；浏览/日志可看全量。 */
            if (final_resp->choices && final_resp->choice_count > 0 &&
                final_resp->choices[0].reasoning_content &&
                final_resp->choices[0].reasoning_content[0]) {
                cli_render_role_line(CLI_ROLE_DUAL_THINK, cli_chat_think_actor(),
                                     "思考", NULL);
                cli_render_collapsed(final_resp->choices[0].reasoning_content,
                                     4, CLI_REPLY_FOLD_KEEP, 1);
            }
            if (final_content[0] != '\0') {
                /* 结果完整渲染，不折叠（2026-08-19：仅折叠思考链，
                 * 结果必须完整展示；长结果靠终端滚动/TUI 视口浏览）。 */
                cli_render_super_agent(render_content);
            } else {
                cli_render_super_agent(CLI_REPLY_EMPTY_HINT);
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
