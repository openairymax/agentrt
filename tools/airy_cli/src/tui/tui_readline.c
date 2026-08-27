// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_readline.c
 * @brief TUI 引擎全屏 readline 主循环（域拆分自 cli_tui.c，2026-08-27）。
 *
 * cli_tui_readline 的完整循环骨架：轮询节拍（面板实时刷新 / 光标闪烁）、
 * 内置拼音输入法交互、视图模式（tab）切换、面板按键分派、Ctrl+R/S 反向
 * 搜索、提交/清行/删除/移动等编辑快捷键，以及方向键/翻页/粘贴/普通字符
 * 分派（后者收敛在 tui_readline_nav.c 的 tui_readline_arrow_keys）。
 * 共享声明见 cli_tui_internal.h。
 */

#include "cli_tui_internal.h"

int cli_tui_readline(cli_tui_t *t, char *buf, size_t cap, size_t *out_len)
{
    if (!buf || cap < 2)
        return 0;
    if (out_len)
        *out_len = 0;

    if (!t || !t->active) {
        /* Non-TUI. 2.3.7：F8 转义序列 (ESC[19~) 出现在行输入中 →
         * 请求进入全屏页面（返回 2）。交互 TTY 走字节级 readline
         * （方向键/PgUp 翻历史、无乱码）；管道/日志走 fgets。 */
        if (cli_term_is_tty())
            return tui_readline_line_mode(t, buf, cap, out_len);
        if (!fgets(buf, (int)cap, stdin))
            return 0;
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
        if (strstr(buf, "\x1b[19~")) {
            if (out_len)
                *out_len = 0;
            return 2;
        }
        if (out_len)
            *out_len = n;
        return 1;
    }

    t->input_len = 0;
    t->input_col = 0;
    if (t->input)
        t->input[0] = '\0';
    t->tab_active = 0; /* 新一轮输入清空 Tab 补全状态 */
    t->tab_count = 0;
    t->tab_sel = 0;
    t->scroll_off = 0;
    t->search_active = 0;
    t->search_query_len = 0;
    t->search_match = -1;
    t->cmd_hist_idx = t->cmd_hist.count;
    tui_render_input(t);
    fflush(stdout);

    for (;;) {
        int eof = 0;
        /* 面板模式（任务看板/事件流）200ms 轮询节拍（实时刷新）；
         * 对话模式按 100ms 轮询：P1 节流兜底。 */
        int timeout = (t->mode != CLI_TUI_MODE_CHAT) ? CLI_TUI_PANEL_POLL_MS
                                                     : CLI_TUI_CHAT_POLL_MS;
        int key = tui_read_key(t, timeout, &eof);
        if (eof)
            return 0;
        if (key == 0)
            return 0; /* EOF */
        if (key == -1) {
            /* 轮询超时：面板实时刷新；SIGWINCH：刷新几何并重绘；
             * 对话：消费节流挂起的增量重绘 / 驱动光标闪烁。 */
            if (t->mode != CLI_TUI_MODE_CHAT) {
                cli_tui_redraw(t);
            }
#ifndef _WIN32
            else if (g_tui_resize_pending) {
                g_tui_resize_pending = 0;
                tui_get_size(t);
                /* 2.2.2 环境突变自适应：终端窗口缩得过小（<7 行或 <11 列）
                 * 自动退出全屏回到行渲染流式模式。 */
                if (t->rows <= 6 || t->cols <= 10) {
                    if (out_len)
                        *out_len = 0;
                    return 3; /* 复用 F8 退出路径（cli_tui_leave + 行模式） */
                }
                cli_tui_redraw(t);
            }
#endif
            else if (t->redraw_pending) {
                t->redraw_pending = 0;
                tui_redraw_tail(t);
            } else if (t->mode == CLI_TUI_MODE_CHAT) {
                /* 2.2.1.5：空闲轮询节拍驱动输入光标黑白交替闪烁 */
                tui_caret_tick(t);
                tui_render_input(t);
                fflush(stdout);
            }
            continue;
        }
        /* 2.3.7：全屏 F8 → 请求退出回行渲染流式模式（返回 3）。 */
        if (key == TUI_KEY_F8) {
            if (out_len)
                *out_len = 0;
            return 3;
        }
        /* 2.2.3 内置拼音输入法：中/英切换。 */
        if (t->mode == CLI_TUI_MODE_CHAT && tui_ime_key_hit(t, key)) {
            t->ime_active = !t->ime_active;
            if (!t->ime_active)
                tui_ime_commit_raw(t); /* 切回英文：拼音原文保留在输入行 */
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (t->ime && t->ime_active && t->mode == CLI_TUI_MODE_CHAT) {
            if (key >= 'a' && key <= 'z') {
                if (t->ime_buf_len < sizeof(t->ime_buf) - 1) {
                    t->ime_buf[t->ime_buf_len++] = (char)key;
                    t->ime_buf[t->ime_buf_len] = '\0';
                    tui_ime_refresh(t);
                }
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key >= '1' && key <= '9') {
                /* 数字选字：当前页内第 N 个候选 */
                size_t i = (size_t)(key - '1');
                int idx = t->ime_page * 9 + (int)i;
                if (i < 9 && idx >= 0 && idx < t->ime_cand_count)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == ' ') {
                /* 空格：上屏高亮候选 */
                int idx = tui_ime_sel_index(t);
                if (idx >= 0)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                else
                    tui_ime_commit_raw(t); /* 无候选：空格输出拼音原文 */
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == ',' || key == '.' || key == TUI_KEY_PGUP ||
                key == TUI_KEY_PGDN) {
                /* 翻页；单页时标点走正常路径 */
                if (key == TUI_KEY_PGUP)
                    tui_ime_page_flip(t, -1);
                else if (key == TUI_KEY_PGDN)
                    tui_ime_page_flip(t, 1);
                else if (t->ime_pages > 1)
                    tui_ime_page_flip(t, (key == '.') ? 1 : -1);
                else {
                    tui_ime_commit_raw(t);
                    t->ime_active = 0;
                    tui_input_append(t, key);
                    tui_render_input(t);
                    fflush(stdout);
                    continue;
                }
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == TUI_KEY_LEFT || key == TUI_KEY_RIGHT) {
                /* 页内高亮移动 */
                int page_cnt = t->ime_cand_count - t->ime_page * 9;
                if (page_cnt > 9)
                    page_cnt = 9;
                if (page_cnt > 0) {
                    if (key == TUI_KEY_LEFT && t->ime_sel > 0)
                        t->ime_sel--;
                    else if (key == TUI_KEY_RIGHT &&
                             t->ime_sel + 1 < page_cnt)
                        t->ime_sel++;
                    tui_render_input(t);
                    fflush(stdout);
                }
                continue;
            }
            if (key == 0x1b) {
                /* Esc：取消拼音 */
                t->ime_buf_len = 0;
                t->ime_buf[0] = '\0';
                t->ime_cand_count = 0;
                t->ime_active = 0;
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == 0x7f || key == 0x08) { /* 退格：删拼音；空则退出拼音 */
                if (t->ime_buf_len > 0) {
                    t->ime_buf[--t->ime_buf_len] = '\0';
                    tui_ime_refresh(t);
                } else {
                    t->ime_active = 0;
                }
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == '\n' || key == '\r') {
                /* Enter：有候选时上屏高亮候选，无候选时提交拼音原文 */
                int idx = tui_ime_sel_index(t);
                if (idx >= 0)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                else
                    tui_ime_commit_raw(t);
                t->ime_active = 0;
                tui_render_input(t);
                fflush(stdout);
            } else if (key >= 0x20 && key <= 0xFF) {
                /* 标点/数字等：先提交拼音原文并退出拼音模式 */
                tui_ime_commit_raw(t);
                t->ime_active = 0;
                tui_render_input(t);
                fflush(stdout);
            }
        }

        /* ---- 阶段 4：视图模式（tab）切换 ---- */
        if (key == 0x10) { /* Ctrl+P: 下一个 tab */
            cli_tui_mode_next(t);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x0f) { /* Ctrl+O: 上一个 tab */
            cli_tui_mode_prev(t);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        /* 2026-08-19：F6 直达任务看板 / F7 直达事件流 */
        if (key == TUI_KEY_F6) {
            cli_tui_mode_set(t, CLI_TUI_MODE_BOARD);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        if (key == TUI_KEY_F7) {
            cli_tui_mode_set(t, CLI_TUI_MODE_EVENTS);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        /* 2026-08-25：F2 直达硬件信息面板（再次按 F2 返回对话） */
        if (key == TUI_KEY_F2) {
            cli_tui_mode_set(t,
                             (t->mode == CLI_TUI_MODE_HW) ? CLI_TUI_MODE_CHAT
                                                          : CLI_TUI_MODE_HW);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        /* 2026-08-25：F5 直达记忆链面板（再次按 F5 返回对话） */
        if (key == TUI_KEY_F5) {
            cli_tui_mode_set(t,
                             (t->mode == CLI_TUI_MODE_MEM) ? CLI_TUI_MODE_CHAT
                                                           : CLI_TUI_MODE_MEM);
            cli_tui_redraw(t);
            fflush(stdout);
            continue;
        }
        /* ---- 面板模式分派（硬件信息/任务看板/事件流/记忆链） ---- */
        if (t->mode != CLI_TUI_MODE_CHAT) {
            if (tui_panel_dispatch(t, key))
                continue;
        }

        /* ---- Ctrl+R / Ctrl+S incremental search mode ---- */
        if (t->search_active) {
            if (key == 0x12) { /* Ctrl+R: next older match */
                t->search_forward = 0;
                tui_search_step(t);
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == 0x13) { /* Ctrl+S: next newer match */
                t->search_forward = 1;
                tui_search_step_forward(t);
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == '\n' || key == '\r') {
                /* Accept the matched line into the edit buffer. */
                if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
                    tui_cmd_hist_apply(t, (size_t)t->search_match);
                t->search_active = 0;
                t->search_query_len = 0;
                t->search_match = -1;
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == 0x7f || key == 0x08) { /* Backspace: shrink query */
                if (t->search_query_len > 0) {
                    size_t n = t->search_query_len;
                    while (n > 1 && ((unsigned char)t->search_query[n - 1] & 0xC0) == 0x80)
                        n--;
                    n--;
                    t->search_query_len = n;
                    t->search_query[t->search_query_len] = '\0';
                    t->search_match = -1;
                    tui_search_step(t);
                } else {
                    t->search_active = 0;
                    t->search_match = -1;
                }
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == 0x1b || key == 0x03 || key == 0x04 || key == 0x07) {
                /* ESC / Ctrl+C / Ctrl+D / Ctrl+G: cancel search. */
                t->search_active = 0;
                t->search_query_len = 0;
                t->search_match = -1;
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key >= 0x20 && key <= 0xFF) { /* extend query */
                if (t->search_query_len + 2 <= TUI_SEARCH_QUERY_MAX) {
                    t->search_query[t->search_query_len++] = (char)key;
                    t->search_query[t->search_query_len] = '\0';
                    t->search_match = -1;
                    tui_search_step(t);
                }
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            continue;
        }

        if (key == '\n' || key == '\r') {
            size_t n = t->input_len;
            if (n >= cap)
                n = cap - 1;
            if (n > 0)
                AIRY_MEMCPY(buf, t->input, n);
            buf[n] = '\0';
            if (out_len)
                *out_len = n;
            /* Remember the submitted line for Up/Down browsing. */
            tui_cmd_hist_push(t, buf);
            tui_cmd_hist_save(t);
            /* Do NOT echo the raw line here: the caller renders the
             * submission, so committing it now would duplicate it. */
            t->input_len = 0;
            t->input_col = 0;
            t->scroll_off = 0;
            return 1;
        }
        if (key == 0x12) { /* Ctrl+R: enter reverse search */
            t->search_active = 1;
            t->search_forward = 0;
            t->search_query_len = 0;
            t->search_query[0] = '\0';
            t->search_match = -1;
            tui_search_step(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x13) { /* Ctrl+S: enter forward search */
            t->search_active = 1;
            t->search_forward = 1;
            t->search_query_len = 0;
            t->search_query[0] = '\0';
            t->search_match = -1;
            tui_search_step_forward(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x03) { /* Ctrl+C: clear the draft; exit only on empty line */
            if (t->input_len > 0) {
                t->input_len = 0;
                t->input_col = 0;
                t->tab_active = 0;
                if (t->input)
                    t->input[0] = '\0';
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            return 0;
        }
        if (key == 0x04) { /* Ctrl+D: delete-fwd when text present; EOF on empty */
            if (t->input_len > 0) {
                tui_input_delete_fwd(t);
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            return 0;
        }
        if (key == 0x15) { /* Ctrl+U: kill before the caret */
            if (t->input_col > 0) {
                tui_input_kill_save(t, t->input, t->input_col);
                AIRY_MEMMOVE(t->input, t->input + t->input_col, t->input_len - t->input_col + 1);
                t->input_len -= t->input_col;
                t->input_col = 0;
            }
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x0b) { /* Ctrl+K: kill after the caret */
            if (t->input_col < t->input_len) {
                tui_input_kill_save(t, t->input + t->input_col, t->input_len - t->input_col);
                t->input[t->input_col] = '\0';
                t->input_len = t->input_col;
            }
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x01) { /* Ctrl+A: move to start */
            t->input_col = 0;
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x05) { /* Ctrl+E: move to end */
            t->input_col = t->input_len;
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x17) { /* Ctrl+W: kill previous word */
            size_t kill_from = t->input_col;
            tui_input_back_word(t);
            if (t->input_col < kill_from)
                tui_input_kill_save(t, t->input + t->input_col, kill_from - t->input_col);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x14) { /* Ctrl+T: transpose chars at the caret */
            tui_input_transpose(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x19) { /* Ctrl+Y: yank the killed text */
            tui_input_yank(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == 0x0c) { /* Ctrl+L: clear screen, re-render */
            cli_tui_redraw(t);
            continue;
        }
        if (key == 0x7f || key == 0x08) { /* Backspace */
            tui_input_backspace(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        if (key == '\t') { /* Tab: complete the current token */
            if (tui_input_tab_complete(t)) {
                tui_render_input(t);
                fflush(stdout);
            }
            continue;
        }
        /* 方向键/翻页/Home/End/粘贴/Delete/普通字符输入：导航编辑键分派
         * （tui_readline_nav.c）。-1 = 粘贴内 EOF，终止 readline。 */
        if (tui_readline_arrow_keys(t, key) < 0)
            return 0;
    }
}
