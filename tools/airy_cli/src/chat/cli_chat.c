// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat.c
 * @brief airy_cli chat domain: intent split, streaming and replies.
 *
 * 处理所有普通用户对话：意图分辨（启发式 + LLM 兜底）、流式渲染（打字机
 * 预览 + 思考进度 + [code] 归一化）、以及直接回复（超级智能体单 t1-f B 模
 * 型生成，决策 2026-08-09）。
 *
 * 2026-08-27 域拆分（2040 行 → 6 个职责模块）：usage/cost 统计 →
 * cli_chat_usage.c；对话记忆读写 → cli_chat_memory.c；GCCP 逐问交互 →
 * cli_chat_gccp.c；历史缓冲/错误描述/系统提示 → cli_chat_history.c；
 * 聊天工具回路 → cli_chat_tools.c。本文件保留意图分辨、流式渲染与
 * cli_chat_reply 主流程。
 */

#include "cli_internal.h"

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

#define CLI_CLASSIFY_PROMPT                                                    \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：要求执行具体工程" \
    "任务（实现、开发、构建、创建、编写、修复、重构、部署等），需要改动代码、" \
    "文件或系统；普通对话：寒暄、提问、解释、闲聊、搜索/查询/了解实时信息（如" \
    "新闻、天气、资料、概念解释）、读取/查看/编辑单个本地文件（读文件、列目录、" \
    "改文件内容等日常操作），只需回答或联网检索或调用文件工具即可完成，无需"   \
    "进入任务调度管线。只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

/**
  * @brief Ask the LLM to classify input as task or chat (returns 1=task 0=chat -1=failure)
  *
  * Reasoning-model note: the classifier must not be starved of output tokens.
  * With a tiny max_tokens a thinking model (e.g. DeepSeek) spends the whole
  * budget on reasoning_content and emits an empty content, which parses as a
  * failure here and degrades intent routing (2026-08-16: "北京今天天气怎么样"
  * misrouted to the task pipeline this way). 64 tokens leaves room for the
  * JSON verdict after the chain of thought.
  */
static int cli_llm_classify(const char *input)
{
    if (!g_chat_adapter || !input)
        return -1;

    /* llm_message_t carries optional fields (reasoning_content/tool_call_id/
     * tool_calls_json) that build_llm_request_json dereferences; zero the
     * array so unset fields are never garbage pointers (2026-08-16). */
    llm_message_t msgs[2];
    __builtin_memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = "system";
    msgs[0].content = CLI_CLASSIFY_PROMPT;
    msgs[1].role = "user";
    msgs[1].content = input;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = getenv("AIRY_MODEL_T1F");
    cfg.messages = msgs;
    cfg.message_count = 2;
    cfg.temperature = 0.0f;
    cfg.max_tokens = 64;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0 ||
        !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        return -1;
    }

    const char *content = resp->choices[0].content;
    /* The classifier prompt mandates {"type":"task"} / {"type":"chat"}; the
     * JSON verdict key with optional spacing still carries the literal
     * "task" token, so a plain substring match is sufficient and robust.
     * Only the emitted content counts: the chain-of-thought may mention
     * "task" while concluding "chat" (thinking models), so never consult
     * reasoning_content — an empty content degrades to the chat default. */
    int is_task = (content[0] != '\0' && strstr(content, "\"task\"") != NULL);
    llm_response_free(resp);
    return is_task ? 1 : 0;
}

/**
  * @brief Intent routing: fast heuristic check + LLM confirmation
  *
  * Clear task words -> task set; clear chat words -> chat set; ambiguous -> LLM;
  * fails SAFE to the chat set (the super-agent default): a misrouted "task"
  * sent into the task pipeline can stall/act, whereas a chat reply is always
  * harmless and lets the user rephrase. The explicit task_marks fast path
  * still catches unambiguous task commands without any LLM round trip.
  *
  * 启发式词表与优先级（consult > task > chat）见 cli_classify_heuristic
  * （2.5.x 意图分辨，独立纯函数可单测）。
  */
int cli_classify_input(const char *input)
{
    int h = cli_classify_heuristic(input);
    if (h >= 0)
        return h;

    int cls = cli_llm_classify(input);
    return cls >= 0 ? cls : 0;
}

