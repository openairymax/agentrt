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
 *
 * 0.1.19 §459：本文件内"JSON 字段提取"「消息序列化」「主流程编排」三层
 * 各自收敛为单一骨架——resp_from_json 只做字段搬运，gw_round 只做请求
 * 构造，reply 只做轮次编排；重复的取字段三元组统一走 take_* 提取器。
 */

#include "cli_internal.h"

#include "cli_chat_internal.h"
#include "cli_gw.h" /* 架构约束：统一经 gateway 派发（C-01 路线 A） */
#include "cli_gw_json.h"
#include "llm_service.h" /* llm_response_free（llm_d 公共接口，airy_cli CMake 已含 llm_d/include） */

#include "cli_prelude.h"

#include <cjson/cJSON.h>

/* ── gateway 路线 A（C-01，2026-09）：对话主路径 llm.complete 单发 ── */

/* 对话主路径超时：与 llm_svc_adapter DEFAULT_REQUEST_TIMEOUT_MS 对齐。 */
#define CLI_CHAT_LLM_TIMEOUT_MS 120000

/* ── cJSON 字段提取器（0.1.19 §459）──
 * 原实现每个字段都是"GetObjectItem + 类型判断 + 赋值"三段式，20 处重复。
 * 提取器把"缺失或类型不符即保持原值"的语义固化，调用点只声明字段名。 */
/* 字段缺失或类型不符时返回 NULL（调用点均为零初始化对象，语义等价）。 */
static char *take_dup(cJSON *obj, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return cJSON_IsString(v) ? AIRY_STRDUP(v->valuestring) : NULL;
}

static void take_u32(cJSON *obj, const char *key, uint32_t *out)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(v))
        *out = (uint32_t)v->valuedouble;
}

static void take_u64(cJSON *obj, const char *key, uint64_t *out)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(v))
        *out = (uint64_t)v->valuedouble;
}

static void take_f64(cJSON *obj, const char *key, double *out)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(v))
        *out = v->valuedouble;
}

/* token 计费：顶层与 usage 嵌套双写兼容（llm_d response_to_json 形态），
 * 嵌套块优先；reasoning_tokens 部分端点只给顶层，此处回填。 */
static void usage_fill(cJSON *root, llm_response_t *resp)
{
    take_u32(root, "prompt_tokens", &resp->prompt_tokens);
    take_u32(root, "completion_tokens", &resp->completion_tokens);
    take_u32(root, "total_tokens", &resp->total_tokens);
    cJSON *usage = cJSON_GetObjectItem(root, "usage");
    if (cJSON_IsObject(usage)) {
        take_u32(usage, "prompt_tokens", &resp->prompt_tokens);
        take_u32(usage, "completion_tokens", &resp->completion_tokens);
        take_u32(usage, "total_tokens", &resp->total_tokens);
        take_u32(usage, "reasoning_tokens", &resp->reasoning_tokens);
    }
    if (resp->reasoning_tokens == 0)
        take_u32(root, "reasoning_tokens", &resp->reasoning_tokens);
}

/* 单条 choice → llm_message_t。tool_calls 以未格式化 JSON 原文保存
 * （llm_message_t 的既有内部字段形态）。 */
static int choice_fill(cJSON *choice, llm_message_t *m)
{
    m->role = take_dup(choice, "role");
    m->content = take_dup(choice, "content");
    m->reasoning_content = take_dup(choice, "reasoning_content");
    cJSON *tc = cJSON_GetObjectItem(choice, "tool_calls");
    if (!cJSON_IsArray(tc) || cJSON_GetArraySize(tc) <= 0)
        return 0;
    m->tool_calls_json = cJSON_PrintUnformatted(tc);
    return m->tool_calls_json ? 0 : -1;
}

static int choices_fill(cJSON *root, llm_response_t *resp)
{
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) <= 0)
        return 0;
    resp->choice_count = (size_t)cJSON_GetArraySize(choices);
    resp->choices = (llm_message_t *)AIRY_CALLOC(resp->choice_count, sizeof(llm_message_t));
    if (!resp->choices)
        return -1;
    size_t i = 0;
    cJSON *choice;
    cJSON_ArrayForEach(choice, choices)
    {
        if (choice_fill(choice, &resp->choices[i++]) != 0)
            return -1;
    }
    return 0;
}

