// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_output.c
 * @brief Conversation output: role lines, progress bars, turn separators.
 *
 * Extracted from cli_render.c.  Owns the role-tagged conversation line
 * renderer (role_line / super_agent / user_message / sub_agent), the
 * progress bar and task line for the work-hall board, and the turn
 * separator that marks the end of each agent turn.
 */

#include "cli_render.h"
#include "cli_tui.h"

#include "airy_memory.h"

extern int g_cli_print_mode;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- role-tagged conversation line ---- */

#define CLI_ROLE_HDR_W 24

static void cli_pad_role_header(const char *hdr)
{
    size_t w = cli_disp_width(hdr);
    for (size_t i = w; i < CLI_ROLE_HDR_W; i++)
        cli_outc(' ');
}

static void cli_build_role_header(char *out, size_t cap, const char *name, const char *tag)
{
    if (!tag || !tag[0]) {
        snprintf(out, cap, "[%s]", name);
        return;
    }
    size_t name_len = strlen(name);
    size_t tag_budget =
        CLI_ROLE_HDR_W > (name_len + 3) ? (CLI_ROLE_HDR_W - name_len - 3) : 0;
    size_t tag_show = strlen(tag);
    if (tag_show > tag_budget)
        tag_show = cli_utf8_safe_len(tag, tag_budget);
    snprintf(out, cap, "[%s:%.*s]", name, (int)tag_show, tag);
}

void cli_render_role_line(cli_role_t role, cli_actor_t actor, const char *tag,
                          const char *content)
{
    if (g_cli_print_mode &&
        (role == CLI_ROLE_TRACE || role == CLI_ROLE_STATUS || role == CLI_ROLE_SUB_AGENT))
        return;
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(role);
    const char *name = cli_render_actor_name(actor);
    char hdr[CLI_ROLE_HDR_W + 1];

    cli_build_role_header(hdr, sizeof(hdr), name, tag);

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);

    if (content)
        cli_render_markdown(content, 2);
    else
        cli_outc('\n');
}

void cli_render_super_agent(const char *content)
{
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL, content);
}

void cli_render_super_agent_begin(void)
{
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(CLI_ROLE_SUPER_AGENT);
    const char *name = cli_render_actor_name(CLI_ACTOR_SUPER_AGENT);
    char hdr[CLI_ROLE_HDR_W + 1];

    cli_build_role_header(hdr, sizeof(hdr), name, NULL);

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
}

void cli_render_user_message(const char *content)
{
    if (g_cli_print_mode)
        return;
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(CLI_ROLE_USER);
    const char *bg = cli_c(CLR_BG_GRAY);
    const char *name = cli_render_actor_name(CLI_ACTOR_USER);
    char hdr[CLI_ROLE_HDR_W + 1];

    snprintf(hdr, sizeof(hdr), "[%s]", name);

    cli_outc('\n');
    cli_out(g);
    cli_out(bg);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
    cli_out(bg);
    cli_out(col);
    cli_out(CLI_ICON_USER " ");
    cli_out(cli_c(CLR_RESET));
    if (content && content[0]) {
        cli_out(bg);
        cli_out(content);
        cli_out(cli_c(CLR_RESET));
    }
    cli_outc('\n');
    cli_outc('\n');
}

void cli_render_sub_agent_line(cli_role_t role, const char *tag, const char *content)
{
    if (g_cli_print_mode && role != CLI_ROLE_ERROR)
        return;
    char hdr[CLI_ROLE_HDR_W + 1];
    const char *t = (tag && tag[0]) ? tag : "exec";

    size_t tag_budget = CLI_ROLE_HDR_W > 12 ? (CLI_ROLE_HDR_W - 12) : 0;
    size_t tag_show = strlen(t);
    if (tag_show > tag_budget)
        tag_show = tag_budget;
    snprintf(hdr, sizeof(hdr), "[Sub %.*s Agent]", (int)tag_show, t);

    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(role);
    const char *icon = (role == CLI_ROLE_ERROR) ? CLI_ICON_CROSS : CLI_ICON_DIAMOND;

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
    cli_out(col);
    cli_out(icon);
    cli_out(" ");
    cli_out(cli_c(CLR_RESET));

    if (content)
        cli_render_markdown(content, 2);
    else
        cli_outc('\n');
}

