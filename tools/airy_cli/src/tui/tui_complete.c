// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_complete.c
 * @brief TUI 引擎 Tab 补全域（域拆分自 tui_input.c，2026-08-27）。
 *
 * 命令补全（SSoT：cli_internal.h 的 CLI_COMMANDS 表）+ 文件系统补全
 * （目录遍历，隐藏文件仅当前缀以 '.' 开头时补全；目录候选补路径分隔符
 * 便于继续 Tab 深入）。共享声明见 cli_tui_internal.h。
 */

#include "cli_tui_internal.h"

/* Windows 无 dirent.h/sys/stat.h 的 S_IS* 宏：airy_dirent.h（FindFirstFileA
 * 实现）与 stat.h 在 _WIN32 下补齐（#112 实证 tui_complete.c C2065/C2079/
 * C2224：DIR/struct stat/_S_IFDIR 均未声明） */
#include "airy_dirent.h"
#include <sys/stat.h>

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

/* 新一轮补全开始时复位循环：token 变化或候选集收缩都要从 0 重新开始。 */
static void tui_tab_arm(cli_tui_t *t, size_t n)
{
    if (!t->tab_active || t->tab_sel >= n)
        t->tab_sel = 0;
    t->tab_active = 1;
    t->tab_count = n;
}

/* 单候选收尾：补一个空格，避免用户重新敲分隔符。 */
static void tui_tab_space(cli_tui_t *t)
{
    if (t->input_len + 1 >= t->input_cap)
        return;
    t->input[t->input_len++] = ' ';
    t->input[t->input_len] = '\0';
    t->input_col = t->input_len;
}

/* 候选落地（命令与文件两条路径共用）：
 * 多候选时推进循环，单候选时补空格——目录以分隔符结尾，不补。 */
static int tui_tab_pick(cli_tui_t *t, size_t ts, size_t tl,
                        const char *pick, size_t n)
{
    size_t mlen = strlen(pick);

    tui_tab_replace(t, ts, tl, pick, mlen);
    if (n > 1) {
        t->tab_sel = (t->tab_sel + 1) % n;
        return 1;
    }
    if (mlen > 0 && pick[mlen - 1] != TUI_PATH_SEP)
        tui_tab_space(t);
    return 1;
}

/* 命令补全：token 以 '/' 开头，在 CLI_COMMANDS 表内做前缀匹配。 */
static int tui_tab_cmd(cli_tui_t *t, size_t ts, size_t tl)
{
    size_t cand[TUI_TAB_CAND_MAX];
    size_t n = 0;
    size_t ncmds = cli_commands_count();

    for (size_t i = 0; i < ncmds && n < TUI_TAB_CAND_MAX; i++) {
        if (strncmp(CLI_COMMANDS[i].name, t->input + ts, tl) == 0)
            cand[n++] = i;
    }
    if (n == 0) {
        t->tab_active = 0;
        return 0;
    }
    tui_tab_arm(t, n);
    for (size_t i = 0; i < n; i++)
        t->tab_cands[i] = cand[i];
    return tui_tab_pick(t, ts, tl, CLI_COMMANDS[cand[t->tab_sel]].name, n);
}

/* 文件补全：把 token 拆成目录与 basename，再交给目录遍历。 */
static int tui_tab_fs(cli_tui_t *t, size_t ts, size_t tl)
{
    const char *tok = t->input + ts;
    size_t slash = tl;
    char dir[TUI_TAB_NAME_MAX];

    while (slash > 0 && tok[slash - 1] != '/' && tok[slash - 1] != '\\')
        slash--;
    if (slash >= sizeof(dir))
        return 0;
    AIRY_MEMCPY(dir, tok, slash);
    dir[slash] = '\0';

    size_t n = tui_tab_complete_fs(t, dir, tok + slash);
    if (n == 0) {
        t->tab_active = 0;
        return 0;
    }
    tui_tab_arm(t, n);
    return tui_tab_pick(t, ts, tl, t->tab_cand_strs[t->tab_sel], n);
}

int tui_tab_complete(cli_tui_t *t)
{
    if (!t->input || t->input_len == 0)
        return 0;

    /* 定位光标处的 token。 */
    size_t ts = 0;
    for (size_t i = 0; i < t->input_col; i++) {
        if (t->input[i] == ' ' || t->input[i] == '\t')
            ts = i + 1;
    }
    size_t tl = t->input_col - ts;

    if (t->input[ts] == '/')
        return tui_tab_cmd(t, ts, tl);
    return tui_tab_fs(t, ts, tl);
}
