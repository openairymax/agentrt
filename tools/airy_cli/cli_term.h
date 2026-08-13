// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_term.h
 * @brief Terminal capability probe: color level, TTY detection, title.
 *
 * Server-friendly rendering needs the CLI to know what it is talking to:
 *   - stdout piped to a log  -> monochrome, static lines
 *   - dumb/NO_COLOR terminal -> monochrome
 *   - 16 / 256 / truecolor   -> progressively richer palettes
 *
 * cli_term_init() probes once at startup; every later query is a cached read.
 */

#ifndef AIRY_CLI_TERM_H
#define AIRY_CLI_TERM_H

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_TERM_COLOR_NONE 0       /* monochrome (NO_COLOR / dumb)      */
#define CLI_TERM_COLOR_BASIC 1      /* ANSI 16                           */
#define CLI_TERM_COLOR_256 2        /* 256-color palette                 */
#define CLI_TERM_COLOR_TRUECOLOR 3  /* 24-bit RGB                        */

/* One-time probe; safe to call before any output. Idempotent. */
void cli_term_init(void);

/* Cached color support level (0..3); valid after cli_term_init. */
int cli_term_color_level(void);

/* True when color output is allowed (level >= BASIC && !NO_COLOR). */
int cli_color_enabled(void);

/* True when stdout is a terminal (live animation / OSC title allowed). */
int cli_term_is_tty(void);

/* OSC 0 terminal title; no-op when stdout is not a TTY or title is NULL.
 * Control characters are stripped so untrusted titles cannot inject escape
 * sequences (Trojan-Source hardening, same intent as Codex terminal_title). */
void cli_term_title(const char *title);

/* ---- fixed header support (TTY only, ANSI scroll region) ---- */

/**
 * @brief Query the terminal size (rows x cols), 0 when unknown / not a TTY.
 *
 * Uses TIOCGWINSZ on POSIX; returns 0,0 on Windows or when stdout is not a
 * terminal (callers then fall back to the line-oriented layout).
 *
 * @param out_rows terminal rows (>= 1) or 0
 * @param out_cols terminal columns (>= 1) or 0
 */
void cli_term_size(int *out_rows, int *out_cols);

/**
 * @brief Lock the scrolling region below the fixed header.
 *
 * Prints "\033[<top>;<bottom>r" then homes the cursor to the first line of
 * the region, so every subsequent newline scrolls only inside the region and
 * the header lines above it stay pinned. No-op when stdout is not a TTY.
 *
 * @param header_lines number of pinned header lines (>= 1)
 */
void cli_term_header_pin(int header_lines);

/**
 * @brief Release the pinned header: restore full-screen scrolling.
 *
 * Prints "\033[r" (entire screen scrolls again). No-op when stdout is not a
 * TTY. Safe to call even when no region was pinned.
 */
void cli_term_header_unpin(void);

/**
 * @brief Move the cursor to an absolute 1-based position.
 *
 * @param row 1-based row
 * @param col 1-based column
 */
void cli_term_cursor_to(int row, int col);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TERM_H */
