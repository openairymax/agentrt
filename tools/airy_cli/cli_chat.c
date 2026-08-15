// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat.c
 * @brief airy_cli chat domain: GCCP callback, session history, intent split and replies.
 *
 * Handles all normal user chat: GCCP four-question interaction callback
 * (two conditional-compilation variants), FIFO chat history buffer,
 * heuristic + LLM task/chat intent classification, and direct replies
 * as the super agent (single t1-f B model generation, decision 2026-08-09).
 */

#include "cli_internal.h"

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

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#ifdef AIRY_HAS_CJSON

/* LLM 上一步生成的追问（跨循环轮次保留；每次提问前清零，未生成则自然退出） */
static char g_last_step_q[512];
static char g_last_step_hint[256];

/**
  * @brief Ask the user four questions and collect answers (returns answer JSON, OWNER; freed by the engine)
  *
  * Question IDs (endpoint/start/bottleneck/audience) are the answer JSON keys,
  * matching the Q1-Q4 fields in gccp.h one-to-one.
  *
  * 逐问交互（2026-08-15）：不再一次性抛全部问题。每次只展示一个问题，
  * 用户回答后调用 airy_gccp_step() 让 LLM 对已答内容思考——决定是收敛
  * （done=1）还是根据已答内容生成下一个针对性追问。LLM 不可用时降级为
  * 逐问机械推进（至少不是批量）；用户跳过某问（空行）即视为意愿不足，
  * 直接收敛不纠缠。追问上限 = 问题数 + 4，防 LLM 无限追问。
 */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    /* One-shot server mode (-p): no interactive confirmation; return empty
     * answers so the engine proceeds with its defaults (non-blocking).
     * 注意：先序列化再释放对象，避免 cJSON 对象泄漏。 */
    if (g_cli_print_mode) {
        cJSON *empty = cJSON_CreateObject();
        if (!empty)
            return NULL;
        char *empty_json = cJSON_PrintUnformatted(empty);
        cJSON_Delete(empty);
        return empty_json;
    }

    cli_render_role_line(CLI_ROLE_SUPER_THINK, CLI_ACTOR_SUPER_THINK, "gccp",
                         "Intent confirmation: I will ask one question at a time (Enter to skip):");
    /* The planning spinner may be animating; pause it so the questions
     * render on clean lines, then resume after the answers. */
    cli_spinner_pause();
    cJSON *answers = cJSON_CreateObject();
    if (!answers)
        return NULL;

    /* 原指令从 prefill 的 raw_prompt 取（probe 阶段已保存） */
    const char *raw = (probe->prefill && probe->prefill->raw_prompt) ? probe->prefill->raw_prompt :
                                                                       "（无原始指令）";
    size_t raw_len = strlen(raw);

    /* 追问上限：基础问题数 + 4，防止 LLM 无限追问（用户主导交互的护栏） */
    size_t max_rounds = probe->question_count + 4;

    /* 待问队列：先按 probe 顺序走，之后由 LLM 动态生成追问。
     * 每轮索引 round 指向 probe->questions[round]（越界即表示进入 LLM 追问区）。 */
    for (size_t round = 0; round < max_rounds; round++) {
        /* 本轮问题：probe 原始问题 or LLM 上一步生成的追问 */
        airy_gccp_question_t local_q;
        __builtin_memset(&local_q, 0, sizeof(local_q));
        const airy_gccp_question_t *q = NULL;
        if (round < probe->question_count) {
            q = &probe->questions[round];
        } else {
            /* 进入 LLM 追问区：使用上一轮 step 生成的问题（question 非空才有意义） */
            if (!g_last_step_q[0])
                break; /* 没有可用追问：结束 */
            snprintf(local_q.id, sizeof(local_q.id), "followup%zu", round);
            AIRY_STRNCPY_TERM(local_q.question, g_last_step_q, sizeof(local_q.question));
            AIRY_STRNCPY_TERM(local_q.hint, g_last_step_hint, sizeof(local_q.hint));
            local_q.required = 0;
            q = &local_q;
        }

        cli_outf("  %sQ%zu%s [%s]%s %s\n", cli_c(CLR_CYAN), round + 1, cli_c(CLR_RESET), q->id,
                 q->required ? "（必答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      %s提示：%s%s\n", cli_c(CLR_GREEN), q->hint, cli_c(CLR_RESET));
        if (!cli_tui_active(cli_tui_get_default())) {
            /* Line-oriented mode: print an inline prompt. In full-screen TUI
             * mode the bottom input line is the prompt itself. */
            cli_outf("  %s>%s ", cli_c(CLR_GREEN), cli_c(CLR_RESET));
            fflush(stdout);
        }

        char line[1024];
        size_t line_len = 0;
        if (!cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len))
            break;
        int answered = (line_len > 0);
        if (answered)
            cJSON_AddStringToObject(answers, q->id, line);
        else
            break; /* 用户跳过：收敛，不再追问（不纠缠） */

        /* 让 LLM 对已答内容思考，决定收敛或生成下一个追问。无 LLM 时
         * airy_gccp_step 降级为"继续下一题"，交互仍逐问推进。 */
        g_last_step_q[0] = '\0';
        g_last_step_hint[0] = '\0';
        char *answers_json = cJSON_PrintUnformatted(answers);
        if (!answers_json)
            break;
        airy_gccp_step_t step;
        airy_err_t serr = airy_gccp_step(g_chat_adapter, NULL, raw, raw_len, answers_json, 1, NULL,
                                         &step);
        cJSON_free(answers_json);
        if (serr != AIRY_SUCCESS)
            break;
        /* 展示 LLM 对上一答的思考（渐进披露，非阻塞） */
        if (step.reasoning[0])
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "gccp", step.reasoning);
        if (step.done)
            break; /* 已收敛 */
        if (step.question[0]) {
            AIRY_STRNCPY_TERM(g_last_step_q, step.question, sizeof(g_last_step_q));
            AIRY_STRNCPY_TERM(g_last_step_hint, step.hint, sizeof(g_last_step_hint));
        }
    }

    char *json = cJSON_PrintUnformatted(answers);
    cJSON_Delete(answers);
    cli_spinner_resume();
    /* 阶段 4：GCCP 意图确认 → 决策链事件（preflight，cognition 角色）。
     * 仅记录结构化信号（问题数），用户回答原文不进事件流（隐私 + JSON 转义安全）。 */
    if (g_cli_hall_store) {
        char ev[256];
        snprintf(ev, sizeof(ev), "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                 probe->question_count);
        airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                              AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
    }
    return json;
}

