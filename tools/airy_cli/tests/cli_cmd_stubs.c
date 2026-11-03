// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_cmd_stubs.c
 * @brief CLI 命令分发测试的命令实现桩：CLI_COMMANDS 表引用的 33 个 cmd_*。
 *
 * 分发测试（test_cli_cmdline.c）只关心 cli_dispatch_command 的匹配行为，
 * 不便把 cli_cmds.c 的整棵依赖树（render/RPC/daemon 客户端）拖进测试
 * 可执行体，故以桩承接链接面。cmd_quit / cmd_tui 的桩与 cli_cmds.c 真
 * 实现的 ctx 副作用语义一致（置 quit / switch_tui 标志），使测试得以
 * 断言「输入被分发到哪条命令」；其余命令桩为空实现返回 0。
 */

#include "cli_internal.h"

#include <stdarg.h>
#include <stdio.h>

/* 渲染面桩：cli_outf/cli_c 真身在 cli_render.c（携带 render 依赖树），
 * 分发测试只关心文案到达 stdout 与否，直通 vprintf 即可；颜色序列原样
 * 返回。g_cli_json_mode 真身由 main.c 拥有（cli_parse_args 读写）。 */
void cli_outf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

const char *cli_c(const char *seq)
{
    return seq;
}

const char *cli_theme_seq(cli_theme_t key)
{
    (void)key;
    return "";
}

int g_cli_json_mode = 0;
int g_cli_tui_mode = 0;
int g_cli_resume_mode = 0;
char *g_cli_lang_sys_prompt = NULL;
int g_cli_lang_output = 0;

/* 与 cli_cmds.c cmd_quit/cmd_tui 同语义：置 ctx 标志，供测试断言。 */
int cmd_quit(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->quit)
        *c->quit = 1;
    return 0;
}

int cmd_tui(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->switch_tui)
        *c->switch_tui = 1;
    return 0;
}

int cmd_help(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    return 0;
}

#define CMD_STUB(nm)                                                                      \
    int nm(const char *arg, void *ctx)                                                    \
    {                                                                                     \
        (void)arg;                                                                        \
        (void)ctx;                                                                        \
        return 0;                                                                         \
    }

CMD_STUB(cmd_clear)
CMD_STUB(cmd_status)
CMD_STUB(cmd_chain)
CMD_STUB(cmd_hall)
CMD_STUB(cmd_orch)
CMD_STUB(cmd_daemons)
CMD_STUB(cmd_daemon)
CMD_STUB(cmd_rpc)
CMD_STUB(cmd_stats)
CMD_STUB(cmd_agents)
CMD_STUB(cmd_tools)
CMD_STUB(cmd_hooks)
CMD_STUB(cmd_plugins)
CMD_STUB(cmd_channels)
CMD_STUB(cmd_market)
CMD_STUB(cmd_models)
CMD_STUB(cmd_apikey)
CMD_STUB(cmd_mem)
CMD_STUB(cmd_a2a)
CMD_STUB(cmd_metrics)
CMD_STUB(cmd_alerts)
CMD_STUB(cmd_tasks)
CMD_STUB(cmd_info)
CMD_STUB(cmd_notify)
CMD_STUB(cmd_vault)
CMD_STUB(cmd_perm)
CMD_STUB(cmd_sanitize)
CMD_STUB(cmd_security)
CMD_STUB(cmd_pending)
CMD_STUB(cmd_approve)
