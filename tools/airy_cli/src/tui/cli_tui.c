// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.c
 * @brief Full-screen TUI engine 主引擎（域拆分后，2026-08-27）。
 *
 * 2026-08-27 域拆分（3988 行 → 7 文件）：
 *   - cli_tui.c              引擎骨架：状态、面板/模式 API、emit、生命周期、
 *                            全屏 readline 主循环
 *   - tui_keys.c             按键读取域（POSIX/Windows + ESC 序列解析）
 *   - tui_input.c            输入编辑域（光标/编辑/Tab 补全/行模式 readline）
 *   - tui_ime.c              内置拼音输入法域
 *   - tui_history.c          历史与搜索域
 *   - tui_render.c           渲染域（header/viewport/input/增量重绘/硬件面板）
 *   - tui_panel_dispatch.c   面板按键分派域（硬件信息/任务看板/事件流）
 * 跨文件共享结构体与内部声明见 cli_tui_internal.h；公共 API 见
 * cli_tui.h（对外不透明 cli_tui_t 不变）。
 *
 * Layout (on an interactive terminal):
 *
 *   ┌──────────────────────────────────────────────┐
 *   │  pinned header  (banner + model panel)        │  fixed
 *   ├──────────────────────────────────────────────┤
 *   │  conversation viewport (scrollable history)  │  middle
 *   │                                              │
 *   ├──────────────────────────────────────────────┤
 *   │  > input line …                              │  bottom (fixed)
 *   └──────────────────────────────────────────────┘
 *
 * ANSI used: alt screen 1049, cursor home, erase line, cursor movement.
 * No curses — plain POSIX termios + ANSI, consistent with the project's
 * "no curses" rendering philosophy.
 */

#include "cli_tui_internal.h"

/* Process-wide engine handle (accessor for GCCP etc.). */
static cli_tui_t *g_default_tui;

cli_tui_t *cli_tui_get_default(void)
{
    return g_default_tui;
}

#ifndef _WIN32
volatile sig_atomic_t g_tui_resize_pending;

void tui_sigwinch_handler(int sig)
{
    (void)sig;
    g_tui_resize_pending = 1;
}
#endif

const char *tui_mode_name(cli_tui_mode_t m)
{
    switch (m) {
    case CLI_TUI_MODE_CHAT:
        return "对话";
    case CLI_TUI_MODE_BOARD:
        return "任务看板";
    case CLI_TUI_MODE_EVENTS:
        return "事件流";
    case CLI_TUI_MODE_HW:
        return "硬件信息";
    case CLI_TUI_MODE_MEM:
        return "记忆链";
    default:
        return "?";
    }
}

/* ---- 阶段 4：视图模式（tab）+ 面板数据源 ---- */

void cli_tui_set_panel(cli_tui_t *t, cli_tui_mode_t mode, void *ud,
                       cli_tui_panel_count_fn count, cli_tui_panel_line_fn line)
{
    if (!t || mode < 0 || mode >= CLI_TUI_MODE_MAX)
        return;
    t->panel[mode].ud = ud;
    t->panel[mode].count = count;
    t->panel[mode].line = line;
    t->dirty = 1;
}

void cli_tui_set_panel_action(cli_tui_t *t, cli_tui_mode_t mode,
                              cli_tui_panel_action_fn fn)
{
    if (!t || mode < 0 || mode >= CLI_TUI_MODE_MAX)
        return;
    t->panel[mode].action = fn;
    t->dirty = 1;
}

cli_tui_mode_t cli_tui_mode(const cli_tui_t *t)
{
    return t ? t->mode : CLI_TUI_MODE_CHAT;
}

void cli_tui_mode_next(cli_tui_t *t)
{
    if (!t)
        return;
    cli_tui_mode_set(t, (cli_tui_mode_t)(((int)t->mode + 1) % CLI_TUI_MODE_MAX));
}

void cli_tui_mode_prev(cli_tui_t *t)
{
    if (!t)
        return;
    cli_tui_mode_set(t,
                     (cli_tui_mode_t)(((int)t->mode + CLI_TUI_MODE_MAX - 1) %
                                      CLI_TUI_MODE_MAX));
}