#else /* !AIRY_HAS_CJSON */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    /* One-shot server mode (-p): non-blocking, empty answers (defaults). */
    if (g_cli_print_mode) {
        char *json = (char *)AIRY_MALLOC(3);
        if (!json)
            return NULL;
        json[0] = '{';
        json[1] = '}';
        json[2] = '\0';
        return json;
    }

    cli_spinner_pause();
    size_t cap = 512;
    for (size_t i = 0; i < probe->question_count; i++)
        cap += strlen(probe->questions[i].id) + 1024;
    char *json = (char *)AIRY_MALLOC(cap);
    if (!json)
        return NULL;
    char *p = json;
    int n = snprintf(p, cap, "{");
    p += n;
    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        cli_outf("  Q%zu [%s]%s %s\n", i + 1, q->id, q->required ? "（必答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      提示：%s\n", q->hint);
        if (!cli_tui_active(cli_tui_get_default()))
            cli_outf("  > ");
        fflush(stdout);
        char line[1024];
        size_t line_len = 0;
        if (!cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len))
            break;
        if (i > 0)
            *p++ = ',';
        n = snprintf(p, cap - (size_t)(p - json), "\"%s\":\"%s\"", q->id, line);
        p += n;
    }
    snprintf(p, cap - (size_t)(p - json), "}");
    cli_spinner_resume();
    /* 阶段 4：GCCP 意图确认 → 决策链事件（同 cJSON 分支，仅记录结构化信号） */
    if (g_cli_hall_store) {
        char ev[256];
        snprintf(ev, sizeof(ev), "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                 probe->question_count);
        airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                              AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
    }
    return json;
}

