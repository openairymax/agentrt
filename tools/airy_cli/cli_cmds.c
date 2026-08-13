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
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, NULL, "可用命令");
    size_t ncmds = cli_commands_count();
    for (size_t i = 0; i < ncmds; i++) {
        printf("    %s%-8s%s  %s\n", cli_c(CLR_CYAN), CLI_COMMANDS[i].name, cli_c(CLR_RESET),
               CLI_COMMANDS[i].desc);
    }
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, NULL,
                         "普通输入直接对话或下达任务指令。");
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
    /* Re-render the pinned system header (banner + model panel) so the clear
     * keeps the fixed-header layout consistent (narrow terminals get the
     * stacked banner + model config instead). */
    cli_print_system_header(getenv("AIRY_MODEL_T2"), getenv("AIRY_MODEL_T1F"),
                            getenv("AIRY_MODEL_T1P"));
    return 0;
}

int cmd_status(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (!c || !c->hall) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "status",
                             "Hall unavailable");
        return 0;
    }

    airy_work_hall_entry_t **entries = NULL;
    size_t count = 0;
    airy_err_t err = airy_work_hall_list(c->hall, &entries, &count);
    if (err != AIRY_SUCCESS) {
        char line[128];
        snprintf(line, sizeof(line), "Hall query failed (err=%d)", (int)err);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "status", line);
        return 0;
    }

    if (count == 0) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "status",
                             "任务大厅空闲，尚无执行实例");
        return 0;
    }

    char line[160];
    snprintf(line, sizeof(line), "任务大厅 · %zu 个执行实例", count);
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "status", line);
    for (size_t i = 0; i < count; i++) {
        airy_work_hall_entry_t *e = entries[i];
        cli_render_task_line("hall", e->execution_id, e->state, e->progress);
    }
    airy_work_hall_list_free(entries, count);
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
