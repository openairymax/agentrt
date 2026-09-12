// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat.c
 * @brief airy_cli chat domain: reply main flow (intent split, gateway
 *        round-trip, tool loop, model-slot cache).
 *
 * 处理所有普通用户对话：意图分辨（启发式 + LLM 兜底）、直接回复（超级智
 * 能体单 t1-f B 模型生成，决策 2026-08-09）。
 *
 * C-01 路线 A（2026-09）：对话主路径统一经 gateway llm.complete 非流式
 * 单发（架构铁律：一切客户端功能走 gateway，禁止直连 daemon socket /
 * 运行时库）。交互 TTY 的打字机流式直出随 cli_chat_stream.c 整体退役，
 * 统一为 spinner + 完整渲染；流式恢复路径 = gateway SSE 能力
 * （0.1.15 后评估，另行立项）。
 *
 * 2026-08-27 域拆分（2040 行 → 职责模块）：usage/cost 统计 →
 * cli_chat_usage.c；对话记忆读写 → cli_chat_memory.c；GCCP 逐问交互 →
 * cli_chat_gccp.c；历史缓冲/错误描述/系统提示 → cli_chat_history.c；
 * 聊天工具回路 → cli_chat_tools.c；cli_chat.c 本文件：cli_chat_reply
 * 主流程（消息组装 / 工具回路）+ 模型槽缓存。
 * 跨文件共享声明见 cli_chat_internal.h；公共 API 见 cli_internal.h。
 */

#include "cli_internal.h"

#include "cli_chat_internal.h"
#include "cli_gw.h" /* 架构约束：统一经 gateway 派发（C-01 路线 A） */
#include "llm_service.h" /* llm_response_free（llm_d 公共接口，airy_cli CMake 已含 llm_d/include） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#include <cjson/cJSON.h>

/* ── gateway 路线 A（C-01，2026-09）：对话主路径 llm.complete 单发 ── */

/* 对话主路径超时：与 llm_svc_adapter DEFAULT_REQUEST_TIMEOUT_MS 对齐。 */
#define CLI_CHAT_LLM_TIMEOUT_MS 120000

/* 网关 llm.complete 的 result JSON → llm_response_t。
 * 字段映射与分配策略镜像 llm_d src/response.c response_from_json
 * （wire 契约 SSoT = llm_d response.c）；释放走公共接口
 * llm_response_free，AIRY_STRDUP / cJSON_PrintUnformatted 与 llm_d
 * 侧同源兼容。解析失败返回 NULL。 */