#endif /* AIRY_HAS_CJSON */

char *g_history_roles[CLI_HISTORY_MAX_MSGS];
char *g_history_contents[CLI_HISTORY_MAX_MSGS];
size_t g_history_count = 0;

static size_t cli_history_capacity(void)
{
    const char *env = getenv("AIRY_CHAT_HISTORY_ROUNDS");
    if (env && env[0] != '\0') {
        long rounds = strtol(env, NULL, 10);
        if (rounds >= 1 && rounds <= 30)
            return (size_t)rounds * 2;
    }
    return 30;
}

static void cli_history_add(const char *role, const char *content)
{
    if (!role || !content)
        return;
    size_t cap = cli_history_capacity();
    if (g_history_count >= cap) {

        AIRY_FREE(g_history_roles[0]);
        AIRY_FREE(g_history_contents[0]);
        AIRY_FREE(g_history_roles[1]);
        AIRY_FREE(g_history_contents[1]);
        AIRY_MEMMOVE(&g_history_roles[0], &g_history_roles[2],
                     (g_history_count - 2) * sizeof(char *));
        AIRY_MEMMOVE(&g_history_contents[0], &g_history_contents[2],
                     (g_history_count - 2) * sizeof(char *));
        g_history_count -= 2;
    }
    g_history_roles[g_history_count] = AIRY_STRDUP(role);
    g_history_contents[g_history_count] = AIRY_STRDUP(content);
    g_history_count++;
}

void cli_history_clear(void)
{
    for (size_t i = 0; i < g_history_count; i++) {
        AIRY_FREE(g_history_roles[i]);
        AIRY_FREE(g_history_contents[i]);
    }
    g_history_count = 0;
}

#define CLI_SYSTEM_PROMPT                                                       \
    "你是 AgentRT，一个智能体操作系统与超级智能体助手。请用中文简洁、友好地"    \
    "回答用户的问题；需要执行具体工程任务时，请引导用户描述为任务指令。\n"      \
    "你具备两个联网工具：web_search（搜索引擎，参数 query/max_results）与 "    \
    "web_fetch（抓取网页正文，参数 url）。当问题涉及实时信息、最新新闻、时效"   \
    "性数据，或你知识截止日期（2025-05）之后发生的事件，必须调用 web_search "  \
    "获取最新结果，必要时再用 web_fetch 深入抓取；不要凭过时知识硬答。"        \
    "工具结果返回后，基于结果组织回答并标注信息时效。"

#define CLI_CLASSIFY_PROMPT                                                    \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：包含可执行动作"   \
    "（实现、开发、构建、创建、编写、修复、重构、部署、测试等），要求执行具体" \
    "工程任务；普通对话：寒暄、提问、解释、闲聊，只需回答无需执行。"           \
    "只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

/* ============================================================================
 * 聊天工具回路（Claude Code / Codex tool-use 范式，2026-08-15）
 *
 * 日常对话同样可以调用工具：模型看到 web_search / web_fetch 的定义，回答时
 * 可返回 tool_calls；CLI 执行工具、把结果以 role="tool" 消息回填，再请求
 * 一轮，直到模型不再调用工具，最后流式渲染最终回复。工具行为（Bing/DDG 抓
 * 取）由 tool_d 的 builtin_net.c 提供（真实实现，非桩），CLI 只做协议编排：
 * 工具执行细节在 tool_d，对话策略在 CLI（机制/策略分离，Linux 哲学）。
 *
 * 安全护栏：工具轮上限 CLI_CHAT_TOOL_MAX_ROUNDS（防模型无限工具循环）；
 * 工具结果按折叠摘要渲染到屏幕（全量文本仍回填模型上下文）。
 * ============================================================================ */
