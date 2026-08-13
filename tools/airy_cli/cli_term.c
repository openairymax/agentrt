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

void cli_term_header_pin(int header_lines)
{
    if (!cli_term_is_tty() || header_lines < 1)
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows <= header_lines + 1) {
        /* Terminal too short for a pinned header: keep full-screen scroll. */
        return;
    }
    /* Scroll region = header+1 .. rows; then home the cursor to the first
     * scrollable line so subsequent output stays below the pinned header. */
    printf("\033[%d;%dr", header_lines + 1, rows);
    cli_term_cursor_to(header_lines + 1, 1);
    fflush(stdout);
}

void cli_term_header_unpin(void)
{
    if (!cli_term_is_tty())
        return;
    fputs("\033[r", stdout);
    fflush(stdout);
}

