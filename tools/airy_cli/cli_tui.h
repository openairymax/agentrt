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
 * 源），TUI 引擎只负责布局、选择与滚动，不感知 work_hall/hall_store，
 * 保持渲染层与机制层解耦。可操作动作（详情/取消/过滤）经面板 action
 * 回调路由回 CLI 层执行（引擎不持有机制状态）。
 * 按键：Ctrl+P 下一个 tab，Ctrl+O 上一个 tab，F6 直达任务看板，
 * F7 直达事件流；面板模式 200ms 轮询重绘（实时刷新）。
 *   - 任务看板：↑/↓ 移动选择，Enter 查看任务详情（Esc 返回列表），
 *     x 请求取消选中任务，PageUp/PageDown/Home/End 按页/首尾跳选。
 *   - 事件流：f 切换实时跟随（尾部刷新），c 循环类别过滤，其余
 *     方向键回放浏览。
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

/* 面板可操作动作（mode=BOARD/EVENTS 时由 TUI 引擎转发给 action 回调）：
 *   BOARD:   DETAIL  查看选中任务详情（TUI 以 t->detail 展示，Esc 返回）
 *            CANCEL  请求取消选中任务（out 返回结果提示）
 *   EVENTS:  CYCLE_FILTER 循环类别过滤（out 返回当前过滤名） */
#define CLI_TUI_ACT_DETAIL        1
#define CLI_TUI_ACT_CANCEL        2
#define CLI_TUI_ACT_CYCLE_FILTER  3

typedef int (*cli_tui_panel_action_fn)(void *ud, int action, size_t sel,
                                       char *out, size_t cap);

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
 * @brief 绑定面板的可操作动作回调（BOARD/EVENTS；NULL 清除）。
 *
 * 动作由 TUI 引擎按键触发，回调在 CLI 层执行（查询/取消等），结果
 * 文本写回 out 供 TUI 在标题栏展示；DETAIL 动作额外把详情写入
 * t->detail（引擎持有缓冲），TUI 切换为详情视图。
 */
void cli_tui_set_panel_action(cli_tui_t *t, cli_tui_mode_t mode,
                              cli_tui_panel_action_fn fn);

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
 * @brief 直接切换到指定视图模式（F6 → BOARD / F7 → EVENTS）。
 *
 * 目标模式为 BOARD 时重置选择与详情视图；EVENTS 保持跟随/过滤状态。
 */
void cli_tui_mode_set(cli_tui_t *t, cli_tui_mode_t m);

/**
 * @brief 面板模式轮询间隔（毫秒）：看板实时刷新节拍（0 = 不轮询）。
 */
#define CLI_TUI_PANEL_POLL_MS 200

/**
 * @brief Create the TUI engine handle (does NOT enter the full-screen page).
 *
 * 2.3.7 (2026-08-17): the interactive session defaults to the line-oriented
 * streaming renderer (typewriter + folded thought chain); the full-screen
 * page is entered explicitly with cli_tui_enter() (F8) so the CLI keeps its
 * streaming interaction until the user asks for the full-screen view.
 * Non-TTY / unsupported platforms degrade to plain stdout everywhere.
 *
 * @param out_tui output engine handle (may return a handle with active==0)
 * @return 0 on success (handle valid), non-zero on allocation failure
 */
int cli_tui_create(cli_tui_t **out_tui);

/**
 * @brief Enter the full-screen page (alt screen + raw mode + redraw).
 *
 * Safe no-op when already active or when stdout is not a TTY / the terminal
 * is too small. Routes the renderer into the TUI (cli_render_set_tui) so
 * subsequent output lands in the page history. The conversation history
 * accumulated while in line mode is replayed via cli_tui_replay_history()
 * by the caller so the page is not empty on switch-in.
 *
 * @return 0 on success, non-zero when the page could not be entered
 */
int cli_tui_enter(cli_tui_t *tui);

/**
 * @brief Leave the full-screen page (restore alt screen + raw mode).
 *
 * Safe no-op when inactive. Routes the renderer back to plain stdout.
 *
 * @return 0 on success, non-zero on failure
 */
int cli_tui_leave(cli_tui_t *tui);

/**
 * @brief Replay the line-mode conversation history (g_history) into the
 * TUI history so a switch-in shows prior turns. No-op when inactive.
 */
void cli_tui_replay_history(cli_tui_t *tui);

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
 * @brief Current committed history length (for fold-span bookkeeping).
 */
size_t cli_tui_hist_count(cli_tui_t *tui);

/**
 * @brief Mark [start, hist.count) as the latest reply fold span.
 *
 * Live-tail rendering shows only the first CLI_REPLY_FOLD_KEEP lines of the
 * span plus a dim fold trailer, so a long reply never floods the viewport;
 * browsing (scroll_off > 0) reveals the full history. A reply whose span is
 * at most CLI_REPLY_FOLD_MAX lines renders unfolded — no visual cost for
 * short replies. Call after the reply rendered, with `start` captured via
 * cli_tui_hist_count() before rendering.
 *
 * @param tui    engine handle
 * @param start  first history index of the reply
 */
void cli_tui_fold_last(cli_tui_t *tui, size_t start);

/**
 * @brief Clear any active fold span (next reply starts fresh).
 */
void cli_tui_fold_clear(cli_tui_t *tui);

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
 * @return 1 on success, 0 on EOF / abort,
 *         2 = line mode F8 → request to enter the full-screen page
 *            (*out_len = 0),
 *         3 = full-screen F8 → request to leave back to line mode
 *            (*out_len = 0)
 */
int cli_tui_readline(cli_tui_t *tui, char *buf, size_t cap, size_t *out_len);

/**
 * @brief Set the session status text shown right-aligned on the input line.
 *
 * Claude-Code-style context indicator (model · messages · elapsed), rendered
 * dim so it never competes with the prompt. Hidden automatically when the
 * typed input would overlap it. Pass "" / NULL to clear.
 */
void cli_tui_set_status(cli_tui_t *tui, const char *status);

/**
 * @brief Redraw the full screen immediately (e.g. before a long blocking
 * call that emits progress without reading input).
 */
void cli_tui_redraw(cli_tui_t *tui);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TUI_H */
