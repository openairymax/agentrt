// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_history.c
 * @brief TUI 引擎历史与搜索域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 包含会话历史（committed lines + 视图 pin）、已提交命令历史
 * （Up/Down 浏览 + Ctrl+R/Ctrl+S 增量搜索）与跨会话持久化。
 */

#include "cli_tui_internal.h"

/* ---- history / viewport model ---- */

#define TUI_HISTORY_REL_PATH "agentrt/cli/history"

static void tui_grow_history(tui_history_t *h)
{
    if (h->count >= h->cap) {
        size_t new_cap = h->cap ? h->cap * 2 : TUI_HIST_INIT_CAP;
        char **grown = (char **)AIRY_REALLOC(h->lines, new_cap * sizeof(char *));
        if (!grown)
            return;
        h->lines = grown;
        h->cap = new_cap;
    }
}

void tui_commit_line(cli_tui_t *t, char *line)
{
    tui_grow_history(&t->hist);
    if (t->hist.count < t->hist.cap)
        t->hist.lines[t->hist.count++] = line;
    else
        AIRY_FREE(line);
    /* Commit growth never resets the pin: header lines were committed
     * before pin_header() ran. */
}

void tui_history_reset(tui_history_t *h)
{
    for (size_t i = 0; i < h->count; i++)
        AIRY_FREE(h->lines[i]);
    AIRY_FREE(h->lines);
    h->lines = NULL;
    h->count = 0;
    h->cap = 0;
    h->pinned = 0;
}

/* ---- submitted-command history (Ctrl+R search / Up browse) ---- */

void tui_cmd_hist_push(cli_tui_t *t, const char *line)
{
    if (!line || !line[0])
        return;
    /* Do not push consecutive duplicates (same as readline dedup). */
    if (t->cmd_hist.count > 0 &&
        strcmp(t->cmd_hist.entries[t->cmd_hist.count - 1], line) == 0)
        return;
    if (t->cmd_hist.count >= TUI_CMD_HIST_MAX) {
        /* Ring: drop the oldest, shift down. */
        AIRY_FREE(t->cmd_hist.entries[0]);
        for (size_t i = 1; i < t->cmd_hist.count; i++)
            t->cmd_hist.entries[i - 1] = t->cmd_hist.entries[i];
        t->cmd_hist.count--;
    }
    if (t->cmd_hist.count >= t->cmd_hist.cap) {
        size_t new_cap = t->cmd_hist.cap ? t->cmd_hist.cap * 2 : 32;
        while (new_cap < t->cmd_hist.count + 1)
            new_cap *= 2;
        char **grown = (char **)AIRY_REALLOC(t->cmd_hist.entries,
                                             new_cap * sizeof(char *));
        if (!grown)
            return;
        t->cmd_hist.entries = grown;
        t->cmd_hist.cap = new_cap;
    }
    char *copy = AIRY_STRDUP(line);
    if (copy)
        t->cmd_hist.entries[t->cmd_hist.count++] = copy;
}

void tui_cmd_hist_reset(cli_tui_t *t)
{
    for (size_t i = 0; i < t->cmd_hist.count; i++)
        AIRY_FREE(t->cmd_hist.entries[i]);
    AIRY_FREE(t->cmd_hist.entries);
    t->cmd_hist.entries = NULL;
    t->cmd_hist.count = 0;
    t->cmd_hist.cap = 0;
    t->cmd_hist_idx = 0;
    AIRY_FREE(t->cmd_hist_edit);
    t->cmd_hist_edit = NULL;
    t->cmd_hist_edit_len = 0;
    t->cmd_hist_edit_cap = 0;
}

static void tui_history_path(char *buf, size_t cap)
{
    snprintf(buf, cap, "%s/%s", airy_data_dir(), TUI_HISTORY_REL_PATH);
}

#ifndef _WIN32
/* Create the parent directories of `path` (path itself is a file). */
static void tui_history_mkdir(const char *path)
{
    char dir[AIRY_PATH_MAX];
    size_t n = strlen(path);
    if (n >= sizeof(dir))
        return;
    AIRY_MEMCPY(dir, path, n + 1);
    /* Drop the final component (the history file name). */
    char *slash = strrchr(dir, '/');
    if (slash)
        *slash = '\0';
    for (char *p = dir + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(dir, 0755);
            *p = '/';
        }
    }
    mkdir(dir, 0755);
}
#endif

