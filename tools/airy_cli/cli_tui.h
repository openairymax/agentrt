// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.h
 * @brief airy_cli full-screen terminal UI engine.
 *
 * Turns the CLI into a Claude-Code-style full-screen page on interactive
 * terminals: a pinned header on top, a scrollable conversation area in the
 * middle (Up/Down / PageUp / PageDown / mouse wheel browse history) and a
 * bottom input line. Piped / logged / non-TTY output keeps the existing
 * line-oriented rendering untouched (all emit functions degrade to stdout).
 *
 * Architecture:
 *   - The rendering layer (cli_render.c / cli_display.c / ...) emits every
 *     line through cli_tui_emit() instead of writing to stdout directly.
 *   - In TUI mode the emitted bytes are folded into an in-memory line
 *     history (\n commits a line, \r rewinds the current partial line for
 *     spinner redraws); a full redraw renders the pinned header + the
 *     visible viewport + the bottom input line.
 *   - Input goes through cli_tui_readline(): raw-mode key handling with
 *     inline editing and browse keys, so arrow keys scroll the history
 *     instead of corrupting the prompt.
 *
 * The engine is POSIX-only; on Windows / non-TTY it stays inert and all
 * functions become pass-throughs.
 */

#ifndef AIRY_CLI_TUI_H
#define AIRY_CLI_TUI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cli_tui_s cli_tui_t;

/**
 * @brief Create and activate the full-screen TUI (if stdout is a TTY).
 *
 * Enters the alternate screen, switches stdin to raw mode, clears the screen
 * and primes the line history. On non-TTY / unsupported platforms returns a
 * handle with active == 0 so every later call degrades to plain stdout.
 *
 * @param out_tui output engine handle (may return a handle with active==0)
 * @return 0 on success (handle valid), non-zero on allocation failure
 */
int cli_tui_create(cli_tui_t **out_tui);

/**
 * @brief Destroy the TUI and restore the terminal (alt screen, raw mode).
 *
 * Safe to call on any handle (including inactive ones).
 */
void cli_tui_destroy(cli_tui_t *tui);

/**
 * @brief True when the full-screen page is active (stdout was a TTY).
 */
int cli_tui_active(const cli_tui_t *tui);

/**
 * @brief Get the process-wide TUI engine (NULL when none was created).
 *
 * Convenience accessor for subsystems that need to route input through the
 * full-screen engine (e.g. GCCP interactions) without threading a handle
 * through every call chain.
 */
cli_tui_t *cli_tui_get_default(void);

/**
 * @brief Emit raw output bytes into the engine.
 *
 * TUI mode: bytes are folded into the current partial line; '\n' commits the
 * line to the history, '\r' rewinds the partial line (spinner redraws).
 * A redraw is scheduled so the new line appears on screen. Non-TUI mode:
 * writes the bytes straight to stdout (stream-safe pass-through).
 *
 * @param tui  engine handle (may be NULL → stdout pass-through)
 * @param data bytes to emit
 * @param len  byte count
 */
void cli_tui_emit(cli_tui_t *tui, const char *data, size_t len);

/**
 * @brief Flush any pending partial line to the history (caller finished a
 * line without '\n', e.g. streamed reply terminator). Non-TUI: no-op.
 */
void cli_tui_emit_flush(cli_tui_t *tui);

/**
 * @brief Mark the current history length as the pinned header boundary.
 *
 * Everything already emitted becomes the fixed header; the scroll viewport
 * starts below it. Typically called after the banner + model panel render.
 */
void cli_tui_pin_header(cli_tui_t *tui);

/**
 * @brief Read one full line of user input (TUI mode) or from stdin.
 *
 * TUI mode: raw-mode key handling. Printable chars edit the input line;
 * Backspace deletes; Enter submits; Up/Down / PageUp / PageDown / wheel
 * browse the conversation history (any editing key returns to the live
 * tail); Ctrl+C / Ctrl+D abort (returns 0, *out_len stays 0). The input
 * line and prompt are rendered as part of the full-screen layout.
 *
 * Non-TUI mode: behaves like fgets — reads a line from stdin.
 *
 * @param tui       engine handle (may be NULL → fgets semantics)
 * @param buf       output buffer
 * @param cap       buffer capacity (must be >= 2)
 * @param out_len   number of chars read (excluding '\0')
 * @return 1 on success, 0 on EOF / abort
 */
int cli_tui_readline(cli_tui_t *tui, char *buf, size_t cap, size_t *out_len);

/**
 * @brief Redraw the full screen immediately (e.g. before a long blocking
 * call that emits progress without reading input).
 */
void cli_tui_redraw(cli_tui_t *tui);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TUI_H */