#define CLI_CHAT_TOOL_MAX_ROUNDS 4
#define CLI_CHAT_TOOL_RESULT_CAP 12000 /* 单工具结果回填模型的最大字节数 */

#ifdef AIRY_HAS_CJSON
/* tool_result_t 定义于 daemons/tool_d/include/tool_service.h（CLI 已含该
 * include 路径）；web_search_tool / web_fetch_tool 声明于 tool_d/src/
 * tool_builtin_internal.h（src 目录不对外），已编译进 airy_tool_service
 * 库（builtin_net.c），此处 extern 声明直接链接调用。 */
#include "tool_service.h"

#ifdef _WIN32
/* Windows：builtin_net.c 的 web_search_tool 仅在 POSIX 编译（#ifndef
 * _WIN32），此处提供与 web_fetch_tool 平台降级一致的实现——真实报告平台
 * 能力边界（网络工具在 Windows 暂缺 curl 子进程实现），非桩函数。 */
int web_search_tool(const char *params_json, tool_result_t *res)
{
    (void)params_json;
    if (!res)
        return AIRY_ERR_INVALID_PARAM;
    res->error = AIRY_STRDUP("web_search is not supported on this platform");
    return AIRY_ERR_NOT_SUPPORTED;
}
#else
int web_search_tool(const char *params_json, tool_result_t *res);
#endif
int web_fetch_tool(const char *params_json, tool_result_t *res);

/* OpenAI function-calling schema（聊天工具回路）：web_search + web_fetch。
 * 与 tool_d builtin_net.c 的实现一一对应；工具行为 SSoT 在 tool_d。 */
static const char *CLI_CHAT_TOOLS_JSON =
    "["
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_search\","
    "\"description\":\"Search the web for up-to-date information relevant to "
    "the user's question. Returns ranked result titles, URLs and snippets.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
    "\"max_results\":{\"type\":\"integer\",\"description\":\"Max result count, "
    "1-8\",\"minimum\":1,\"maximum\":8}},"
    "\"required\":[\"query\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_fetch\","
    "\"description\":\"Fetch and read the text content of a web page by URL.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"Full http(s) URL\"}},"
    "\"required\":[\"url\"]}}}"
    "]";

/* 动态消息缓冲：工具轮需要逐步追加 assistant tool_calls 与 role="tool"
 * 结果消息。content / tool_call_id / tool_calls_json 的副本由 owned 数组
 * 统一管理，msgbuf_free 时一并释放。 */
typedef struct {
    llm_message_t *msgs;
    size_t count;
    size_t cap;
    char **owned;
    size_t owned_count;
    size_t owned_cap;
} cli_chat_msgbuf_t;

static void cli_msgbuf_free(cli_chat_msgbuf_t *b)
{
    if (!b)
        return;
    for (size_t i = 0; i < b->owned_count; i++)
        AIRY_FREE(b->owned[i]);
    AIRY_FREE(b->owned);
    AIRY_FREE(b->msgs);
    AIRY_MEMSET(b, 0, sizeof(*b));
}

static const char *cli_msgbuf_own(cli_chat_msgbuf_t *b, const char *s)
{
    if (!s)
        return NULL;
    char *copy = AIRY_STRDUP(s);
    if (!copy)
        return NULL;
    if (b->owned_count >= b->owned_cap) {
        size_t new_cap = b->owned_cap ? b->owned_cap * 2 : 16;
        char **grown = (char **)AIRY_REALLOC(b->owned, new_cap * sizeof(char *));
        if (!grown) {
            AIRY_FREE(copy);
            return NULL;
        }
        b->owned = grown;
        b->owned_cap = new_cap;
    }
    b->owned[b->owned_count++] = copy;
    return copy;
}