static llm_response_t *cli_chat_resp_from_json(const char *json)
{
    if (!json)
        return NULL;
    cJSON *root = cJSON_Parse(json);
    if (!root)
        return NULL;
    llm_response_t *resp = (llm_response_t *)AIRY_CALLOC(1, sizeof(llm_response_t));
    if (!resp) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON *v = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(v))
        resp->id = AIRY_STRDUP(v->valuestring);
    v = cJSON_GetObjectItem(root, "model");
    if (cJSON_IsString(v))
        resp->model = AIRY_STRDUP(v->valuestring);
    v = cJSON_GetObjectItem(root, "created");
    if (cJSON_IsNumber(v))
        resp->created = (uint64_t)v->valuedouble;
    v = cJSON_GetObjectItem(root, "prompt_tokens");
    if (cJSON_IsNumber(v))
        resp->prompt_tokens = (uint32_t)v->valuedouble;
    v = cJSON_GetObjectItem(root, "completion_tokens");
    if (cJSON_IsNumber(v))
        resp->completion_tokens = (uint32_t)v->valuedouble;
    v = cJSON_GetObjectItem(root, "total_tokens");
    if (cJSON_IsNumber(v))
        resp->total_tokens = (uint32_t)v->valuedouble;
    /* usage 嵌套块覆盖顶层（llm_d response_to_json 双写兼容形态） */
    cJSON *usage = cJSON_GetObjectItem(root, "usage");
    if (cJSON_IsObject(usage)) {
        cJSON *u;
        if ((u = cJSON_GetObjectItem(usage, "prompt_tokens")) && cJSON_IsNumber(u))
            resp->prompt_tokens = (uint32_t)u->valuedouble;
        if ((u = cJSON_GetObjectItem(usage, "completion_tokens")) && cJSON_IsNumber(u))
            resp->completion_tokens = (uint32_t)u->valuedouble;
        if ((u = cJSON_GetObjectItem(usage, "total_tokens")) && cJSON_IsNumber(u))
            resp->total_tokens = (uint32_t)u->valuedouble;
        if ((u = cJSON_GetObjectItem(usage, "reasoning_tokens")) && cJSON_IsNumber(u))
            resp->reasoning_tokens = (uint32_t)u->valuedouble;
    }
    /* 兼容顶层 reasoning_tokens（部分端点直接输出顶层而非嵌套） */
    if (resp->reasoning_tokens == 0) {
        v = cJSON_GetObjectItem(root, "reasoning_tokens");
        if (cJSON_IsNumber(v))
            resp->reasoning_tokens = (uint32_t)v->valuedouble;
    }
    v = cJSON_GetObjectItem(root, "cost_usd");
    if (cJSON_IsNumber(v))
        resp->cost_usd = v->valuedouble;
    v = cJSON_GetObjectItem(root, "finish_reason");
    if (cJSON_IsString(v))
        resp->finish_reason = AIRY_STRDUP(v->valuestring);

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices)) {
        resp->choice_count = (size_t)cJSON_GetArraySize(choices);
        resp->choices = (llm_message_t *)AIRY_CALLOC(resp->choice_count, sizeof(llm_message_t));
        if (!resp->choices) {
            llm_response_free(resp);
            cJSON_Delete(root);
            return NULL;
        }
        size_t i = 0;
        cJSON *choice;
        cJSON_ArrayForEach(choice, choices)
        {
            llm_message_t *m = &resp->choices[i++];
            v = cJSON_GetObjectItem(choice, "role");
            if (cJSON_IsString(v))
                m->role = AIRY_STRDUP(v->valuestring);
            v = cJSON_GetObjectItem(choice, "content");
            if (cJSON_IsString(v))
                m->content = AIRY_STRDUP(v->valuestring);
            v = cJSON_GetObjectItem(choice, "reasoning_content");
            if (cJSON_IsString(v))
                m->reasoning_content = AIRY_STRDUP(v->valuestring);
            v = cJSON_GetObjectItem(choice, "tool_calls");
            if (cJSON_IsArray(v) && cJSON_GetArraySize(v) > 0) {
                m->tool_calls_json = cJSON_PrintUnformatted(v);
                if (!m->tool_calls_json) {
                    llm_response_free(resp);
                    cJSON_Delete(root);
                    return NULL;
                }
            }
        }
    }
    cJSON_Delete(root);
    return resp;
}

/* 单轮对话完成调用：消息缓冲 → llm.complete params → cli_gw_call →
 * llm_response_t。with_tools=0（工具轮次用尽的总结轮）时不携带工具
 * 定义。params 契约（llm_daemon_request.c parse_params 逐字段判据）：
 * messages 非空数组、role/content 须字符串、stream 字段不写（daemon
 * 侧默认非流式）、tool_calls 必须是数组、消息数 ≤ MAX_MESSAGES_PER_REQUEST。
 * 失败返回非 0：可执行原因已写入 g_cli_gw_err（cli_err_desc 一次性消费）。 */
