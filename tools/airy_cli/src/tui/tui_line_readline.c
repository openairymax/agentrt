// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_line_readline.c
 * @brief TUI 引擎行渲染模式（非全屏）readline（域拆分自 tui_input.c，2026-08-27）。
 *
 * 非全屏 TTY 下的字节级 readline：输入行重绘（底部输入条或当前行）、
 * 反显光标闪烁、内置拼音输入法、方向键翻命令历史 / ↑ 空行进入全屏 TUI、
 * 以及编辑快捷键。F8 转义序列（ESC[19~）请求进入全屏页面（返回 2）。
 * 共享声明见 cli_tui_internal.h。
 */

#include "cli_tui_internal.h"

void tui_line_redraw(cli_tui_t *t)
{
    char num[16];
    if (cli_term_input_active()) {
        /* 三区布局：提示符画在固定底部输入条（绝对定位）。 */
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%d", t->rows > 0 ? t->rows : 1);
        tui_write_literal(num);
        tui_write_literal(";1H");
    } else {
        /* 无底部输入条（窄终端/降级布局）：在当前行重绘。 */
        tui_write_literal("\r\033[2K");
    }
    tui_clear_line();
    /* 2.2.3 IME 状态标记：[中]/[英]（词典可用时显示，dim） */
    const char *ime_tag = t->ime ? (t->ime_active ? "[中] " : "[英] ") : "";
    size_t ime_tag_w = strlen(ime_tag);
    if (ime_tag_w > 0) {
        fputs(cli_c(CLR_DIM), stdout);
        fputs(ime_tag, stdout);
        fputs(cli_c(CLR_RESET), stdout);
    }
    fputs(cli_c(CLR_CYAN), stdout);
    fputs(TUI_INPUT_PREFIX, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    size_t input_w = tui_caret_print(t); /* 2.2.1.5 反显光标（黑白闪烁） */
    /* 拼音候选条（输入条上方一行；不占用输入行本身） */
    if (cli_term_input_active())
        tui_ime_draw_cands(t, t->rows > 0 ? t->rows : 1);
    /* 光标落在编辑位置（UTF-8 显示宽度对齐，CJK 不漂移）。 */
    size_t col = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) + input_w;
    if (cli_term_input_active()) {
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%d", t->rows > 0 ? t->rows : 1);
        tui_write_literal(num);
        tui_write_literal(";");
        snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
        tui_write_literal(num);
        tui_write_literal("H");
    }
    fflush(stdout);
}

int tui_readline_line_mode(cli_tui_t *t, char *buf, size_t cap,
                           size_t *out_len)
{
    if (!t)
        return 0;
    t->input_len = 0;
    t->input_col = 0;
    if (t->input)
        t->input[0] = '\0';
    t->tab_active = 0;
    t->tab_count = 0;
    t->tab_sel = 0;
    t->scroll_off = 0;
    t->search_active = 0;
    t->search_query_len = 0;
    t->search_match = -1;
    t->cmd_hist_idx = t->cmd_hist.count;
    tui_get_size(t);

#ifndef _WIN32
    struct termios saved;
    int saved_ok = 0;
    if (tcgetattr(STDIN_FILENO, &saved) == 0) {
        struct termios raw = saved;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~OPOST;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cflag &= ~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            saved_ok = 1;
    }
#endif
    /* 2.2.1.5：隐藏硬件光标，改用反显块自绘光标（黑白交替闪烁） */
    fputs("\033[?25l", stdout);
    t->caret_tick = cli_now_ms();
    t->caret_visible = 1;
#ifndef _WIN32
    /* 2.2.1.1 环境突变自适应：行模式也监听 SIGWINCH。 */
    if (saved_ok) {
        signal(SIGWINCH, tui_sigwinch_handler);
        g_tui_resize_pending = 0;
    }
#endif
    tui_line_redraw(t);

    int rc = 1;
    for (;;) {
        int eof = 0;
        /* 以半周期超时轮询：到达闪烁节拍翻转光标并局部重绘输入行 */
        int key = tui_read_key(t, (int)CLI_CARET_BLINK_MS, &eof);
        if (eof || key == 0) {
            rc = 0;
            break;
        }
        if (key == -1) {
#ifndef _WIN32
            if (g_tui_resize_pending) {
                g_tui_resize_pending = 0;
                /* 终端尺寸变化：滚动区与 hero 错位 → 重建三区 */
                cli_tui_rebuild_three_zone(t);
            }
#endif
            tui_caret_tick(t);
            tui_line_redraw(t);
            continue;
        }
        if (key == TUI_KEY_F8) {
            /* 行渲染 → 全屏 TUI（与 main.c 的 rl==2 分支一致）。 */
            if (out_len)
                *out_len = 0;
            rc = 2;
            break;
        }
        /* 2.2.3 内置拼音输入法：中/英切换。 */
        if (tui_ime_key_hit(t, key)) {
            t->ime_active = !t->ime_active;
            if (!t->ime_active)
                tui_ime_commit_raw(t); /* 切回英文：拼音原文保留在输入行 */
            tui_line_redraw(t);
            continue;
        }
        if (t->ime && t->ime_active) {
            if (key >= 'a' && key <= 'z') {
                if (t->ime_buf_len < sizeof(t->ime_buf) - 1) {
                    t->ime_buf[t->ime_buf_len++] = (char)key;
                    t->ime_buf[t->ime_buf_len] = '\0';
                    tui_ime_refresh(t);
                }
                tui_line_redraw(t);
                continue;
            }
            if (key >= '1' && key <= '9') {
                /* 数字选字：选中当前页内第 N 个候选 */
                size_t i = (size_t)(key - '1');
                int idx = t->ime_page * 9 + (int)i;
                if (i < 9 && idx >= 0 && idx < t->ime_cand_count)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                tui_line_redraw(t);
                continue;
            }
            if (key == ' ') {
                /* 空格：上屏高亮候选 */
                int idx = tui_ime_sel_index(t);
                if (idx >= 0)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                else
                    tui_ime_commit_raw(t); /* 无候选：空格输出拼音原文 */
                tui_line_redraw(t);
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
                    tui_line_redraw(t);
                    continue;
                }
                tui_line_redraw(t);
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
                    tui_line_redraw(t);
                }
                continue;
            }
            if (key == 0x1b) {
                /* Esc：取消拼音 */
                t->ime_buf_len = 0;
                t->ime_buf[0] = '\0';
                t->ime_cand_count = 0;
                t->ime_active = 0;
                tui_line_redraw(t);
                continue;
            }
            if (key == 0x7f || key == 0x08) { /* 退格：删拼音；空则退出拼音 */
                if (t->ime_buf_len > 0) {
                    t->ime_buf[--t->ime_buf_len] = '\0';
                    tui_ime_refresh(t);
                } else {
                    t->ime_active = 0;
                }
                tui_line_redraw(t);
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
                tui_line_redraw(t);
            } else if (key >= 0x20 && key <= 0xFF) {
                /* 标点/数字等：先提交拼音原文并退出拼音模式 */
                tui_ime_commit_raw(t);
                t->ime_active = 0;
                tui_line_redraw(t);
            }
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
            tui_cmd_hist_push(t, buf);
            tui_cmd_hist_save(t);
            t->input_len = 0;
            t->input_col = 0;
            break;
        }
        if (key == TUI_KEY_UP && t->input_len == 0) {
            /* 空输入 ↑：翻动会话历史 → 进入全屏 TUI（return 2）。 */
            if (out_len)
                *out_len = 0;
            rc = 2;
            break;
        }
        if (key == TUI_KEY_DOWN && t->input_len == 0)
            continue; /* 已在尾部，忽略 */
        if (key == 0x03) { /* Ctrl+C：非空清行，空行退出 */
            if (t->input_len > 0) {
                t->input_len = 0;
                t->input_col = 0;
                if (t->input)
                    t->input[0] = '\0';
                tui_line_redraw(t);
            } else {
                rc = 0;
                break;
            }
            continue;
        }
        if (key == 0x04) { /* Ctrl+D：非空删光标处字符；空行 EOF */
            if (t->input_len > 0) {
                tui_input_delete_fwd(t);
                tui_line_redraw(t);
                continue;
            }
            rc = 0;
            break;
        }
        if (key == 0x7f || key == 0x08) {
            tui_input_backspace(t);
            tui_line_redraw(t);
            continue;
        }
        if (key == 0x01) { /* Ctrl+A */
            t->input_col = 0;
            tui_line_redraw(t);
            continue;
        }
        if (key == 0x05) { /* Ctrl+E */
            t->input_col = t->input_len;
            tui_line_redraw(t);
            continue;
        }
        if (key == 0x15) { /* Ctrl+U */
            if (t->input_col > 0) {
                tui_input_kill_save(t, t->input, t->input_col);
                AIRY_MEMMOVE(t->input, t->input + t->input_col,
                             t->input_len - t->input_col + 1);
                t->input_len -= t->input_col;
                t->input_col = 0;
            }
            tui_line_redraw(t);
            continue;
        }
        if (key == 0x0b) { /* Ctrl+K */
            if (t->input_col < t->input_len) {
                tui_input_kill_save(t, t->input + t->input_col,
                                    t->input_len - t->input_col);
                t->input[t->input_col] = '\0';
                t->input_len = t->input_col;
            }
            tui_line_redraw(t);
            continue;
        }
        if (key == TUI_KEY_LEFT) {
            if (t->input_col > 0) {
                size_t n = t->input_col;
                while (n > 1 && ((unsigned char)t->input[n - 1] & 0xC0) == 0x80)
                    n--;
                if (n > 0)
                    n--;
                t->input_col = n;
                tui_line_redraw(t);
            }
            continue;
        }
        if (key == TUI_KEY_RIGHT) {
            if (t->input_col < t->input_len) {
                size_t n = t->input_col + 1;
                while (n < t->input_len &&
                       ((unsigned char)t->input[n] & 0xC0) == 0x80)
                    n++;
                t->input_col = n;
                tui_line_redraw(t);
            }
            continue;
        }
        if (key == TUI_KEY_UP || key == TUI_KEY_DOWN) {
            /* 非空输入：浏览已提交命令历史（readline Up/Down）。 */
            if (key == TUI_KEY_UP && t->cmd_hist_idx > 0) {
                tui_cmd_hist_save_draft(t);
                t->cmd_hist_idx--;
                tui_cmd_hist_apply(t, t->cmd_hist_idx);
            } else if (key == TUI_KEY_DOWN &&
                       t->cmd_hist_idx < t->cmd_hist.count) {
                t->cmd_hist_idx++;
                if (t->cmd_hist_idx >= t->cmd_hist.count) {
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
            }
            tui_line_redraw(t);
            continue;
        }
        if (key == '\t') {
            if (tui_input_tab_complete(t))
                tui_line_redraw(t);
            continue;
        }
        if (key >= 0x20 && key <= 0xFF) {
            tui_input_append(t, (char)key);
            /* UTF-8 完整序列到达才重绘：避免逐字节渲染的乱码帧 */
            if (tui_input_utf8_complete(t->input, t->input_len))
                tui_line_redraw(t);
            continue;
        }
    }

#ifndef _WIN32
    if (saved_ok) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        signal(SIGWINCH, SIG_DFL); /* 行模式退出：SIGWINCH 交还终端默认 */
    }
#endif
    /* 2.2.1.5：恢复硬件光标（自绘反显光标只在输入期间接管） */
    fputs("\033[?25h", stdout);
    return rc;
}
