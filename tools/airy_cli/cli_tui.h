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

/* ================================================================
 * 阶段 4：TUI 视图模式（tab）+ 面板渲染回调
 *
 * 多视图切换（底部 tab 栏）：对话（默认）/ 任务看板 / 事件流。
 * 看板与事件流是"数据面板"：内容由 main.c 注册的回调生成（面板数据
 * 源），TUI 引擎只负责布局与滚动，不感知 work_hall/hall_store，保持
 * 渲染层与机制层解耦。
 * 按键：Ctrl+P 下一个 tab，Ctrl+O 上一个 tab；看板模式 200ms 轮询
 * 重绘（实时刷新）；面板内 Up/Down/PageUp/PageDown 滚动回放。
 * ================================================================ */

typedef enum {
    CLI_TUI_MODE_CHAT = 0,  /* 对话（默认视图） */
    CLI_TUI_MODE_BOARD,     /* 任务看板（任务大厅实时状态） */
    CLI_TUI_MODE_EVENTS,    /* 事件流（hall_store gseq 全局因果序回放） */
    CLI_TUI_MODE_MAX
} cli_tui_mode_t;

/* 面板内容回调（mode=BOARD/EVENTS 时由 TUI 调用）：
 *   count: 返回当前行数（调用方可借此重建/刷新缓存）
 *   line:  将第 idx 行写入 out（TUI 提供 cap 字节缓冲），返回 1 成功 0 越界 */
typedef size_t (*cli_tui_panel_count_fn)(void *ud);
typedef int (*cli_tui_panel_line_fn)(void *ud, size_t idx, char *out, size_t cap);

/**
 * @brief 绑定一个视图模式的面板数据源（BOARD/EVENTS）。
 *
 * TUI 进入该模式时通过回调拉取内容渲染（引擎不持有面板数据）。
 * ud/count/line 全为 BORROW 语义，生命周期由调用方（main.c）保证；
 * 传 NULL 清除绑定（该模式回退为空面板）。
 */
void cli_tui_set_panel(cli_tui_t *t, cli_tui_mode_t mode, void *ud,
                       cli_tui_panel_count_fn count, cli_tui_panel_line_fn line);

/**
 * @brief 当前视图模式。
 */
cli_tui_mode_t cli_tui_mode(const cli_tui_t *t);

/**
 * @brief 切换到下一个/上一个视图模式（Chat → Board → Events 循环）。
 */
void cli_tui_mode_next(cli_tui_t *t);
void cli_tui_mode_prev(cli_tui_t *t);

/**
 * @brief 面板模式轮询间隔（毫秒）：看板实时刷新节拍（0 = 不轮询）。
 */
#define CLI_TUI_PANEL_POLL_MS 200

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
 * TUI mode: raw-mode key handling with full readline-style editing.
 *   - printable chars insert at the cursor; Backspace / Delete / Ctrl+W edit
 *   - Ctrl+A / Ctrl+E / Left / Right move the caret; Ctrl+U / Ctrl+K kill
 *     to line start / end; Ctrl+T transposes; Alt+b/f or Ctrl+Left/Right
 *     jump by word; Ctrl+W kills the previous word; Ctrl+Y yanks
 *   - Up / Down browse the submitted-command history while typing, or the
 *     conversation viewport on an empty line
 *   - Ctrl+R reverse / Ctrl+S forward incremental search over past commands
 *   - Tab completes "/" commands; PageUp / PageDown / Home / End scroll
 *   - bracketed paste (ESC[200~..ESC[201~) inserts literally
 *   - Ctrl+C / Ctrl+D abort (returns 0, *out_len stays 0)
 * The input line and prompt are rendered as part of the full-screen layout.
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
