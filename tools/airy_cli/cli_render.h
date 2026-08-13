// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_render.h
 * @brief airy_cli rendering layer: roles, colors, markdown and progress display.
 *
 * Unified terminal rendering for the AgentRT CLI. Every conversation line is
 * tagged with a speaker role so the user can tell actors apart at a glance:
 *
 *   - [For Thee]     the human operator (cyan)
 *   - [Super Agent]  agentrt itself, replies and decisions (green)
 *   - [Super Think]  system-level thinking / dual-thinking traces (yellow)
 *   - [Sub xxx Agent] sub-agents and executors running nodes (magenta)
 *
 * Design follows the Claude Code / Codex CLI conventions:
 *   - everything left-aligned on a shared gutter
 *   - a small icon set (› • ✓ ✗ ◇ □ ✔ ⚠ ⓘ) encodes status at a glance
 *   - a one-line status indicator (spinner + title + elapsed) during work
 *   - a thin turn separator after each finished turn
 *   - lightweight markdown (headings, lists, checkboxes, quotes, code blocks,
 *     inline bold/emph/code); tables degrade to key/value rows
 *
 * The renderer is line-oriented and stream-safe (no TTY capture, no curses):
 * it prints to stdout, keeps the Linux "small tools that do one thing" spirit
 * and works identically over SSH / plain terminals / logged output. When
 * stdout is not a TTY the animated status line degrades to a static line.
 */

#ifndef AIRY_CLI_RENDER_H
#define AIRY_CLI_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "cli_term.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ANSI color helpers (no-op on Windows). */
#define CLR_BOLD "\033[1m"
#define CLR_DIM "\033[2m"
#define CLR_UNDERLINE "\033[4m"
#define CLR_CYAN "\033[36m"
#define CLR_GREEN "\033[32m"
#define CLR_YELLOW "\033[33m"
#define CLR_RED "\033[31m"
#define CLR_MAGENTA "\033[35m"
#define CLR_BLUE "\033[34m"
#define CLR_BG_GRAY "\033[48;5;236m"
#define CLR_BG_BLUE "\033[48;5;24m"
#define CLR_RESET "\033[0m"

/* Small icon set shared across the CLI (Claude Code / Codex style). */
#define CLI_ICON_USER "\u203A"          /* › user prompt  */
#define CLI_ICON_BULLET "\u2022"        /* • generic event */
#define CLI_ICON_CHECK "\u2713"         /* ✓ success      */
#define CLI_ICON_CROSS "\u2717"         /* ✗ failure      */
#define CLI_ICON_DIAMOND "\u25C7"       /* ◇ plan/think   */
#define CLI_ICON_TODO "\u25A1"          /* □ pending      */
#define CLI_ICON_DONE "\u2714"          /* ✔ completed    */
#define CLI_ICON_WARN "\u26A0"          /* ⚠ warning      */
#define CLI_ICON_INFO "\u24D8"          /* ⓘ info         */
#define CLI_ICON_ERR "\u25A0"           /* ■ error        */
#define CLI_ICON_BRANCH "\u2514"        /* └ detail indent */

typedef enum {
    CLI_ROLE_USER = 0,        /* [For Thee]     cyan     */
    CLI_ROLE_SUPER_AGENT,     /* [Super Agent]  green    */
    CLI_ROLE_SUPER_THINK,     /* [Super Think]  yellow   */
    CLI_ROLE_SUB_AGENT,       /* [Sub xxx Agent] magenta  */
    CLI_ROLE_STATUS,          /* status / info  dim      */
    CLI_ROLE_ERROR,           /* error / warn   red      */
} cli_role_t;

/* Conversation actor label used inside the bracket header. */
typedef enum {
    CLI_ACTOR_USER = 0,
    CLI_ACTOR_SUPER_AGENT,
    CLI_ACTOR_SUPER_THINK,
    CLI_ACTOR_SUB_AGENT,
} cli_actor_t;

/**
 * @brief Return the ANSI color sequence for a role ("" on Windows).
 */
const char *cli_render_role_color(cli_role_t role);

/**
 * @brief Runtime color gate: return `seq` when color is enabled, "" otherwise.
 *
 * Every ANSI sequence in the CLI must flow through this gate so NO_COLOR /
 * piped / logged output stays clean monochrome (server-grade). Backed by
 * cli_term_color_enabled(); on Windows it always returns "".
 */
const char *cli_c(const char *seq);

/**
 * @brief Terminal display width of a UTF-8 string (CJK/full-width = 2 cells).
 *
 * Shared by the banner and the truncation helpers so every column accounts
 * for wide glyphs. Mirrors the width used by wcwidth for the common ranges
 * the CLI emits.
 */
size_t cli_disp_width(const char *s);

/**
 * @brief Render a long text collapsed to at most `max_lines` lines.
 *
 * Progressive disclosure (Claude Code / Codex convention): only the first
 * lines are shown, followed by a dim "└ … N more lines" trailer so a long
 * reasoning trace or tool output cannot flood the terminal. The full text
 * stays available in the logs.
 *
 * @param text      raw text (may contain \n and markdown markers)
 * @param indent    left gutter width in spaces
 * @param max_lines maximum fully rendered lines (>= 1)
 */
void cli_render_collapsed(const char *text, size_t indent, size_t max_lines);

/**
 * @brief Print a dim footer hint line (TTY only).
 *
 * A one-line reminder after each finished turn ("? 快捷键 · /help 命令"),
 * matching the Codex footer convention; no-op when stdout is not a TTY so
 * piped / logged output stays clean.
 */
void cli_render_footer_hint(void);

