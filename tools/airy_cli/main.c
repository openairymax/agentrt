// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file main.c
 * @brief airy_cli — AgentRT 交互式产品入口
 *
 * 完整闭环演示（产品化形态）：
 *   用户自然语言大任务指令
 *     → GCCP 意图完备确认（推理 + 向用户询问四问）
 *     → 认知管线规划（Phase 0-1）
 *     → Plan → TaskFlow DAG 适配
 *     → 工作大厅提交/看板/等待
 *     → agent_d 驱动 ecosystem/agents 真实执行
 *
 * 机制/策略分离：CLI 是产品层（交互策略），agentrt 是机制层。
 * llm_d/agent_d 守护进程未启动时自动降级（启发式确认 + agent 不可用）。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "airy_rt.h"
#include "loop.h"
#include "roadmap_sched.h" /* 蓝图调度：执行结果回灌钩子（协同点1） */
#include "platform.h"      /* airy_data_dir()：L2 持久化快照路径（P1e 双写） */
#include "cognition.h"
#include "gccp.h"
#include "work_hall.h"
#include "hall_store.h"   /* 决策 C：任务文件模型（全流程可见性存储） */
#include "plan_to_dag.h"
#include "taskflow_advanced.h"
#include "llm_svc_adapter.h"
#include "logger.h"
#include "logging.h"
#include "airy_memory.h"
#include "string_compat.h"

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

#define AIRY_CLI_VERSION "0.1.0"

/* ==================== 任务集取消（SIGINT） ==================== */

/* 任务集取消请求标志：SIGINT 处理器置位，taskflow run_to_completion 每轮
 * 轮询检查（engine 持此指针），置位后当前节点执行完即中止任务。
 * 每个新任务开始前复位为 0。 */
static volatile sig_atomic_t g_cli_cancel = 0;

#if !defined(_WIN32)
static void cli_sigint_handler(int sig)
{
    (void)sig;
    g_cli_cancel = 1;
}
#endif

/* ==================== 终端美化（跨平台保护） ==================== */

#ifdef _WIN32
/* Windows 控制台默认不支持 ANSI 转义，降级为无颜色 */
#define CLR_CYAN    ""
#define CLR_GREEN   ""
#define CLR_YELLOW  ""
#define CLR_RED     ""
#define CLR_RESET   ""
#else
#define CLR_CYAN    "\033[36m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_RED     "\033[31m"
#define CLR_RESET   "\033[0m"
#endif

#define CLI_SEP "  ──────────────────────────────────────────────"

/* ==================== GCCP 交互回调（产品层策略） ==================== */

#ifdef AIRY_HAS_CJSON

/**
 * @brief 向用户展示四问并收集回答（返回回答 JSON，OWNER，由引擎释放）
 *
 * 问题 ID（endpoint/start/bottleneck/audience）直接作为回答 JSON 的 key，
 * 与 gccp.h 的 Q1-Q4 字段一一对应。
 */
static char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    printf("\n%s[GCCP] 该任务需进一步确认，请回答以下问题（直接回车跳过）：%s\n",
           CLR_YELLOW, CLR_RESET);
    cJSON *answers = cJSON_CreateObject();
    if (!answers)
        return NULL;

    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        printf("  %sQ%zu%s [%s]%s %s\n",
               CLR_CYAN, i + 1, CLR_RESET, q->id,
               q->required ? "（必答）" : "", q->question);
        if (q->hint[0])
            printf("      %s提示：%s%s\n", CLR_GREEN, q->hint, CLR_RESET);
        printf("  %s>%s ", CLR_GREEN, CLR_RESET);
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
    return json;
}

#else /* !AIRY_HAS_CJSON */

static char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;
    /* 无 cJSON：以固定格式构造回答 JSON（简单转义双引号） */
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
        printf("  Q%zu [%s]%s %s\n  > ", i + 1, q->id,
               q->required ? "（必答）" : "", q->question);
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
    return json;
}

#endif /* AIRY_HAS_CJSON */

/* ==================== 超级智能体对话（任务集 / 对话集分流） ==================== */

/* AgentRT 全局对话适配器（独立于 loop 内部的 llm_adapter，仅供 CLI 对话/分类使用） */
static llm_svc_adapter_t *g_chat_adapter = NULL;

/* ==================== 会话历史（对话上下文记忆） ==================== */

/* 对话历史最大消息数：默认 30 条（≈15 轮），环境变量 AIRY_CHAT_HISTORY_ROUNDS
 * 可覆盖为轮数（消息数 = 轮数*2）。上限 60，与 build_llm_request_json 的
 * 64 条消息上限对齐（留余量给 system 与当前输入）。历史满时丢弃最老一轮
 * （user+assistant），保持 FIFO。 */
#define CLI_HISTORY_MAX_MSGS 60