void cli_tui_mode_set(cli_tui_t *t, cli_tui_mode_t m)
{
    if (!t || m < 0 || m >= CLI_TUI_MODE_MAX || m == t->mode)
        return;
    t->mode = m;
    t->scroll_off = 0;
    /* 进入任务看板：重置选择与详情（事件流保持跟随/过滤状态） */
    if (m == CLI_TUI_MODE_BOARD) {
        t->sel = 0;
        t->detail_active = 0;
        t->detail_len = 0;
    }
    /* 记忆链：默认尾部实时跟随（新记忆即现） */
    if (m == CLI_TUI_MODE_MEM)
        t->follow = 1;
    t->note[0] = '\0';
    t->dirty = 1;
}

void cli_tui_set_status(cli_tui_t *t, const char *status)
{
    if (!t)
        return;
    if (status && status[0])
        AIRY_STRNCPY_TERM(t->status, status, sizeof(t->status));
    else
        t->status[0] = '\0';
    t->dirty = 1;
}

/* ---- emit / history model ---- */

void tui_append_byte(cli_tui_t *t, char c)
{
    if (t->cur_len + 2 > t->cur_cap) {
        size_t new_cap = t->cur_cap ? t->cur_cap * 2 : TUI_LINE_INIT_CAP;
        while (new_cap < t->cur_len + 2)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->cur, new_cap);
        if (!grown)
            return;
        t->cur = grown;
        t->cur_cap = new_cap;
    }
    t->cur[t->cur_len++] = c;
    t->cur[t->cur_len] = '\0';
}

/* 内容行数：历史（去 header）+ 未提交的 partial 行。增量/全量渲染
 * 共用同一几何口径（与 tui_render_viewport 聊天分支一致）。 */
size_t tui_content_lines(cli_tui_t *t)
{
    size_t total = t->hist.count;
    int have_partial = t->cur && t->cur_len > 0;
    return (total > t->hist.pinned ? total - t->hist.pinned : 0) +
           (have_partial ? 1 : 0);
}

void cli_tui_emit(cli_tui_t *t, const char *data, size_t len)
{
    if (!t || !data) {
        /* NULL engine: stream-safe passthrough (non-TTY callers). */
        if (data && len)
            fwrite(data, 1, len, stdout);
        return;
    }
    if (!t->active) {
        if (len)
            fwrite(data, 1, len, stdout);
        return;
    }

    size_t c0 = tui_content_lines(t);
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\r') {
            /* Rewind the partial line: spinner / status redraws replace the
             * current line instead of appending. */
            t->cur_len = 0;
            if (t->cur)
                t->cur[0] = '\0';
            continue;
        }
        if (c == '\n') {
            if (t->cur_len > 0) {
                tui_commit_line(t, t->cur);
                t->cur = NULL;
            } else {
                /* Blank line: commit an empty marker line. */
                char *blank = (char *)AIRY_STRDUP("");
                if (blank)
                    tui_commit_line(t, blank);
            }
            t->cur_len = 0;
            t->cur_cap = 0;
            continue;
        }
        tui_append_byte(t, c);
    }
    /* P1 重绘优化：不再每 chunk 全量重绘。 */
    if (t->mode != CLI_TUI_MODE_CHAT || t->scroll_off > 0) {
        t->dirty = 1;
        return;
    }
    if (tui_content_lines(t) - c0 > 1) {
        cli_tui_redraw(t);
        return;
    }
    tui_schedule_redraw(t);
}

void cli_tui_emit_flush(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    if (t->cur_len > 0) {
        tui_commit_line(t, t->cur);
        t->cur = NULL;
        t->cur_len = 0;
        t->cur_cap = 0;
    }
    /* P1：flush 提交了部分行 → 立即增量重绘（跳过节流窗口）。 */
    if (t->mode != CLI_TUI_MODE_CHAT || t->scroll_off > 0) {
        t->dirty = 1;
        return;
    }
    t->redraw_pending = 0;
    tui_redraw_tail(t);
}

size_t cli_tui_hist_count(cli_tui_t *t)
{
    return t ? t->hist.count : 0;
}

void cli_tui_pin_header(cli_tui_t *t)
{
    if (!t)
        return;
    /* Commit any partial header line, then fix the header boundary. */
    if (t->active && t->cur_len > 0) {
        tui_commit_line(t, t->cur);
        t->cur = NULL;
        t->cur_len = 0;
        t->cur_cap = 0;
    }
    t->hist.pinned = t->hist.count;
    t->dirty = 1;
}