static int cli_chat_gw_round(const char *model, const cli_chat_msgbuf_t *buf,
                             int with_tools, llm_response_t **out_resp)
{
    *out_resp = NULL;
    cJSON *params = cJSON_CreateObject();
    if (!params)
        return -1;
    if (model && model[0])
        cJSON_AddStringToObject(params, "model", model);
    cJSON *messages = cJSON_AddArrayToObject(params, "messages");
    if (!messages) {
        cJSON_Delete(params);
        return -1;
    }
    for (size_t i = 0; buf && i < buf->count; i++) {
        const llm_message_t *m = &buf->msgs[i];
        cJSON *j = cJSON_CreateObject();
        if (!j)
            break;
        cJSON_AddStringToObject(j, "role", m->role ? m->role : "user");
        /* parse_params 要求 content 必为字符串：工具调用轮的 assistant
         * 消息常无正文，空串占位（与 llm_d response_to_json 同形）。 */
        cJSON_AddStringToObject(j, "content", m->content ? m->content : "");
        if (m->reasoning_content && m->reasoning_content[0])
            cJSON_AddStringToObject(j, "reasoning_content", m->reasoning_content);
        if (m->tool_call_id && m->tool_call_id[0])
            cJSON_AddStringToObject(j, "tool_call_id", m->tool_call_id);
        if (m->tool_calls_json && m->tool_calls_json[0]) {
            /* wire 形态：tool_calls 是数组（tool_calls_json 是内部字段） */
            cJSON *tc = cJSON_Parse(m->tool_calls_json);
            if (tc && cJSON_IsArray(tc) && cJSON_GetArraySize(tc) > 0)
                cJSON_AddItemToObject(j, "tool_calls", tc);
            else if (tc)
                cJSON_Delete(tc);
        }
        cJSON_AddItemToArray(messages, j);
    }
    cJSON_AddNumberToObject(params, "temperature", 0.7);
    cJSON_AddNumberToObject(params, "max_tokens", 2048);
    if (with_tools) {
        cJSON *tools = cJSON_Parse(cli_chat_tools_json);
        if (tools && cJSON_IsArray(tools) && cJSON_GetArraySize(tools) > 0)
            cJSON_AddItemToObject(params, "tools", tools);
        else if (tools)
            cJSON_Delete(tools);
    }
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json)
        return -1;

    char *result_json = NULL;
    int ret = cli_gw_call("llm.complete", params_json, CLI_CHAT_LLM_TIMEOUT_MS, &result_json);
    AIRY_FREE(params_json);
    if (ret != 0) {
        AIRY_FREE(result_json);
        /* 失败/取消码直通（S-02）：失败时 g_cli_gw_err 已由 cli_gw_call
         * 填写（cli_err_desc 一次性消费）；取消（AIRY_ERR_CANCELED）语义
         * 自明，由 cli_chat_reply 渲染如实告知。 */
        return ret;
    }
    *out_resp = cli_chat_resp_from_json(result_json);
    AIRY_FREE(result_json);
    if (!*out_resp) {
        extern char g_cli_gw_err[256]; /* 同 cli_chat_tools.c 的既有访问方式 */
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "模型响应解析失败（网关返回格式异常）");
        return -1;
    }
    return 0;
}

/**
  * @brief Chat-set handling: reply to the user directly as the super agent
  *
  * Decision A (2026-08-09): daily chat is generated and routed by the B model
  * (t1-f); no full dual-thinking loop (t2/t1-f/t1-p critique). Single t1-f
  * model replies (AIRY_MODEL_T1F; falls back to the provider default).
  *
  * Decision C (2026-08-15): the chat turn is a tool loop. The model sees the
  * web_search / web_fetch / fs_* tools and may return tool_calls; the CLI
  * executes them (real implementations in tool_d builtin/builtin_net, 经
  * gateway tool.execute 派发), feeds the results back as role="tool"
  * messages, and repeats until the model answers without tools. Rendering
  * follows the Claude Code tool-use convention: tool cards (⛏ name +
  * folded result) during the loop, then the final reply rendered as
  * markdown.
  *
  * C-01 路线 A（2026-09）：每轮经 cli_chat_gw_round → gateway
  * llm.complete 非流式单发；--json/-p/TUI/交互 TTY 统一形态（spinner +
  * 完整渲染）；收尾渲染/历史写入收敛在 cli_chat_reply_finalize。
  */
