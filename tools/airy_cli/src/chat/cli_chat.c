// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat.c
 * @brief airy_cli chat domain: reply main flow (intent split, tool loop, streaming).
 *
 * 处理所有普通用户对话：意图分辨（启发式 + LLM 兜底）、流式渲染（打字机
 * 预览 + 思考进度 + [code] 归一化）、以及直接回复（超级智能体单 t1-f B 模
 * 型生成，决策 2026-08-09）。
 *
 * 2026-08-27 域拆分（2040 行 → 6 个职责模块）：usage/cost 统计 →
 * cli_chat_usage.c；对话记忆读写 → cli_chat_memory.c；GCCP 逐问交互 →
 * cli_chat_gccp.c；历史缓冲/错误描述/系统提示 → cli_chat_history.c；
 * 聊天工具回路 → cli_chat_tools.c。
 *
 * 2026-08-27 二轮拆分（802 行 → 4 个职责模块）：
 *   - cli_chat.c           本文件：cli_chat_reply 主流程（消息组装 / 工具回路）
 *   - cli_chat_classify.c  意图分辨（启发式 + LLM 兜底）
 *   - cli_chat_stream.c    流式归一化 / 思考进度 / 模型槽缓存 / 流式单轮
 *   - cli_chat_finalize.c  回复最终化（后处理 / 渲染 / 历史写入）
 * 跨文件共享声明见 cli_chat_internal.h；公共 API 见 cli_internal.h。
 */

#include "cli_internal.h"

#include "cli_chat_internal.h"
#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/**
  * @brief Chat-set handling: reply to the user directly as the super agent
  *
  * Decision A (2026-08-09): daily chat is generated and routed by the B model
  * (t1-f); no full dual-thinking loop (t2/t1-f/t1-p critique). Single t1-f
  * model replies (AIRY_MODEL_T1F; falls back to the provider default).
  *
  * Decision C (2026-08-15): the chat turn is a tool loop. The model sees the
  * web_search / web_fetch / fs_* tools and may return tool_calls; the CLI
  * executes them (real implementations in tool_d builtin/builtin_net), feeds
  * the results back as role="tool" messages, and repeats until the model
  * answers without tools. Rendering follows the Claude Code tool-use
  * convention: tool cards (⛏ name + folded result) during the loop, then the
  * final reply rendered as markdown. -p 模式整轮走 complete_stream（增量文本
  * 实时直出 stdout）；tool_calls 由 provider 以控制帧暴露，流式结束后的
  * 响应与非流式同构，工具轮逻辑完全复用（2026-08-16）。交互模式仍非流式
  * （spinner + markdown 完整渲染），避免流式下 markdown 标记裸露。
  *
  * 2026-08-27：token/费用统计、记忆读写、工具回路分别收敛到
  * cli_chat_usage.c / cli_chat_memory.c / cli_chat_tools.c（域拆分）；
  * 流式单轮调用收敛到 cli_chat_stream_round（cli_chat_stream.c），
  * 收尾渲染/历史写入收敛到 cli_chat_reply_finalize（cli_chat_finalize.c）。
  */
void cli_chat_reply(const char *input)
{
    if (!g_chat_adapter) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话",
                             "对话服务不可用（模型服务未连接）。");
        return;
    }

    /* 2.1.1.5：新一轮开始清零统计（main.c 在上一轮结束后已读取展示）。
     * 清零逻辑收敛在 cli_chat_usage_reset（cli_chat_usage.c）。 */
    cli_chat_usage_reset();

    const char *t1f_model = cli_chat_t1f_cached();
    cli_trace("chat", "%s start model=%s", CLI_ICON_DIAMOND,
              (t1f_model && t1f_model[0]) ? t1f_model : "default");

    /* Decision B (2026-08-09): config reminder - t1-f (B model) activates first.
      * If unset, hint the three config points and order without blocking (provider default).
      * The hint prints once per session only. 2026-08-17: 改为 cli_trace 输出
      * （日志/stderr），不再渲染进对话列——配置提示是内部运维信息，出现在
      * 会话正文会污染对话（用户要求对话只展示过程，不暴露内部细节）。
      * 2026-08-19: model.yaml 的 llm.model 默认可满足 t1-f，此时不再提示。 */
    static int s_t1f_hint_shown = 0;
    if (!s_t1f_hint_shown && (!t1f_model || !t1f_model[0])) {
        s_t1f_hint_shown = 1;
        cli_trace("config",
                  "t1-f (B model) not configured; set AIRY_MODEL_T1F (local Ollama/vLLM or "
                  "cloud API), then AIRY_MODEL_T2 (A) and AIRY_MODEL_T1P (C) as needed. "
                  "Chat will use the llm_d default model for now.");
    }

