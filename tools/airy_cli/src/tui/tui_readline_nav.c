// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_readline_nav.c
 * @brief TUI 引擎全屏 readline 导航编辑键分派（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 方向键（Up/Down 翻会话视口 / 浏览已提交命令历史、Left/Right 按 UTF-8
 * 字符移动光标、Ctrl/Alt+Left/Right 按词移动）、bracketed paste（原始字节
 * 直插至 ESC[201~）、PgUp/PgDn/Home/End（滚动视口）、Del 与普通字符输入
 * （UTF-8 完整序列才重绘）。由 tui_readline.c 的 cli_tui_readline 调用。
 * 共享声明见 cli_tui_internal.h。
 */

#include "cli_tui_internal.h"

/* 返回 0 = 正常处理，继续主循环；-1 = 请求终止 readline（粘贴内 EOF）。 */
int tui_readline_arrow_keys(cli_tui_t *t, int key)
{
    switch (key) {
    case TUI_KEY_UP:
        /* 0.1.6h 修复：分支仅依输入是否为空判定。原条件
         * `input_len > 0 || cmd_hist.count > 0` 让空输入 + 有命令历史时
         * Up 被命令历史劫持（idx==count 时静默无动作），会话视口永远
         * 无法滚动——社区反馈"不能上下翻动记录"。空输入应滚动会话视口；
         * 有输入才浏览命令历史（readline 惯例）。 */
        if (t->input_len > 0) {
            /* With typed text: browse the submitted-command history. */
            if (t->cmd_hist_idx > 0) {
                tui_cmd_hist_save_draft(t);
                t->cmd_hist_idx--;
                tui_cmd_hist_apply(t, t->cmd_hist_idx);
                tui_render_input(t);
                fflush(stdout);
            }
        } else {
            /* Empty input: browse the conversation viewport. */
            if (t->scroll_off < t->hist.count)
                t->scroll_off++;
            cli_tui_redraw(t);
        }
        break;
    case TUI_KEY_DOWN:
        /* 0.1.6h 修复：与 Up 对称，空输入滚动会话视口（见上）。 */
        if (t->input_len > 0) {
            if (t->cmd_hist_idx < t->cmd_hist.count) {
                t->cmd_hist_idx++;
                if (t->cmd_hist_idx >= t->cmd_hist.count) {
                    /* Past the newest: restore the preserved draft. */
                    if (t->cmd_hist_edit && t->cmd_hist_edit_len > 0) {
                        t->input_len = t->cmd_hist_edit_len;
                        AIRY_MEMCPY(t->input, t->cmd_hist_edit, t->cmd_hist_edit_len);
                        t->input[t->input_len] = '\0';
                        t->input_col = t->input_len;
                    } else {
                        t->input_len = 0;
                        t->input_col = 0;
                        if (t->input)
                            t->input[0] = '\0';
                    }
                } else {
                    tui_cmd_hist_apply(t, t->cmd_hist_idx);
                }
                tui_render_input(t);
                fflush(stdout);
            }
        } else {
            if (t->scroll_off > 0)
                t->scroll_off--;
            cli_tui_redraw(t);
        }
        break;
    case TUI_KEY_LEFT:
        if (t->input_col > 0) {
            /* Move one UTF-8 char left (rewind continuation bytes). */
            size_t n = t->input_col;
            while (n > 1 && ((unsigned char)t->input[n - 1] & 0xC0) == 0x80)
                n--;
            if (n > 0)
                n--;
            t->input_col = n;
            tui_render_input(t);
            fflush(stdout);
        }
        break;
    case TUI_KEY_RIGHT:
        if (t->input_col < t->input_len) {
            /* Move one UTF-8 char right (skip continuation bytes). */
            size_t n = t->input_col + 1;
            while (n < t->input_len && ((unsigned char)t->input[n] & 0xC0) == 0x80)
                n++;
            t->input_col = n;
            tui_render_input(t);
            fflush(stdout);
        }
        break;
    case TUI_KEY_CTRL_LEFT:
    case TUI_KEY_ALT_LEFT:
    case TUI_KEY_ALT_B:
        tui_input_word_left(t);
        tui_render_input(t);
        fflush(stdout);
        break;
    case TUI_KEY_CTRL_RIGHT:
    case TUI_KEY_ALT_RIGHT:
    case TUI_KEY_ALT_F:
        tui_input_word_right(t);
        tui_render_input(t);
        fflush(stdout);
        break;
    case TUI_KEY_PASTE_START:
        /* bracketed paste: insert raw bytes literally until ESC[201~. */
        t->paste_active = 1;
        while (t->paste_active) {
            char pb;
            int peof = 0;
            if (!tui_wait_byte(t, &pb, -1, &peof)) {
                if (peof)
                    return -1; /* EOF during paste: terminate readline */
                continue; /* resize tick / EINTR: keep pasting */
            }
            if (pb == 0x1b) {
                if (tui_paste_read_end(t)) {
                    t->paste_active = 0;
                    break;
                }
                continue; /* stray ESC inside paste: skip */
            }
            if (pb == '\n' || pb == '\r')
                pb = ' ';
            tui_input_append(t, pb);
        }
        tui_render_input(t);
        fflush(stdout);
        break;
    case TUI_KEY_DEL:
        tui_input_delete_fwd(t);
        tui_render_input(t);
        fflush(stdout);
        break;
    case TUI_KEY_PGUP:
        t->scroll_off += tui_middle_rows(t) - 1;
        cli_tui_redraw(t);
        break;
    case TUI_KEY_PGDN:
        if (t->scroll_off > tui_middle_rows(t) - 1)
            t->scroll_off -= tui_middle_rows(t) - 1;
        else
            t->scroll_off = 0;
        cli_tui_redraw(t);
        break;
    case TUI_KEY_HOME:
        t->scroll_off = t->hist.count;
        cli_tui_redraw(t);
        break;
    case TUI_KEY_END:
        t->scroll_off = 0;
        cli_tui_redraw(t);
        break;
    default:
        /* Accept every byte >= 0x20, including UTF-8 multi-byte
         * sequences (CJK input arrives byte-by-byte at 0x80..0xFF). */
        if (key >= 0x20 && key <= 0xFF) {
            tui_input_append(t, (char)key);
            /* UTF-8 完整序列才重绘（与行模式 readline 一致） */
            if (tui_input_utf8_complete(t->input, t->input_len))
                tui_render_input(t);
            fflush(stdout);
        }
        break;
    }
    return 0;
}
