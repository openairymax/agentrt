// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_cmds.c
 * @brief airy_cli builtin command domain: /help /clear /status /quit.
 *
 * Four local daemon-independent commands: command list (walks CLI_COMMANDS),
 * clear screen and chat context, work-hall status, and quit flag.
 * Daemon-related commands (/daemons /rpc etc.) live in daemon_cmds.c.
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_help(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    printf("  %s可用命令：%s\n", CLR_GREEN, CLR_RESET);
    size_t ncmds = cli_commands_count();
    for (size_t i = 0; i < ncmds; i++) {
        printf("    %s%-8s%s  %s\n", CLR_CYAN, CLI_COMMANDS[i].name, CLR_RESET,
               CLI_COMMANDS[i].desc);
    }
    printf("  %s普通输入%s 直接对话或下达任务指令。\n", CLR_GREEN, CLR_RESET);
    return 0;
}

int cmd_clear(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    cli_history_clear();
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
    cli_print_banner();
    return 0;
}

int cmd_status(const char *arg, void *ctx)
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

int cmd_quit(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->quit)
        *c->quit = 1;
    return 0;
}
