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

int tui_tab_complete(cli_tui_t *t)
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