static void cli_msgbuf_push(cli_chat_msgbuf_t *b, const char *role, const char *content,
                            const char *tool_call_id, const char *tool_calls_json,
                            const char *reasoning_content)
{
    if (b->count >= b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 16;
        llm_message_t *grown = (llm_message_t *)AIRY_REALLOC(b->msgs, new_cap * sizeof(llm_message_t));
        if (!grown)
            return;
        b->msgs = grown;
        b->cap = new_cap;
    }
    llm_message_t *m = &b->msgs[b->count++];
    m->role = role;
    m->content = cli_msgbuf_own(b, content);
    m->tool_call_id = cli_msgbuf_own(b, tool_call_id);
    m->tool_calls_json = cli_msgbuf_own(b, tool_calls_json);
    /* DeepSeek thinking mode requires the assistant turn's reasoning_content
     * to be echoed verbatim on tool-loop re-send (else HTTP 400). */
    m->reasoning_content = cli_msgbuf_own(b, reasoning_content);
}

/* 执行一个工具调用，返回回填模型的 result JSON（OWNER，AIRY_FREE）。 */
static char *cli_chat_exec_tool(const char *tool_id, const char *args_json, int *out_ok)
{
    if (!tool_id || !args_json) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"missing tool arguments\"}");
    }

    tool_result_t *res = (tool_result_t *)AIRY_CALLOC(1, sizeof(tool_result_t));
    if (!res) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    }

    int rc = -1;
    if (strcmp(tool_id, "web_search") == 0)
        rc = web_search_tool(args_json, res);
    else if (strcmp(tool_id, "web_fetch") == 0)
        rc = web_fetch_tool(args_json, res);

    int ok = (rc == 0 && res->success);
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        tool_result_free(res);
        if (out_ok)
            *out_ok = 0;
        return NULL;
    }
    cJSON_AddBoolToObject(root, "ok", ok ? 1 : 0);
    if (ok && res->output) {
        cJSON_AddStringToObject(root, "output", res->output);
    } else {
        cJSON_AddStringToObject(root, "error", res->error ? res->error : "tool failed");
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    tool_result_free(res);
    if (out_ok)
        *out_ok = ok;
    return json;
}

/* 解析一轮 LLM 返回的 tool_calls，渲染 + 执行 + 回填消息缓冲。
 * 返回 0 = 本轮已消费（调用方应继续下一轮）；-1 = 终止（解析失败/无工具）。 */
static int cli_chat_tool_round(cli_chat_msgbuf_t *b, const llm_response_t *resp)
{
    if (!resp || resp->choice_count == 0 || !resp->choices[0].tool_calls_json)
        return -1;

    cJSON *calls = cJSON_Parse(resp->choices[0].tool_calls_json);
    if (!calls || !cJSON_IsArray(calls)) {
        if (calls)
            cJSON_Delete(calls);
        return -1;
    }
    size_t n = (size_t)cJSON_GetArraySize(calls);
    if (n == 0) {
        cJSON_Delete(calls);
        return -1;
    }

    /* assistant 消息：保留原 content 与 reasoning_content，附 tool_calls
     * （OpenAI 要求续轮必需 tool_calls；DeepSeek thinking 要求回传
     * reasoning_content，否则 tool 续轮 HTTP 400） */
    cli_msgbuf_push(b, "assistant", resp->choices[0].content, NULL,
                    resp->choices[0].tool_calls_json,
                    resp->choices[0].reasoning_content);

    cJSON *call = NULL;
    cJSON_ArrayForEach(call, calls)
    {
        cJSON *fn = cJSON_GetObjectItem(call, "function");
        cJSON *id = cJSON_GetObjectItem(call, "id");
        const char *name = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "name")->valuestring : NULL;
        const char *args = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "arguments")->valuestring : NULL;
        const char *call_id = (id && cJSON_IsString(id)) ? id->valuestring : "call_unknown";
        if (!name)
            name = "unknown";

        /* 工具调用卡片：⛏ web_search(...)（Claude Code tool-use 范式） */
        cli_render_tool_use(name, args);

        int ok = 0;
        char *result_json = cli_chat_exec_tool(name, args ? args : "{}", &ok);
        if (!result_json)
            result_json = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool failed\"}");

        /* 折叠摘要渲染（全量文本已回填模型上下文） */
        const char *detail = NULL;