/* 网关 llm.complete 的 result 树 → llm_response_t。
 * 字段映射与分配策略镜像 llm_d src/response.c response_from_json
 * （wire 契约 SSoT = llm_d response.c）；释放走公共接口 llm_response_free。 */
static llm_response_t *resp_from_json(cJSON *root)
{
    llm_response_t *resp = (llm_response_t *)AIRY_CALLOC(1, sizeof(llm_response_t));
    if (!resp)
        return NULL;
    resp->id = take_dup(root, "id");
    resp->model = take_dup(root, "model");
    resp->finish_reason = take_dup(root, "finish_reason");
    take_u64(root, "created", &resp->created);
    take_f64(root, "cost_usd", &resp->cost_usd);
    usage_fill(root, resp);
    if (choices_fill(root, resp) != 0) {
        llm_response_free(resp);
        return NULL;
    }
    return resp;
}

/* ── 请求构造（0.1.19 §459）── */

/* wire 形态：tool_calls 是数组（tool_calls_json 是内部字段）。 */
static void add_tool_calls(cJSON *j, const char *tc_json)
{
    if (!tc_json || !tc_json[0])
        return;
    cJSON *tc = cJSON_Parse(tc_json);
    if (!tc)
        return;
    if (cJSON_IsArray(tc) && cJSON_GetArraySize(tc) > 0)
        cJSON_AddItemToObject(j, "tool_calls", tc);
    else
        cJSON_Delete(tc);
}

static cJSON *msg_to_json(const llm_message_t *m)
{
    cJSON *j = cJSON_CreateObject();
    if (!j)
        return NULL;
    cJSON_AddStringToObject(j, "role", m->role ? m->role : "user");
    /* parse_params 要求 content 必为字符串：工具调用轮的 assistant
     * 消息常无正文，空串占位（与 llm_d response_to_json 同形）。 */
    cJSON_AddStringToObject(j, "content", m->content ? m->content : "");
    if (m->reasoning_content && m->reasoning_content[0])
        cJSON_AddStringToObject(j, "reasoning_content", m->reasoning_content);
    if (m->tool_call_id && m->tool_call_id[0])
        cJSON_AddStringToObject(j, "tool_call_id", m->tool_call_id);
    add_tool_calls(j, m->tool_calls_json);
    return j;
}

static int buf_to_json(cJSON *messages, const cli_chat_msgbuf_t *buf)
{
    for (size_t i = 0; buf && i < buf->count; i++) {
        cJSON *j = msg_to_json(&buf->msgs[i]);
        if (!j)
            return -1;
        cJSON_AddItemToArray(messages, j);
    }
    return 0;
}

static void add_tools(cJSON *params)
{
    cJSON *tools = cJSON_Parse(cli_chat_tools_json);
    if (!tools)
        return;
    if (cJSON_IsArray(tools) && cJSON_GetArraySize(tools) > 0)
        cJSON_AddItemToObject(params, "tools", tools);
    else
        cJSON_Delete(tools);
}

/* params 契约（rpc/dispatch.c parse_params 逐字段判据）：messages 非空
 * 数组、role/content 须字符串、stream 字段不写（daemon 侧默认非流式）、
 * tool_calls 必须是数组、消息数 ≤ MAX_MESSAGES_PER_REQUEST。
 * 不写 max_tokens：输出上限由 llm_d 依 model.yaml 的 max_output 裁决，
 * 客户端写死会覆盖用户配置。 */
static cJSON *params_build(const char *model, const cli_chat_msgbuf_t *buf, int with_tools)
{
    cJSON *params = cJSON_CreateObject();
    if (!params)
        return NULL;
    if (model && model[0])
        cJSON_AddStringToObject(params, "model", model);
    cJSON *messages = cJSON_AddArrayToObject(params, "messages");
    if (!messages || buf_to_json(messages, buf) != 0) {
        cJSON_Delete(params);
        return NULL;
    }
    cJSON_AddNumberToObject(params, "temperature", 0.7);
    if (with_tools)
        add_tools(params);
    return params;
}

/* 单轮对话完成调用。with_tools=0（工具轮次用尽的总结轮）时不携带工具
 * 定义。失败返回非 0：可执行原因已写入 g_cli_gw_err（cli_err_desc 一次性
 * 消费）。 */
