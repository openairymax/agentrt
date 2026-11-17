// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmdline.c
 * @brief CLI 命令面：命令注册表 / 分发 / 用法 / 参数解析（域拆分自 main.c，2026-08-27）。
 *
 * CLI_COMMANDS 表（Tab 补全 SSoT，cli_internal.h 声明 extern）、
 * cli_commands_count、/xxx 命令分发（cli_dispatch_command）、-p/--json/-h
 * 参数解析（cli_parse_args）与用法输出。全局运行态见 main.c
 * （g_cli_print_mode / g_cli_json_mode 由本文件读写）。
 */

#include "cli_internal.h"
#include "compat.h"

#include <stdio.h>
#include <string.h>

const cli_command_t CLI_COMMANDS[] = {
    {"/help", "显示所有命令", "/help [命令]", CLI_CAT_SESSION, 0, cmd_help},
    {"/clear", "清屏并清空对话上下文", "/clear", CLI_CAT_SESSION, 0, cmd_clear},
    {"/status", "查看调度器任务状态", "/status", CLI_CAT_SYSTEM, 0, cmd_status},
    {"/chain", "决策链可视化：/chain [task_id]（默认列出最近任务）", "/chain [task_id]", CLI_CAT_SYSTEM, 0, cmd_chain},
    {"/hall", "事件流：/hall（统计）/hall audit（审计完整性）/hall replay [n]（全局回放）", "/hall [audit | replay [n]]", CLI_CAT_SYSTEM, 0, cmd_hall},
    {"/orch", "流程编排：/orch <task>（七阶段管线：分解→规划→生成→批判→验证→审计→对齐）", "/orch <task>", CLI_CAT_SYSTEM, 1, cmd_orch},
    {"/quit", "退出 agentrt", "/quit", CLI_CAT_SESSION, 0, cmd_quit},
    {"/tui", "切换到全屏 TUI 渲染层（退出后返回行式对话）", "/tui", CLI_CAT_SESSION, 0, cmd_tui},
    {"/daemons", "查看全部 daemon 在线状态", "/daemons", CLI_CAT_SYSTEM, 0, cmd_daemons},
    {"/daemon", "管理 daemon：/daemon start|stop|restart|status [ns...]（默认全部）", "/daemon [start|stop|restart|status] [ns...]", CLI_CAT_SYSTEM, 0, cmd_daemon},
    {"/rpc", "直接调用 daemon 方法：/rpc <ns>.<method> [json]（ns 或 ns_d 均可）", "/rpc <ns>.<method> [json]", CLI_CAT_SYSTEM, 1, cmd_rpc},
    {"/stats", "查看 daemon 统计：/stats [ns]", "/stats [ns]", CLI_CAT_SYSTEM, 0, cmd_stats},
    {"/agents", "列出已注册智能体", "/agents", CLI_CAT_RESOURCE, 0, cmd_agents},
    {"/tools", "列出可用工具", "/tools", CLI_CAT_RESOURCE, 0, cmd_tools},
    {"/hooks", "列出事件钩子", "/hooks", CLI_CAT_RESOURCE, 0, cmd_hooks},
    {"/plugins", "列出插件", "/plugins", CLI_CAT_RESOURCE, 0, cmd_plugins},
    {"/channels", "列出消息通道", "/channels", CLI_CAT_RESOURCE, 0, cmd_channels},
    {"/market", "搜索市场（/market skill 搜技能）", "/market [skill] [query]", CLI_CAT_RESOURCE, 0, cmd_market},
    {"/models", "列出 LLM 模型", "/models", CLI_CAT_RESOURCE, 0, cmd_models},
    {"/apikey", "配置模型 API Key：/apikey list | set <N> <key>（N=model.yaml 行号）", "/apikey list | set <N> <key>", CLI_CAT_SECURITY, 0, cmd_apikey},
    {"/mem", "记忆链：/mem（最近记忆） /mem <query>（检索） /mem get <id>（详情）", "/mem [query | get <id>]", CLI_CAT_RESOURCE, 0, cmd_mem},
    {"/a2a", "发现 A2A 智能体", "/a2a", CLI_CAT_RESOURCE, 0, cmd_a2a},
    {"/metrics", "查询观测指标", "/metrics", CLI_CAT_SYSTEM, 0, cmd_metrics},
    {"/alerts", "查看监控告警", "/alerts", CLI_CAT_SYSTEM, 0, cmd_alerts},
    {"/tasks", "调度状态与检查点", "/tasks", CLI_CAT_SYSTEM, 0, cmd_tasks},
    {"/info", "系统信息", "/info", CLI_CAT_SYSTEM, 0, cmd_info},
    {"/notify", "发布通知：/notify <topic> <msg>", "/notify <topic> <msg>", CLI_CAT_SECURITY, 1, cmd_notify},
    {"/vault", "凭据保险库：/vault list", "/vault list", CLI_CAT_SECURITY, 1, cmd_vault},
    {"/perm", "权限裁决：/perm <agent> <action> <resource>", "/perm <agent> <action> <resource>", CLI_CAT_SECURITY, 1, cmd_perm},
    {"/sanitize", "输入净化：/sanitize <input>", "/sanitize <input>", CLI_CAT_SESSION, 1, cmd_sanitize},
    {"/security", "安全状态（网络规则统计）", "/security", CLI_CAT_SECURITY, 0, cmd_security},
    {"/pending", "待审批工具调用（只读视图）", "/pending", CLI_CAT_SECURITY, 0, cmd_pending},
    {"/approve", "回传审批决议：/approve <request_id> allow|always|deny", "/approve <request_id> allow|always|deny", CLI_CAT_SECURITY, 1, cmd_approve},
};