static char *g_history_roles[CLI_HISTORY_MAX_MSGS];
static char *g_history_contents[CLI_HISTORY_MAX_MSGS];
static size_t g_history_count = 0;

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
        /* 满：丢弃最老一轮（user+assistant） */
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

static void cli_history_clear(void)
{
    for (size_t i = 0; i < g_history_count; i++) {
        AIRY_FREE(g_history_roles[i]);
        AIRY_FREE(g_history_contents[i]);
    }
    g_history_count = 0;
}

#define CLI_SYSTEM_PROMPT                                                       \
    "你是 AgentRT，一个智能体操作系统与超级智能体助手。请用中文简洁、友好地"   \
    "回答用户的问题；需要执行具体工程任务时，请引导用户描述为任务指令。"

#define CLI_CLASSIFY_PROMPT                                                     \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：包含可执行动作"   \
    "（实现、开发、构建、创建、编写、修复、重构、部署、测试等），要求执行具体" \
    "工程任务；普通对话：寒暄、提问、解释、闲聊，只需回答无需执行。"           \
    "只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

/**
 * @brief 调用 LLM 分类输入为任务或对话（返回 1=任务 0=对话 -1=失败）
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
    cfg.model = getenv("AIRY_MODEL_T1F"); /* 决策 A：任务/对话分流由 B 模型（t1-f）执行 */
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
 * @brief 意图分流：启发式快判 + LLM 确认
 *
 * 明确任务词 → 任务集；明确对话词 → 对话集；模糊 → LLM 分类；
 * LLM 不可用时兜底为任务集（用户多输入可执行任务）。
 */
static int cli_classify_input(const char *input)
{
    static const char *const task_marks[] = {
        "实现", "开发", "构建", "创建", "编写", "修复", "重构", "部署",
        "设计", "添加", "支持", "优化", "迁移", "集成", "写一个", "写一",
        "做一个", "实现一个", "帮我实现", "开发一个",
    };
    static const char *const chat_marks[] = {
        "你好", "谢谢", "再见", "你是谁", "介绍一下", "解释", "为什么",
        "讲讲", "聊聊", "帮助", "请问", "你好呀", "hello", "hi",
    };

    for (size_t i = 0; i < sizeof(task_marks) / sizeof(task_marks[0]); i++) {
        if (strstr(input, task_marks[i]))
            return 1; /* 任务集 */
    }
    for (size_t i = 0; i < sizeof(chat_marks) / sizeof(chat_marks[0]); i++) {
        if (strstr(input, chat_marks[i]))
            return 0; /* 对话集 */
    }

    int cls = cli_llm_classify(input);
    return cls >= 0 ? cls : 1; /* LLM 失败兜底：任务集 */
}

/**
 * @brief 对话集处理：以超级智能体身份直接回复用户
 *
 * 决策 A（2026-08-09）：日常对话全部由 B 模型（t1-f）生成与分流，
 * 不启动全量双思考（t2/t1-f/t1-p 三模型批判循环）；t2（A）仅保留
 * 规划/生成职责（L3 多方案、GRAD 修正、改进6 深度复核）。
 * 实现：t1-f 单模型生成回复（AIRY_MODEL_T1F，未设置回落 provider 默认）。
 */
static void cli_chat_reply(const char *input)
{
    if (!g_chat_adapter) {
        printf("  %s[对话]%s 智能体对话不可用（llm_d 未连接）\n",
               CLR_YELLOW, CLR_RESET);
        return;
    }

    const char *t1f_model = getenv("AIRY_MODEL_T1F");

    /* 决策 B（2026-08-09）：配置环节提醒——t1-f（B 模型）最先激活。
     * 未配置时提示三配置点与激活顺序，但不阻断对话（回落 provider 默认）。 */
    if (!t1f_model || !t1f_model[0]) {
        printf("  %s[配置]%s 未检测到 t1-f（B 模型，日常对话/意图分流）配置：\n"
               "        建议先配置 AIRY_MODEL_T1F（本地 Ollama/vLLM 或云端 API），\n"
               "        再按需配置 AIRY_MODEL_T2（A，规划/修正）与 AIRY_MODEL_T1P\n"
               "        （C，逻辑验证）；当前对话将使用 llm_d 默认模型。\n",
               CLR_YELLOW, CLR_RESET);
    }

    /* 消息数组：[system] + 会话历史 + [当前用户输入]，让 LLM 记住上下文。
     * 指针指向全局历史缓冲与栈上 input，在 complete 调用期间保持有效。 */
    size_t msg_n = g_history_count + 2;
    llm_message_t *msgs =
        (llm_message_t *)AIRY_MALLOC(msg_n * sizeof(llm_message_t));
    if (!msgs) {
        printf("  %s[对话]%s 内存不足，无法回复\n", CLR_RED, CLR_RESET);
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
    cfg.model = t1f_model; /* 决策 A：B 模型（t1-f）生成；未设置时 provider 默认 */
    cfg.messages = msgs;
    cfg.message_count = msg_n;
    cfg.temperature = 0.7f;
    cfg.max_tokens = 1024;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0 ||
        !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        AIRY_FREE(msgs);
        printf("  %s[对话]%s 回复失败（err=%d）\n", CLR_RED, CLR_RESET, ret);
        return;
    }

    /* 决策 A：无 t1-f 验证段——生成者即 B 模型（t1-f），不再需要
     * "t2 生成 → t1-f 验证 → 重生成"路径（AIRY_CHAT_T1F_VERIFY 已废弃）。 */

    /* 会话历史：记录本轮 user/assistant，供下一轮上下文使用 */
    cli_history_add("user", input);
    cli_history_add("assistant", resp->choices[0].content);

    printf("  %sAgentRT >%s %s\n", CLR_GREEN, CLR_RESET,
           resp->choices[0].content);
    llm_response_free(resp);
    AIRY_FREE(msgs);
}