static int cli_chat_gw_round(const char *model, const cli_chat_msgbuf_t *buf,
                             int with_tools, llm_response_t **out_resp)
{
    *out_resp = NULL;
    cJSON *params = params_build(model, buf, with_tools);
    if (!params)
        return -1;
    cJSON *root = NULL;
    int ret = cli_gw_json("llm.complete", params, CLI_CHAT_LLM_TIMEOUT_MS, &root);
    if (ret != 0) {
        /* 失败/取消码直通（S-02）：失败时 g_cli_gw_err 已由 cli_gw_call
         * 填写（cli_err_desc 一次性消费）；取消（AIRY_ERR_CANCELED）语义
         * 自明，由 cli_chat_reply 渲染如实告知。 */
        return ret;
    }
    *out_resp = resp_from_json(root);
    cJSON_Delete(root);
    if (!*out_resp) {
        cli_gw_err_set("模型响应解析失败（网关返回格式异常）");
        return -1;
    }
    return 0;
}

/* ── 主流程编排（0.1.19 §459）── */

/* Decision B (2026-08-09): t1-f (B model) 未配置时提示三个配置点，不阻塞
 * （走 provider 默认模型）。2026-08-17：改 cli_trace 输出（日志/stderr），
 * 不渲染进对话列——配置提示是内部运维信息。2026-08-19：model.yaml 的
 * llm.model 默认可满足 t1-f，此时不再提示。每次会话仅提示一次。 */
static void hint_model(const char *t1f_model)
{
    static int s_hint_shown = 0;
    if (s_hint_shown || (t1f_model && t1f_model[0]))
        return;
    s_hint_shown = 1;
    cli_trace("config",
              "t1-f (B model) not configured; set AIRY_MODEL_T1F (local Ollama/vLLM or "
              "cloud API), then AIRY_MODEL_T2 (A) and AIRY_MODEL_T1P (C) as needed. "
              "Chat will use the llm_d default model for now.");
}

/* 消息缓冲：[system] + [语言网关 system] + [相关记忆 system] + history +
 * 本轮输入。工具轮会动态扩展（assistant tool_calls + role="tool" 结果），
 * 所有内容副本由缓冲统一管理。 */
static void buf_build(cli_chat_msgbuf_t *buf, const char *input)
{
    /* 2.2.4 对话记忆读取：相关历史记忆注入 system 上下文（此前对话
     * 路径零记忆，用户"记不住/想不准"的直接根因）。 */
    char mem_sys[768];
    cli_chat_mem_inject_system(input, mem_sys, sizeof(mem_sys));
    cli_msgbuf_push(buf, "system", cli_system_prompt_now(), NULL, NULL, NULL);
    /* 1.3 推理语言网关：语言约束 System Prompt 注入（首条 system 之后）。
     * 约束模型内部推理语言与最终输出语言，从源头抑制语言漂移。 */
    if (g_cli_lang_sys_prompt && g_cli_lang_sys_prompt[0])
        cli_msgbuf_push(buf, "system", g_cli_lang_sys_prompt, NULL, NULL, NULL);
    if (mem_sys[0])
        cli_msgbuf_push(buf, "system", mem_sys, NULL, NULL, NULL);
    for (size_t hi = 0; hi < g_history_count; hi++)
        cli_msgbuf_push(buf, g_history_roles[hi], g_history_contents[hi], NULL, NULL,
                        g_history_reasonings[hi]);
    cli_msgbuf_push(buf, "user", input, NULL, NULL, NULL);
}

/* 失败/取消渲染：数字码对用户无意义；C-01 ③ 下 gateway 路径已把可执行
 * 原因写入 g_cli_gw_err，cli_err_desc 一次性消费。S-02：取消码直通时
 * g_cli_gw_err 未填，如实告知服务端可能仍在推理（服务端可取消登记 0.2.x）。 */
static void fail_render(int ret, int no_text)
{
    int canceled = (ret == AIRY_ERR_CANCELED);
    const char *desc;
    if (canceled)
        desc = "用户中断（Ctrl+C）；服务端推理可能仍在进行，本轮结果将被丢弃";
    else if (no_text)
        desc = "模型未返回文本（可能仅生成了思考内容）";
    else
        desc = cli_chat_err_desc((int)ret);
    char line[384];
    snprintf(line, sizeof(line), "%s：%s", canceled ? "已取消" : "回复失败", desc);
    cli_print_line("Super Agent:对话", line);
}

