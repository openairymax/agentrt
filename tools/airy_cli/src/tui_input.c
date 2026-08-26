// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_input.c
 * @brief TUI 引擎输入编辑域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 包含输入行编辑（insert/backspace/delete/word/transpose/kill/yank）、
 * 自绘反显光标（黑白闪烁）、Tab 补全（命令 SSoT + 文件系统）与
 * 行渲染模式（非全屏）readline。
 */

#include "cli_tui_internal.h"

void tui_input_append(cli_tui_t *t, char c)
{
    t->tab_active = 0; /* 编辑输入即离开补全模式 */
    if (t->input_len + 2 > t->input_cap) {
        size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
        while (new_cap < t->input_len + 2)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
        if (!grown)
            return;
        t->input = grown;
        t->input_cap = new_cap;
    }
    /* Insert at the edit position (mid-line typing) instead of appending,
     * so Ctrl+A / Left-arrow editing works before committing. */
    if (t->input_col >= t->input_len) {
        t->input[t->input_len++] = c;
    } else {
        if (t->input_len + 2 > t->input_cap) {
            size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
            while (new_cap < t->input_len + 2)
                new_cap *= 2;
            char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
            if (!grown)
                return;
            t->input = grown;
            t->input_cap = new_cap;
        }
        AIRY_MEMMOVE(t->input + t->input_col + 1, t->input + t->input_col,
                t->input_len - t->input_col);
        t->input[t->input_col] = c;
        t->input_len++;
    }
    t->input[t->input_len] = '\0';
    t->input_col++;
}

/* ==================== 输入光标（2.2.1.5 黑白交替闪烁） ==================== */

/* 推进闪烁状态机：到达半周期翻转反显状态。输入循环每轮调用（轮询节拍）。 */
void tui_caret_tick(cli_tui_t *t)
{
    uint64_t now = cli_now_ms();
    if (now - t->caret_tick >= CLI_CARET_BLINK_MS) {
        t->caret_visible = !t->caret_visible;
        t->caret_tick = now;
    }
}

/* 打印输入文本，光标处字符按 caret_visible 反显（黑白交替）。UTF-8 光标
 * 位置以 input_col 字节定位，多字节字符整体反显；光标在行尾/空输入时
 * 以反显空格块呈现（Word 光标行为）。返回文本显示宽度。 */
