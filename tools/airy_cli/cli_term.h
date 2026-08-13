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

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TERM_H */