/* ==================== 斜杠命令（借鉴 atomcode 数据驱动命令表） ==================== */

static void cli_print_banner(void); /* 前置声明（cmd_clear 引用） */

typedef struct {
    const char *name;        /* 命令名（不含 /） */
    const char *desc;        /* 描述 */
    int needs_args;          /* 是否需要参数（选中时只提示不执行） */
    int (*fn)(const char *arg, void *ctx); /* 执行函数，arg 可为 NULL */
} cli_command_t;

typedef struct {
    airy_work_hall_t *hall;
    int *quit;
} cli_cmd_ctx_t;

static int cmd_help(const char *arg, void *ctx);
static int cmd_clear(const char *arg, void *ctx);
static int cmd_status(const char *arg, void *ctx);
static int cmd_quit(const char *arg, void *ctx);

static const cli_command_t CLI_COMMANDS[] = {
    {"/help",   "显示所有命令", 0, cmd_help},
    {"/clear",  "清屏并清空对话上下文", 0, cmd_clear},
    {"/status", "查看执行大厅状态", 0, cmd_status},
    {"/quit",   "退出 agentrt", 0, cmd_quit},
};

#define CLI_COMMANDS_COUNT (sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]))

/* 命令实现 */
static int cmd_help(const char *arg, void *ctx)
{
    (void)arg; (void)ctx;
    printf("  %s可用命令：%s\n", CLR_GREEN, CLR_RESET);
    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++) {
        printf("    %s%-8s%s  %s\n", CLR_CYAN, CLI_COMMANDS[i].name,
               CLR_RESET, CLI_COMMANDS[i].desc);
    }
    printf("  %s普通输入%s 直接对话或下达任务指令。\n", CLR_GREEN, CLR_RESET);
    return 0;
}

static int cmd_clear(const char *arg, void *ctx)
{
    (void)arg; (void)ctx;
    cli_history_clear(); /* 同时清空对话上下文（LLM 不再记得之前内容） */
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
    cli_print_banner();
    return 0;
}

static int cmd_status(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (!c || !c->hall) {
        printf("  %s[状态]%s 大厅不可用\n", CLR_YELLOW, CLR_RESET);
        return 0;
    }
    printf("  %s[状态]%s 执行大厅已就绪（WorkHall）\n", CLR_GREEN, CLR_RESET);
    return 0;
}

static int cmd_quit(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->quit)
        *c->quit = 1;
    return 0;
}

/**
 * @brief 斜杠命令分发：匹配 /name，未命中返回 0 交给普通流程
 */
static int cli_dispatch_command(const char *input, void *ctx)
{
    if (input[0] != '/')
        return 0;

    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++) {
        const cli_command_t *cmd = &CLI_COMMANDS[i];
        size_t nlen = strlen(cmd->name);
        if (strncmp(input, cmd->name, nlen) == 0) {
            /* 名称精确匹配（/help、/quit 等） */
            const char *rest = input + nlen;
            if (*rest == '\0' || *rest == ' ') {
                const char *arg = (*rest == ' ') ? rest + 1 : NULL;
                if (cmd->needs_args && (!arg || arg[0] == '\0')) {
                    printf("  %s%s%s 需要参数。%s\n",
                           CLR_YELLOW, cmd->name, CLR_RESET, cmd->desc);
                    return 1;
                }
                cmd->fn(arg, ctx);
                return 1;
            }
            /* 前缀补全：/h → /help（借鉴 atomcode command_name_or_alias_has_prefix） */
            if (nlen >= 2 && strncmp(input, cmd->name, strlen(input) - 1) == 0 &&
                cmd->name[strlen(input) - 1] != '\0' && !cmd->needs_args) {
                cmd->fn(NULL, ctx);
                return 1;
            }
        }
    }
    /* 未知斜杠命令 warn-and-skip（不打给模型，避免浪费 token） */
    printf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n",
           CLR_YELLOW, CLR_RESET, input, CLR_CYAN, CLR_RESET);
    return 1;
}