void cli_chat_reply(const char *input)
{
    /* 2.1.1.5：新一轮开始清零统计（main.c 在上一轮结束后已读取展示）。
     * 清零逻辑收敛在 cli_chat_usage_reset（cli_chat_usage.c）。 */
    cli_chat_usage_reset();
    /* S-02：逐轮复位取消标志（对齐 taskflow_run 入口惯例）——SIGINT 只
     * 取消当前轮的等待，不污染下一轮对话。 */
    g_cli_cancel = 0;

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

    /* "思考中"状态行（spinner）。C-01 路线 A：对话主路径统一经 gateway
     * llm.complete 非流式单发，交互 TTY 亦走 spinner + 完整渲染（打字机
     * 直出随 cli_chat_stream.c 一并退役）。-p/--json 抑制 chrome。 */
    int spinner_on = !g_cli_print_mode && !g_cli_json_mode;
    if (spinner_on) {
        char think_title[128];
        /* 2.3.14：思考角色细分——t1-f 思考中显示 [Dual Fast Think] */
        snprintf(think_title, sizeof(think_title), "%s (%s)",
                 cli_render_actor_name(cli_chat_think_actor()),
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    /* 工具回路：每轮 cli_chat_gw_round → gateway llm.complete 非流式单发。
     * 模型返回 tool_calls → 渲染过程卡片 + 执行 + 回填 → 续轮；
     * 不再调用工具 → final_resp 即最终回复。护栏：轮次上限。 */
    llm_response_t *final_resp = NULL;
    int tool_rounds = 0;
    int force_summary = 0; /* 工具轮次用尽：撤下工具定义，强制基于已有结果总结 */
    for (;;) {
        llm_response_t *resp = NULL;
        int ret = cli_chat_gw_round(t1f_model, &buf, !force_summary, &resp);
        if (ret != 0 || !resp || resp->choice_count == 0) {
            if (resp)
                llm_response_free(resp);
            int canceled = (ret == AIRY_ERR_CANCELED);
            if (spinner_on)
                cli_spinner_stop(0, canceled ? "canceled" : "reply failed");
            /* 人类可读的错误描述（数字码对用户无意义）。C-01 ③：gateway
             * 路径下 cli_gw_call / cli_chat_gw_round 已把可执行原因写入
             * g_cli_gw_err，cli_err_desc 一次性消费。S-02：取消码直通时
             * g_cli_gw_err 未填，如实告知服务端可能仍在推理（服务端可
             * 取消登记 0.2.x）。 */
            const char *err_desc;
            if (canceled)
                err_desc = "用户中断（Ctrl+C）；服务端推理可能仍在进行，本轮结果将被丢弃";
            else
                err_desc = cli_chat_err_desc((int)ret);
            if (ret == 0 && (!resp || resp->choice_count == 0))
                err_desc = "模型未返回文本（可能仅生成了思考内容）";
            char line[384];
            snprintf(line, sizeof(line), "%s：%s",
                     canceled ? "已取消" : "回复失败", err_desc);
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

    /* 收尾：语言网关输出后处理 + 最终渲染 + 历史写入（cli_chat_finalize.c）。
     * C-01 后 stream_mode 恒 0（网关非流式单发）；final_resp 归本函数释放。 */
    cli_chat_reply_finalize(final_resp, input, 0, tool_rounds);

    llm_response_free(final_resp);
    cli_msgbuf_free(&buf);
}

/* ── 模型槽缓存（自 cli_chat_stream.c 迁入；C-01 ⑤ 退役该文件） ── */

/* 2.3.14 (2026-08-17)：对话思考链来自 t1-f（context arbiter）模型，
 * 实时思考标签为 [Dual Fast Think]；未配置（走 llm_d 默认模型）时
 * 回落通用 [Dual Think]。2026-08-19：t1-f 配置统一来自
 * cli_think_cfg_load（env > model.yaml think 段 > llm.model 默认），
 * 与 think_d 实际生效模型一致；静态缓存避免每帧进度行重复读文件。 */
const char *cli_chat_t1f_cached(void)
{
    static char s_t1f[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1p[128];
        cli_think_cfg_load(t2, sizeof(t2), s_t1f, sizeof(s_t1f), t1p, sizeof(t1p));
    }
    return s_t1f;
}

/* 2.1.1.2 修复：t1-p（PROF）模型缓存——GCCP 意图确认（[Dual Prof Think]）
 * 使用该模型槽，与 CLI 渲染标签一致。配置源与 t1f 同（cli_think_cfg_load：
 * env > model.yaml think 段 > 默认）。非 static：cli_chat_gccp.c 的
 * cli_gccp_interact 逐问确认调用（原型见 cli_internal.h）。 */
const char *cli_chat_t1p_cached(void)
{
    static char s_t1p[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1f[128];
        cli_think_cfg_load(t2, sizeof(t2), t1f, sizeof(t1f), s_t1p, sizeof(s_t1p));
    }
    return s_t1p;
}

cli_actor_t cli_chat_think_actor(void)
{
    const char *t1f = cli_chat_t1f_cached();
    return (t1f && t1f[0]) ? CLI_ACTOR_DUAL_FAST_THINK : CLI_ACTOR_DUAL_THINK;
}