size_t tui_caret_print(cli_tui_t *t)
{
    size_t p = t->input_col < t->input_len ? t->input_col : t->input_len;
    size_t w = cli_disp_width_of(t->input, t->input_len);
    if (p > 0)
        fwrite(t->input, 1, p, stdout);
    if (p < t->input_len) {
        size_t q = p + 1;
        while (q < t->input_len && ((unsigned char)t->input[q] & 0xC0) == 0x80)
            q++;
        if (t->caret_visible) {
            fputs(cli_c(CLR_REVERSE), stdout);
            fwrite(t->input + p, 1, q - p, stdout);
            fputs(cli_c(CLR_RESET), stdout);
        } else {
            fwrite(t->input + p, 1, q - p, stdout);
        }
        if (q < t->input_len)
            fwrite(t->input + q, 1, t->input_len - q, stdout);
    } else if (t->caret_visible) {
        /* 行尾/空输入：反显空格块作为光标位置 */
        fputs(cli_c(CLR_REVERSE), stdout);
        fputs(" ", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        w += 1;
    }
    return w;
}

/* 判断 input 末尾的 UTF-8 序列是否完整：ASCII 单字节恒完整；多字节序列
 * 需前导字节 + 足量续字节（2/3/4 字节）才完整。readline 逐字节读入时，
 * 中文的每个字节都会走到这里——序列不完整时暂缓重绘，攒齐后才刷新，
 * 避免输入过程渲染出 � 中间乱码帧。 */
int tui_input_utf8_complete(const char *s, size_t len)
{
    if (len == 0)
        return 1;
    unsigned char last = (unsigned char)s[len - 1];
    if (last < 0x80)
        return 1; /* ASCII：单字节，恒完整 */

    /* 从末尾回退到最后一个前导字节（0xC0-0xF7），统计续字节数。 */
    size_t i = len;
    size_t cont = 0;
    while (i > 1 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) {
        i--;
        cont++;
    }
    if (i == 0)
        return 0; /* 只有续字节，无前导 → 不完整 */
    unsigned char lead = (unsigned char)s[i - 1];
    size_t need;
    if ((lead & 0xE0) == 0xC0)
        need = 1; /* 2 字节 */
    else if ((lead & 0xF0) == 0xE0)
        need = 2; /* 3 字节 */
    else if ((lead & 0xF8) == 0xF0)
        need = 3; /* 4 字节 */
    else
        return 1; /* 非法前导（0x80-0xBF 等），按完整处理避免卡住 */
    return (cont >= need) ? 1 : 0;
}

void tui_input_backspace(cli_tui_t *t)
{
    t->tab_active = 0; /* 编辑输入即离开补全模式 */
    if (t->input_len == 0 || t->input_col == 0)
        return;
    /* Delete one full UTF-8 character before the cursor: rewind trailing
     * continuation bytes (0x80..0xBF) plus the leading byte, so CJK input
     * is not torn apart. */
    size_t n = t->input_col;
    while (n > 1 && ((unsigned char)t->input[n - 1] & 0xC0) == 0x80)
        n--;
    if (n > 0)
        n--;
    AIRY_MEMMOVE(t->input + n, t->input + t->input_col, t->input_len - t->input_col + 1);
    t->input_len -= (t->input_col - n);
    t->input_col = n;
}

void tui_input_delete_fwd(cli_tui_t *t)
{
    if (t->input_col >= t->input_len)
        return;
    /* Delete the UTF-8 character at the cursor. */
    size_t n = t->input_col + 1;
    while (n < t->input_len && ((unsigned char)t->input[n] & 0xC0) == 0x80)
        n++;
    AIRY_MEMMOVE(t->input + t->input_col, t->input + n, t->input_len - n + 1);
    t->input_len -= (n - t->input_col);
}

void tui_input_back_word(cli_tui_t *t)
{
    size_t p = t->input_col;
    while (p > 0 && (t->input[p - 1] == ' ' || t->input[p - 1] == '\t'))
        p--;
    while (p > 0 && t->input[p - 1] != ' ' && t->input[p - 1] != '\t')
        p--;
    AIRY_MEMMOVE(t->input + p, t->input + t->input_col, t->input_len - t->input_col + 1);
    t->input_len -= (t->input_col - p);
    t->input_col = p;
}

/* Move the caret one word to the left (Alt+b / Ctrl+Left). */
void tui_input_word_left(cli_tui_t *t)
{
    size_t p = t->input_col;
    if (p == 0)
        return;
    /* Skip the blanks immediately before the caret (if any). */
    while (p > 0 && (t->input[p - 1] == ' ' || t->input[p - 1] == '\t'))
        p--;
    while (p > 0 && t->input[p - 1] != ' ' && t->input[p - 1] != '\t')
        p--;
    t->input_col = p;
}

/* Move the caret one word to the right (Alt+f / Ctrl+Right). */
void tui_input_word_right(cli_tui_t *t)
{
    size_t p = t->input_col;
    if (p >= t->input_len)
        return;
    /* Skip the word the caret is in, then the following blanks. */
    while (p < t->input_len && t->input[p] != ' ' && t->input[p] != '\t')
        p++;
    while (p < t->input_len && (t->input[p] == ' ' || t->input[p] == '\t'))
        p++;
    t->input_col = p;
}

/* Ctrl+T: transpose the two characters around the caret (readline
 * semantics). UTF-8 safe: whole characters are swapped, never torn. */
void tui_input_transpose(cli_tui_t *t)
{
    if (t->input_col == 0 || t->input_len < 2)
        return;

    size_t first, second, second_end;
    if (t->input_col >= t->input_len) {
        /* caret at line end: swap the last two characters */
        second_end = t->input_len;
        second = t->input_len - 1;
        while (second > 0 && ((unsigned char)t->input[second] & 0xC0) == 0x80)
            second--;
        if (second == 0)
            return;
        first = second - 1;
        while (((unsigned char)t->input[first] & 0xC0) == 0x80)
            first--;
    } else {
        /* caret in the middle: swap the char at the caret and the one before */
        second = t->input_col;
        while (second < t->input_len && ((unsigned char)t->input[second] & 0xC0) == 0x80)
            second++;
        second_end = second + 1;
        while (second_end < t->input_len &&
               ((unsigned char)t->input[second_end] & 0xC0) == 0x80)
            second_end++;
        first = t->input_col - 1;
        while (((unsigned char)t->input[first] & 0xC0) == 0x80)
            first--;
        if (first >= second)
            return;
    }

    size_t first_len = second - first;
    size_t second_len = second_end - second;
    if (second_len >= 8)
        return; /* UTF-8 单字符最长 4 字节，8 字节缓冲绝对安全 */

    /* 前字符右移 second_len，后字符落到 first 位置。 */
    char tmp[8];
    AIRY_MEMCPY(tmp, t->input + second, second_len);
    AIRY_MEMMOVE(t->input + first + second_len, t->input + first, first_len);
    AIRY_MEMCPY(t->input + first, tmp, second_len);

    t->input_col = (t->input_col >= t->input_len) ? t->input_len : second_end;
}

/* Stash the killed region for Ctrl+Y (simple single-slot kill-ring). */
void tui_input_kill_save(cli_tui_t *t, const char *text, size_t n)
{
    if (!text || n == 0) {
        t->kill_len = 0;
        return;
    }
    if (n >= sizeof(t->kill_buf))
        n = sizeof(t->kill_buf) - 1;
    AIRY_MEMCPY(t->kill_buf, text, n);
    t->kill_buf[n] = '\0';
    t->kill_len = n;
}

/* Ctrl+Y: insert the killed text at the caret. */
void tui_input_yank(cli_tui_t *t)
{
    if (t->kill_len == 0)
        return;
    size_t n = t->kill_len;
    if (t->input_len + n + 1 > t->input_cap) {
        size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
        while (new_cap < t->input_len + n + 1)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
        if (!grown)
            return;
        t->input = grown;
        t->input_cap = new_cap;
    }
    AIRY_MEMMOVE(t->input + t->input_col + n, t->input + t->input_col,
            t->input_len - t->input_col);
    AIRY_MEMCPY(t->input + t->input_col, t->kill_buf, n);
    t->input_len += n;
    t->input[t->input_len] = '\0';
    t->input_col += n;
}

/* ---- Tab 补全 ---- */

/* 用候选文本替换输入缓冲区中的 token（tok_start..tok_start+tok_len），
 * 光标落在替换文本末尾。 */
static void tui_tab_replace(cli_tui_t *t, size_t tok_start, size_t tok_len,
                            const char *text, size_t tlen)
{
    if (tok_start + tlen + 2 > t->input_cap) {
        size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
        while (new_cap < tok_start + tlen + 2)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
        if (!grown)
            return;
        t->input = grown;
        t->input_cap = new_cap;
    }
    AIRY_MEMMOVE(t->input + tok_start + tlen, t->input + tok_start + tok_len,
                 t->input_len - (tok_start + tok_len) + 1);
    AIRY_MEMCPY(t->input + tok_start, text, tlen);
    t->input_len += (tlen - tok_len);
    t->input_col = tok_start + tlen;
}

/* 目录遍历补全：dir 为目录路径（空串表示当前目录），base 为匹配前缀。 */
static size_t tui_tab_complete_fs(cli_tui_t *t, const char *dir, const char *base)
{
    size_t n = 0;
    const char *dpath = (dir && dir[0]) ? dir : ".";
    DIR *d = opendir(dpath);
    if (!d)
        return 0;

    size_t blen = strlen(base);
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < TUI_TAB_CAND_MAX) {
        const char *nm = e->d_name;
        if (nm[0] == '.') {
            /* 隐藏文件仅当前缀以 '.' 开头时补全。 */
            if (blen == 0 || base[0] != '.')
                continue;
        }
        if (strncmp(nm, base, blen) != 0)
            continue;

        int is_dir = 0;
        char full[TUI_TAB_NAME_MAX];
        if (dir && dir[0])
            snprintf(full, sizeof(full), "%s%c%s", dir, TUI_PATH_SEP, nm);
        else
            snprintf(full, sizeof(full), "%s", nm);
        struct stat st;
#ifdef _WIN32
        if (stat(full, &st) == 0 && (st.st_mode & _S_IFDIR))
            is_dir = 1;
#else
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            is_dir = 1;
#endif
        snprintf(t->tab_cand_strs[n], TUI_TAB_NAME_MAX, "%s%s", nm,
                 is_dir ? (TUI_PATH_SEP_STR) : "");
        n++;
    }
    closedir(d);
    return n;
}