/* -p 模式流式渲染回调：把增量文本直写 stdout（stdout 保持纯净可管道；
 * 工具/进度 chrome 走 stderr，见 cli_trace）。交互模式仍走 spinner +
 * markdown 完整渲染，避免流式下 markdown 标记裸露。
 *
 * 流式归一化：部分模型用 [code]/[/code] 包裹代码而非 markdown 围栏
 * ``` ```。交互模式由 cli_render_markdown 统一识别；-p 流式直出时必须
 * 在此归一化（[code] → ```），否则管道输出裸露标签。跨 chunk 边界
 * （token 被 provider 截断成多个增量）用静态 carry 缓冲拼接。 */
static char s_code_carry[8];
static size_t s_code_carry_len = 0;

/* 交互 TTY 流式：首片到达时擦除 "Connecting..." 提示行（stderr 上的
 * 光标行），避免残留。仅在交互 TTY 流式模式下有意义（-p 不打印提示）。 */
static int s_stream_first_chunk = 0;

/* 交互 TTY 流式状态（2026-08-17）：流式预览直出后记录其物理行数，
 * 完成后擦除预览并完整重绘最终形态（结果不折叠，仅思考链折叠）。
 * 仅最终轮有意义（工具轮的预览保留为过程展示，不擦除）。 */
static size_t g_chat_fold_phys = 0;
/* 预览末尾是否无换行（光标停在最后一行行尾）：擦除时上移行数须少 1，
 * 否则会把预览起点上方那行（消息行）一并覆盖。 */
static int g_chat_fold_tail_no_nl = 0;

static void cli_stream_norm_emit(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        cli_outc(s[i]);
    fflush(stdout);
}

static void cli_stream_norm_flush_carry(void)
{
    if (s_code_carry_len) {
        cli_stream_norm_emit(s_code_carry, s_code_carry_len);
        s_code_carry_len = 0;
    }
}

/* s[0..n) 是否是 [code] 或 [/code] 的严格前缀（不含完整标签本身）？ */
static int cli_code_prefix(const char *s, size_t n)
{
    static const char *const tags[] = {"[code]", "[/code]"};
    if (n == 0 || n >= 7)
        return 0;
    for (size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
        if (n < strlen(tags[t]) && strncmp(s, tags[t], n) == 0)
            return 1;
    }
    return 0;
}

/* 2.3.5/2.3.6：thinking 模型（DeepSeek/Kimi 等）先产思考链再产正文，
 * 思考阶段可长达数十秒。原始推理碎片逐块上屏无阅读价值（用户反馈
 * "太长看不懂"），因此思考期间只显示轻量进度行
 * （[Dual Fast Think] 思考中… N 字 · 耗时，\r 原地刷新），首个
 * content 分片到达即擦除；完整思考链在回复完成后折叠展示。
 * 通道说明：交互模式 stderr 被重定向到日志，实时 UI 反馈（Connecting /
 * 思考进度）与流式正文**同流直写 stdout**（同一 fd 光标同步，\r 覆盖
 * 不留痕）；-p / --json / TUI 下这些反馈被各自前置条件禁用。 */
static cli_actor_t cli_chat_think_actor(void);
static int s_reasoning_progress = 0;      /* 进度行当前可见 */
static int s_reasoning_chars = 0;         /* 思考字数累计 */
static uint64_t s_reasoning_start_ms = 0; /* 思考阶段开始时刻 */

static void cli_chat_reasoning_cb(const char *delta, void *user_data)
{
    (void)user_data;
    /* 仅交互 TTY 流式显示进度；-p / --json / TUI 走各自无进度路径。 */
    if (!delta || g_cli_print_mode || g_cli_json_mode)
        return;
    /* 正文已开始（首片到达）：思考阶段结束，不再刷新进度行——上游
     * reasoning 增量与 content 首片可能交错到达（DeepSeek 流式分块），
     * 否则进度行 \r 覆盖会压到正文首行（2026-08-19 实测竞态）。 */
    if (!s_stream_first_chunk)
        return;
    if (!s_reasoning_start_ms)
        s_reasoning_start_ms = cli_now_ms();
    s_reasoning_chars += (int)strlen(delta);
    s_reasoning_progress = 1;
    uint64_t ms = cli_now_ms() - s_reasoning_start_ms;
    /* 角色名与全站一致：[Dual Slow/Fast/Prof Think]（2.3.14） */
    fprintf(stdout, "\r    %s[%s]%s 思考中… %d 字 · %lu.%lus   ",
            cli_c(CLR_YELLOW), cli_render_actor_name(cli_chat_think_actor()),
            cli_c(CLR_RESET), s_reasoning_chars, (unsigned long)(ms / 1000),
            (unsigned long)(ms % 1000) / 100);
    fflush(stdout);
}