#ifdef AIRY_HAS_CJSON
        cJSON *rroot = cJSON_Parse(result_json);
        if (rroot) {
            cJSON *out = cJSON_GetObjectItem(rroot, "output");
            cJSON *err = cJSON_GetObjectItem(rroot, "error");
            detail = (ok && out && cJSON_IsString(out)) ? out->valuestring
                     : (err && cJSON_IsString(err)) ? err->valuestring
                                                    : NULL;
            cJSON_Delete(rroot);
        }
#else
        detail = ok ? NULL : result_json;
#endif
        cli_render_tool_result(name, detail, ok);

        /* role="tool" 消息回填：携带匹配的 tool_call_id；超长结果在 UTF-8
         * 字符边界截断，防止上下文无界膨胀（web_fetch 页面可能很大）。 */
        char trunc_buf[CLI_CHAT_TOOL_RESULT_CAP + 1];
        const char *feed = result_json;
        size_t rl = strlen(result_json);
        if (rl > CLI_CHAT_TOOL_RESULT_CAP) {
            size_t n = CLI_CHAT_TOOL_RESULT_CAP;
            while (n > 0 && ((unsigned char)result_json[n] & 0xC0) == 0x80)
                n--;
            AIRY_MEMCPY(trunc_buf, result_json, n);
            trunc_buf[n] = '\0';
            feed = trunc_buf;
        }
        cli_msgbuf_push(b, "tool", feed, call_id, NULL, NULL);
        AIRY_FREE(result_json);
    }
    cJSON_Delete(calls);
    return 0;
}

#endif /* AIRY_HAS_CJSON */

/**
  * @brief Ask the LLM to classify input as task or chat (returns 1=task 0=chat -1=failure)
  */
static int cli_llm_classify(const char *input)
{
    if (!g_chat_adapter || !input)
        return -1;

    llm_message_t msgs[2];
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
    cfg.max_tokens = 16;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0 ||
        !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        return -1;
    }

    const char *content = resp->choices[0].content;
    int is_task = (strstr(content, "\"task\"") != NULL);
    llm_response_free(resp);
    return is_task ? 1 : 0;
}

/**
  * @brief Intent routing: fast heuristic check + LLM confirmation
 *
  * Clear task words -> task set; clear chat words -> chat set; ambiguous -> LLM;
  * falls back to the task set when the LLM is unavailable.
 */
int cli_classify_input(const char *input)
{
    static const char *const task_marks[] = {
        /* 中文命令式 */
        "实现", "开发", "构建", "创建", "编写", "修复", "重构",
        "部署", "设计", "添加", "支持", "优化", "迁移", "集成",
        "写一个", "写一", "做一个", "实现一个", "帮我实现", "开发一个",
        "生成", "运行", "测试", "检查", "分析", "调研", "搜索",
        "启动", "停止", "安装", "配置", "删除", "更新", "下载",
        /* 英文命令式（Claude Code/Codex 约定：命令式动词即任务） */
        "create ", "create a", "create an", "write ", "write a", "write an",
        "implement ", "implement a", "implement an", "build ", "build a",
        "fix ", "fix a", "refactor ", "add ", "add a", "add an",
        "support ", "optimize ", "migrate ", "integrate ", "update ",
        "remove ", "delete ", "run ", "run a", "install ", "configure ",
        "test ", "search ", "research ", "analyze ", "generate ", "generate a",
        "deploy ", "start ", "stop ", "download ", "rename ", "move ",
        "make a", "make an", "make ", "change ", "modify ", "convert ",
    };
    static const char *const chat_marks[] = {
        "你好", "谢谢", "再见", "你是谁", "介绍一下", "解释", "为什么",
        "讲讲", "聊聊", "帮助", "请问", "你好呀", "hello", "hi",
        "thanks", "thank you", "bye", "who are you", "what is", "what are",
        "why ", "explain ", "tell me", "help me", "how do i", "how to",
    };

    for (size_t i = 0; i < sizeof(task_marks) / sizeof(task_marks[0]); i++) {
        if (strstr(input, task_marks[i]))
            return 1;
    }
    for (size_t i = 0; i < sizeof(chat_marks) / sizeof(chat_marks[0]); i++) {
        if (strstr(input, chat_marks[i]))
            return 0;
    }

    int cls = cli_llm_classify(input);
    return cls >= 0 ? cls : 1;
}