/* 工具回路：每轮 cli_chat_gw_round → gateway llm.complete 非流式单发。
 * 模型返回 tool_calls → 渲染过程卡片 + 执行 + 回填 → 续轮；不再调用工具
 * → 返回值即最终回复。护栏：轮次上限 CLI_CHAT_TOOL_MAX_ROUNDS。
 * 用尽后撤下工具定义并追加总结提示，保证用户拿到基于已获取信息的完整
 * 回答（此前直接采纳该过渡响应，用户只能看到一行半截文本）。
 * 失败/取消返回 NULL（错误行已在 fail_render 渲染）。 */
static llm_response_t *chat_loop(cli_chat_msgbuf_t *buf, const char *model, int *rounds_out)
{
    int rounds = 0;
    int force_summary = 0;
    for (;;) {
        llm_response_t *resp = NULL;
        int ret = cli_chat_gw_round(model, buf, !force_summary, &resp);
        if (ret != 0 || !resp || resp->choice_count == 0) {
            llm_response_free(resp);
            fail_render(ret, ret == 0);
            return NULL;
        }
        /* 2.1.1.5/2.1.1.6：累计本轮真实 token/费用与思考链（工具轮与
         * 最终轮都计入）。思考链仅在进程内累积（续轮回传 + 诊断日志），
         * 0.1.18 B4 起默认不上屏。 */
        cli_chat_usage_add(resp);
        if (resp->choices[0].reasoning_content)
            cli_chat_reasoning_add(resp->choices[0].reasoning_content);
        int has_tools = resp->choices[0].tool_calls_json && resp->choices[0].tool_calls_json[0];
        if (has_tools && !force_summary && rounds < CLI_CHAT_TOOL_MAX_ROUNDS) {
            if (cli_chat_tool_round(buf, resp) == 0) {
                rounds++;
                llm_response_free(resp);
                continue;
            }
        }
        if (has_tools && !force_summary) {
            force_summary = 1;
            cli_msgbuf_push(buf, "user",
                            "（工具调用轮次已用尽。请仅基于以上已获取的信息给出最终回答，"
                            "不要再调用任何工具。）",
                            NULL, NULL, NULL);
            llm_response_free(resp);
            continue;
        }
        *rounds_out = rounds;
        return resp;
    }
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

    const char *model = cli_chat_t1f_cached();
    cli_trace("chat", "%s start model=%s", CLI_ICON_DIAMOND,
              (model && model[0]) ? model : "default");
    hint_model(model);

    cli_chat_msgbuf_t buf;
    AIRY_MEMSET(&buf, 0, sizeof(buf));
    buf_build(&buf, input);

    /* 0.1.19 直印化：spinner 动画随体验面清退（agentrt-tui 承接），
     * -p 进度走 cli_trace；行式 REPL 静默等待回复。 */
    int rounds = 0;
    llm_response_t *final_resp = chat_loop(&buf, model, &rounds);
    if (!final_resp) {
        cli_msgbuf_free(&buf);
        return;
    }

    /* 2.2.4 对话记忆写入：一轮对话完成且有回复时落盘（用户输入+回复，只记
     * 事实）。0.1.18 B4：思考链不入长期记忆——该库为 CLI 与 TUI 共用，写入
     * 即等于经 system 段回灌到后续所有会话（§12.4 步 3）；诊断留存走
     * airy_reasoning.log（cli_chat_reasoning_persist）。 */
    if (final_resp->choices[0].content)
        cli_chat_mem_record(input, final_resp->choices[0].content);

    /* 收尾：语言网关输出后处理 + 最终渲染 + 历史写入（cli_chat_finalize.c）。
     * C-01 后 stream_mode 恒 0（网关非流式单发）；final_resp 归本函数释放。 */
    cli_chat_reply_finalize(final_resp, input, 0, rounds);

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

const char *cli_chat_think_name(void)
{
    const char *t1f = cli_chat_t1f_cached();
    return (t1f && t1f[0]) ? "Dual Fast Think" : "Dual Think";
}