static void cli_chat_reasoning_clear(void)
{
    if (s_reasoning_progress) {
        s_reasoning_progress = 0;
        /* 2K erases the whole line (not just cursor-to-end): a partially
         * rendered progress row must not leak "… 字 · 0.6s" onto the first
         * streaming-reply line (reported debris). */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }
}

static void cli_chat_stream_cb(const char *chunk, void *user_data)
{
    (void)user_data;
    if (!chunk)
        return;
    size_t n = strlen(chunk);
    if (n == 0)
        return;

    /* 首片到达：擦除 "Connecting..." / 思考进度提示行（同位置） */
    if (s_stream_first_chunk) {
        s_stream_first_chunk = 0;
        s_reasoning_progress = 0; /* 进度行已被首片擦除，避免收尾重复擦除 */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }

    /* 跨 chunk：上一片末尾的标签前缀先与本次开头拼接判断。 */
    if (s_code_carry_len) {
        size_t take = 7 - s_code_carry_len;
        if (take > n)
            take = n;
        AIRY_MEMCPY(s_code_carry + s_code_carry_len, chunk, take);
        s_code_carry_len += take;
        chunk += take;
        n -= take;
        if (strncmp(s_code_carry, "[code]", 6) == 0 ||
            strncmp(s_code_carry, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            s_code_carry_len = 0;
        } else {
            cli_stream_norm_flush_carry();
        }
        if (n == 0)
            return;
    }

    size_t i = 0;
    while (i < n) {
        if (n - i >= 6 && strncmp(chunk + i, "[code]", 6) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 6;
            continue;
        }
        if (n - i >= 7 && strncmp(chunk + i, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 7;
            continue;
        }
        /* chunk 末尾的标签前缀（被 token 截断）→ 缓存，不提前输出。 */
        if (i + 6 >= n && cli_code_prefix(chunk + i, n - i)) {
            AIRY_MEMCPY(s_code_carry, chunk + i, n - i);
            s_code_carry_len = n - i;
            return;
        }
        cli_outc(chunk[i]);
        i++;
    }
    fflush(stdout);
}

/* 2.3.14 (2026-08-17)：对话思考链来自 t1-f（context arbiter）模型，
 * 实时思考标签为 [Dual Fast Think]；未配置（走 llm_d 默认模型）时
 * 回落通用 [Dual Think]。2026-08-19：t1-f 配置统一来自
 * cli_think_cfg_load（env > model.yaml think 段 > llm.model 默认），
 * 与 think_d 实际生效模型一致；静态缓存避免每帧进度行重复读文件。 */
static const char *cli_chat_t1f_cached(void)
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

static cli_actor_t cli_chat_think_actor(void)
{
    const char *t1f = cli_chat_t1f_cached();
    return (t1f && t1f[0]) ? CLI_ACTOR_DUAL_FAST_THINK : CLI_ACTOR_DUAL_THINK;
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
  * cli_chat_usage.c / cli_chat_memory.c / cli_chat_tools.c（域拆分）。
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

    /* 交互 TTY 走流式（打字机预览，完成后折叠/重绘最终形态）；
     * TUI、--json 与 -p 保持非流式（markdown 完整渲染进历史 / 结构化
     * 输出 / 纯 stdout 最终答案）。-p 非流式还避免工具轮之间模型的
     * 过程叙述混入 stdout——脚本模式只消费最终回答（2026-08-17）。
     * 交互流式不再使用 spinner——打字机即进度指示。 */
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
            /* 交互 TTY 流式：计量本轮直出预览的行数，完成后擦除重绘
             * 最终形态（短回复 markdown 精修 / 长回复折叠）。-p 流式
             * 直出供管道消费，不计量不重绘。 */
            cli_line_meter_t meter;
            int folding = !g_cli_print_mode;
            if (folding)
                cli_render_meter_begin(&meter);
            /* 连接反馈：流式前显示连接状态（与正文同流 stdout），首片
             * 到达时擦除。避免连接阶段（RPC 握手 + 模型推理首 token）
             * 用户无任何反馈。 */
            s_stream_first_chunk = !g_cli_print_mode;
            if (s_stream_first_chunk) {
                fprintf(stdout, "    %s●%s Connecting…\r",
                        cli_c(CLR_DIM), cli_c(CLR_RESET));
                fflush(stdout);
            }
            /* 思考阶段进度：每轮重置计数，reasoning 增量经回调实时上屏 */
            s_reasoning_start_ms = 0;
            s_reasoning_chars = 0;
            s_reasoning_progress = 0;
            ret = llm_svc_adapter_complete_stream(g_chat_adapter, &cfg,
                                                  cli_chat_stream_cb, NULL,
                                                  cli_chat_reasoning_cb, NULL,
                                                  &resp);
            /* 思考进度行残留清理（reasoning-only 或异常中断场景） */
            cli_chat_reasoning_clear();
            /* 流式收尾：flush 可能残留的 [code] 前缀 carry（流提前结束时
             * 最后一片未触达标签判定的字节）。 */
            cli_stream_norm_flush_carry();
            /* 流结束但无首片到达（连接失败/空响应）：擦除 Connecting 行 */
            if (s_stream_first_chunk) {
                s_stream_first_chunk = 0;
                fputs("\r\033[2K", stdout);
                fflush(stdout);
            }
            if (folding) {
                g_chat_fold_phys = cli_render_meter_phys(&meter);
                g_chat_fold_tail_no_nl = (meter.row_len > 0) ? 1 : 0;
                cli_render_meter_end(&meter);
            }
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
            /* 层级修复（2026-08-20）：工具轮中间叙述（"第一次搜索不太
             * 直接…"）流式直出后与工具卡片拼接在同一行，层级混乱。
             * 先擦除本轮流式直出的中间叙述预览，折叠为一行弱化摘要，
             * 工具卡片从新行独立渲染——过程叙述 / 工具卡片 / 最终
             * 结果三层清晰分隔（2.3.12 层级编排）。 */
            if (stream_mode && g_chat_fold_phys > 0 && cli_term_is_tty()) {
                fflush(stdout);
                size_t up = g_chat_fold_phys;
                if (g_chat_fold_tail_no_nl && up > 0)
                    up -= 1;
                /* CUU 参数 0 在 ANSI 中等于默认值 1（上移 1 行）！
                 * up=0 时必须避免 `\033[0A`（会误删上一行内容），
                 * 改用回车 + 清当前行。 */
                char erase[32];
                int en;
                if (up > 0)
                    en = snprintf(erase, sizeof(erase), "\033[%zuA\r\033[J", up);
                else
                    en = snprintf(erase, sizeof(erase), "\r\033[2K");
                if (en > 0)
                    fwrite(erase, 1, (size_t)en, stdout);
                fflush(stdout);
                g_chat_fold_phys = 0;
                g_chat_fold_tail_no_nl = 0;
                /* 中间叙述折叠为一行弱化摘要（保留首行 + 折叠尾） */
                const char *mid = resp->choices[0].content;
                if (mid && mid[0]) {
                    cli_render_role_line(CLI_ROLE_TRACE, cli_chat_think_actor(),
                                         "分析", NULL);
                    cli_render_collapsed(mid, 2, 1, 1);
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

    /* 最终回复渲染：
     *   --json  结构化 JSON（Codex exec 约定）
     *   -p      纯文本（Claude Code -p / Codex exec 约定；流式已直出）
     *   交互    TTY 流式：擦除预览后 markdown 精修 / 长回复折叠；
     *           TUI/非流式：markdown 渲染 + 折叠区（浏览展开） */
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

    llm_response_free(final_resp);
    AIRY_FREE(lg_final);
#ifdef AIRY_HAS_CJSON
    cli_msgbuf_free(&buf);
#else
    AIRY_FREE(msgs);
#endif
}
