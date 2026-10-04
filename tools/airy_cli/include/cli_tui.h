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

/* ================================================================
 * 视图模式（tab）+ 面板数据源
 *
 * 多视图切换：对话（默认）/ 任务看板 / 事件流。看板与事件流是"数据
 * 面板"：内容由 main.c 注册的回调生成，引擎只负责选择与滚动，不感知
 * work_hall/hall_store，保持渲染层与机制层解耦。可操作动作（详情/取消/
 * 过滤）经面板 action 回调路由回 CLI 层执行（引擎不持有机制状态）。
 * 面板渲染依赖全屏页，随 0.1.19 t143 退役；本组绑定 API 待 t144 收敛。
 * ================================================================ */

typedef enum {
    CLI_TUI_MODE_CHAT = 0,  /* 对话（默认视图） */
    CLI_TUI_MODE_BOARD,     /* 任务看板（任务大厅实时状态） */
    CLI_TUI_MODE_EVENTS,    /* 事件流（hall_store gseq 全局因果序回放） */
    CLI_TUI_MODE_HW,        /* 硬件信息（本机 CPU/内存/OS/架构实时展示） */
    CLI_TUI_MODE_MEM,       /* 记忆链（mem_d 最近记忆记录实时展示） */
    CLI_TUI_MODE_MAX
} cli_tui_mode_t;

/* 面板内容回调：
 *   count: 返回当前行数（调用方可借此重建/刷新缓存）
 *   line:  将第 idx 行写入 out（引擎提供 cap 字节缓冲），返回 1 成功 0 越界 */
typedef size_t (*cli_tui_panel_count_fn)(void *ud);
typedef int (*cli_tui_panel_line_fn)(void *ud, size_t idx, char *out, size_t cap);

/* 面板可操作动作（mode=BOARD/EVENTS 时由引擎转发给 action 回调）：
 *   BOARD:   DETAIL  查看选中任务详情；CANCEL 请求取消选中任务
 *   EVENTS:  CYCLE_FILTER 循环类别过滤（out 返回当前过滤名） */
#define CLI_TUI_ACT_DETAIL        1
#define CLI_TUI_ACT_CANCEL        2
#define CLI_TUI_ACT_CYCLE_FILTER  3

typedef int (*cli_tui_panel_action_fn)(void *ud, int action, size_t sel,
                                       char *out, size_t cap);

/**
 * @brief 绑定一个视图模式的面板数据源（BOARD/EVENTS）。
 *
 * ud/count/line 全为 BORROW 语义，生命周期由调用方（main.c）保证；
 * 传 NULL 清除绑定（该模式回退为空面板）。
 */
void cli_tui_set_panel(cli_tui_t *t, cli_tui_mode_t mode, void *ud,
                       cli_tui_panel_count_fn count, cli_tui_panel_line_fn line);

/**
 * @brief 绑定面板的可操作动作回调（BOARD/EVENTS；NULL 清除）。
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
 * @brief 直接切换到指定视图模式（BOARD / EVENTS 直达）。
 */
void cli_tui_mode_set(cli_tui_t *t, cli_tui_mode_t m);

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