void cli_render_sub_agent(const char *tag, const char *content)
{
    cli_render_sub_agent_line(CLI_ROLE_SUB_AGENT, tag, content);
}

/* ---- turn separator ---- */

void cli_render_turn_separator(uint64_t elapsed_ms, const char *metrics)
{
    if (g_cli_print_mode)
        return;
    cli_outc('\n');
    uint64_t secs = elapsed_ms / 1000;
    char label[128];
    if (secs < 1)
        snprintf(label, sizeof(label), "完成");
    else if (secs < 60)
        snprintf(label, sizeof(label), "耗时 %llus", (unsigned long long)secs);
    else
        snprintf(label, sizeof(label), "耗时 %llum %02llus",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));

    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t dash_left = 12;
    size_t dash_right = 12;
    (void)rows;
    (void)cols;

    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    for (size_t i = 0; i < dash_left; i++)
        cli_out("─");
    cli_out(" ");
    cli_out(cli_c(CLR_RESET));
    cli_out(label);
    if (metrics && metrics[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(" · ");
        cli_out(cli_c(CLR_RESET));
        cli_out(metrics);
    }
    cli_out(cli_c(CLR_DIM));
    cli_out(" ");
    for (size_t i = 0; i < dash_right; i++)
        cli_out("─");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* ---- progress bar ---- */

void cli_render_progress_bar(double progress, size_t width, const char *label)
{
    if (g_cli_print_mode)
        return;
    if (width < 4) width = 4;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    size_t filled = (size_t)(progress * (double)width);
    if (filled > width) filled = width;

    const char *g = cli_gutter(4);
    cli_out(g);
    if (label && label[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(CLI_ICON_BRANCH " ");
        cli_out(label);
        cli_out(cli_c(CLR_RESET));
        cli_out(": ");
    }

    cli_out("[");
    for (size_t i = 0; i < width; i++)
        cli_out(i < filled ? "█" : "░");
    cli_outf("] %3.0f%%\n", progress * 100.0);
}

/* ---- compact task line for the work-hall board ---- */

void cli_render_task_line(const char *tag, const char *id, const char *state,
                          double progress)
{
    if (g_cli_print_mode)
        return;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    size_t filled = (size_t)(progress * 10.0);
    if (filled > 10) filled = 10;
    char bar[32];
    size_t bo = 0;
    for (size_t b = 0; b < 10; b++) {
        bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", b < filled ? "█" : "░");
    }

    const char *st = state ? state : "?";
    const char *st_col = cli_c(CLR_DIM);
    const char *st_icon = CLI_ICON_BULLET;
    int bar_bright = 0;
    if (strcmp(st, "completed") == 0) {
        st_col = cli_c(CLR_GREEN); st_icon = CLI_ICON_CHECK; bar_bright = 1;
    } else if (strcmp(st, "failed") == 0) {
        st_col = cli_c(CLR_RED); st_icon = CLI_ICON_CROSS; bar_bright = 0;
    } else if (strcmp(st, "running") == 0 || strcmp(st, "active") == 0 ||
               strcmp(st, "queued") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_DIAMOND;
    } else if (strcmp(st, "pending") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_TODO;
    } else if (strcmp(st, "scheduled") == 0) {
        st_col = cli_c(CLR_CYAN); st_icon = CLI_ICON_CLOCK;
    } else if (strcmp(st, "canceled") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_CANCEL;
    }

    const char *g = cli_gutter(2);
    if (tag && tag[0])
        cli_outf("%s%s[%s]%s ", g, cli_c(CLR_DIM), tag, cli_c(CLR_RESET));
    cli_outf("%s%s %s%s%s ", st_col, st_icon, cli_c(CLR_RESET), cli_c(CLR_DIM),
           id ? id : "?");
    cli_outf("%s%s%s ", st_col, cli_state_cn(st), cli_c(CLR_RESET));
    cli_outf("%s[%s]%s %s%3.0f%%%s\n", cli_c(bar_bright ? CLR_GREEN : CLR_DIM), bar,
           cli_c(CLR_RESET), cli_c(bar_bright ? CLR_GREEN : CLR_DIM), progress * 100.0,
           cli_c(CLR_RESET));
}