int tui_input_tab_complete(cli_tui_t *t)
{
    if (!t->input || t->input_len == 0)
        return 0;

    /* 定位光标处的 token。 */
    size_t tok_start = 0;
    for (size_t i = 0; i < t->input_col; i++) {
        if (t->input[i] == ' ' || t->input[i] == '\t')
            tok_start = i + 1;
    }
    size_t tok_len = t->input_col - tok_start;

    if (t->input[tok_start] == '/') {
        /* 命令补全：token 以 '/' 开头。 */
        t->tab_kind = 0;
        size_t ncmds = cli_commands_count();

        /* Collect candidates whose name starts with the typed prefix. */
        size_t cand_idx[TUI_TAB_CAND_MAX];
        size_t n_cand = 0;
        for (size_t i = 0; i < ncmds && n_cand < TUI_TAB_CAND_MAX; i++) {
            const char *name = CLI_COMMANDS[i].name;
            if (strncmp(name, t->input + tok_start, tok_len) == 0)
                cand_idx[n_cand++] = i;
        }
        if (n_cand == 0) {
            t->tab_active = 0;
            return 0;
        }

        /* Reset the cycle when the typed token changed since the last Tab. */
        if (!t->tab_active || t->tab_sel >= n_cand)
            t->tab_sel = 0;
        t->tab_active = 1;
        t->tab_count = n_cand;
        for (size_t i = 0; i < n_cand; i++)
            t->tab_cands[i] = cand_idx[i];

        const char *match = CLI_COMMANDS[cand_idx[t->tab_sel]].name;
        size_t mlen = strlen(match);
        tui_tab_replace(t, tok_start, tok_len, match, mlen);

        /* Single candidate: append a trailing space so typing continues.
         * Multiple candidates: advance the cycle for the next Tab press. */
        if (n_cand == 1) {
            if (t->input_len + 1 < t->input_cap) {
                t->input[t->input_len++] = ' ';
                t->input[t->input_len] = '\0';
                t->input_col = t->input_len;
            }
        } else {
            t->tab_sel = (t->tab_sel + 1) % n_cand;
        }
        return 1;
    }

    /* 文件补全：token 为普通文本。拆分目录与 basename。 */
    const char *tok = t->input + tok_start;
    size_t slash = tok_len;
    while (slash > 0 && tok[slash - 1] != '/' && tok[slash - 1] != '\\')
        slash--;
    char dir[TUI_TAB_NAME_MAX];
    if (slash >= sizeof(dir))
        return 0;
    AIRY_MEMCPY(dir, tok, slash);
    dir[slash] = '\0';

    t->tab_kind = 1;
    size_t n = tui_tab_complete_fs(t, dir, tok + slash);
    if (n == 0) {
        t->tab_active = 0;
        return 0;
    }
    if (!t->tab_active || t->tab_sel >= n)
        t->tab_sel = 0;
    t->tab_active = 1;
    t->tab_count = n;

    const char *match = t->tab_cand_strs[t->tab_sel];
    size_t mlen = strlen(match);
    tui_tab_replace(t, tok_start, tok_len, match, mlen);

    /* 单候选：文件追加空格便于继续输入；目录不加（可继续 Tab 深入）。 */
    if (n == 1) {
        if (mlen > 0 && match[mlen - 1] != '/') {
            if (t->input_len + 1 < t->input_cap) {
                t->input[t->input_len++] = ' ';
                t->input[t->input_len] = '\0';
                t->input_col = t->input_len;
            }
        }
    } else {
        t->tab_sel = (t->tab_sel + 1) % n;
    }
    return 1;
}

/* ---- 行渲染模式（非全屏）readline ---- */

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