/* ==================== 主流程 ==================== */

/**
 * @brief 美化展示执行结果（优先解析 JSON 提取 output 字段）
 *
 * result 为 dag_wait 返回的输出 JSON；含 output/agent_id 字段时
 * 分段展示，否则原样截断输出。仅 POSIX 终端启用颜色。
 */
static void cli_print_result(const char *result)
{
    if (!result) {
        printf("  %s[结果]%s （未获取到结果）\n", CLR_YELLOW, CLR_RESET);
        return;
    }

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    if (root) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        cJSON *agent = cJSON_GetObjectItem(root, "agent_id");
        if (output && cJSON_IsString(output)) {
            printf("  %s[结果]%s 执行完成\n", CLR_GREEN, CLR_RESET);
            if (agent && cJSON_IsString(agent))
                printf("    %s执行体%s %s\n", CLR_CYAN, CLR_RESET,
                       agent->valuestring);
            printf("    %s产出%s\n", CLR_CYAN, CLR_RESET);
            printf("    %s%s%s\n", CLR_GREEN, output->valuestring, CLR_RESET);
            cJSON_Delete(root);
            return;
        }
        cJSON_Delete(root);
    }
#endif /* AIRY_HAS_CJSON */

    printf("  %s[结果]%s %.600s%s\n", CLR_GREEN, CLR_RESET, result,
           strlen(result) > 600 ? "..." : "");
}

/**
 * @brief 任务集节点级进度回调（工作大厅转发 taskflow 引擎进度）
 *
 * run_to_completion 驱动各节点时按节点触发（node_id 非空），
 * 执行收尾时再触发一次（node_id 为空，state 为整体终态）。
 * 让用户在执行长任务集期间看到逐节点推进，而非"无输出像卡死"。
 */
static void cli_progress_cb(const char *execution_id, const char *node_id,
                            taskflow_state_t state, double progress, void *user_data)
{
    (void)user_data;
    if (!node_id) {
        if (state == TASKFLOW_STATE_COMPLETED)
            printf("  %s[看板]%s %s 执行完成 %3.0f%%\n",
                   CLR_GREEN, CLR_RESET, execution_id, progress * 100.0);
        else if (state == TASKFLOW_STATE_FAILED)
            printf("  %s[看板]%s %s 执行失败\n",
                   CLR_RED, CLR_RESET, execution_id);
        return;
    }
    int filled = (int)(progress * 10.0f);
    if (filled < 0) filled = 0;
    if (filled > 10) filled = 10;
    char bar[32];
    size_t bo = 0;
    for (int b = 0; b < 10; b++) {
        const char *seg = (b < filled) ? "█" : "░";
        bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", seg);
    }
    printf("  %s[执行]%s 节点 %s 完成 %s %3.0f%%\n",
           CLR_GREEN, CLR_RESET, node_id, bar, progress * 100.0);
}

static void cli_print_banner(void)
{
    printf("\n"
           "  %s╔══════════════════════════════════════════════════╗%s\n"
           "  %s║          AgentRT 智能体运行时 — 交互入口          ║%s\n"
           "  %s╚══════════════════════════════════════════════════╝%s\n",
           CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET);
    printf("  %s版本%s v%s | 超级智能体对话 + GCCP 意图确认 + WorkHall 调度\n",
           CLR_GREEN, CLR_RESET, AIRY_CLI_VERSION);
    printf("  %s说明%s 普通对话直接回复；任务指令（如：\n"
           "        \"为项目实现登录模块，包含前端页面、后端接口与单元测试\"）\n"
           "        将启动完备四问确认后规划执行。\n"
           "  %s提示%s 输入 %s/help%s 查看命令，%squit%s/%sexit%s 退出。\n",
           CLR_GREEN, CLR_RESET,
           CLR_GREEN, CLR_RESET, CLR_YELLOW, CLR_RESET,
           CLR_YELLOW, CLR_RESET, CLR_YELLOW, CLR_RESET);
}