/**
  * @brief Chat-set handling: reply to the user directly as the super agent
  *
  * Decision A (2026-08-09): daily chat is generated and routed by the B model
  * (t1-f); no full dual-thinking loop (t2/t1-f/t1-p critique). Single t1-f
  * model replies (AIRY_MODEL_T1F; falls back to the provider default).
  *
  * Decision C (2026-08-15): the chat turn is a tool loop. The model sees the
  * web_search / web_fetch tools and may return tool_calls; the CLI executes
  * them (real implementations in tool_d builtin_net), feeds the results back
  * as role="tool" messages, and repeats until the model answers without
  * tools. Rendering follows the Claude Code tool-use convention: tool cards
  * (⛏ name + folded result) during the loop, then the final reply rendered
  * as markdown. Requests are non-streaming because llm_d's streaming path
  * does not (yet) surface tool_calls (protocol limitation); the typewriter
  * dynamism lives in the task/progress path (spinner, progress bars, task
  * lines, tool cards).
  */

void cli_chat_reply(const char *input)
{
    if (!g_chat_adapter) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "chat",
                             "Chat unavailable (llm_d not connected).");
        return;
    }

    const char *t1f_model = getenv("AIRY_MODEL_T1F");

    /* Decision B (2026-08-09): config reminder - t1-f (B model) activates first.
      * If unset, hint the three config points and order without blocking (provider default).
      * The hint prints once per session only: repeating it on every turn floods the
      * conversation with the same advisory line (Claude Code keeps such setup hints
      * out of the chat column entirely). */
    static int s_t1f_hint_shown = 0;
    if (!g_cli_print_mode && !s_t1f_hint_shown && (!t1f_model || !t1f_model[0])) {
        s_t1f_hint_shown = 1;
        cli_render_role_line(
            CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "config",
            "t1-f (B model) not configured; set AIRY_MODEL_T1F (local Ollama/vLLM or "
            "cloud API), then AIRY_MODEL_T2 (A) and AIRY_MODEL_T1P (C) as needed. "
            "Chat will use the llm_d default model for now.");
    }

