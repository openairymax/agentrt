// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_term.c
 * @brief Terminal capability probe implementation.
 *
 * Color level follows the common de-facto chain (NO_COLOR / TERM=dumb /
 * COLORTERM=truecolor / TERM=*256color / basic), so the CLI behaves
 * identically on a laptop, over SSH and in server logs.
 */

#include "cli_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static int g_color_level = -1; /* -1 = not probed yet */
static int g_tty = -1;

/* Fixed bottom strip rows reserved below the scroll region (three-zone
 * layout: hero / dialogue / input). Updated by cli_term_header_pin. */
static int g_footer_lines = 0;

static void cli_term_probe(void)
{
    int level = CLI_TERM_COLOR_BASIC;

    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");
    const char *colorterm = getenv("COLORTERM");

    if (no_color && no_color[0] && strcmp(no_color, "0") != 0) {
        level = CLI_TERM_COLOR_NONE;
    } else if (term && (strcmp(term, "dumb") == 0 || term[0] == '\0')) {
        level = CLI_TERM_COLOR_NONE;
    } else if (colorterm && (strstr(colorterm, "truecolor") != NULL ||
                             strstr(colorterm, "24bit") != NULL)) {
        level = CLI_TERM_COLOR_TRUECOLOR;
    } else if (term && strstr(term, "256color") != NULL) {
        level = CLI_TERM_COLOR_256;
    }
    g_color_level = level;

#ifdef _WIN32
    g_tty = _isatty(_fileno(stdout)) != 0;
#else
    g_tty = isatty(fileno(stdout)) != 0;
#endif
}

void cli_term_init(void)
{
    cli_term_probe();
}

int cli_term_color_level(void)
{
    if (g_color_level < 0)
        cli_term_probe();
    return g_color_level;
}

int cli_color_enabled(void)
{
    if (cli_term_color_level() < CLI_TERM_COLOR_BASIC)
        return 0;
    if (cli_term_is_tty())
        return 1;
    /* Server-grade: piped / logged output stays monochrome even when the
     * environment advertises colors — color only means something on a TTY.
     * FORCE_COLOR overrides that explicitly (Claude Code / Codex parity). */
    const char *force = getenv("FORCE_COLOR");
    return force && force[0] && strcmp(force, "0") != 0;
}

int cli_term_is_tty(void)
{
    if (g_tty < 0)
        cli_term_probe();
    return g_tty;
}

void cli_term_title(const char *title)
{
    if (!title || !cli_term_is_tty())
        return;

    /* OSC 0 (xterm): set the terminal title; strip ESC so an untrusted
     * title cannot inject further escape sequences. */
    fputs("\033]0;", stdout);
    for (const char *p = title; *p; p++) {
        if (*p == '\033')
            break;
        fputc(*p, stdout);
    }
    fputs("\007", stdout);
    fflush(stdout);
}

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

void cli_term_size(int *out_rows, int *out_cols)
{
    if (out_rows)
        *out_rows = 0;
    if (out_cols)
        *out_cols = 0;
    if (!cli_term_is_tty())
        return;

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        if (out_rows)
            *out_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (out_cols)
            *out_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (out_rows && ws.ws_row > 0)
            *out_rows = ws.ws_row;
        if (out_cols && ws.ws_col > 0)
            *out_cols = ws.ws_col;
    }
#endif
}

void cli_term_cursor_to(int row, int col)
{
    if (!cli_term_is_tty())
        return;
    if (row < 1)
        row = 1;
    if (col < 1)
        col = 1;
    printf("\033[%d;%dH", row, col);
}

void cli_term_header_pin(int header_lines, int footer_lines)
{
    g_footer_lines = (footer_lines > 0) ? footer_lines : 0;
    if (!cli_term_is_tty() || header_lines < 1)
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows - g_footer_lines <= header_lines) {
        /* Terminal too short for a pinned header plus a fixed footer strip:
         * keep full-screen scroll (three-zone layout degrades gracefully). */
        g_footer_lines = 0;
        return;
    }
    /* Scroll region = header+1 .. rows-footer_lines; then home the cursor to
     * the first scrollable line so subsequent output stays below the pinned
     * hero. The bottom footer_lines rows stay fixed (input zone). */
    printf("\033[%d;%dr", header_lines + 1, rows - g_footer_lines);
    cli_term_cursor_to(header_lines + 1, 1);
    fflush(stdout);
}

void cli_term_header_unpin(void)
{
    g_footer_lines = 0;
    if (!cli_term_is_tty())
        return;
    fputs("\033[r", stdout);
    /* 部分终端（tmux 等）在重置滚动区（DECSTBM 空参）时会把光标送回
     * 屏幕左上角；显式回到最后一行，保证后续输出（如退出横幅）继续
     * 画在对话结束处，而不是覆盖屏幕顶部的 hero 区。 */
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows > 0)
        printf("\033[%d;1H", rows);
    fflush(stdout);
}

/* ---- fixed bottom input strip (three-zone layout helpers) ----
 *
 * 输入区：pin 时保留的底部行。这些助手只在「TTY + 已保留底部条」时生效，
 * 否则返回 0 / no-op，piped / logged 输出保持传统换行提示符布局。
 */

int cli_term_input_active(void)
{
    if (!cli_term_is_tty() || g_footer_lines <= 0)
        return 0;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    return (rows > g_footer_lines) ? 1 : 0;
}

int cli_term_input_begin(void)
{
    if (!cli_term_input_active())
        return 0;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    /* 三区布局（2026-08-19）：footer >= 2 时，倒数第二行画一条 dim
     * 分隔线，末行留给输入。对话滚动区止于 rows-footer，分隔线与
     * 输入行固定不动，hero / 对话 / 输入三区边界始终清晰。 */
    if (g_footer_lines >= 2 && cols > 0) {
        cli_term_cursor_to(rows - 1, 1);
        printf("\033[2K");
        printf("\033[2m");
        for (int c = 0; c < cols; c++)
            fputs("─", stdout); /* UTF-8 整字符（fputc 只写首字节会乱码） */
        printf("\033[0m");
    }
    /* 定位到末行并整行擦除，随后由调用方打印提示符。 */
    cli_term_cursor_to(rows, 1);
    printf("\033[2K");
    fflush(stdout);
    return 1;
}

void cli_term_input_submit(void)
{
    if (!cli_term_input_active())
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows < 1)
        return;
    /* 擦除用户输入回显，光标回到滚动区末行，对话输出从那里继续流动，
     * 永不覆盖底部输入条。 */
    cli_term_cursor_to(rows, 1);
    printf("\033[2K");
    cli_term_cursor_to(rows - g_footer_lines, 1);
    fflush(stdout);
}

void cli_term_input_hop(void)
{
    if (!cli_term_input_active())
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows < 1)
        return;
    /* 在底部输入条上打印内容（如占位提示符）后，把光标送回滚动区末行，
     * 后续流式输出从对话区继续，不会写进输入条。 */
    cli_term_cursor_to(rows - g_footer_lines, 1);
    fflush(stdout);
}