/**
 * @brief Return a left gutter of `indent` spaces (shared static buffer).
 */
const char *cli_gutter_pad(size_t indent);

/**
 * @brief Return the display name of an actor (e.g. "Super Agent").
 *
 * The user role renders as "For Thee" (the human operator addressed by the
 * agent), matching the four-role conversation scheme.
 */
const char *cli_render_actor_name(cli_actor_t actor);

/**
 * @brief Render a lightweight markdown subset to stdout with a left gutter.
 *
 * Supported while keeping the output clean over plain terminals:
 *   - headings  (# / ## / ###)
 *   - bullets   (- / * items)
 *   - GFM task checkboxes (- [ ] / - [x])
 *   - numbered lists (1. 2. ...)
 *   - quotes    (> line)
 *   - inline    **bold**, *emph*, `code`
 *   - fenced code blocks (``` ... ```) printed verbatim with a gray gutter
 *   - pipe tables degrade to aligned key/value rows
 *
 * @param text   raw text (may contain \n and markdown markers)
 * @param indent left gutter width in spaces
 */
void cli_render_markdown(const char *text, size_t indent);

/**
 * @brief Print a role-tagged line: "[<actor>] <content>".
 *
 * The bracket is colored by role, content is rendered with markdown support.
 *
 * @param role    which role owns the line (drives bracket color)
 * @param actor   actor label shown inside the brackets
 * @param tag     optional extra tag (e.g. node id); pass "" or NULL to skip
 * @param content message text (markdown)
 */
void cli_render_role_line(cli_role_t role, cli_actor_t actor, const char *tag,
                          const char *content);

/**
 * @brief Print a bare markdown block as the super agent's final answer.
 * Shortcut for cli_render_role_line(CLI_ROLE_SUPER_AGENT, ...).
 */
void cli_render_super_agent(const char *content);

/**
 * @brief Print the user's own message: "[For Thee] › text".
 *
 * Uses the CLI_ICON_USER prefix and cyan accent so the human side of the
 * conversation is instantly recognizable (Claude Code caret convention).
 */
void cli_render_user_message(const char *content);

/**
 * @brief Print a sub-agent report: "[Sub <tag> Agent] ◇ <content>".
 *
 * The tag names the sub-agent role (e.g. "exec", "code", "review", "search");
 * the header renders as `[Sub exec Agent]` so every executor type is visible.
 */
void cli_render_sub_agent(const char *tag, const char *content);

/**
 * @brief Print a sub-agent line with an explicit role color.
 *
 * Same header as cli_render_sub_agent, but the caller picks the role:
 * pass CLI_ROLE_ERROR to render failures in red with a "✗" marker while
 * keeping the "[Sub <tag> Agent]" identity.
 */
void cli_render_sub_agent_line(cli_role_t role, const char *tag, const char *content);

/**
 * @brief Monotonic wall-clock in milliseconds (for turn separators).
 */
uint64_t cli_now_ms(void);

/**
 * @brief Print a thin turn separator: "─ Worked for 5s ──────────".
 *
 * Marks the end of one agent turn and shows how long it took, following the
 * Claude Code "Worked for Ns" convention. Metrics (optional) are appended
 * dim after the elapsed text.
 */
void cli_render_turn_separator(uint64_t elapsed_ms, const char *metrics);

/**
 * @brief Render a horizontal progress bar of `width` cells.
 *
 *   [████████████░░░░░░░░]  62%
 *
 * @param progress 0.0 .. 1.0 (clamped)
 * @param width    bar width in cells (>= 4)
 * @param label    optional short label shown left of the bar ("" to skip)
 */
void cli_render_progress_bar(double progress, size_t width, const char *label);

/**
 * @brief Print a compact task line for the work-hall board:
 *
 *   ◇ <id>  <state>  [██████████] 100%
 *
 * @param tag    section tag (e.g. "exec", "sched_d")
 * @param id     task / dag id
 * @param state  current state string (completed/running/...)
 * @param progress 0.0 .. 1.0
 */
void cli_render_task_line(const char *tag, const char *id, const char *state,
                          double progress);

/* ---- one-line status indicator (spinner) ---- */

/**
 * @brief Begin an animated one-line status indicator.
 *
 * Renders "<frame> <title> (<elapsed>)" on a single line. When stdout is not
 * a TTY (piped / logged / SSH without a terminal) the line is printed once
 * without carriage-return animation, so the log stays readable. Non-zero
 * return means animation is disabled (caller should print its own line).
 *
 * @param title short activity title (e.g. "Thinking", "Running")
 * @return 1 if animation started (call cli_spinner_stop when done),
 *         0 if degraded to static (caller prints and skips spinner calls)
 */
int cli_spinner_start(const char *title);

/**
 * @brief Refresh the status line (called by the polling loop or a timer).
 * No-op when animation was not started.
 */
void cli_spinner_tick(void);

/**
 * @brief Erase the status line before printing other full lines.
 *
 * The animated line occupies the current cursor row; any printf before
 * cli_spinner_resume() would otherwise append to it. No-op when the
 * spinner is not animating (static/non-TTY mode).
 */
void cli_spinner_pause(void);

/**
 * @brief Re-render the status line after other output has been printed.
 * Pair with cli_spinner_pause; no-op in static/non-TTY mode.
 */
void cli_spinner_resume(void);

/**
 * @brief Stop the status line and print a completion line.
 *
 * @param ok   1 -> "✓ title (elapsed)" in green; 0 -> "✗ title (elapsed)" red
 * @param detail optional trailing detail text appended dim after the elapsed
 */
void cli_spinner_stop(int ok, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_RENDER_H */
