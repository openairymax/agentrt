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

/**
  * @brief Ask the user four questions and collect answers (returns answer JSON, OWNER; freed by the engine)
 *
  * Question IDs (endpoint/start/bottleneck/audience) are the answer JSON keys,
  * matching the Q1-Q4 fields in gccp.h one-to-one.
 */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    cli_render_role_line(CLI_ROLE_SUPER_THINK, CLI_ACTOR_SUPER_THINK, "gccp",
                         "Intent confirmation: answer the questions below (Enter to skip):");
    /* The planning spinner may be animating; pause it so the questions
     * render on clean lines, then resume after the answers. */
    cli_spinner_pause();
    cJSON *answers = cJSON_CreateObject();
    if (!answers)
        return NULL;

    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        printf("  %sQ%zu%s [%s]%s %s\n", cli_c(CLR_CYAN), i + 1, cli_c(CLR_RESET), q->id,
               q->required ? "（必答）" : "", q->question);
        if (q->hint[0])
            printf("      %s提示：%s%s\n", cli_c(CLR_GREEN), q->hint, cli_c(CLR_RESET));
        printf("  %s>%s ", cli_c(CLR_GREEN), cli_c(CLR_RESET));
        fflush(stdout);

        char line[1024];
        if (!fgets(line, sizeof(line), stdin))
            break;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
            cJSON_AddStringToObject(answers, q->id, line);
    }

    char *json = cJSON_PrintUnformatted(answers);
    cJSON_Delete(answers);
    cli_spinner_resume();
    return json;
}

#else /* !AIRY_HAS_CJSON */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

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
        printf("  Q%zu [%s]%s %s\n  > ", i + 1, q->id, q->required ? "（必答）" : "", q->question);
        if (q->hint[0])
            printf("      提示：%s\n", q->hint);
        fflush(stdout);
        char line[1024];
        if (!fgets(line, sizeof(line), stdin))
            break;
        line[strcspn(line, "\r\n")] = '\0';
        if (i > 0)
            *p++ = ',';
        n = snprintf(p, cap - (size_t)(p - json), "\"%s\":\"%s\"", q->id, line);
        p += n;
    }
    snprintf(p, cap - (size_t)(p - json), "}");
    cli_spinner_resume();
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

#define CLI_SYSTEM_PROMPT                                                    \
    "你是 AgentRT，一个智能体操作系统与超级智能体助手。请用中文简洁、友好地" \
    "回答用户的问题；需要执行具体工程任务时，请引导用户描述为任务指令。"

#define CLI_CLASSIFY_PROMPT                                                    \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：包含可执行动作"   \
    "（实现、开发、构建、创建、编写、修复、重构、部署、测试等），要求执行具体" \
    "工程任务；普通对话：寒暄、提问、解释、闲聊，只需回答无需执行。"           \
    "只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

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
        "实现",   "开发", "构建",   "创建",     "编写",     "修复",     "重构",
        "部署",   "设计", "添加",   "支持",     "优化",     "迁移",     "集成",
        "写一个", "写一", "做一个", "实现一个", "帮我实现", "开发一个",
    };
    static const char *const chat_marks[] = {
        "你好", "谢谢", "再见", "你是谁", "介绍一下", "解释",  "为什么",
        "讲讲", "聊聊", "帮助", "请问",   "你好呀",   "hello", "hi",
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
  * Decision A (2026-08-09): daily chat is generated and routed by the B model (t1-f);
  * no full dual-thinking loop (t2/t1-f/t1-p critique); t2 (A) keeps only
  * planning/generation (L3 alternatives, GRAD refinement, deep review).
  * Implementation: single t1-f model replies (AIRY_MODEL_T1F; falls back to the provider default).
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
      * If unset, hint the three config points and order without blocking (provider default). */
    if (!t1f_model || !t1f_model[0]) {
        cli_render_role_line(
            CLI_ROLE_STATUS, CLI_ACTOR_SUPER_THINK, "config",
            "t1-f (B model) not configured; set AIRY_MODEL_T1F (local Ollama/vLLM or "
            "cloud API), then AIRY_MODEL_T2 (A) and AIRY_MODEL_T1P (C) as needed. "
            "Chat will use the llm_d default model for now.");
    }

    /* Message array: [system] + history + [current input], so the LLM keeps context.
      * Pointers target the global history buffer and stack input; valid during complete. */
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

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = t1f_model;
    cfg.messages = msgs;
    cfg.message_count = msg_n;
    cfg.temperature = 0.7f;
    cfg.max_tokens = 1024;

    /* Super Think status line while the LLM is generating (spinner on a TTY,
      * a plain "• Thinking" line when piped/logged). */
    const char *think_model = t1f_model ? t1f_model : "default";
    char think_title[128];
    snprintf(think_title, sizeof(think_title), "Thinking (%s)", think_model);
    cli_spinner_start(think_title);

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0 ||
        !resp->choices[0].content) {
        cli_spinner_stop(0, "reply failed");
        if (resp)
            llm_response_free(resp);
        AIRY_FREE(msgs);
        char line[128];
        snprintf(line, sizeof(line), "Reply failed (err=%d).", ret);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "chat", line);
        return;
    }
    cli_spinner_stop(1, NULL);

    /* Decision A: no t1-f verification phase - the generator is already t1-f;
      * the "t2 -> t1-f verify -> regenerate" path is gone (AIRY_CHAT_T1F_VERIFY deprecated). */

    /* Reasoning models (DeepSeek-R1/Kimi-K2) return the chain-of-thought in
     * reasoning_content; surface it as a Super Think trace before the reply
     * so the user can follow the model's reasoning. Long traces are collapsed
     * (Claude Code progressive disclosure): only the first lines render, the
     * rest stays available in the logs. */
    if (resp->choices[0].reasoning_content && resp->choices[0].reasoning_content[0]) {
        cli_render_role_line(CLI_ROLE_SUPER_THINK, CLI_ACTOR_SUPER_THINK, "reasoning", "");
        /* Content column: 2-space gutter + 24-char role header (CLI_ROLE_HDR_W). */
        cli_render_collapsed(resp->choices[0].reasoning_content, 26, 10);
    }

    cli_history_add("user", input);
    cli_history_add("assistant", resp->choices[0].content);

    cli_render_super_agent(resp->choices[0].content);
    llm_response_free(resp);
    AIRY_FREE(msgs);
}
