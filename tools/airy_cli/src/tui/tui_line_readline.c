// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_line_readline.c
 * @brief TUI 引擎行渲染模式（非全屏）readline（域拆分自 tui_input.c，2026-08-27）。
 *
 * 非全屏 TTY 下的字节级 readline：输入行重绘（底部输入条或当前行）、
 * 反显光标闪烁、内置拼音输入法、方向键翻命令历史，以及编辑快捷键。
 * 0.1.17 R5-G2 起本文件是 CLI 唯一的交互形态：不再存在进入全屏页面的
 * 按键通路（F8 / 空输入 ↑ 均已退役），全屏由 `--tui` 模式调起渲染层。
 * 共享声明见 cli_tui_internal.h。
 *
 * 0.1.19：按键处理按「拼音 / 通用分派」两段拆分，主循环只保留取键与
 * 分派骨架，避免单函数圈复杂度随快捷键增长而失控。
 */

#include "cli_tui_internal.h"

#include "cli_live.h"

/* 列绝对定位（CHA）：降级布局（无底部输入条）把光标移回编辑位。 */
static void tui_cursor_col(int col)
{
    char num[16];

    snprintf(num, sizeof(num), "%d", col > 0 ? col : 1);
    tui_write_literal("\033[");
    tui_write_literal(num);
    tui_write_literal("G");
}

void tui_line_redraw(cli_tui_t *t)
{
    int bar = cli_term_input_on();
    int row = t->rows > 0 ? t->rows : 1;

    if (bar)
        cli_term_cursor_to(row, 1); /* 三区布局：提示符画在固定底部输入条 */
    else
        tui_write_literal("\r\033[2K"); /* 降级布局：在当前行重绘 */
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
    size_t col = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) + input_w;
    /* 拼音候选条（0.1.15）：行渲染模式内联于输入行末。底部固定输入条
     * 仅在终端行数大于页脚行数时启用，候选条需按同一条件选择绝对定位
     * 方式：三区布局回到输入条首列，降级布局用 CHA 回编辑位。 */
    int ime_bar = tui_ime_draw_cands_inline(t, col);
    if (bar)
        cli_term_cursor_to(row, (int)col);
    else if (ime_bar)
        tui_cursor_col((int)col);
    fflush(stdout);
}

typedef struct {
    cli_tui_t *t;
    char *buf;
    size_t cap;
    size_t *out_len;
} rd_ctx_t;

/* 拼音分支结果：NONE 未处理；TAKEN 已消费；PASS 已处理但需落回通用路径。 */
enum { RD_IME_NONE = 0, RD_IME_TAKEN, RD_IME_PASS };

static void rd_clear(rd_ctx_t *c)
{
    c->t->input_len = 0;
    c->t->input_col = 0;
    if (c->t->input)
        c->t->input[0] = '\0';
}

static void rd_init(rd_ctx_t *c)
{
    cli_tui_t *t = c->t;

    rd_clear(c);
    t->tab_active = 0;
    t->tab_count = 0;
    t->tab_sel = 0;
    t->cmd_hist_idx = t->cmd_hist.count;
    tui_get_size(t);
}

#ifndef _WIN32
static int rd_raw_on(struct termios *saved)
{
    if (tcgetattr(STDIN_FILENO, saved) != 0)
        return 0;
    struct termios raw = *saved;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
        return 0;
    cli_term_note_raw_enter(saved); /* T-19 崩溃守卫登记 */
    return 1;
}

static void rd_raw_off(int saved_ok, struct termios *saved)
{
    if (!saved_ok)
        return;
    tcsetattr(STDIN_FILENO, TCSANOW, saved);
    cli_term_note_raw_leave(); /* T-19 崩溃守卫注销 */
    signal(SIGWINCH, SIG_DFL); /* 行模式退出：SIGWINCH 交还终端默认 */
}
#endif

/* 半周期超时唤醒：刷新光标闪烁节拍，必要时重建三区。 */
static void rd_idle(cli_tui_t *t)
{
#ifndef _WIN32
    if (g_tui_resize_pending) {
        g_tui_resize_pending = 0;
        cli_tui_rebuild_three_zone(t);
    }
#endif
    cli_live_tick();
    tui_caret_tick(t);
    tui_line_redraw(t);
}

/* 拼音字母与选字：只改候选串，不改输入行文本。 */
static int rd_ime_ch(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;
    int idx = -1;

    if (key >= 'a' && key <= 'z') {
        if (t->ime_buf_len < sizeof(t->ime_buf) - 1) {
            t->ime_buf[t->ime_buf_len++] = (char)key;
            t->ime_buf[t->ime_buf_len] = '\0';
            tui_ime_refresh(t);
        }
    } else if (key >= '1' && key <= '9') {
        idx = t->ime_page * 9 + (key - '1');
        if (idx >= 0 && idx < t->ime_cand_count)
            tui_ime_commit_cand(t, t->ime_cands[idx].text);
    } else if (key == ' ') {
        idx = tui_ime_sel_index(t);
        if (idx >= 0)
            tui_ime_commit_cand(t, t->ime_cands[idx].text);
        else
            tui_ime_commit_raw(t); /* 无候选：空格输出拼音原文 */
    } else {
        return RD_IME_NONE;
    }
    tui_line_redraw(t);
    return RD_IME_TAKEN;
}