#define CLI_COMMANDS_COUNT (sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]))

size_t cli_commands_count(void)
{
    return CLI_COMMANDS_COUNT;
}

/* ---- 命令词匹配 helpers（社区反馈②）----
 * 此前分发对前缀回退的判定写反了比较长度：任何单字符输入（如 "/t"）
 * 以首字节 '/' 与首条命令恒等而误触 /help，"/tu" 这类短一字符输入则
 * 直触 /tui——用户输入被静默改道。现收敛为三步：精确命中优先；
 * 前缀缩写仅在唯一命中时放行；多义并列候选交用户裁决。 */

const cli_command_t *cli_cmd_exact(const char *tok)
{
    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++)
        if (strcmp(CLI_COMMANDS[i].name, tok) == 0)
            return &CLI_COMMANDS[i];
    return NULL;
}

/* tok 的前缀候选数；唯一命中时经 *out 带回表项。 */
int cli_cmd_prefix(const char *tok, const cli_command_t **out)
{
    size_t len = strlen(tok);
    int hits = 0;
    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++) {
        if (strncmp(CLI_COMMANDS[i].name, tok, len) == 0) {
            hits++;
            if (hits == 1 && out)
                *out = &CLI_COMMANDS[i];
        }
    }
    return hits;
}

void cli_cmd_list(const char *tok)
{
    size_t len = strlen(tok);
    cli_outf("  %s%s%s 有歧义，可匹配：", cli_c(CLR_YELLOW), tok, cli_c(CLR_RESET));
    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++)
        if (strncmp(CLI_COMMANDS[i].name, tok, len) == 0)
            cli_outf(" %s", CLI_COMMANDS[i].name);
    cli_outf("\n");
}

void cli_cmd_token(const char *input, char *tok, size_t cap)
{
    const char *sp = strchr(input, ' ');
    size_t n = sp ? (size_t)(sp - input) : strlen(input);
    snprintf(tok, cap, "%.*s", (int)n, input);
}

/**
 * @brief Slash-command dispatch: match /name; return 0 to fall through on miss
 */
int cli_dispatch_command(const char *input, void *ctx)
{
    if (input[0] != '/')
        return 0;

    char tok[CLI_CMD_TOK_MAX];
    cli_cmd_token(input, tok, sizeof(tok));

    const cli_command_t *cmd = cli_cmd_exact(tok);
    if (!cmd) {
        int hits = cli_cmd_prefix(tok, &cmd);
        if (hits > 1) {
            cli_cmd_list(tok);
            return 1;
        }
        if (hits == 0) {
            cli_outf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n",
                   cli_c(CLR_YELLOW), cli_c(CLR_RESET), input, cli_c(CLR_CYAN),
                   cli_c(CLR_RESET));
            return 1;
        }
    }

    const char *rest = input + strlen(tok);
    const char *arg = (*rest == ' ') ? rest + 1 : NULL;
    if (cmd->needs_args && (!arg || arg[0] == '\0')) {
        cli_outf("  %s%s%s 需要参数。%s\n", cli_c(CLR_YELLOW), cmd->name,
               cli_c(CLR_RESET), cmd->desc);
        cli_outf("  用法：%s\n", cmd->usage);
        return 1;
    }
    cmd->fn(arg, ctx);
    return 1;
}

/* 0.1.21 A-6：JSON 字符串转义（Windows 路径含反斜杠，双引号亦须转义）。 */
void cli_json_esc(char *dst, size_t cap, const char *src)
{
    size_t n = 0;

    for (; src && *src && n + 7 < cap; src++) {
        unsigned char c = (unsigned char)*src;

        if (c == '"' || c == '\\') {
            dst[n++] = '\\';
            dst[n++] = (char)c;
        } else if (c < 0x20) {
            n += (size_t)snprintf(dst + n, cap - n, "\\u%04x", c);
        } else {
            dst[n++] = (char)c;
        }
    }
    dst[n] = '\0';
}

/* 0.1.21 A-6：路径自证子命令。doctor、安装器与门禁同取此源（机制侧
 * airy_paths_query），禁止各自拼 $AIRY_HOME/... 造成双根分叉；单行 JSON
 * 便于 POSIX shell 用 sed 取字段（容器内未必有 python/jq）。 */