void tui_cmd_hist_load(cli_tui_t *t)
{
    if (!t)
        return;
#ifdef _WIN32
    (void)t;
    return;
#else
    char path[AIRY_PATH_MAX];
    tui_history_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[TUI_CMD_HIST_MAX];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0)
            continue;
        tui_cmd_hist_push(t, line);
    }
    fclose(f);
    t->cmd_hist_idx = t->cmd_hist.count;
#endif
}

void tui_cmd_hist_save(cli_tui_t *t)
{
    if (!t)
        return;
#ifdef _WIN32
    (void)t;
    return;
#else
    char path[AIRY_PATH_MAX];
    tui_history_path(path, sizeof(path));
    tui_history_mkdir(path);
    char tmp[AIRY_PATH_MAX + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    for (size_t i = 0; i < t->cmd_hist.count; i++)
        fprintf(f, "%s\n", t->cmd_hist.entries[i]);
    fclose(f);
    rename(tmp, path);
#endif
}

/* Preserve the current in-progress input so Up/Down browsing can return. */
void tui_cmd_hist_save_draft(cli_tui_t *t)
{
    if (t->cmd_hist_edit_len == t->input_len &&
        (t->input_len == 0 ||
         (t->cmd_hist_edit && memcmp(t->cmd_hist_edit, t->input, t->input_len) == 0)))
        return;
    if (t->input_len + 1 > t->cmd_hist_edit_cap) {
        size_t new_cap = t->cmd_hist_edit_cap ? t->cmd_hist_edit_cap * 2 : 128;
        while (new_cap < t->input_len + 1)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->cmd_hist_edit, new_cap);
        if (!grown)
            return;
        t->cmd_hist_edit = grown;
        t->cmd_hist_edit_cap = new_cap;
    }
    AIRY_MEMCPY(t->cmd_hist_edit, t->input, t->input_len);
    t->cmd_hist_edit[t->input_len] = '\0';
    t->cmd_hist_edit_len = t->input_len;
}

/* Replace the input line with a history entry. */
void tui_cmd_hist_apply(cli_tui_t *t, size_t idx)
{
    const char *src = t->cmd_hist.entries[idx];
    size_t n = strlen(src);
    if (n + 1 > t->input_cap) {
        size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
        while (new_cap < n + 1)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
        if (!grown)
            return;
        t->input = grown;
        t->input_cap = new_cap;
    }
    AIRY_MEMCPY(t->input, src, n);
    t->input[n] = '\0';
    t->input_len = n;
    t->input_col = n;
}

/* Case-insensitive substring match (used by Ctrl+R search). */
static int tui_search_match(const char *hay, const char *needle)
{
    if (!needle[0])
        return 1;
    if (!hay)
        return 0;
    size_t hn = strlen(hay);
    size_t nn = strlen(needle);
    if (nn > hn)
        return 0;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t j = 0;
        while (j < nn) {
            char a = hay[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z')
                a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = (char)(b - 'A' + 'a');
            if (a != b)
                break;
            j++;
        }
        if (j == nn)
            return 1;
    }
    return 0;
}

/* Re-run the reverse search from the current match backward. */
void tui_search_step(cli_tui_t *t)
{
    if (t->cmd_hist.count == 0) {
        t->search_match = -1;
        return;
    }
    size_t start = (t->search_match >= 0) ? (size_t)t->search_match : t->cmd_hist.count;
    for (size_t k = 1; k <= t->cmd_hist.count; k++) {
        size_t idx = (start + t->cmd_hist.count - k) % t->cmd_hist.count;
        if (tui_search_match(t->cmd_hist.entries[idx], t->search_query)) {
            t->search_match = (ssize_t)idx;
            t->search_wrapped = 0;
            return;
        }
    }
    t->search_match = -1;
}

/* Forward search (Ctrl+S): walk from the current match toward the newest
 * entries. Initial call (no match yet) starts at the oldest entry. */
void tui_search_step_forward(cli_tui_t *t)
{
    if (t->cmd_hist.count == 0) {
        t->search_match = -1;
        return;
    }
    size_t start = (t->search_match >= 0) ? (size_t)t->search_match : t->cmd_hist.count - 1;
    for (size_t k = 1; k <= t->cmd_hist.count; k++) {
        size_t idx = (start + k) % t->cmd_hist.count;
        if (tui_search_match(t->cmd_hist.entries[idx], t->search_query)) {
            t->search_match = (ssize_t)idx;
            t->search_wrapped = 0;
            return;
        }
    }
    t->search_match = -1;
}