static int rd_ime_page(rd_ctx_t *c, int key)
{
    if (key == TUI_KEY_PGUP)
        tui_ime_page_flip(c->t, -1);
    else if (key == TUI_KEY_PGDN)
        tui_ime_page_flip(c->t, 1);
    else if (c->t->ime_pages > 1)
        tui_ime_page_flip(c->t, (key == '.') ? 1 : -1);
    else {
        /* 单页时标点走正常路径：先提交拼音原文再插入该标点。 */
        tui_ime_commit_raw(c->t);
        c->t->ime_active = 0;
        tui_input_append(c->t, (char)key);
    }
    tui_line_redraw(c->t);
    return RD_IME_TAKEN;
}

static int rd_ime_sel(rd_ctx_t *c, int key)
{
    int page_cnt = c->t->ime_cand_count - c->t->ime_page * 9;

    if (page_cnt > 9)
        page_cnt = 9;
    if (page_cnt > 0) {
        if (key == TUI_KEY_LEFT && c->t->ime_sel > 0)
            c->t->ime_sel--;
        else if (key == TUI_KEY_RIGHT && c->t->ime_sel + 1 < page_cnt)
            c->t->ime_sel++;
        tui_line_redraw(c->t);
    }
    return RD_IME_TAKEN;
}

/* 取消、退格、翻页与页内高亮：只影响候选态，不改输入行文本。 */
static int rd_ime_nav(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;

    if (key == 0x1b) {
        t->ime_buf_len = 0;
        t->ime_buf[0] = '\0';
        t->ime_cand_count = 0;
        t->ime_active = 0;
    } else if (key == 0x7f || key == 0x08) {
        if (t->ime_buf_len > 0) {
            t->ime_buf[--t->ime_buf_len] = '\0';
            tui_ime_refresh(t);
        } else {
            t->ime_active = 0; /* 拼音已空：退格转为退出拼音模式 */
        }
    } else if (key == ',' || key == '.' || key == TUI_KEY_PGUP ||
               key == TUI_KEY_PGDN) {
        return rd_ime_page(c, key);
    } else if (key == TUI_KEY_LEFT || key == TUI_KEY_RIGHT) {
        return rd_ime_sel(c, key);
    } else {
        return RD_IME_NONE;
    }
    tui_line_redraw(t);
    return RD_IME_TAKEN;
}

/* 上屏收尾：Enter 有候选则上屏候选，其余一律提交拼音原文后落回通用路径。 */
static int rd_ime_out(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;
    int enter = key == '\n' || key == '\r';
    int idx = tui_ime_sel_index(t);

    if (!enter && (key < 0x20 || key > 0xFF))
        return RD_IME_NONE;
    if (enter && idx >= 0)
        tui_ime_commit_cand(t, t->ime_cands[idx].text);
    else
        tui_ime_commit_raw(t);
    t->ime_active = 0;
    tui_line_redraw(t);
    return RD_IME_PASS;
}

static int rd_ime_key(rd_ctx_t *c, int key)
{
    int r = rd_ime_ch(c, key);

    if (r == RD_IME_NONE)
        r = rd_ime_nav(c, key);
    return r == RD_IME_NONE ? rd_ime_out(c, key) : r;
}

/* 历史回到底部：恢复进入历史前的编辑草稿。 */
static void rd_draft(rd_ctx_t *c)
{
    cli_tui_t *t = c->t;

    if (t->cmd_hist_edit && t->cmd_hist_edit_len > 0) {
        t->input_len = t->cmd_hist_edit_len;
        AIRY_MEMCPY(t->input, t->cmd_hist_edit, t->cmd_hist_edit_len);
        t->input[t->input_len] = '\0';
        t->input_col = t->input_len;
        return;
    }
    rd_clear(c);
}

static int rd_hist(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;

    if (key == TUI_KEY_UP && t->cmd_hist_idx > 0) {
        tui_cmd_hist_save_draft(t);
        t->cmd_hist_idx--;
        tui_cmd_hist_apply(t, t->cmd_hist_idx);
    } else if (key == TUI_KEY_DOWN && t->cmd_hist_idx < t->cmd_hist.count) {
        t->cmd_hist_idx++;
        if (t->cmd_hist_idx >= t->cmd_hist.count)
            rd_draft(c);
        else
            tui_cmd_hist_apply(t, t->cmd_hist_idx);
    }
    tui_line_redraw(t);
    return 1;
}

