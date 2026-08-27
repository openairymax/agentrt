// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_history.c
 * @brief airy_cli chat history buffer / error description / system prompt.
 *
 * 对话历史环形缓冲（FIFO）：容量 60 条（约 15 轮，AIRY_CHAT_HISTORY_ROUNDS
 * 按轮覆盖），满时丢最老一轮（user+assistant 成对）。每轮历史携带思考链
 * （reasoning）——DeepSeek 续轮规范要求 assistant 消息原样回传 reasoning，
 * 缺省会语义断裂。定义 g_history_* 全局（cli_internal.h 声明 extern）。
 *
 * 另含聊天场景错误描述（llm_d 是聊天回复的唯一 RPC 目标，NOT_FOUND 给出
 * 可执行提示）与系统提示词（含宿主机时间注入）。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 聊天场景的错误描述：llm_d 是聊天回复的唯一 RPC 目标，NOT_FOUND 即
 * llm.sock 不存在（llm_d 未启动），与通用"目标不存在"相比给出可执行
 * 提示；其余错误沿用通用映射（cli_err_desc）。 */
const char *cli_chat_err_desc(int err)
{
    if (err == AIRY_ERR_NOT_FOUND)
        return "LLM 服务未运行，请先启动 airymaxrt（llm_d 守护进程）";
    return cli_err_desc(err);
}

char *g_history_roles[CLI_HISTORY_MAX_MSGS];
char *g_history_contents[CLI_HISTORY_MAX_MSGS];
/* 2.1.1.6：历史携带每轮思考链——多轮对话上下文中前一轮的 reasoning 原样
 * 保留并随 assistant 消息回传（DeepSeek 续轮规范要求，缺省会语义断裂）。 */
char *g_history_reasonings[CLI_HISTORY_MAX_MSGS];
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

void cli_history_add(const char *role, const char *content, const char *reasoning)
{
    if (!role || !content)
        return;
    size_t cap = cli_history_capacity();
    if (g_history_count >= cap) {

        AIRY_FREE(g_history_roles[0]);
        AIRY_FREE(g_history_contents[0]);
        AIRY_FREE(g_history_reasonings[0]);
        AIRY_FREE(g_history_roles[1]);
        AIRY_FREE(g_history_contents[1]);
        AIRY_FREE(g_history_reasonings[1]);
        AIRY_MEMMOVE(&g_history_roles[0], &g_history_roles[2],
                     (g_history_count - 2) * sizeof(char *));
        AIRY_MEMMOVE(&g_history_contents[0], &g_history_contents[2],
                     (g_history_count - 2) * sizeof(char *));
        AIRY_MEMMOVE(&g_history_reasonings[0], &g_history_reasonings[2],
                     (g_history_count - 2) * sizeof(char *));
        g_history_count -= 2;
    }
    g_history_roles[g_history_count] = AIRY_STRDUP(role);
    g_history_contents[g_history_count] = AIRY_STRDUP(content);
    g_history_reasonings[g_history_count] =
        (reasoning && reasoning[0]) ? AIRY_STRDUP(reasoning) : NULL;
    g_history_count++;
}

void cli_history_clear(void)
{
    for (size_t i = 0; i < g_history_count; i++) {
        AIRY_FREE(g_history_roles[i]);
        AIRY_FREE(g_history_contents[i]);
        AIRY_FREE(g_history_reasonings[i]);
    }
    g_history_count = 0;
}

/* 2.1.1.6：思考链全量落盘——交互模式 cli_trace 是 no-op（仅 -p 模式
 * 写 stderr），思考链此前只在内存折叠展示后即释放。这里独立追加写入
 * $AIRY_HOME/logs/airy_reasoning.log（所有模式生效），思考 token 不丢失。
 * 每轮带时间戳与角色前缀，便于按会话回溯。 */
void cli_chat_reasoning_persist(const char *text)
{
    if (!text || !text[0])
        return;
    const char *logdir = airy_log_dir();
    if (!logdir || airy_mkdir_p(logdir) != 0)
        return;
    char logpath[512];
    int plen = snprintf(logpath, sizeof(logpath), "%s/airy_reasoning.log", logdir);
    if (plen < 0 || plen >= (int)sizeof(logpath))
        return;
    FILE *lf = fopen(logpath, "a");
    if (!lf)
        return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[40];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(lf, "\n[%s] [assistant reasoning] %s\n", ts, text);
    fclose(lf);
}

#define CLI_SYSTEM_PROMPT                                                       \
    "你是 AgentRT，一个智能体操作系统与超级智能体助手。请用中文简洁、友好地"    \
    "回答用户的问题；需要执行具体工程任务时，请引导用户描述为任务指令。\n"      \
    "你具备本地文件工具：fs_read（读文件）、fs_write（写/覆盖文件）、fs_list"  \
    "（列目录）、fs_glob（通配符找文件）、fs_grep（正则搜文件内容）、fs_edit"  \
    "（精确字符串替换编辑）。用户要求查看、修改本地文件时，先 fs_read/fs_list" \
    " 确认现状再操作；fs_edit 需精确匹配原文，改完可 fs_read 复核。\n"          \
    "你具备两个联网工具：web_search（搜索引擎，参数 query/max_results）与 "    \
    "web_fetch（抓取网页正文，参数 url）。当问题涉及实时信息、最新新闻、时效"   \
    "性数据，或你知识截止日期（2025-05）之后发生的事件，必须调用 web_search "  \
    "获取最新结果，必要时再用 web_fetch 深入抓取；不要凭过时知识硬答。"        \
    "工具结果返回后，基于结果组织回答并标注信息时效。工具结果是真实抓取的"    \
    "内容，除非结果为空或明确报错，否则不得声称\"搜索失败\"\"结果无关\"或"      \
    "编造工具异常原因（如\"分词有问题\"）；应逐条核实返回的标题/摘要/链接，"    \
    "据实引用作答。\n"                                                           \
    "系统上下文已注入宿主机当前时间。用户问现在几点/今天几号/星期几等时间类"    \
    "问题时，直接依据注入的时间作答，不要为查询时间调用任何工具。"

/* 2.3.4 宿主机时间注入（2026-08-17 补强）：system prompt 声明"已注入宿主机
 * 当前时间"，但此前从未真正注入——LLM 只能靠知识截止日期猜测，问"今天几号"
 * 会答错。现在每次会话真实注入本地时间（含时区偏移）。静态缓冲够用（每个
 * 会话一条 system 消息，msgbuf 已 STRDUP 复制，生命周期安全）。 */
const char *cli_system_prompt_now(void)
{
    static char s_sys[1536];
    time_t now = time(NULL);
    struct tm tmv;
    if (airy_localtime_r(&now, &tmv) != 0)
        return CLI_SYSTEM_PROMPT; /* 时间转换失败时回退无时间戳提示 */
    char ts[96];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z", &tmv);
    snprintf(s_sys, sizeof(s_sys),
             "当前宿主机时间：%s（本地时区）。\n%s", ts, CLI_SYSTEM_PROMPT);
    return s_sys;
}
