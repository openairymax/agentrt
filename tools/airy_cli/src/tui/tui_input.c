// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_input.c
 * @brief TUI 引擎输入编辑域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 输入行编辑原语（insert/backspace/delete/word/transpose/kill/yank）与
 * 自绘反显光标（黑白闪烁）及 UTF-8 完整性判定。2026-08-27 二轮拆分：
 * Tab 补全 → tui_complete.c；行渲染模式 readline → tui_line_readline.c。
 * 共享声明见 cli_tui_internal.h。
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