#ifdef AIRY_HAS_CJSON
    /* 消息缓冲：[system] + history + [current input]；工具轮会动态扩展
     * （assistant tool_calls + role="tool" 结果），所有内容副本由缓冲统一管理。 */
    cli_chat_msgbuf_t buf;
    AIRY_MEMSET(&buf, 0, sizeof(buf));
    cli_msgbuf_push(&buf, "system", CLI_SYSTEM_PROMPT, NULL, NULL, NULL);
    for (size_t hi = 0; hi < g_history_count; hi++)
        cli_msgbuf_push(&buf, g_history_roles[hi], g_history_contents[hi], NULL, NULL, NULL);
    cli_msgbuf_push(&buf, "user", input, NULL, NULL, NULL);

    /* 交互模式的"思考中"状态行（spinner；-p/--json 抑制 chrome）。 */
    int spinner_on = !g_cli_print_mode && !g_cli_json_mode;
    if (spinner_on) {
        char think_title[128];
        snprintf(think_title, sizeof(think_title), "Thinking (%s)",
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    /* 工具回路：非流式 complete（llm_d 流式路径暂不返回 tool_calls）。
     * 模型返回 tool_calls → 渲染卡片 + 执行 + 回填 → 续轮；
     * 不再调用工具 → final_resp 即最终回复。护栏：轮次上限。 */
    llm_response_t *final_resp = NULL;
    int tool_rounds = 0;
    for (;;) {
        llm_request_config_t cfg;
        __builtin_memset(&cfg, 0, sizeof(cfg));
        cfg.model = t1f_model;
        cfg.messages = buf.msgs;
        cfg.message_count = buf.count;
        cfg.temperature = 0.7f;
        cfg.max_tokens = 2048;
        cfg.tools_json = CLI_CHAT_TOOLS_JSON;

        llm_response_t *resp = NULL;
        int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
        if (ret != 0 || !resp || resp->choice_count == 0) {
            if (resp)
                llm_response_free(resp);
            if (spinner_on)
                cli_spinner_stop(0, "reply failed");
            char line[128];
            snprintf(line, sizeof(line), "Reply failed (err=%d).", ret);
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "chat", line);
            cli_msgbuf_free(&buf);
            return;
        }
        int has_tools =
            resp->choices[0].tool_calls_json && resp->choices[0].tool_calls_json[0];
        if (has_tools && tool_rounds < CLI_CHAT_TOOL_MAX_ROUNDS &&
            cli_chat_tool_round(&buf, resp) == 0) {
            tool_rounds++;
            llm_response_free(resp);
            continue;
        }
        final_resp = resp;
        break;
    }

    if (spinner_on)
        cli_spinner_stop(1, NULL);
#else
    /* 无 cJSON 平台：固定消息数组 + 单轮非流式（不带工具），保持纯对话。 */
    size_t msg_n = g_history_count + 2;
    llm_message_t *msgs = (llm_message_t *)AIRY_MALLOC(msg_n * sizeof(llm_message_t));
    if (!msgs) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "chat",
                             "Out of memory, cannot reply.");
        return;
    }
    size_t mi = 0;
    msgs[mi].role = "system";
    msgs[mi].content = CLI_SYSTEM_PROMPT;
    mi++;
    for (size_t hi = 0; hi < g_history_count; hi++) {
        msgs[mi].role = g_history_roles[hi];
        msgs[mi].content = g_history_contents[hi];
        mi++;
    }
    msgs[mi].role = "user";
    msgs[mi].content = input;
    mi++;

    int spinner_on = !g_cli_print_mode && !g_cli_json_mode;
    if (spinner_on) {
        char think_title[128];
        snprintf(think_title, sizeof(think_title), "Thinking (%s)",
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
        snprintf(line, sizeof(line), "Reply failed (err=%d).", ret);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "chat", line);
        AIRY_FREE(msgs);
        return;
    }
    if (spinner_on)
        cli_spinner_stop(1, NULL);
#endif /* AIRY_HAS_CJSON */

    const char *final_content = (final_resp->choices && final_resp->choice_count > 0 &&
                                 final_resp->choices[0].content)
                                    ? final_resp->choices[0].content
                                    : "";

    /* 最终回复渲染：
     *   --json  结构化 JSON（Codex exec 约定）
     *   -p      纯文本（Claude Code -p / Codex exec 约定）
     *   交互    markdown 完整渲染（角色行 [Super Agent]） */
    if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "role", "super_agent");
        cJSON_AddStringToObject(root, "type", "chat");
        if (final_content[0])
            cJSON_AddStringToObject(root, "content", final_content);
        else
            cJSON_AddStringToObject(root, "error", "reply failed");
        char *js = cJSON_PrintUnformatted(root);
        if (js) {
            cli_outf("%s\n", js);
            cJSON_free(js);
        }
        cJSON_Delete(root);
#else
        cli_outf("{\"role\":\"super_agent\",\"type\":\"chat\"");
        if (final_content[0])
            cli_outf(",\"content\":\"%s\"", final_content);
        else
            cli_outf(",\"error\":\"reply failed\"");
        cli_outf("}\n");
#endif /* AIRY_HAS_CJSON */
    } else if (g_cli_print_mode) {
        cli_outf("%s\n", final_content);
    } else {
        cli_render_super_agent(final_content);
    }

    cli_history_add("user", input);
    cli_history_add("assistant", final_content);

    llm_response_free(final_resp);
#ifdef AIRY_HAS_CJSON
    cli_msgbuf_free(&buf);
#else
    AIRY_FREE(msgs);
#endif
}
