// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_render.c
 * @brief TUI 引擎渲染域（0.1.19 t143：全屏渲染引擎物理退役）。
 *
 * 全屏套件（备用屏页、三区布局全量/增量重绘、header/viewport/input 渲染、
 * 硬件信息面板 F2）已随 0.1.17 R5-G2 冻结退役，本文件仅保留行式 readline
 * 所需的终端原语：尺寸探测（tui_get_size）与转义输出（tui_write_literal /
 * tui_clear_line）。全屏渲染唯一由 Rust 侧控制台渲染层（`airy_cli --tui`
 * 调起的 agentrt-tui）提供。
 */

#include "cli_tui_internal.h"

void tui_get_size(cli_tui_t *t)
{
    if (!t)
        return;
    /* 复用 cli_term_size 的完整回退链（ioctl → COLUMNS/LINES → 默认
     * 24/80），与 cli_term_header_pin 的 rows 计算保持同步。 */
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    t->rows = (rows > 0) ? rows : 24;
    t->cols = (cols > 0) ? cols : 80;
}

void tui_write_literal(const char *s)
{
    if (s && *s)
        fputs(s, stdout);
}

void tui_clear_line(void)
{
    tui_write_literal("\033[2K");
}
