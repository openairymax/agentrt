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
 *   - [Dual Think]   dual-thinking system traces (yellow); chain variants
 *                    [Dual Slow Think] (t2) / [Dual Fast Think] (t1-f) /
 *                    [Dual Prof Think] (t1-p) tag the active thinking track
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

/* One-shot server mode flags (defined in main.c): -p/--print suppresses
 * execution chrome; --json switches command/task output to structured JSON.
 * Exported via the render header so command handlers (cmd/*/cli_cmds)
 * can bypass the suppression for command *responses* (server scripting). */
extern int g_cli_print_mode;
extern int g_cli_json_mode;

/* ---- reply folding (2026-08-17) ----
 *
 * 对话不再暴露工具参数与结果内容，只展示"过程"（动作名）；长回复在
 * 完成后折叠为前几行 + 折叠尾，避免占屏（Linux 工程哲学：最小输出，
 * 全量保留在日志/历史中）。 */

/* 一次回复渲染/流式输出的物理行数计量器：供 TTY 折叠（ANSI 上移擦除
 * 重绘）与流式空回复判定使用。统计"逻辑行数 + 物理行数（含软换行）"，
 * 通过 cli_out*() 输出挂钩自动累积，ANSI 转义序列不计入宽度。 */
typedef struct cli_line_meter_s {
    int active;       /* begin 后为 1，end 清 0 */
    int done;         /* phys 已结算（残行已 flush） */
    size_t lines;     /* 逻辑行数（'\n' 计数） */
    size_t phys_lines;/* 物理行数（软换行感知，TTY 擦除量） */
    size_t col;       /* 当前行已用列宽 */
    size_t cols;      /* 终端宽度（0 = 未知，不计算软换行） */
    char *row;        /* 当前行字节缓冲（UTF-8 宽度结算用） */
    size_t row_len;
    size_t row_cap;
    int in_esc;       /* 处于 ANSI 转义序列中（不计入输出） */
} cli_line_meter_t;

/**
 * @brief 开始计量：挂接后续 cli_out*() 输出到 meter，并探测终端宽度。
 * @param m   meter（调用方持有，begin 前应清零）
 */
void cli_render_meter_begin(cli_line_meter_t *m);

/**
 * @brief 结束计量：解除挂接（后续输出不再计入）。
 */
void cli_render_meter_end(cli_line_meter_t *m);

/**
 * @brief 结算并返回已输出的物理行数（幂等，可多次调用）。
 *
 * 残留的末行（无 '\n' 结尾）一并计入，供 TTY 擦除重绘精确计算
 * 需要上移的行数。
 */
size_t cli_render_meter_phys(cli_line_meter_t *m);

/* 空回复占位（2026-08-17）：模型未产生文本回复（thinking 模型可能
 * 只输出 reasoning_content，或 provider 异常）时渲染明确提示，避免
 * 对话中出现"空返回"却无任何说明。 */
#define CLI_REPLY_EMPTY_HINT "（未产生回复：模型可能仅生成了思考内容，请重试）"

/* Opaque TUI engine handle (full definition in cli_tui.h). */
struct cli_tui_s;

/**
 * @brief Attach (or detach, tui=NULL) the full-screen TUI engine.
 *
 * Once attached, every cli_out*() call below routes through the engine so
 * the output lands in the TUI line history (full-screen page). Detaching
 * restores plain stdout streaming. Call before the banner render so the
 * header lines are captured for the pinned header.
 */
void cli_render_set_tui(struct cli_tui_s *tui);

/**
 * @brief Unified output primitives.
 *
 * These are the single exit point for every conversation/render line:
 *   - cli_out(s)      writes a NUL-terminated string
 *   - cli_outn(s, n)  writes n bytes
 *   - cli_outc(c)     writes one char
 *   - cli_outf(fmt…)  formatted output
 * With an attached TUI they flow into the full-screen history; otherwise
 * they stream straight to stdout (stream-safe, pipe/log friendly).
 */
void cli_out(const char *s);
void cli_outn(const char *s, size_t n);
void cli_outc(char c);
#if defined(__GNUC__)
void cli_outf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void cli_outf(const char *fmt, ...);
#endif