#ifdef AIRY_HAS_CJSON
    /* 消息缓冲：[system] + history + [current input]；工具轮会动态扩展
     * （assistant tool_calls + role="tool" 结果），所有内容副本由缓冲统一管理。 */
    cli_chat_msgbuf_t buf;
    AIRY_MEMSET(&buf, 0, sizeof(buf));
    /* 2.2.4 对话记忆读取：相关历史记忆注入 system 上下文（此前对话
     * 路径零记忆，用户"记不住/想不准"的直接根因） */
    char mem_sys[768];
    cli_chat_mem_inject_system(input, mem_sys, sizeof(mem_sys));
    cli_msgbuf_push(&buf, "system", cli_system_prompt_now(), NULL, NULL, NULL);
    /* 1.3 推理语言网关：语言约束 System Prompt 注入（首条 system 之后）。
     * 约束模型内部推理语言与最终输出语言，从源头抑制语言漂移。 */
    if (g_cli_lang_sys_prompt && g_cli_lang_sys_prompt[0])
        cli_msgbuf_push(&buf, "system", g_cli_lang_sys_prompt, NULL, NULL, NULL);
    if (mem_sys[0])
        cli_msgbuf_push(&buf, "system", mem_sys, NULL, NULL, NULL);
    for (size_t hi = 0; hi < g_history_count; hi++)
        cli_msgbuf_push(&buf, g_history_roles[hi], g_history_contents[hi], NULL, NULL,
                        g_history_reasonings[hi]);
    cli_msgbuf_push(&buf, "user", input, NULL, NULL, NULL);

    /* 交互 TTY 走流式（正文直出即终态，0.1.7 弃用「预览→擦除→重绘」
     * 三段式，从根源消除 ANSI 光标操作在跨终端/跨管道下的重叠乱码）；
     * TUI、--json 与 -p 保持非流式（markdown 完整渲染进历史 / 结构化
     * 输出 / 纯 stdout 最终答案）。-p 非流式还避免工具轮之间模型的
     * 过程叙述混入 stdout——脚本模式只消费最终回答（2026-08-17）。
     * 流式正文自身即进度指示，交互非流式路径（TUI/JSON 以外的兜底）
     * 仍用 spinner。 */
    cli_tui_t *tui = cli_tui_get_default();
    int tui_active = tui && cli_tui_active(tui);
    int stream_mode = !g_cli_json_mode && !g_cli_print_mode && !tui_active;

    /* 交互模式的"思考中"状态行（spinner；流式/-p/--json 抑制 chrome）。 */
    int spinner_on = !g_cli_print_mode && !g_cli_json_mode && !stream_mode;
    if (spinner_on) {
        char think_title[128];
        /* 2.3.14：思考角色细分——t1-f 思考中显示 [Dual Fast Think] */
        snprintf(think_title, sizeof(think_title), "%s (%s)",
                 cli_render_actor_name(cli_chat_think_actor()),
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    /* 工具回路：流式模式走 complete_stream（增量文本实时直出，tool_calls
     * 经控制帧暴露）；TUI/--json 走非流式 complete（markdown / 结构化）。
     * 模型返回 tool_calls → 渲染过程卡片 + 执行 + 回填 → 续轮；
     * 不再调用工具 → final_resp 即最终回复。护栏：轮次上限。 */
    llm_response_t *final_resp = NULL;
    int tool_rounds = 0;
    int force_summary = 0; /* 工具轮次用尽：撤下工具定义，强制基于已有结果总结 */
    for (;;) {
        llm_request_config_t cfg;
        __builtin_memset(&cfg, 0, sizeof(cfg));
        cfg.model = t1f_model;
        cfg.messages = buf.msgs;
        cfg.message_count = buf.count;
        cfg.temperature = 0.7f;
        cfg.max_tokens = 2048;
        cfg.tools_json = force_summary ? NULL : cli_chat_tools_json;
        cfg.stream = stream_mode ? 1 : 0;

        llm_response_t *resp = NULL;
        int ret;
        if (stream_mode) {
            /* 交互 TTY 流式：正文直出即终态（0.1.7：不再计量/擦除重绘），
             * 连接/思考进度反馈收敛在 cli_chat_stream_round。 */
            ret = cli_chat_stream_round(g_chat_adapter, &cfg, &resp);
        } else {
            ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
        }
        if (ret != 0 || !resp || resp->choice_count == 0) {
            if (resp)
                llm_response_free(resp);
            if (spinner_on)
                cli_spinner_stop(0, "reply failed");
            /* 人类可读的错误描述（数字码对用户无意义） */
            const char *err_desc = cli_chat_err_desc((int)ret);
            if (ret == 0 && (!resp || resp->choice_count == 0))
                err_desc = "模型未返回文本（可能仅生成了思考内容）";
            char line[256];
            snprintf(line, sizeof(line), "回复失败：%s", err_desc);
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话", line);
            cli_msgbuf_free(&buf);
            return;
        }
        /* 2.1.1.5/2.1.1.6：累计本轮真实 token/费用与思考链（工具轮与
         * 最终轮都计入；reasoning 全量保留，折叠展示之外完整进日志）。 */
        cli_chat_usage_add(resp);
        if (resp->choices[0].reasoning_content)
            cli_chat_reasoning_add(resp->choices[0].reasoning_content);
        int has_tools =
            resp->choices[0].tool_calls_json && resp->choices[0].tool_calls_json[0];
        if (has_tools && !force_summary && tool_rounds < CLI_CHAT_TOOL_MAX_ROUNDS) {
            /* 工具轮中间叙述已随流式直出（0.1.7：不再擦除折叠——擦除依赖
             * ANSI 光标上移，跨终端/管道易重叠乱码）。只需保证叙述与工具
             * 卡片分层：叙述末尾无换行时补一个，工具卡片从新行独立渲染。 */
            if (stream_mode && cli_term_is_tty()) {
                const char *mid = resp->choices[0].content;
                if (mid && mid[0]) {
                    size_t mlen = strlen(mid);
                    if (mlen > 0 && mid[mlen - 1] != '\n')
                        cli_outc('\n');
                }
            }
            if (cli_chat_tool_round(&buf, resp) == 0) {
                tool_rounds++;
                llm_response_free(resp);
                continue;
            }
        }
        /* 工具轮次用尽但模型仍想调用工具：不再放行工具，追加一条
         * 总结提示并进入最终轮，保证用户拿到基于已获取信息的完整回答
         * （此前直接采纳该过渡响应，用户只能看到一行半截文本）。 */
        if (has_tools && !force_summary) {
            force_summary = 1;
            cli_msgbuf_push(&buf, "user",
                            "（工具调用轮次已用尽。请仅基于以上已获取的信息给出最终回答，"
                            "不要再调用任何工具。）",
                            NULL, NULL, NULL);
            llm_response_free(resp);
            continue;
        }
        final_resp = resp;
        break;
    }

    if (spinner_on)
        cli_spinner_stop(1, NULL);

    /* 2.2.4 对话记忆写入：一轮对话完成且有回复时落盘（用户输入+回复+
     * 思考链，供下轮/下次会话检索注入；2.1.1.6 起携带 reasoning）。 */
    if (final_resp && final_resp->choice_count > 0 && final_resp->choices[0].content) {
        const char *mem_reasoning =
            (final_resp->choices[0].reasoning_content && final_resp->choices[0].reasoning_content[0])
                ? final_resp->choices[0].reasoning_content
                : cli_chat_reasoning_peek();
        cli_chat_mem_record(input, final_resp->choices[0].content, mem_reasoning);
    }
#else
    /* 无 cJSON 平台：固定消息数组 + 单轮非流式（不带工具），保持纯对话。
     * 消息数 = 历史 + 2（首 system + 当前 user）+ 1（语言约束 system，可能缺省）。 */
    int stream_mode = 0;
    int tool_rounds = 0;
    size_t msg_n = g_history_count + 3;
    llm_message_t *msgs = (llm_message_t *)AIRY_CALLOC(msg_n, sizeof(llm_message_t));
    if (!msgs) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话",
                             "内存不足，无法生成回复。");
        return;
    }
    size_t mi = 0;
    msgs[mi].role = "system";
    msgs[mi].content = cli_system_prompt_now();
    mi++;
    /* 1.3 推理语言网关：语言约束 System Prompt 注入（非 cJSON 平台） */
    if (g_cli_lang_sys_prompt && g_cli_lang_sys_prompt[0] && mi < msg_n) {
        msgs[mi].role = "system";
        msgs[mi].content = g_cli_lang_sys_prompt;
        mi++;
    }
    for (size_t hi = 0; hi < g_history_count; hi++) {
        msgs[mi].role = g_history_roles[hi];
        msgs[mi].content = g_history_contents[hi];
        msgs[mi].reasoning_content = g_history_reasonings[hi];
        mi++;
    }
    msgs[mi].role = "user";
    msgs[mi].content = input;
    mi++;

    int spinner_on = !g_cli_print_mode && !g_cli_json_mode;
    if (spinner_on) {
        char think_title[128];
        /* 2.3.14：思考角色细分——t1-f 思考中显示 [Dual Fast Think] */
        snprintf(think_title, sizeof(think_title), "%s (%s)",
                 cli_render_actor_name(cli_chat_think_actor()),
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = t1f_model;
    cfg.messages = msgs;
    cfg.message_count = msg_n;
    cfg.temperature = 0.7f;
    cfg.max_tokens = 2048;

    llm_response_t *final_resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &final_resp);
    if (ret != 0 || !final_resp || final_resp->choice_count == 0) {
        if (final_resp)
            llm_response_free(final_resp);
        if (spinner_on)
            cli_spinner_stop(0, "reply failed");
        char line[128];
        snprintf(line, sizeof(line), "回复失败：%s", cli_chat_err_desc(ret));
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话", line);
        AIRY_FREE(msgs);
        return;
    }
    /* 2.1.1.5/2.1.1.6：累计本轮真实 token/费用与思考链（无 cJSON 平台） */
    cli_chat_usage_add(final_resp);
    if (final_resp->choices[0].reasoning_content)
        cli_chat_reasoning_add(final_resp->choices[0].reasoning_content);
    if (spinner_on)
        cli_spinner_stop(1, NULL);
#endif /* AIRY_HAS_CJSON */

    /* 收尾：语言网关输出后处理 + 最终渲染 + 历史写入（cli_chat_finalize.c）。
     * final_resp 归本函数释放；buf/msgs 按平台分支释放。 */
    cli_chat_reply_finalize(final_resp, input, stream_mode, tool_rounds);

    llm_response_free(final_resp);
#ifdef AIRY_HAS_CJSON
    cli_msgbuf_free(&buf);
#else
    AIRY_FREE(msgs);
#endif
}
