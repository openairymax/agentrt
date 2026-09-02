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

#include <stdio.h>
#include <string.h>

const cli_command_t CLI_COMMANDS[] = {
    {"/help", "显示所有命令", CLI_CAT_SESSION, 0, cmd_help},
    {"/clear", "清屏并清空对话上下文", CLI_CAT_SESSION, 0, cmd_clear},
    {"/status", "查看执行大厅状态", CLI_CAT_SYSTEM, 0, cmd_status},
    {"/chain", "决策链可视化：/chain [task_id]（默认列出最近任务）", CLI_CAT_SYSTEM, 0, cmd_chain},
    {"/hall", "事件流：/hall（统计）/hall audit（审计完整性）/hall replay [n]（全局回放）", CLI_CAT_SYSTEM, 0, cmd_hall},
    {"/orch", "流程编排：/orch <task>（七阶段管线：分解→规划→生成→批判→验证→审计→对齐）", CLI_CAT_SYSTEM, 1, cmd_orch},
    {"/quit", "退出 agentrt", CLI_CAT_SESSION, 0, cmd_quit},
    {"/tui", "切换到图形 TUI（agentrt-tui）", CLI_CAT_SESSION, 0, cmd_tui},
    {"/daemons", "查看全部 daemon 在线状态", CLI_CAT_SYSTEM, 0, cmd_daemons},
    {"/daemon", "管理 daemon：/daemon start|stop|restart|status [ns...]（默认全部）", CLI_CAT_SYSTEM, 0, cmd_daemon},
    {"/rpc", "直接调用 daemon 方法：/rpc <ns>.<method> [json]（ns 或 ns_d 均可）", CLI_CAT_SYSTEM, 1, cmd_rpc},
    {"/stats", "查看 daemon 统计：/stats [ns]", CLI_CAT_SYSTEM, 0, cmd_stats},
    {"/agents", "列出已注册智能体", CLI_CAT_RESOURCE, 0, cmd_agents},
    {"/tools", "列出可用工具", CLI_CAT_RESOURCE, 0, cmd_tools},
    {"/hooks", "列出事件钩子", CLI_CAT_RESOURCE, 0, cmd_hooks},
    {"/plugins", "列出插件", CLI_CAT_RESOURCE, 0, cmd_plugins},
    {"/channels", "列出消息通道", CLI_CAT_RESOURCE, 0, cmd_channels},
    {"/market", "搜索市场（/market skill 搜技能）", CLI_CAT_RESOURCE, 0, cmd_market},
    {"/models", "列出 LLM 模型", CLI_CAT_RESOURCE, 0, cmd_models},
    {"/apikey", "配置模型 API Key：/apikey list | set <N> <key>（N=model.yaml 行号）", CLI_CAT_SECURITY, 0, cmd_apikey},
    {"/mem", "记忆链：/mem（最近记忆） /mem <query>（检索） /mem get <id>（详情）", CLI_CAT_RESOURCE, 0, cmd_mem},
    {"/a2a", "发现 A2A 智能体", CLI_CAT_RESOURCE, 0, cmd_a2a},
    {"/metrics", "查询观测指标", CLI_CAT_SYSTEM, 0, cmd_metrics},
    {"/alerts", "查看监控告警", CLI_CAT_SYSTEM, 0, cmd_alerts},
    {"/tasks", "调度状态与检查点", CLI_CAT_SYSTEM, 0, cmd_tasks},
    {"/info", "系统信息", CLI_CAT_SYSTEM, 0, cmd_info},
    {"/notify", "发布通知：/notify <topic> <msg>", CLI_CAT_SECURITY, 1, cmd_notify},
    {"/vault", "凭据保险库：/vault list", CLI_CAT_SECURITY, 1, cmd_vault},
    {"/perm", "权限裁决：/perm <agent> <action> <resource>", CLI_CAT_SECURITY, 1, cmd_perm},
    {"/sanitize", "输入净化：/sanitize <input>", CLI_CAT_SESSION, 1, cmd_sanitize},
    {"/security", "安全状态（网络规则统计）", CLI_CAT_SECURITY, 0, cmd_security},
};

#define CLI_COMMANDS_COUNT (sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]))

size_t cli_commands_count(void)
{
    return CLI_COMMANDS_COUNT;
}

/**
 * @brief Slash-command dispatch: match /name; return 0 to fall through on miss
 */
int cli_dispatch_command(const char *input, void *ctx)
{
    if (input[0] != '/')
        return 0;

    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++) {
        const cli_command_t *cmd = &CLI_COMMANDS[i];
        size_t nlen = strlen(cmd->name);
        if (strncmp(input, cmd->name, nlen) == 0) {

            const char *rest = input + nlen;
            if (*rest == '\0' || *rest == ' ') {
                const char *arg = (*rest == ' ') ? rest + 1 : NULL;
                if (cmd->needs_args && (!arg || arg[0] == '\0')) {
                    cli_outf("  %s%s%s 需要参数。%s\n", cli_c(CLR_YELLOW), cmd->name,
                           cli_c(CLR_RESET), cmd->desc);
                    return 1;
                }
                cmd->fn(arg, ctx);
                return 1;
            }

            if (nlen >= 2 && strncmp(input, cmd->name, strlen(input) - 1) == 0 &&
                cmd->name[strlen(input) - 1] != '\0' && !cmd->needs_args) {
                cmd->fn(NULL, ctx);
                return 1;
            }
        }
    }

    cli_outf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n", cli_c(CLR_YELLOW),
           cli_c(CLR_RESET), input, cli_c(CLR_CYAN), cli_c(CLR_RESET));
    return 1;
}

static void cli_print_usage(void)
{
    cli_outf("用法: airy_cli [选项]\n");
    cli_outf("  %s-p%s, %s--print%s [PROMPT]  服务器单轮模式：执行一条指令后退出\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("                           （无 banner/提示符；省略 PROMPT 时从 stdin 读取）\n");
    cli_outf("  %s--json%s                 结构化 JSON 输出（与 %s-p%s 组合使用）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s-h%s, %s--help%s             显示本帮助\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("交互模式：直接运行 airy_cli 进入对话；输入 /help 查看命令。\n");
}

/* 命令行解析（2026-08-21 自 main 抽离降圈复杂度）：支持选项与
 * -p 模式 prompt 任意顺序（`-p --json "prompt"`、`--json -p "prompt"`、
 * `-p "prompt" --json` 均合法），首个非选项 token 作为 prompt（含空格的
 * prompt 须用引号包裹）。返回 0=继续启动；1=已输出帮助或错误并退出。 */
int cli_parse_args(int argc, char *argv[], const char **out_print_prompt)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--print") == 0) {
            g_cli_print_mode = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            g_cli_json_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_usage();
            return 1;
        } else if (argv[i][0] == '-') {
            cli_outf("airy_cli: 未知选项 %s%s%s\n", cli_c(CLR_YELLOW), argv[i],
                   cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        } else if (g_cli_print_mode && !*out_print_prompt) {
            *out_print_prompt = argv[i];
        } else if (g_cli_print_mode) {
            cli_outf("airy_cli: 多出的参数 %s%s%s（-p 只接受一个 prompt）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        } else {
            cli_outf("airy_cli: 未知参数 %s%s%s（非选项参数仅可用于 %s-p%s 模式）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET),
                   cli_c(CLR_CYAN), cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        }
    }
    return 0;
}