/**
 * @brief One-shot server mode (-p) progress channel: "[tag] message" to stderr.
 *
 * stdout stays reserved for the final answer (Claude Code -p convention), so
 * scripts can parse the reply while operators watch the pipeline progress on
 * stderr. No-op in interactive mode, where the role chrome already shows
 * progress.
 *
 * @param tag  short phase tag (e.g. "plan", "submit", "status")
 * @param fmt  printf-style message
 */
#if defined(__GNUC__)
void cli_trace(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void cli_trace(const char *tag, const char *fmt, ...);
#endif

/* Shortcuts mirroring the historical fputs/fputc(stdout) call sites. */
#define cli_puts(s) cli_out(s)
#define cli_putc(c) cli_outc(c)

/* ANSI color helpers. 2026-08-25：经 cli_theme_seq() 解析，随终端背景
 * 在浅色 / 深色主题间自动切换（见 cli_term.h theme 段；调用点零改动，
 * 编译期常量场景不可用，仅用于 printf 参数）。 */
#define CLR_BOLD cli_theme_seq(CLI_TH_BOLD)
#define CLR_DIM cli_theme_seq(CLI_TH_DIM)
#define CLR_UNDERLINE cli_theme_seq(CLI_TH_UNDERLINE)
#define CLR_CYAN cli_theme_seq(CLI_TH_CYAN)
#define CLR_GREEN cli_theme_seq(CLI_TH_GREEN)
#define CLR_YELLOW cli_theme_seq(CLI_TH_YELLOW)
#define CLR_RED cli_theme_seq(CLI_TH_RED)
#define CLR_MAGENTA cli_theme_seq(CLI_TH_MAGENTA)
#define CLR_BLUE cli_theme_seq(CLI_TH_BLUE)
#define CLR_BG_GRAY cli_theme_seq(CLI_TH_BG_GRAY)
#define CLR_BG_BLUE cli_theme_seq(CLI_TH_BG_BLUE)
#define CLR_REVERSE cli_theme_seq(CLI_TH_REVERSE) /* 反显：输入光标闪烁 */
#define CLR_RESET cli_theme_seq(CLI_TH_RESET)

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
#define CLI_ICON_CLOCK "\u25F7"         /* ◷ scheduled    */
#define CLI_ICON_CANCEL "\u2298"        /* ⊘ canceled     */
#define CLI_ICON_ERR "\u25A0"           /* ■ error        */
#define CLI_ICON_BRANCH "\u2514"        /* └ detail indent */
#define CLI_ICON_TOOL "\u26CF"          /* ⛏ tool invocation */
#define CLI_ICON_HALF "\u25D0"          /* ◐ in-progress (live plan board) */

/* 非 TTY 状态行辅助（2026-08-16）：状态图标 + 紧凑进度条，供 -p 模式下
 * [progress]/[status]/[result] 等 cli_trace 行使用，让管道/CI 输出同样
 * 一眼可读（Linux 工程哲学：最小但完整的结构化信号）。 */
const char *cli_icon_for_state(const char *state);
void cli_compact_bar(char *out, size_t cap, double progress, size_t cells);

/* 用户可读的错误描述（2026-08-17）：内部错误码（AIRY_ERR_* 负值）对用户
 * 无意义，统一映射为可理解的中文描述；未识别码返回通用描述。返回值指向
 * 静态字符串，调用方直接用于打印即可。 */
const char *cli_err_desc(int err);

/* 任务状态中文化（2026-08-17）：内部状态机字符串（completed/running/...）
 * 对用户无意义，统一映射为简短中文；未识别状态返回原文。返回值指向静态
 * 字符串，直接用于进度行展示即可。 */
const char *cli_state_cn(const char *state);

typedef enum {
    CLI_ROLE_USER = 0,        /* [For Thee]     cyan     */
    CLI_ROLE_SUPER_AGENT,     /* [Super Agent]  green    */
    CLI_ROLE_DUAL_THINK,      /* [Dual Think]   yellow   */
    CLI_ROLE_SUB_AGENT,       /* [Sub xxx Agent] magenta  */
    CLI_ROLE_STATUS,          /* status / info  dim      */
    CLI_ROLE_TRACE,           /* internal trace  dim-gray */
    CLI_ROLE_ERROR,           /* error / warn   red      */
} cli_role_t;

/* Conversation actor label used inside the bracket header.
 * Dual-thinking chain（2026-08-17）：t2=慢思考、t1-f=快思考、t1-p=专业
 * 思考，展示时按实时思考链区分，通用场景回落 CLI_ACTOR_DUAL_THINK。 */
typedef enum {
    CLI_ACTOR_USER = 0,
    CLI_ACTOR_SUPER_AGENT,
    CLI_ACTOR_DUAL_THINK,
    CLI_ACTOR_DUAL_SLOW_THINK,  /* t2     [Dual Slow Think] */
    CLI_ACTOR_DUAL_FAST_THINK,  /* t1-f   [Dual Fast Think] */
    CLI_ACTOR_DUAL_PROF_THINK,  /* t1-p   [Dual Prof Think] */
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
 * @brief Terminal display width of the first `n` bytes of a UTF-8 string.
 *
 * Same width rules as cli_disp_width but stops at a byte boundary, so the
 * cursor can be positioned mid-line (edit position) without miscounting
 * partial multi-byte sequences.
 */
size_t cli_disp_width_of(const char *s, size_t n);

/**
 * @brief Largest byte count <= max_bytes that ends on a UTF-8 boundary.
 *
 * Truncating with snprintf precision can cut a multi-byte char in half and
 * print a stray continuation byte. Call this before any byte-wise
 * truncation of user/agent text (tags, headers, previews).
 */
size_t cli_utf8_safe_len(const char *s, size_t max_bytes);

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
 * @param weak      non-zero renders the body dim (internal trace, e.g.
 *                  chain-of-thought) so it never competes with the reply
 */
void cli_render_collapsed(const char *text, size_t indent, size_t max_lines, int weak);

/**
 * @brief Return a left gutter of `indent` spaces (shared static buffer).
 */
const char *cli_gutter(size_t indent);
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
 * @brief Begin a streaming super-agent reply: print the role header without
 * a newline so streamed chunks can follow on the same line.
 *
 * The header renders exactly like cli_render_role_line's "[Super Agent]" but
 * the trailing newline is omitted; the caller then prints the streamed text
 * and finishes the line itself (newline or fold trailer).
 */
void cli_render_super_agent_begin(void);

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
 * @brief Render an LLM tool invocation (Claude Code tool-use card):
 *
 *   ⛏ web_search("agentrt")        (magenta icon + cyan name + dim args)
 *
 * @param name tool name (web_search / web_fetch)
 * @param args preview of the tool arguments (single line, trimmed)
 */
void cli_render_tool_use(const char *name, const char *args);

/**
 * @brief Render a tool-execution result, folded to a summary line:
 *
 *   [Sub search Agent] ✓ web_search — <first line of output>
 *   [Sub search Agent] ✗ web_search — <error>
 *
 * @param name   tool name
 * @param text   result output / error text
 * @param ok     true when the tool returned success
 */
void cli_render_tool_result(const char *name, const char *text, int ok);

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

/**
 * @brief Cancel the status line silently (no completion line).
 *
 * Erases the animated line (or leaves the degraded static line in place) and
 * clears the spinner state without printing a ✓/✗ line. Used by the
 * streaming chat path, where the spinner hands over to the streamed reply
 * and a separate completion line would duplicate the output.
 */
void cli_spinner_cancel(void);

/**
 * @brief Render a phase indicator line for task execution stages.
 *
 * Displays a dim separator with a cyan diamond and the phase label:
 *
 *   ◇ 认知规划 ─────────────────────────────
 *
 * Provides clear visual hierarchy during multi-phase task execution so the
 * user can track progression at a glance (Claude Code phase convention).
 * No-op in -p server mode (stdout stays clean for piping).
 *
 * @param label short phase name (e.g. "认知规划", "执行提交", "结果汇总")
 */
void cli_render_phase(const char *label);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_RENDER_H */
