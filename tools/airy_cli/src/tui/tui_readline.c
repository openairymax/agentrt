// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_readline.c
 * @brief readline 入口分派（域拆分自 cli_tui.c，2026-08-27）。
 *
 * cli_tui_readline 的入口分派：非 TTY 走 fgets，TTY 走行式
 * tui_readline_line_mode——这是 CLI 唯一的交互形态。全屏分支的循环骨架
 * （面板实时刷新、视图模式切换、面板按键分派、Ctrl+R/S 反向搜索等）
 * 随全屏套件一并退役：cli_tui_enter 恒拒绝、t->active 恒 0，分支不可达。
 * 共享声明见 cli_tui_internal.h。
 */

#include "cli_tui_internal.h"

int cli_tui_readline(cli_tui_t *t, char *buf, size_t cap, size_t *out_len)
{
    if (!buf || cap < 2)
        return 0;
    if (out_len)
        *out_len = 0;

    /* 交互 TTY 走字节级 readline（方向键/PgUp 翻历史、无乱码）；
     * 管道/日志走 fgets。 */
    if (cli_term_is_tty())
        return tui_readline_line_mode(t, buf, cap, out_len);
    if (!fgets(buf, (int)cap, stdin))
        return 0;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    if (out_len)
        *out_len = n;
    return 1;
}
