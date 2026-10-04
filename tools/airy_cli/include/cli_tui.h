// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.h
 * @brief airy_cli 行式终端 UI 引擎（命令历史 / 内建输入法 / 补全）。
 *
 * 会话始终为行式：readline 提供 raw-mode 按键处理、行内编辑与历史浏览，
 * 正文按行直出 stdout。全屏页（pinned header + 滚动对话区 + 底部输入行）
 * 已于 0.1.17 R5-G2 冻结、0.1.19 t143 物理退役；全屏形态唯一由 Rust 侧
 * 控制台渲染层（`airy_cli --tui` 调起的 agentrt-tui）提供。
 *
 * 引擎为 POSIX-only；Windows / 非 TTY 下 readline 退化为 fgets 语义。
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
 * @brief Create the TUI engine handle (line-oriented session state).
 *
 * The engine handle carries the line-mode state (command history, built-in
 * IME, completion). Non-TTY / unsupported platforms degrade to plain stdout
 * (fgets semantics) everywhere.
 *
 * @param out_tui output engine handle
 * @return 0 on success (handle valid), non-zero on allocation failure
 */
int cli_tui_create(cli_tui_t **out_tui);

/**
 * @brief Destroy the TUI and release the line-mode state.
 *
 * Safe to call on any handle (including NULL).
 */
void cli_tui_destroy(cli_tui_t *tui);

/**
 * @brief Get the process-wide TUI engine (NULL when none was created).
 *
 * Convenience accessor for subsystems that need to route input through the
 * line-mode engine (e.g. GCCP interactions) without threading a handle
 * through every call chain.
 */
cli_tui_t *cli_tui_get_default(void);

/**
 * @brief Read one full line of user input.
 *
 * TTY: raw-mode key handling with full readline-style editing.
 *   - printable chars insert at the cursor; Backspace / Delete / Ctrl+W edit
 *   - Ctrl+A / Ctrl+E / Left / Right move the caret; Ctrl+U / Ctrl+K kill
 *     to line start / end; Ctrl+T transposes; Alt+b/f or Ctrl+Left/Right
 *     jump by word; Ctrl+W kills the previous word; Ctrl+Y yanks
 *   - Up / Down browse the submitted-command history
 *   - Ctrl+R reverse / Ctrl+S forward incremental search over past commands
 *   - Tab completes "/" commands
 *   - bracketed paste (ESC[200~..ESC[201~) inserts literally
 *   - Ctrl+C / Ctrl+D abort (returns 0, *out_len stays 0)
 *
 * Non-TTY: behaves like fgets — reads a line from stdin.
 *
 * @param tui       engine handle (may be NULL → fgets semantics)
 * @param buf       output buffer
 * @param cap       buffer capacity (must be >= 2)
 * @param out_len   number of chars read (excluding '\0')
 * @return 1 on success, 0 on EOF / abort
 */
int cli_tui_readline(cli_tui_t *tui, char *buf, size_t cap, size_t *out_len);

/**
 * @brief Snapshot the three-model names for hero re-rendering (2.2.1.3).
 *
 * main 启动时填充（与 cli_print_system_header 同一组模型名）。终端
 * resize 触发 cli_tui_rebuild_three_zone 行渲染重建时，用它重绘 hero，
 * 不依赖 main 的局部变量。传 NULL/"" 表示该模型未设置。
 */
void cli_tui_set_header_models(cli_tui_t *tui, const char *t2, const char *t1f,
                               const char *t1p);

/**
 * @brief Rebuild the line-mode three-zone layout (hero / dialogue / input).
 *
 * 2.2.1.2/2.2.1.3：终端 resize 后滚动区与 hero 可能错位导致重叠。
 * 此函数：解 pin → 清屏 → 用模型名快照重绘 hero → 按角色重放对话
 * 历史。仅 TTY 时生效，否则 no-op。
 */
void cli_tui_rebuild_three_zone(cli_tui_t *tui);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TUI_H */