/* UTF-8 安全横移一列：左移回退到上一字符首字节，右移跳过续字节。 */
static int rd_move(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;
    size_t n = t->input_col;

    if (key == TUI_KEY_LEFT) {
        if (n == 0)
            return 1;
        while (n > 1 && ((unsigned char)t->input[n - 1] & 0xC0) == 0x80)
            n--;
        t->input_col = n - 1;
    } else {
        if (n >= t->input_len)
            return 1;
        n++;
        while (n < t->input_len && ((unsigned char)t->input[n] & 0xC0) == 0x80)
            n++;
        t->input_col = n;
    }
    tui_line_redraw(t);
    return 1;
}

static void rd_submit(rd_ctx_t *c)
{
    size_t n = c->t->input_len;

    if (n >= c->cap)
        n = c->cap - 1;
    if (n > 0)
        AIRY_MEMCPY(c->buf, c->t->input, n);
    c->buf[n] = '\0';
    if (c->out_len)
        *c->out_len = n;
    tui_cmd_hist_push(c->t, c->buf);
    tui_cmd_hist_save(c->t);
    rd_clear(c);
}

/* 控制键与 Tab：返回 1 已消费；0 未处理；-1 空行上 Ctrl+C/Ctrl+D 请求退出。 */
static int rd_ctrl(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;

    switch (key) {
    case 0x09: /* Tab：命令补全，无候选时跳过尾部重绘 */
        if (!tui_tab_complete(t))
            return 1;
        break;
    case 0x03: /* Ctrl+C：非空清行，空行退出 */
        if (t->input_len == 0)
            return -1;
        rd_clear(c);
        break;
    case 0x04: /* Ctrl+D：非空删光标处字符，空行 EOF */
        if (t->input_len == 0)
            return -1;
        tui_input_delete_fwd(t);
        break;
    case 0x01: /* Ctrl+A */
        t->input_col = 0;
        break;
    case 0x05: /* Ctrl+E */
        t->input_col = t->input_len;
        break;
    case 0x15: /* Ctrl+U */
        if (t->input_col > 0) {
            AIRY_MEMMOVE(t->input, t->input + t->input_col,
                         t->input_len - t->input_col + 1);
            t->input_len -= t->input_col;
            t->input_col = 0;
        }
        break;
    case 0x0b: /* Ctrl+K */
        if (t->input_col < t->input_len) {
            t->input[t->input_col] = '\0';
            t->input_len = t->input_col;
        }
        break;
    default:
        return 0;
    }
    tui_line_redraw(t);
    return 1;
}

/* 通用按键：返回 1 已消费；0 未识别；-1 提交后退出循环。 */
static int rd_dispatch(rd_ctx_t *c, int key)
{
    cli_tui_t *t = c->t;

    if (key == '\n' || key == '\r') {
        rd_submit(c);
        return -1;
    }
    if (key == 0x7f || key == 0x08) {
        tui_input_backspace(t);
        tui_line_redraw(t);
        return 1;
    }
    if (key == TUI_KEY_LEFT || key == TUI_KEY_RIGHT)
        return rd_move(c, key);
    if (key == TUI_KEY_UP || key == TUI_KEY_DOWN)
        return rd_hist(c, key);
    if (key >= 0x01 && key <= 0x1a)
        return rd_ctrl(c, key);
    if (key >= 0x20 && key <= 0xFF) {
        tui_input_append(t, (char)key);
        /* UTF-8 完整序列到达才重绘：避免逐字节渲染的乱码帧 */
        if (tui_input_utf8_complete(t->input, t->input_len))
            tui_line_redraw(t);
        return 1;
    }
    return 0;
}

int tui_readline_line_mode(cli_tui_t *t, char *buf, size_t cap,
                           size_t *out_len)
{
    rd_ctx_t c = { t, buf, cap, out_len };
    int rc = 1;

    if (!t)
        return 0;
    rd_init(&c);

#ifndef _WIN32
    struct termios saved;
    int saved_ok = rd_raw_on(&saved);
#else
    int saved_ok = 0;
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

    for (;;) {
        int eof = 0;
        /* 以半周期超时轮询：到达闪烁节拍翻转光标并局部重绘输入行 */
        int key = tui_read_key(t, (int)CLI_CARET_BLINK_MS, &eof);

        if (eof || key == 0) {
            rc = 0;
            break;
        }
        if (key == -1) {
            rd_idle(t);
            continue;
        }
        /* 2.2.3 内置拼音输入法：中/英切换。 */
        if (tui_ime_key_hit(t, key)) {
            t->ime_active = !t->ime_active;
            if (!t->ime_active)
                tui_ime_commit_raw(t); /* 切回英文：拼音原文保留在输入行 */
            tui_line_redraw(t);
            continue;
        }
        if (t->ime && t->ime_active && rd_ime_key(&c, key) == RD_IME_TAKEN)
            continue;
        if (rd_dispatch(&c, key) < 0)
            break;
    }

#ifndef _WIN32
    rd_raw_off(saved_ok, &saved);
#else
    (void)saved_ok;
#endif
    /* 2.2.1.5：恢复硬件光标（自绘反显光标只在输入期间接管） */
    fputs("\033[?25h", stdout);
    return rc;
}