void cli_tui_redraw(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    tui_render_header(t);
    tui_render_viewport(t);
    tui_render_input(t);
    t->dirty = 0;
    fflush(stdout);
}

/* ==================== 全屏 readline 主循环 ==================== */

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
        switch (key) {
        case TUI_KEY_UP:
            if (t->input_len > 0 || t->cmd_hist.count > 0) {
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
            if (t->input_len > 0 || t->cmd_hist_idx < t->cmd_hist.count) {
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
                        return 0;
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
    }
}

/* ---- lifecycle ---- */

void cli_tui_set_header_models(cli_tui_t *t, const char *t2, const char *t1f,
                               const char *t1p)
{
    if (!t)
        return;
    t->hdr_t2[0] = t->hdr_t1f[0] = t->hdr_t1p[0] = '\0';
    if (t2 && t2[0])
        snprintf(t->hdr_t2, sizeof(t->hdr_t2), "%s", t2);
    if (t1f && t1f[0])
        snprintf(t->hdr_t1f, sizeof(t->hdr_t1f), "%s", t1f);
    if (t1p && t1p[0])
        snprintf(t->hdr_t1p, sizeof(t->hdr_t1p), "%s", t1p);
}

/* 2.2.1.2/2.2.1.3：重建行渲染三区（hero/对话/输入）。详情见 cli_tui.h。 */
void cli_tui_rebuild_three_zone(cli_tui_t *t)
{
    if (!t || !cli_term_is_tty() || t->active)
        return;
    cli_term_header_unpin();
    cli_out("\033[2J\033[H");
    cli_print_system_header(t->hdr_t2[0] ? t->hdr_t2 : NULL,
                            t->hdr_t1f[0] ? t->hdr_t1f : NULL,
                            t->hdr_t1p[0] ? t->hdr_t1p : NULL);
    /* P0（F8 退出全屏后英雄区混乱）：必须在重放历史之前重建 DECSTBM 滚动区
     * 并恢复 footer 计数——unpin 已把 g_footer_lines 清零，若拖到重放之后再
     * pin，长历史会把刚画好的 hero 滚出屏外，且 input_hop 因 footer=0 变成
     * no-op（光标定位失效）。先 pin 后重放，hero/对话/输入三区才真正固定。 */
    cli_term_header_pin(CLI_HDR_LINES, 2);
    for (size_t i = 0; i < g_history_count; i++) {
        if (strcmp(g_history_roles[i], "user") == 0)
            cli_render_user_message(g_history_contents[i]);
        else
            cli_render_super_agent(g_history_contents[i]);
    }
    if (cli_term_input_active())
        cli_term_input_hop();
    fflush(stdout);
}

int cli_tui_create(cli_tui_t **out_tui)
{
    if (!out_tui)
        return -1;

    cli_tui_t *t = (cli_tui_t *)AIRY_CALLOC(1, sizeof(cli_tui_t));
    if (!t)
        return -1;
    *out_tui = t;
    if (!g_default_tui)
        g_default_tui = t;
    /* Recall past submitted commands (Up / Ctrl+R) from the previous session. */
    tui_cmd_hist_load(t);
    /* 2.2.3 内置拼音输入法：词典加载失败仅降级（ime==NULL），不阻断
     * 启动；但必须明确提示，避免"按 F10/F9 没反应"的静默困惑。 */
    t->ime = tui_ime_load_dict();
    if (!t->ime) {
        fprintf(stderr,
                "airy_cli: 警告 内置输入法词典未加载，F10/F9 中文输入不可用\n"
                "          （可用 AIRY_IME_DICT=/path/to/airy_ime.dat 指定词典）\n");
    } else if (cli_term_is_tty()) {
        /* 2026-08-25：词典就绪时给出明确提示，避免"按 F10/F9 没反应"的
         * 静默困惑。F10 在 VS Code/GNOME Terminal 等常被占用，明确告知 F9。 */
        fprintf(stderr,
                "airy_cli: 内置输入法就绪：输入拼音后按 F10/F9 切换中/英（F9 为备键，"
                "F10 被终端占用时用 F9）\n");
    }
    t->ime_active = 0;
    t->ime_key = tui_ime_key_resolve();
    t->ime_key_alt = tui_ime_key_alt_resolve();
    /* 2.3.7 (2026-08-17)：交互默认行渲染流式模式，不自动进入全屏页面；
     * 全屏由 cli_tui_enter() 显式进入（F8 切换）。 */
    return 0;
}

int cli_tui_enter(cli_tui_t *t)
{
    if (!t || t->active)
        return 0;
    if (!cli_term_is_tty())
        return -1;

    t->active = 1;
    t->scr_set = 0; /* 滚动区在首次全量渲染时建立 */
    tui_get_size(t);
    if (t->rows <= 6 || t->cols <= 10) {
        t->active = 0;
        return -1;
    }

#ifdef _WIN32
    t->active = 0; /* POSIX-only full-screen mode */
    return -1;
#else
    /* Enter alternate screen + bracketed paste + raw mode.
     * 2.2.1.5：隐藏硬件光标，输入光标由反显块自绘（黑白交替闪烁）。 */
    fputs("\033[?1049h\033[?2004h\033[?25l\033[2J\033[H", stdout);
    fflush(stdout);

    g_tui_resize_pending = 0;
    signal(SIGWINCH, tui_sigwinch_handler);

    if (tcgetattr(STDIN_FILENO, &t->saved_termios) == 0) {
        /* Full raw mode (cfmakeraw semantics): the CLI owns every byte of
         * input. IXON must go so Ctrl+S is not swallowed as terminal flow
         * control; ISIG goes so Ctrl+C is delivered to the readline loop. */
        struct termios raw = t->saved_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~OPOST;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cflag &= ~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            t->termios_saved = 1;
    }
    return 0;
#endif
}

int cli_tui_leave(cli_tui_t *t)
{
    if (!t || !t->active)
        return 0;
#ifndef _WIN32
    if (t->termios_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &t->saved_termios);
    signal(SIGWINCH, SIG_DFL);
#endif
    /* 恢复全屏滚动区（\x1b[r），随后退出 alt screen。
     * 2.2.1.5：退出前恢复硬件光标。 */
    fputs("\033[r\033[?2004l\033[?25h\033[?1049l", stdout);
    fflush(stdout);
    t->active = 0;
    t->termios_saved = 0;
    t->scr_set = 0;
    return 0;
}

void cli_tui_replay_history(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    for (size_t i = 0; i < g_history_count; i++) {
        const char *role = g_history_roles[i];
        const char *content = g_history_contents[i];
        if (!role || !content)
            continue;
        const char *tag = (strcmp(role, "user") == 0) ? "[For Thee]" : "[Super Agent]";
        size_t cap = 64 + strlen(tag) + strlen(content);
        char *line = (char *)AIRY_MALLOC(cap);
        if (!line)
            continue;
        int n = snprintf(line, cap, "  %s  %s", tag, content);
        if (n < 0) {
            AIRY_FREE(line);
            continue;
        }
        tui_commit_line(t, line);
    }
    t->dirty = 1;
}

void cli_tui_reset_history(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    tui_history_reset(&t->hist);
    t->cur_len = 0;
    if (t->cur)
        t->cur[0] = '\0';
    t->scroll_off = 0;
    t->dirty = 1;
}

void cli_tui_destroy(cli_tui_t *t)
{
    if (!t)
        return;
    if (t->active)
        cli_tui_leave(t);
    tui_history_reset(&t->hist);
    tui_cmd_hist_reset(t);
    AIRY_FREE(t->cur);
    AIRY_FREE(t->input);
    airy_ime_destroy(t->ime); /* 可为 NULL */
    if (g_default_tui == t)
        g_default_tui = NULL;
    AIRY_FREE(t);
}

int cli_tui_active(const cli_tui_t *t)
{
    return t && t->active;
}

/* 任务执行期间（wait 循环）TUI 模式非阻塞按键读取：
 *   - 0   无输入（未激活 / 超时）
 *   - -1  EOF（*eof=1）
 *   - 其他 按键码（0x03 = Ctrl+C 等，与 readline 同语义）
 * 供 main.c 的 cli_task_poll_input 在 TUI 全屏下开放中断能力。 */
int cli_tui_poll_key(cli_tui_t *t, int *eof)
{
    if (!t || !cli_tui_active(t)) {
        if (eof)
            *eof = 0;
        return 0;
    }
    return tui_read_key(t, 0, eof);
}