static int cli_cmd_paths(int argc, char *argv[])
{
    static const char *const keys[] = {
        "home", "bin", "lib", "run", "log",
        "config", "data", "tmp", "cache", "workspace",
    };
    airy_paths_view_t v;
    const char *vals[10];
    char esc[AIRY_PATH_MAX * 2];
    int json = g_cli_json_mode;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            continue;
        }
        cli_outf("airy_cli: paths 不认识参数 %s\n", argv[i]);
        return 2;
    }
    if (airy_paths_query(&v) != AIRY_SUCCESS) {
        cli_outf("airy_cli: 路径解析失败\n");
        return 2;
    }
    vals[0] = v.home;
    vals[1] = v.bin;
    vals[2] = v.lib;
    vals[3] = v.run;
    vals[4] = v.log;
    vals[5] = v.config;
    vals[6] = v.data;
    vals[7] = v.tmp;
    vals[8] = v.cache;
    vals[9] = v.workspace;
    if (!json) {
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
            cli_outf("%s=%s\n", keys[i], vals[i]);
        return 0;
    }
    cli_outf("{\"schema\":1");
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        cli_json_esc(esc, sizeof(esc), vals[i]);
        cli_outf(",\"%s\":\"%s\"", keys[i], esc);
    }
    cli_outf("}\n");
    return 0;
}

static void cli_print_usage(void)
{
    cli_outf("用法: airy_cli [选项]\n");
    cli_outf("  %s-p%s, %s--print%s [PROMPT]  服务器单轮模式：执行一条指令后退出\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("                           （无 banner/提示符；省略 PROMPT 时从 stdin 读取）\n");
    cli_outf("  %s--json%s                 结构化 JSON 输出（与 %s-p%s 组合使用）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s--tui%s                  全屏 TUI 渲染层（agentrt-tui 子进程，退出码回传）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s--continue%s, %s--resume%s   恢复上次会话上下文（读 mem_d 最近记忆装配视图）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s-V%s, %s--version%s          输出版本号（构建期 VERSION SSoT 注入）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s-h%s, %s--help%s             显示本帮助\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %spaths%s [--json]          路径体系快照（自检/门禁与安装器同一来源）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %smodels%s [--json] [--file F]  模型连接表快照（mode/端点/key 是否存在）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("交互模式：直接运行 airy_cli 进入对话；输入 /help 查看命令。\n");
    cli_outf("退出码：0 成功；1 用法错误或本轮未拿到结果（-p 下可判定）；"
             "126 TUI 不可用。\n");
}

/* 命令行解析（2026-08-21 自 main 抽离降圈复杂度）：支持选项与
 * -p 模式 prompt 任意顺序（`-p --json "prompt"`、`--json -p "prompt"`、
 * `-p "prompt" --json` 均合法），首个非选项 token 作为 prompt（含空格的
 * prompt 须用引号包裹）。返回 0=继续启动；1=已输出版本/帮助（正常终止，
 * 调用方以 0 退出）；2=参数错误（调用方以非 0 退出）。 */
int cli_parse_args(int argc, char *argv[], const char **out_print_prompt)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--print") == 0) {
            g_cli_print_mode = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            g_cli_json_mode = 1;
        } else if (strcmp(argv[i], "--tui") == 0) {
            g_cli_tui_mode = 1;
        } else if (strcmp(argv[i], "--continue") == 0 ||
                   strcmp(argv[i], "--resume") == 0) {
            g_cli_resume_mode = 1;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            /* 版本自证取运行期身份（airy_build_id）：与 /health、启动横幅
             * 同一来源，现场三处对照不会出现两个说法。 */
            cli_outf("airy_cli %s\n", airy_build_id());
            return 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_usage();
            return 1;
        } else if (!g_cli_print_mode && strcmp(argv[i], "paths") == 0) {
            /* 子命令按「已输出、正常终止」返回；失败仍走用法错误码。 */
            return cli_cmd_paths(argc - i - 1, argv + i + 1) == 0 ? 1 : 2;
        } else if (!g_cli_print_mode && strcmp(argv[i], "models") == 0) {
            return cli_cmd_models(argc - i - 1, argv + i + 1) == 0 ? 1 : 2;
        } else if (argv[i][0] == '-') {
            cli_outf("airy_cli: 未知选项 %s%s%s\n", cli_c(CLR_YELLOW), argv[i],
                   cli_c(CLR_RESET));
            cli_print_usage();
            return 2;
        } else if (g_cli_print_mode && !*out_print_prompt) {
            *out_print_prompt = argv[i];
        } else if (g_cli_print_mode) {
            cli_outf("airy_cli: 多出的参数 %s%s%s（-p 只接受一个 prompt）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET));
            cli_print_usage();
            return 2;
        } else {
            cli_outf("airy_cli: 未知参数 %s%s%s（非选项参数仅可用于 %s-p%s 模式）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET),
                   cli_c(CLR_CYAN), cli_c(CLR_RESET));
            cli_print_usage();
            return 2;
        }
    }
    if (g_cli_tui_mode && g_cli_print_mode) {
        cli_outf("airy_cli: %s--tui%s 与 %s-p%s 互斥（全屏渲染层 vs 无界面单轮）\n",
               cli_c(CLR_YELLOW), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
        return 2;
    }
    return 0;
}