int main(void)
{
    /* CLI 对话界面保持纯净：全局日志级别提升至 ERROR，屏蔽 GCCP/ThinkDual
     * 等过程 INFO/WARN 刷屏（logging 模块 constructor 已以默认配置初始化
     * log_init，此处运行时调整级别，无需重新初始化）。 */
    log_set_module_level("*", LOG_LEVEL_ERROR);

    /* 任务集取消入口：Ctrl+C 置位取消标志（不终止进程），
     * run_to_completion 轮询检查后在当前节点完成后中止任务。 */
#if !defined(_WIN32)
    {
        struct sigaction sa;
        __builtin_memset(&sa, 0, sizeof(sa));
        sa.sa_handler = cli_sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }
#endif

    cli_print_banner();

    /* 1. 创建核心循环 */
    airy_core_loop_t *loop = NULL;
    airy_err_t err = airy_loop_create(NULL, &loop);
    if (err != AIRY_EOK || !loop) {
        AIRY_LOG_ERROR("airy_cli: loop create failed (err=%d)", (int)err);
        return 1;
    }

    /* 2. 获取认知引擎并注入 GCCP 交互回调 */
    airy_cognition_engine_t *cog = NULL;
    airy_loop_get_engines(loop, &cog, NULL, NULL);
    if (cog) {
        airy_cognition_set_gccp_interact(cog, cli_gccp_interact, NULL);
        AIRY_LOG_INFO("airy_cli: GCCP interaction callback attached");

        /* 双思考系统三模型注入（GRAD 三权分立：用户自选模型，非固定厂商）：
         *   AIRY_MODEL_T2    → 模型 A（生成者）
         *   AIRY_MODEL_T1F   → 模型 B（语境终裁者）
         *   AIRY_MODEL_T1P   → 模型 C（逻辑验证者）
         * 环境变量未设置时使用 provider 默认模型（向后兼容）。 */
        const char *m_s2 = getenv("AIRY_MODEL_T2");
        const char *m_verify = getenv("AIRY_MODEL_T1F");
        const char *m_expert = getenv("AIRY_MODEL_T1P");
        airy_cognition_set_tc3_models(cog, m_s2, m_verify, m_expert);
        if (m_s2 || m_verify || m_expert) {
            AIRY_LOG_INFO("airy_cli: TC3 models injected (s2=%s verify=%s expert=%s)",
                          m_s2 ? m_s2 : "(default)",
                          m_verify ? m_verify : "(default)",
                          m_expert ? m_expert : "(default)");
        }

        /* 决策 B（2026-08-09）：模型三配置点提醒——t2=A（生成者，云端为主）、
         * t1-f=B（语境终裁者/日常对话，最先激活，本地为主）、t1-p=C（逻辑验证者）；
         * 每点既可用云端 API 又可用本地端点（Ollama/vLLM），开关由用户决定。 */
        printf("  %s[模型配置]%s t2(A)=%s | t1-f(B)=%s | t1-p(C)=%s\n",
               CLR_GREEN, CLR_RESET,
               m_s2 ? m_s2 : "(未配置，llm_d 默认)",
               m_verify ? m_verify : "(未配置，llm_d 默认)",
               m_expert ? m_expert : "(未配置，llm_d 默认)");
        printf("    %s提示：%s 每点均可指向云端 API 或本地（Ollama/vLLM）；"
               "激活顺序 t1-f（B）最先（日常对话/意图分流）。\n",
               CLR_YELLOW, CLR_RESET);

        /* GRAD 计划级批判循环：任务集默认启用（放弃文本级批判循环） */
        airy_cognition_set_grad_enabled(cog, 1);
    }

    /* 3. 蓝图调度（Roadmap Sched）：执行结果回灌钩子（协同点1）。
     * 工作大厅节点终态经 progress_cb 回灌 L2 缓存 / 失败指纹 / 取消联动 L1。
     * P1e：L2 双写持久化（$AIRY_HOME/data/agentrt/roadmap/l2_semantic_cache.json，
     * 重启恢复）；Embedding + HNSW 向量索引（MemoryRovol，未链接自动降级）。
     * 创建失败仅降级（无回灌），不阻断交互式 CLI 主流程。 */
    airy_rs_config_t rs_cfg;
    __builtin_memset(&rs_cfg, 0, sizeof(rs_cfg));
    {
        static char rs_persist_path[512];
        snprintf(rs_persist_path, sizeof(rs_persist_path), "%s/agentrt/roadmap/l2_semantic_cache.json",
                 airy_data_dir());
        rs_cfg.l2_persist_path = rs_persist_path;
    }
    airy_roadmap_sched_t *rsched = NULL;
    err = airy_roadmap_sched_create(&rs_cfg, &rsched);
    if (err != AIRY_EOK || !rsched) {
        AIRY_LOG_WARN("airy_cli: roadmap_sched create failed (err=%d), "
                      "execution feed-back disabled", (int)err);
        rsched = NULL;
    }

    /* 4. 创建工作大厅并注入 orchestration ops */
    /* 决策 G（2026-08-09）：验证门禁落地——注入产物验证器。
     * 规则来自 AIRY_VALIDATOR_RULES（JSON），缺省 exit_code=0；
     * 节点级门禁由 sched_d write_back 前置验证承担，CLI 侧在 wait 后标注 FAIL。 */
    airy_artifact_validator_t *cli_validator = NULL;
    const char *rules_json = getenv("AIRY_VALIDATOR_RULES");
    if (!rules_json || !rules_json[0])
        rules_json = "{\"exit_code\":0}";
    airy_err_t vrc = airy_artifact_validator_from_json(&cli_validator, rules_json);
    if (vrc != AIRY_SUCCESS) {
        AIRY_LOG_WARN("airy_cli: output_validator create failed (err=%d), gate disabled",
                      (int)vrc);
        cli_validator = NULL;
    }
    airy_work_hall_config_t wh_cfg;
    __builtin_memset(&wh_cfg, 0, sizeof(wh_cfg));
    wh_cfg.progress_cb = cli_progress_cb; /* 节点级进度实时反馈 */
    wh_cfg.roadmap_sched = rsched;        /* BORROW：执行结果回灌蓝图调度 */
    wh_cfg.output_validator = cli_validator; /* BORROW：产物验证门禁（决策 G） */
    /* 决策 C（2026-08-09）：任务文件模型——全流程可见性存储（$AIRY_HOME/data/agentrt/hall）。
     * 进度/结果/问题事件写入大厅存储，支持检索回放与记忆层经验提取。 */
    airy_hall_store_t *hall_store = airy_hall_store_create(NULL);
    if (!hall_store)
        AIRY_LOG_WARN("airy_cli: hall store create failed, full visibility disabled");
    wh_cfg.hall_store = hall_store; /* BORROW：任务文件模型（决策 C） */
    /* 决策 E（2026-08-09）：工作区隔离主工作区路径。
     * AIRY_WORKSPACE_MAIN_DIR 显式指定时启用（节点执行快照主工作区 → 独立沙箱目录
     * → 产物 merge 回主工作区）；未指定则保持现状（执行体直接操作主工作区）。
     * AIRY_WORKSPACE_ISOLATION=0 可进一步关闭隔离（快照/合并返回 ENOTSUP）。 */
    {
        const char *ws_main = getenv("AIRY_WORKSPACE_MAIN_DIR");
        if (ws_main && ws_main[0])
            wh_cfg.main_workspace_dir = ws_main;
    }
    airy_work_hall_t *hall = NULL;
    err = airy_work_hall_create(&wh_cfg, loop, &hall);
    if (err != AIRY_EOK || !hall) {
        AIRY_LOG_ERROR("airy_cli: work hall create failed (err=%d)", (int)err);
        if (rsched)
            airy_roadmap_sched_destroy(rsched);
        airy_loop_destroy(loop);
        return 1;
    }
    airy_work_hall_bind_ops(hall);

    /* 任务集取消支持：引擎持 g_cli_cancel 指针，SIGINT 置位后
     * run_to_completion 每轮轮询中止（当前节点执行完后生效） */
    err = airy_loop_dag_set_cancel_flag(loop, &g_cli_cancel);
    if (err != AIRY_EOK)
        AIRY_LOG_WARN("airy_cli: set cancel flag failed (err=%d)", (int)err);

    /* 5. 创建对话适配器（任务集/对话集分流用，独立于 loop 内部 adapter） */
    llm_svc_adapter_config_t chat_cfg;
    __builtin_memset(&chat_cfg, 0, sizeof(chat_cfg));
    chat_cfg.llm_d_service_name = "llm_d";
    chat_cfg.channel_name = "coreloopthree-llm";
    g_chat_adapter = llm_svc_adapter_create(&chat_cfg);
    if (!g_chat_adapter)
        AIRY_LOG_WARN("airy_cli: chat adapter create failed, "
                         "falling back to task-only mode");

    /* 6. 主交互循环 */
    char input[8192];
    int quit_flag = 0;
    cli_cmd_ctx_t cmd_ctx = {.hall = hall, .quit = &quit_flag};
    for (;;) {
        printf("\n%sairy>%s ", CLR_CYAN, CLR_RESET);
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            break;
        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] == '\0')
            continue;
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
            break;

        size_t input_len = strlen(input);

        /* 4.0a 斜杠命令分发（/help /clear /status /quit 等） */
        if (cli_dispatch_command(input, &cmd_ctx)) {
            if (quit_flag)
                break;
            continue;
        }

        printf("  %s[你]%s %s\n", CLR_CYAN, CLR_RESET, input);

        /* 4.0 意图分流：对话集直接智能体回复，任务集才启动完备四问 + 执行 */
        int is_task = cli_classify_input(input);
        if (is_task == 0) {
            cli_chat_reply(input);
            continue;
        }

        /* 4.0b 蓝图调度三级路由（§3.1 接线：认知入口之前挂接）。
         * L1 状态机命中（零 Token）→ 输出下一步；L2 语义缓存命中（低 Token）→
         * 输出建议；两者均不进入 LLM 管线。仅 MISS_L3 走五阶段认知管线。
         * L3 收敛图纸经 absorb 登记状态机（§3.5），后续"继续/下一步"L1 命中。 */
        if (rsched) {
            char *rs_out = NULL;
            airy_rs_dispatch_t rs_disp = AIRY_RS_DISPATCH_MISS_L3;
            airy_err_t rs_err = airy_roadmap_sched_process(
                rsched, input, NULL, &rs_out, &rs_disp);
            if (rs_err == AIRY_EOK &&
                rs_disp == AIRY_RS_DISPATCH_HIT_L1) {
                printf("  %s[蓝图]%s L1 状态机命中（零 Token）：%s%s%s\n",
                       CLR_GREEN, CLR_RESET, CLR_YELLOW,
                       rs_out ? rs_out : "{}", CLR_RESET);
                AIRY_FREE(rs_out);
                continue;
            }
            if (rs_err == AIRY_EOK &&
                rs_disp == AIRY_RS_DISPATCH_HIT_L2) {
                printf("  %s[蓝图]%s L2 语义缓存命中（低 Token）：%s\n",
                       CLR_GREEN, CLR_RESET,
                       rs_out ? rs_out : "{}");
                AIRY_FREE(rs_out);
                continue;
            }
            AIRY_FREE(rs_out); /* MISS_L3 或错误：进入认知管线 */
        }

        /* 4.1 认知处理（任务集：GCCP 意图确认在 Phase 0 后自动发生） */
        g_cli_cancel = 0; /* 新任务开始，复位取消标志 */
        printf("  %s[认知]%s 正在分析任务（LLM 拆解 + 意图确认）...\n",
               CLR_CYAN, CLR_RESET);
        airy_task_plan_t *plan = NULL;
        err = airy_cognition_process(cog, input, input_len, &plan);
        if (err != AIRY_EOK || !plan) {
            printf("  %s[认知]%s 规划失败：err=%d\n",
                   CLR_RED, CLR_RESET, (int)err);
            continue;
        }
        /* 4.1.5 收敛图纸 absorb（§3.5）：登记 L1 状态机 + 重置 TTL，
         * 使后续"继续/下一步"指令 L1 命中（零 Token 推进）。失败仅降级。 */
        if (rsched)
            airy_roadmap_sched_absorb(rsched, plan, NULL, NULL);
        printf("  %s[认知]%s 计划已生成：%splan_id=%s%s 节点=%zu 入口=%zu\n",
               CLR_GREEN, CLR_RESET, CLR_YELLOW,
               plan->task_plan_id ? plan->task_plan_id : "?",
               CLR_RESET, plan->task_plan_node_count,
               plan->task_plan_entry_count);

        /* 4.2 Plan → TaskFlow DAG 适配 */
        taskflow_workflow_t *wf = NULL;
        err = airy_plan_to_workflow(plan, &wf);
        if (err != AIRY_EOK || !wf) {
            printf("  %s[DAG]%s 工作流适配失败：err=%d\n",
                   CLR_RED, CLR_RESET, (int)err);
            airy_task_plan_free(plan);
            continue;
        }
        printf("  %s[DAG]%s 工作流已适配：id=%s 节点=%zu 边=%zu\n",
               CLR_GREEN, CLR_RESET, wf->id, wf->node_count, wf->edge_count);

        /* 展示计划节点路由（执行体 + 目标），提升执行透明度 */
        printf("  %s[计划]%s 节点明细：\n", CLR_GREEN, CLR_RESET);
        for (size_t ni = 0; ni < wf->node_count; ni++) {
            const taskflow_node_t *nd = &wf->nodes[ni];
            printf("    %s%-8s%s %s%-14s%s %s\n",
                   CLR_CYAN, nd->id, CLR_RESET,
                   CLR_GREEN, nd->task_handler_name ? nd->task_handler_name : "?",
                   CLR_RESET, nd->name[0] ? nd->name : "");
        }

        /* 4.3 提交到工作大厅（input 传入原始任务文本，agent 节点据此执行；
         * 若不传，首个节点 handler 收到空输入，产出无意义模板话） */
        char *exec_id = NULL;
        err = airy_work_hall_submit(hall, wf, input, &exec_id);
        if (err != AIRY_EOK || !exec_id) {
            printf("  %s[大厅]%s 提交失败：err=%d\n",
                   CLR_RED, CLR_RESET, (int)err);
            airy_workflow_free(wf);
            airy_task_plan_free(plan);
            continue;
        }
        printf("  %s[大厅]%s 已提交：exec=%s\n",
               CLR_GREEN, CLR_RESET, exec_id);

        /* 4.4 看板轮询
         * taskflow_engine_start 仅同步执行起始节点（progress=1/N），多节点
         * 工作流的剩余推进由 run_to_completion（dag_wait）驱动。此处轮询
         * 展示实时状态；借鉴 Claude CLI 的交互习惯：状态/进度发生变化时才
         * 打印一行（含进度条），避免固定 200ms 刷屏。若状态长时间无推进
         * （stale_polls 超过阈值），主动 break 转入 4.5 wait 驱动完成。 */
        int board_polls = 0;
        int stale_polls = 0;
        int done = 0;
        char last_state[16] = "";
        double last_progress = -1.0;
        for (;;) {
#ifdef _WIN32
            Sleep(200);
#else
            usleep(200 * 1000);
#endif
            /* Ctrl+C 取消：中止看板轮询，转入 dag_wait 由引擎感知取消 */
            if (g_cli_cancel) {
                printf("  %s[取消]%s 已收到中止请求，正在停止任务...\n",
                       CLR_YELLOW, CLR_RESET);
                break;
            }
            airy_work_hall_entry_t *entry = NULL;
            airy_err_t st_err = airy_work_hall_status(hall, exec_id, &entry);
            if (st_err != AIRY_EOK || !entry) {
                printf("  %s[看板]%s 状态查询失败\n", CLR_RED, CLR_RESET);
                break;
            }
            int state_changed = (strcmp(entry->state, last_state) != 0);
            double prog_changed = (entry->progress - last_progress) >= 0.01 ||
                                  (entry->progress - last_progress) <= -0.01;
            if (state_changed || prog_changed) {
                /* 进度条：10 格，'█'=完成 '░'=未完成（UTF-8 每格 3 字节） */
                int filled = (int)(entry->progress * 10.0f);
                if (filled < 0) filled = 0;
                if (filled > 10) filled = 10;
                char bar[32];
                size_t bo = 0;
                for (int b = 0; b < 10; b++) {
                    const char *seg = (b < filled) ? "█" : "░";
                    bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", seg);
                }
                printf("  %s[看板]%s %s%s%s %s%-6s%s %s[%s]%s %3.0f%%\n",
                       CLR_GREEN, CLR_RESET,
                       CLR_YELLOW, entry->execution_id, CLR_RESET,
                       CLR_CYAN, entry->state, CLR_RESET,
                       CLR_GREEN, bar, CLR_RESET,
                       entry->progress * 100.0);
                snprintf(last_state, sizeof(last_state), "%s", entry->state);
                last_progress = entry->progress;
            }
            done = (strcmp(entry->state, "completed") == 0 ||
                    strcmp(entry->state, "failed") == 0 ||
                    strcmp(entry->state, "canceled") == 0);
            board_polls++;
            if (!state_changed && !prog_changed) {
                stale_polls++;
            } else {
                stale_polls = 0;
            }
            airy_work_hall_entry_free(entry);
            if (done || board_polls >= 300 || stale_polls >= 10)
                break;
        }
        if (!done && board_polls > 0 && stale_polls >= 10)
            printf("  %s[看板]%s 状态无推进，驱动执行完成...\n",
                   CLR_YELLOW, CLR_RESET);

        /* 6.5 获取结果（若期间收到 Ctrl+C 取消，优先提示已中止） */
        uint32_t vf_before = 0, vf_after = 0;
        airy_work_hall_verify_stats(hall, NULL, &vf_before, NULL); /* 决策 G：基线 */
        char *result = NULL;
        err = airy_work_hall_wait(hall, exec_id, 0, &result);
        if (g_cli_cancel) {
            printf("  %s[取消]%s 任务已中止（当前节点执行完后停止）\n",
                   CLR_YELLOW, CLR_RESET);
        } else if (err == AIRY_EOK && result) {
            cli_print_result(result);
        } else {
            printf("  %s[结果]%s （未获取到结果，err=%d）\n",
                   CLR_YELLOW, CLR_RESET, (int)err);
        }
        /* 决策 G：验证门禁标注——本次执行产物验证 FAIL 时明确标注，
         * 供用户决定重新规划/重试（节点级门禁重试由 sched_d 承担）。 */
        if (!g_cli_cancel) {
            airy_work_hall_verify_stats(hall, NULL, &vf_after, NULL);
            if (vf_after > vf_before)
                printf("  %s[验证]%s 本次执行产物验证失败——结果已标注，"
                       "可通过重新规划或重试处理\n",
                       CLR_RED, CLR_RESET);
        }

        if (result)
            AIRY_FREE(result);
        if (exec_id)
            AIRY_FREE(exec_id);
        /* submit 成功后 wf 内部字段（nodes/edges/initial_node_id）由 taskflow
         * 引擎浅拷贝持有，随 airy_loop_destroy 释放；此处仅释放结构体外壳，
         * 避免退出时 engine destroy 访问已释放内存（UAF）。 */
        AIRY_FREE(wf);
        airy_task_plan_free(plan);
    }

    /* 7. 清理（先 hall 后 rsched：hall BORROW 不拥有蓝图调度） */
    if (g_chat_adapter)
        llm_svc_adapter_destroy(g_chat_adapter);
    airy_work_hall_destroy(hall);
    if (cli_validator) /* 决策 G：验证器 OWNER，hall destroy 后释放 */
        airy_artifact_validator_destroy(cli_validator);
    if (hall_store) /* 决策 C：任务文件存储 OWNER，hall destroy 后释放 */
        airy_hall_store_destroy(hall_store);
    if (rsched)
        airy_roadmap_sched_destroy(rsched);
    airy_loop_destroy(loop);
    printf("\n%sAgentRT 已退出，感谢使用。%s\n", CLR_CYAN, CLR_RESET);
    return 0;
}
