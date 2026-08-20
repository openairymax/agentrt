// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.c
 * @brief Full-screen TUI engine implementation.
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
 * Emitted lines (cli_tui_emit) are folded into an in-memory history; the
 * visible viewport is the tail of that history (or a scroll offset into it
 * while browsing). Every redraw rewrites the header + viewport + input line
 * from the buffer, so scrolling is fully local to the CLI (no reliance on
 * the terminal's own scrollback).
 *
 * ANSI used: alt screen 1049, cursor home, erase line, cursor movement.
 * No curses — plain POSIX termios + ANSI, consistent with the project's
 * "no curses" rendering philosophy.
 */

#include "cli_tui.h"

#include "cli_render.h"
#include "cli_term.h"
#include "cli_internal.h" /* CLI_COMMANDS (Tab 补全 SSoT) */
#include "airy_dirent.h"  /* 跨平台 opendir/readdir/closedir（文件补全） */
#include "airy_memory.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TUI_PATH_SEP '\\'
#define TUI_PATH_SEP_STR "\\"
#else
#define TUI_PATH_SEP '/'
#define TUI_PATH_SEP_STR "/"
#endif

#ifdef _WIN32
/* TUI is POSIX-only: everything degrades to line-oriented stdout. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

/* SIGWINCH notification: set by the handler (async-signal-safe), cleared by
 * the read loop when a redraw happens. Kept as a plain sig_atomic_t flag so
 * the handler never touches the TUI structure from signal context. */
#ifndef _WIN32
static volatile sig_atomic_t g_tui_resize_pending;

static void tui_sigwinch_handler(int sig)
{
    (void)sig;
    g_tui_resize_pending = 1;
}
#endif

/* ---- history / viewport model ---- */

/* Process-wide engine handle (accessor for GCCP etc.). */
static cli_tui_t *g_default_tui;

cli_tui_t *cli_tui_get_default(void)
{
    return g_default_tui;
}

#define TUI_HIST_INIT_CAP 256
#define TUI_LINE_INIT_CAP 128
#define TUI_BOTTOM_INPUT_LINES 2 /* input line + one spare row */

#define TUI_TAB_CAND_MAX 64      /* Tab 补全候选上限 */
#define TUI_TAB_NAME_MAX 192     /* 文件/目录候选字符串长度上限 */
#define TUI_CMD_HIST_MAX 500
#define TUI_SEARCH_QUERY_MAX 256
#define TUI_INPUT_PREFIX "airy> " /* input prompt, width tracked in bytes */

typedef struct {
    char **lines;    /* committed history lines (no trailing '\n') */
    size_t count;    /* number of committed lines */
    size_t cap;      /* allocated slots */
    size_t pinned;   /* lines [0, pinned) form the fixed header */
} tui_history_t;

typedef struct {
    char **entries;  /* submitted input lines, oldest first */
    size_t count;
    size_t cap;
} tui_cmd_history_t;

struct cli_tui_s {
    int active;      /* full-screen page active */
    int dirty;       /* a redraw is pending */

    int rows;
    int cols;

    tui_history_t hist;
    char *cur;       /* partial line being accumulated */
    size_t cur_len;
    size_t cur_cap;

    size_t viewport_rows; /* rows usable by the middle viewport */
    size_t scroll_off;    /* history lines scrolled back (0 = live tail) */

    /* ---- 阶段 4：视图模式（tab）+ 面板数据源 ---- */
    cli_tui_mode_t mode;
    struct {
        void *ud;
        cli_tui_panel_count_fn count;
        cli_tui_panel_line_fn line;
        cli_tui_panel_action_fn action;
    } panel[CLI_TUI_MODE_MAX];

    /* ---- 阶段 4：面板可操作状态（2026-08-19）----
     *   sel          任务看板选中行索引（↑/↓ 移动，Enter 详情）
     *   detail_active 任务详情视图激活（BOARD，Esc/Enter 返回列表）
     *   detail        DETAIL action 回填的详情文本（\n 分隔行）
     *   follow        事件流实时跟随（尾部刷新；浏览时关闭）
     *   note          操作结果提示（标题栏展示，如取消反馈/过滤名） */
    size_t sel;
    int detail_active;
    char detail[4096];
    size_t detail_len;
    int follow;
    char note[160];

    /* ---- input line state ---- */
    char *input;     /* input line being edited */
    size_t input_len;
    size_t input_cap;
    size_t input_col;   /* cursor column within input (byte offset) */

    /* submitted-command history (Up/Down browse while typing) */
    tui_cmd_history_t cmd_hist;
    size_t cmd_hist_idx;     /* browsing index (count = past-newest) */
    char *cmd_hist_edit;     /* preserved in-progress input while browsing */
    size_t cmd_hist_edit_len;
    size_t cmd_hist_edit_cap;

    /* Ctrl+R reverse incremental search */
    int search_active;
    char search_query[TUI_SEARCH_QUERY_MAX];
    size_t search_query_len;
    ssize_t search_match;    /* index into cmd_hist.entries (-1 = none) */
    int search_wrapped;
    int search_forward;      /* Ctrl+S forward search (Ctrl+R = reverse) */

    /* kill-ring: Ctrl+U/K/W stash the killed text, Ctrl+Y yanks it back.
     * Single-slot is enough for the interactive line-editing model. */
    char kill_buf[4096];
    size_t kill_len;

    /* bracketed paste: raw bytes between ESC[200~ and ESC[201~ are inserted
     * literally (newlines folded to spaces), never interpreted as keys. */
    int paste_active;

    /* Tab completion (SSoT: CLI_COMMANDS in main.c) */
    int tab_active;          /* 多候选轮转中 */
    size_t tab_count;        /* 匹配候选数 */
    size_t tab_sel;          /* 当前选中在候选流中的位置 */
    int tab_kind;            /* 0=命令索引候选；1=文件/目录字符串候选 */
    size_t tab_cands[TUI_TAB_CAND_MAX];             /* 命令索引（tab_kind==0） */
    char tab_cand_strs[TUI_TAB_CAND_MAX][TUI_TAB_NAME_MAX]; /* 文件候选（tab_kind==1） */

    char status[96];         /* 会话状态（输入行右侧 dim 指示：模型/消息/耗时） */

#ifndef _WIN32
    struct termios saved_termios;
    int termios_saved;
#endif
};

/* ---- 阶段 4：视图模式（tab）+ 面板数据源（struct 定义后，可访问字段） ---- */

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

static const char *tui_mode_name(cli_tui_mode_t m)
{
    switch (m) {
    case CLI_TUI_MODE_CHAT:
        return "对话";
    case CLI_TUI_MODE_BOARD:
        return "任务看板";
    case CLI_TUI_MODE_EVENTS:
        return "事件流";
    default:
        return "?";
    }
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

static void tui_commit_line(cli_tui_t *t, char *line)
{
    tui_grow_history(&t->hist);
    if (t->hist.count < t->hist.cap)
        t->hist.lines[t->hist.count++] = line;
    else
        AIRY_FREE(line);
    /* Commit growth never resets the pin: header lines were committed
     * before pin_header() ran. */
}

static void tui_history_reset(tui_history_t *h)
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

static void tui_cmd_hist_push(cli_tui_t *t, const char *line)
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

static void tui_cmd_hist_reset(cli_tui_t *t)
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

/* ---- submitted-command history persistence (cross-session recall) ----
 * The command history lives at $AIRY_HOME/data/agentrt/cli/history, one
 * command per line, so Up/Ctrl+R recall survives restarts (Claude Code /
 * readline convention). Loaded at engine create; appended on every submit.
 * File I/O is best-effort: a missing/unwritable file only degrades recall,
 * never blocks input. */
#define TUI_HISTORY_REL_PATH "agentrt/cli/history"

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

static void tui_cmd_hist_load(cli_tui_t *t)
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

static void tui_cmd_hist_save(cli_tui_t *t)
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
static void tui_cmd_hist_save_draft(cli_tui_t *t)
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
static void tui_cmd_hist_apply(cli_tui_t *t, size_t idx)
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
static void tui_search_step(cli_tui_t *t)
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
static void tui_search_step_forward(cli_tui_t *t)
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

static void tui_append_byte(cli_tui_t *t, char c)
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
    /* Redraw immediately so streamed chunks appear live (typewriter effect).
     * Each cli_outn() call is one render tick — chunk-granular, cheap enough. */
    t->dirty = 1;
    cli_tui_redraw(t);
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
        t->dirty = 1;
    }
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

/* ---- terminal helpers ---- */

static void tui_get_size(cli_tui_t *t)
{
    if (!t)
        return;
    /* 复用 cli_term_size 的完整回退链（ioctl → COLUMNS/LINES → 默认 24/80）。
     * 此前 winsize=0（script/CI/非交互终端）时回退固定 24×80，与
     * cli_term_header_pin 的 rows 计算（COLUMNS/LINES 回退）不同步——
     * 输入行画进滚动区（row 24），破坏三区布局（2026-08-20 PTY 复现）。 */
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    t->rows = (rows > 0) ? rows : 24;
    t->cols = (cols > 0) ? cols : 80;
}

static void tui_write_literal(const char *s)
{
    if (s && *s)
        fputs(s, stdout);
}

/* ---- viewport geometry ---- */

static size_t tui_middle_rows(cli_tui_t *t)
{
    size_t avail = t->rows > 0 ? (size_t)t->rows : 24;
    size_t header = t->hist.pinned;
    size_t input_rows = TUI_BOTTOM_INPUT_LINES;
    if (avail <= header + input_rows)
        return 0;
    return avail - header - input_rows;
}

/* ---- full redraw ---- */

static void tui_clear_line(void)
{
    tui_write_literal("\033[2K");
}

/* 看板选中行反显（reverse video）；不改变字节长度，供 fputs 直接输出 */
static void tui_render_select(char *buf, size_t cap)
{
    char tmp[512];
    size_t n = strlen(buf);
    if (n + 1 >= sizeof(tmp))
        n = sizeof(tmp) - 1;
    AIRY_MEMCPY(tmp, buf, n);
    tmp[n] = '\0';
    snprintf(buf, cap, "\033[7m%s\033[27m", tmp);
}

static void tui_render_header(cli_tui_t *t)
{
    for (size_t i = 0; i < t->hist.pinned && i < t->hist.count; i++) {
        tui_write_literal("\033[");
        /* 1-based row */
        char num[16];
        snprintf(num, sizeof(num), "%zu", i + 1);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(t->hist.lines[i], stdout);
    }
}

static void tui_render_viewport(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    size_t start_row = t->hist.pinned + 1;

    if (rows == 0)
        return;

    /* 阶段 4：面板模式（任务看板/事件流）渲染面板内容；对话模式走历史 */
    if (t->mode != CLI_TUI_MODE_CHAT) {
        const cli_tui_panel_count_fn count = t->panel[t->mode].count;
        const cli_tui_panel_line_fn line = t->panel[t->mode].line;
        const void *ud = t->panel[t->mode].ud;

        /* ---- 任务详情视图（BOARD，Enter 进入，Esc/Enter 返回）---- */
        if (t->detail_active) {
            size_t dl = t->detail_len;
            size_t dstart = 0;
            char buf[512];
            for (size_t r = 0; r < rows; r++) {
                tui_write_literal("\033[");
                char num[16];
                snprintf(num, sizeof(num), "%zu", start_row + r);
                tui_write_literal(num);
                tui_write_literal(";1H");
                tui_clear_line();
                if (r == 0) {
                    snprintf(buf, sizeof(buf), "%s%s%s  %s%s%s",
                             cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                             "任务详情", cli_c(CLR_DIM),
                             "· Esc 返回列表", cli_c(CLR_RESET));
                } else {
                    size_t ln = 0;
                    while (dstart + ln < dl && t->detail[dstart + ln] != '\n')
                        ln++;
                    size_t keep = (ln < sizeof(buf) - 1) ? ln : sizeof(buf) - 1;
                    AIRY_MEMCPY(buf, t->detail + dstart, keep);
                    buf[keep] = '\0';
                    dstart += ln;
                    if (dstart < dl)
                        dstart++; /* 跳过换行 */
                }
                fputs(buf, stdout);
            }
            return;
        }

        size_t total = (count && ud) ? count((void *)ud) : 0;
        size_t lines = total + 1; /* 标题行 + 内容行 */

        /* 滚动/跟随窗口：
         *   - 任务看板：选择游标驱动（sel 始终可见，↑/↓ 移动）
         *   - 事件流：跟随模式（t->follow）固定尾部实时刷新，否则
         *     按 scroll_off 回放浏览 */
        size_t live = lines > rows ? rows : lines;
        size_t max_off = lines > rows ? lines - rows : 0;
        if (t->mode == CLI_TUI_MODE_BOARD) {
            if (total == 0)
                t->sel = 0;
            else if (t->sel >= total)
                t->sel = total - 1;
            /* 选中行保持在窗口内（默认窗口头部，越界时整体移动） */
            t->scroll_off = 0;
            size_t start = 0;
            if (max_off && t->sel + 1 >= live)
                start = t->sel + 1 - live + 1;
            if (start > max_off)
                start = max_off;
            for (size_t r = 0; r < rows; r++) {
                tui_write_literal("\033[");
                char num[16];
                snprintf(num, sizeof(num), "%zu", start_row + r);
                tui_write_literal(num);
                tui_write_literal(";1H");
                tui_clear_line();
                size_t rel = start + r;
                if (rel < lines) {
                    char buf[512];
                    if (rel == 0) {
                        snprintf(buf, sizeof(buf), "%s%s%s  %s%zu 条%s  %s%s%s",
                                 cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                                 tui_mode_name(t->mode), cli_c(CLR_DIM), total,
                                 cli_c(CLR_RESET), cli_c(CLR_DIM),
                                 "· ↑↓ 选择 Enter 详情 x 取消",
                                 cli_c(CLR_RESET));
                        if (t->note[0]) {
                            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                                     "  %s", t->note);
                        }
                    } else if (line && ud) {
                        if (!line((void *)ud, rel - 1, buf, sizeof(buf)))
                            buf[0] = '\0';
                        /* 选中行反显（标记是看板行索引 rel-1） */
                        if (rel - 1 == t->sel)
                            tui_render_select(buf, sizeof(buf));
                    } else {
                        buf[0] = '\0';
                    }
                    fputs(buf, stdout);
                }
            }
            return;
        }

        /* ---- 事件流：跟随尾部 / 回放浏览 ---- */
        if (t->follow)
            t->scroll_off = 0;
        if (t->scroll_off > max_off)
            t->scroll_off = max_off;
        size_t start = t->follow
                           ? (lines > rows ? lines - rows : 0)
                           : (t->scroll_off > 0 ? lines - t->scroll_off - live : 0);

        for (size_t r = 0; r < rows; r++) {
            tui_write_literal("\033[");
            char num[16];
            snprintf(num, sizeof(num), "%zu", start_row + r);
            tui_write_literal(num);
            tui_write_literal(";1H");
            tui_clear_line();
            size_t rel = start + r;
            if (rel < lines) {
                char buf[512];
                if (rel == 0) {
                    snprintf(buf, sizeof(buf), "%s%s%s  %s%zu 条%s  %s%s%s",
                             cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                             tui_mode_name(t->mode), cli_c(CLR_DIM), total,
                             cli_c(CLR_RESET), cli_c(CLR_DIM),
                             t->follow ? "· 跟随中(f 关)" : "· 回放浏览(f 跟随)",
                             cli_c(CLR_RESET));
                    if (t->note[0]) {
                        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                                 "  %s", t->note);
                    }
                } else if (line && ud) {
                    if (!line((void *)ud, rel - 1, buf, sizeof(buf)))
                        buf[0] = '\0';
                } else {
                    buf[0] = '\0';
                }
                fputs(buf, stdout);
            }
        }
        return;
    }

    size_t total = t->hist.count;
    int have_partial = t->cur && t->cur_len > 0;

    /* Viewport window over the content (history minus header), plus the
     * in-progress partial line (streamed text not yet committed). 结果
     * 完整进历史（2026-08-19 起不再折叠最新回复），长内容经 ↑/PgUp 浏览。 */
    size_t content = (total > t->hist.pinned ? total - t->hist.pinned : 0) +
                     (have_partial ? 1 : 0);
    size_t live = content > rows ? rows : content;
    size_t start = 0;
    if (t->scroll_off > 0) {
        size_t max_off = content > rows ? content - rows : 0;
        if (t->scroll_off > max_off)
            t->scroll_off = max_off;
        start = content - t->scroll_off - live;
    } else {
        start = content > rows ? content - rows : 0;
    }

    for (size_t r = 0; r < rows; r++) {
        tui_write_literal("\033[");
        char num[16];
        snprintf(num, sizeof(num), "%zu", start_row + r);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        size_t rel = start + r;
        if (rel >= content)
            continue;
        if (have_partial && rel == content - 1) {
            fputs(t->cur, stdout);
            continue;
        }
        size_t idx = t->hist.pinned + rel;
        if (idx < t->hist.count)
            fputs(t->hist.lines[idx], stdout);
    }
}

static void tui_render_input(cli_tui_t *t)
{
    size_t row = (size_t)t->rows;
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();

    /* 多候选 Tab 补全：在输入行上方显示候选列表（当前候选高亮）。 */
    if (t->tab_active && t->tab_count > 1) {
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        fputs("candidates:", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        size_t used = 0;
        for (size_t i = 0; i < t->tab_count; i++) {
            const char *nm = NULL;
            if (t->tab_kind == 1)
                nm = t->tab_cand_strs[i];
            else if (t->tab_cands[i] < cli_commands_count())
                nm = CLI_COMMANDS[t->tab_cands[i]].name;
            if (!nm)
                break;
            size_t w = cli_disp_width(nm) + 1;
            if (used + w > (size_t)t->cols)
                break;
            if (i == t->tab_sel) {
                fputs(cli_c(CLR_CYAN), stdout);
                fputs(nm, stdout);
                fputs(cli_c(CLR_RESET), stdout);
            } else {
                fputs(cli_c(CLR_DIM), stdout);
                fputs(nm, stdout);
                fputs(cli_c(CLR_RESET), stdout);
            }
            fputs(" ", stdout);
            used += w;
        }
        fflush(stdout);
    } else if (t->mode != CLI_TUI_MODE_CHAT) {
        /* 阶段 4：面板模式的 tab 栏（输入行上方一行，当前 tab 高亮） */
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        fputs("  tabs:", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        for (int m = 0; m < CLI_TUI_MODE_MAX; m++) {
            const char *name = tui_mode_name((cli_tui_mode_t)m);
            if ((cli_tui_mode_t)m == t->mode) {
                fputs(cli_c(CLR_BG_BLUE), stdout);
                fputs(cli_c(CLR_BOLD), stdout);
            } else {
                fputs(cli_c(CLR_DIM), stdout);
            }
            fputs(" ", stdout);
            fputs(name, stdout);
            fputs(" ", stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
        fputs(cli_c(CLR_DIM), stdout);
        fputs("  Ctrl+P/O 切换", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fflush(stdout);
    } else {
        /* 对话视图（2026-08-19）：输入行上方画一条 dim 分隔线，与
         * 行渲染三区布局一致——对话滚动区与输入区边界清晰不重叠。 */
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        for (int c = 0; c < t->cols; c++)
            fputs("─", stdout); /* UTF-8 整字符（fputc 只写首字节会乱码） */
        fputs(cli_c(CLR_RESET), stdout);
        fflush(stdout);
    }

    if (t->search_active) {
        /* Search line (readline convention): "(reverse-i-search)`q': <match>"
         * for Ctrl+R, "(i-search)`q': <match>" for Ctrl+S. The matched
         * entry is shown dim after the prompt. */
        const char *prompt = t->search_forward ? "(i-search)`" : "(reverse-i-search)`";
        fputs(cli_c(CLR_DIM), stdout);
        fputs(prompt, stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fwrite(t->search_query, 1, t->search_query_len, stdout);
        fputs(cli_c(CLR_DIM), stdout);
        fputs("': ", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(cli_c(CLR_CYAN), stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(t->cmd_hist.entries[t->search_match], stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(cli_c(CLR_RESET), stdout);
        else if (t->search_query_len > 0)
            fputs(cli_c(CLR_RED), stdout), fputs("(failed)", stdout), fputs(cli_c(CLR_RESET), stdout);
        size_t col = 1 + cli_disp_width(prompt) +
                     cli_disp_width(t->search_query) + cli_disp_width("': ") + 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", row);
        tui_write_literal(num);
        tui_write_literal(";");
        snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
        tui_write_literal(num);
        tui_write_literal("H");
        fflush(stdout);
        return;
    }

    fputs(cli_c(CLR_CYAN), stdout);
    fputs(TUI_INPUT_PREFIX, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    if (t->input_len > 0)
        fwrite(t->input, 1, t->input_len, stdout);
    /* Session status (model · msgs · elapsed), right-aligned dim; hidden
     * when the typed line would overlap it. Drawn before the caret is
     * repositioned, so cursor math stays untouched. */
    if (t->status[0] && t->mode == CLI_TUI_MODE_CHAT) {
        size_t st_w = cli_disp_width(t->status) + 2;
        size_t used_w = (size_t)strlen(TUI_INPUT_PREFIX) +
                        cli_disp_width_of(t->input, t->input_len);
        if (used_w + st_w < (size_t)t->cols) {
            size_t scol = (size_t)t->cols - st_w + 2;
            tui_write_literal("\033[");
            snprintf(num, sizeof(num), "%zu", row);
            tui_write_literal(num);
            tui_write_literal(";");
            snprintf(num, sizeof(num), "%zu", scol);
            tui_write_literal(num);
            tui_write_literal("H");
            fputs(cli_c(CLR_DIM), stdout);
            fputs(t->status, stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
    }
    /* Place the cursor at the edit position (byte offset -> display width).
     * CJK chars occupy two columns, so the caret never drifts. */
    size_t col = (size_t)strlen(TUI_INPUT_PREFIX) +
                 cli_disp_width_of(t->input, t->input_col < t->input_len ? t->input_col : t->input_len);
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
    tui_write_literal(num);
    tui_write_literal("H");
    fflush(stdout);
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

/* ---- input ---- */

/* 阶段 4：带超时的按键等待。timeout_ms < 0 无限等待（原阻塞语义）。
 * 返回 1 有数据（*out 有效）；0 = 超时（*eof=0）或 EOF（*eof=1）。
 * 看板模式用它做 200ms 轮询节拍实现"实时刷新"。 */
static int tui_wait_byte(cli_tui_t *t, char *out, int timeout_ms, int *eof)
{
#ifdef _WIN32
    (void)t;
    (void)timeout_ms;
    (void)eof;
    (void)out;
    return 0; /* TUI 为 POSIX-only，Windows 不读键 */
#else
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int r;
    for (;;) {
        r = poll(&pfd, 1, timeout_ms);
        if (r >= 0 || errno != EINTR)
            break;
        /* SIGWINCH interrupted the wait: surface a resize tick so the caller
         * refreshes the geometry and redraws. */
        if (g_tui_resize_pending)
            break;
    }
    if (r <= 0) {
        *eof = 0; /* 超时 / poll 错误 / resize tick */
        return 0;
    }
    ssize_t n = read(STDIN_FILENO, out, 1);
    if (n == 1) {
        *eof = 0;
        return 1;
    }
    *eof = 1; /* read 0 = EOF */
    return 0;
#endif
}

/* Read an escape-sequence remainder (following ESC); returns a key code. */
enum {
    TUI_KEY_UP = 0x1001,
    TUI_KEY_DOWN,
    TUI_KEY_PGUP,
    TUI_KEY_PGDN,
    TUI_KEY_HOME,
    TUI_KEY_END,
    TUI_KEY_LEFT,
    TUI_KEY_RIGHT,
    TUI_KEY_DEL,
    TUI_KEY_CTRL_LEFT,   /* ESC [ 1 ; 5 D / ESC [ 5 D：词左移 */
    TUI_KEY_CTRL_RIGHT,  /* ESC [ 1 ; 5 C / ESC [ 5 C：词右移 */
    TUI_KEY_ALT_LEFT,    /* ESC [ 1 ; 3 D / ESC [ 3 D：词左移 */
    TUI_KEY_ALT_RIGHT,   /* ESC [ 1 ; 3 C / ESC [ 3 C：词右移 */
    TUI_KEY_ALT_B,       /* ESC b：词左移 */
    TUI_KEY_ALT_F,       /* ESC f：词右移 */
    TUI_KEY_F8,          /* ESC [ 1 9 ~：全屏 ↔ 行渲染 切换（2.3.7） */
    TUI_KEY_F6,          /* ESC [ 1 7 ~：直达任务看板（2026-08-19） */
    TUI_KEY_F7,          /* ESC [ 1 8 ~：直达事件流 */
    TUI_KEY_PASTE_START, /* ESC [ 200 ~：bracketed paste 开始 */
    TUI_KEY_UNKNOWN,
};

/* ---- bracketed paste (ESC [ 200 ~ ... ESC [ 201 ~) ---- */

/* 读取 bracketed-paste 结束序列的剩余字节（ESC 已被调用方消费，
 * 这里只读 "[201~" 5 字节）。返回 1 = 完整结束序列；0 = 不匹配。 */
static int tui_paste_read_end(void)
{
    char want[] = {'[', '2', '0', '1', '~'};
    char got[sizeof(want)];
    for (size_t i = 0; i < sizeof(want); i++) {
        ssize_t n = read(STDIN_FILENO, &got[i], 1);
        if (n != 1 || got[i] != want[i])
            return 0;
    }
    return 1;
}

/* 读取一个按键（带第一字节超时）。返回键码；0 = EOF；-1 = 轮询超时
 * （*eof 保持 0；看板模式以此节拍刷新）。ESC 序列后续字节用 50ms
 * 短超时，避免孤立 ESC 键阻塞。 */
static int tui_read_key(cli_tui_t *t, int timeout_ms, int *eof)
{
    char c;
    if (!tui_wait_byte(t, &c, timeout_ms, eof))
        return *eof ? 0 : -1;
    if (c == 0x1b) {
        char b;
        if (!tui_wait_byte(t, &b, 50, eof))
            return 0x1b; /* lone ESC */
        if (b == '[') {
            char x;
            if (!tui_wait_byte(t, &x, 50, eof))
                return TUI_KEY_UNKNOWN;
            switch (x) {
            case 'A': return TUI_KEY_UP;
            case 'B': return TUI_KEY_DOWN;
            case 'C': return TUI_KEY_RIGHT;
            case 'D': return TUI_KEY_LEFT;
            case 'H': return TUI_KEY_HOME;
            case 'F': return TUI_KEY_END;
            case '3': /* ESC [ 3 ~ = Delete; ESC [ 3 D / C = Alt+Left/Right */
                if (!tui_wait_byte(t, &b, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == '~')
                    return TUI_KEY_DEL;
                if (b == 'D')
                    return TUI_KEY_ALT_LEFT;
                if (b == 'C')
                    return TUI_KEY_ALT_RIGHT;
                return TUI_KEY_UNKNOWN;
            case '5': /* ESC [ 5 D / C = Ctrl+Left/Right; ESC [ 5 ~ = PageUp */
                if (!tui_wait_byte(t, &b, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == 'D')
                    return TUI_KEY_CTRL_LEFT;
                if (b == 'C')
                    return TUI_KEY_CTRL_RIGHT;
                if (b == '~')
                    return TUI_KEY_PGUP;
                return TUI_KEY_UNKNOWN;
            case '6': /* page down: ESC [ 6 ~ */
                if (tui_wait_byte(t, &b, 50, eof) && b == '~')
                    return TUI_KEY_PGDN;
                return TUI_KEY_UNKNOWN;
            case '1': /* ESC[19~ = F8；ESC[1;5D 等 = Ctrl/Alt+Left/Right */
            {
                char semi, mod, dir;
                if (!tui_wait_byte(t, &semi, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (semi == '9') { /* F8: ESC [ 1 9 ~ */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F8;
                }
                if (semi == '7') { /* F6: ESC [ 1 7 ~ */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F6;
                }
                if (semi == '8') { /* F7: ESC [ 1 8 ~ */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F7;
                }
                if (semi != ';')
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &mod, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &dir, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (mod == '5' && dir == 'D')
                    return TUI_KEY_CTRL_LEFT;
                if (mod == '5' && dir == 'C')
                    return TUI_KEY_CTRL_RIGHT;
                if (mod == '3' && dir == 'D')
                    return TUI_KEY_ALT_LEFT;
                if (mod == '3' && dir == 'C')
                    return TUI_KEY_ALT_RIGHT;
                return TUI_KEY_UNKNOWN;
            }
            case '2': /* ESC [ 2 0 0 ~ = bracketed paste start */
                if (!tui_wait_byte(t, &b, 50, eof) || b != '0')
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &b, 50, eof) || b != '0')
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &b, 50, eof) || b != '~')
                    return TUI_KEY_UNKNOWN;
                return TUI_KEY_PASTE_START;
            case '<': /* SGR mouse: ESC [ < b ; r ; c M  (ignore) */
                return TUI_KEY_UNKNOWN;
            default:
                return TUI_KEY_UNKNOWN;
            }
        }
        /* Alt+letter: xterm sends ESC followed by the letter. */
        if (b == 'b')
            return TUI_KEY_ALT_B;
        if (b == 'f')
            return TUI_KEY_ALT_F;
        return TUI_KEY_UNKNOWN;
    }
    return (unsigned char)c;
}

static void tui_input_append(cli_tui_t *t, char c)
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

/* 判断 input 末尾的 UTF-8 序列是否完整：ASCII 单字节恒完整；多字节序列
 * 需前导字节 + 足量续字节（2/3/4 字节）才完整。readline 逐字节读入时，
 * 中文的每个字节都会走到这里——序列不完整时暂缓重绘，攒齐后才刷新，
 * 避免输入过程渲染出 � 中间乱码帧（2026-08-20 复现：中文逐字节回显）。 */
static int tui_input_utf8_complete(const char *s, size_t len)
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

static void tui_input_backspace(cli_tui_t *t)
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

static void tui_input_delete_fwd(cli_tui_t *t)
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

static void tui_input_back_word(cli_tui_t *t)
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

/* Move the caret one word to the left (Alt+b / Ctrl+Left). Word = run of
 * non-blank characters; leading blanks are skipped so the caret lands at the
 * start of the previous word. */
static void tui_input_word_left(cli_tui_t *t)
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
static void tui_input_word_right(cli_tui_t *t)
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
 * semantics). At the line end the last two characters are swapped and the
 * caret stays at the end; in the middle the char at the caret and the one
 * before it are swapped and the caret moves past them. UTF-8 safe: whole
 * characters are swapped, never torn. */
static void tui_input_transpose(cli_tui_t *t)
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
static void tui_input_kill_save(cli_tui_t *t, const char *text, size_t n)
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
static void tui_input_yank(cli_tui_t *t)
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

/* ---- Tab 补全 ----
 * 命令补全（SSoT）："/xxx" 前缀从 main.c 的 CLI_COMMANDS 权威命令表匹配
 * （无手维护镜像——新增 /command 即刻可补全，删除的不再泄漏进候选）。
 * 文件补全：普通文本 token 按目录遍历匹配 cwd 下文件/目录（Claude Code
 * 风格），目录加平台分隔符后缀，支持 "src/cl" 这类目录前缀。
 * 单候选直接替换（文件追加空格、目录不加以支持连续 Tab 深入）；多候选
 * 轮转并在输入行上方显示候选列表（当前候选高亮）。 */

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

/* 目录遍历补全：dir 为目录路径（空串表示当前目录），base 为匹配前缀。
 * 匹配条目写入 t->tab_cand_strs，目录加平台分隔符后缀。返回候选数。 */
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

static int tui_input_tab_complete(cli_tui_t *t)
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

    /* 文件补全：token 为普通文本（Claude Code 风格 Tab 补全路径）。
     * 拆分目录与 basename，支持 "src/cl" 这类前缀。 */
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

    /* 单候选：文件追加空格便于继续输入；目录不加（可继续 Tab 深入）。
     * 多候选：推进轮转，下次 Tab 显示下一个。 */
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

/* ---- 行渲染模式（非全屏）readline（2026-08-19）----
 *
 * 默认交互是行渲染流式（三区布局：hero / 对话 / 输入）。全屏 TUI 用
 * alt screen + 自身历史翻动；行渲染模式不切屏，但输入区同样需要
 * 字节级按键解析——否则方向键/PgUp 的转义序列会被 fgets 原样吞进
 * 输入行（用户反馈"方向键乱码"），↑ 也无法翻动会话历史。
 *
 * 实现：读取期间临时切 raw mode（结束恢复），逐键解析；输入行在
 * 屏幕末行原位重绘（与 cli_term_input_begin 的输入条同一位置）。
 * 空输入时 ↑/PgUp 请求进入全屏 TUI 翻历史（返回 2，等价 F8）。
 */

static void tui_line_redraw(cli_tui_t *t)
{
    char num[16];
    if (cli_term_input_active()) {
        /* 三区布局：提示符画在固定底部输入条（绝对定位，避免与对话
         * 滚动区重叠）。 */
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%d", t->rows > 0 ? t->rows : 1);
        tui_write_literal(num);
        tui_write_literal(";1H");
    } else {
        /* 无底部输入条（窄终端/降级布局）：在当前行重绘，不做绝对定位，
         * 避免覆盖 hero 或对话内容。 */
        tui_write_literal("\r\033[2K");
    }
    tui_clear_line();
    fputs(cli_c(CLR_CYAN), stdout);
    fputs(TUI_INPUT_PREFIX, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    if (t->input_len > 0)
        fwrite(t->input, 1, t->input_len, stdout);
    /* 光标落在编辑位置（UTF-8 显示宽度对齐，CJK 不漂移）。 */
    size_t col = (size_t)strlen(TUI_INPUT_PREFIX) +
                 cli_disp_width_of(t->input, t->input_len);
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

static int tui_readline_line_mode(cli_tui_t *t, char *buf, size_t cap,
                                  size_t *out_len)
{
#ifdef _WIN32
    (void)t;
    (void)buf;
    (void)cap;
    (void)out_len;
    return 0;
#else
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
    tui_line_redraw(t);

    int rc = 1;
    for (;;) {
        int eof = 0;
        int key = tui_read_key(t, -1, &eof);
        if (eof || key == 0) {
            rc = 0;
            break;
        }
        if (key == TUI_KEY_F8) {
            /* 行渲染 → 全屏 TUI（与 main.c 的 rl==2 分支一致）。 */
            if (out_len)
                *out_len = 0;
            rc = 2;
            break;
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
            /* UTF-8 完整序列到达才重绘：中文输入攒齐 2/3/4 字节后一次
             * 刷新，避免逐字节渲染的 � 中间乱码帧（输入过程保持干净）。 */
            if (tui_input_utf8_complete(t->input, t->input_len))
                tui_line_redraw(t);
            continue;
        }
    }

    if (saved_ok)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    return rc;
#endif
}

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
#ifndef _WIN32
        if (cli_term_is_tty())
            return tui_readline_line_mode(t, buf, cap, out_len);
#endif
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
         * 其余模式无限等待（原语义）。 */
        int timeout = (t->mode != CLI_TUI_MODE_CHAT) ? CLI_TUI_PANEL_POLL_MS : -1;
        int key = tui_read_key(t, timeout, &eof);
        if (eof)
            return 0;
        if (key == 0)
            return 0; /* EOF */
        if (key == -1) {
            /* 轮询超时：面板实时刷新（count 回调重建缓存 → 重绘）。
             * SIGWINCH 到达：刷新几何尺寸并全量重绘。 */
            if (t->mode != CLI_TUI_MODE_CHAT)
                cli_tui_redraw(t);
            else if (g_tui_resize_pending) {
                g_tui_resize_pending = 0;
                tui_get_size(t);
                cli_tui_redraw(t);
            }
            continue;
        }
        /* 2.3.7：全屏 F8 → 请求退出回行渲染流式模式（返回 3）。放在
         * 模式/搜索处理之前：任何视图下 F8 都能切换，语义单一。 */
        if (key == TUI_KEY_F8) {
            if (out_len)
                *out_len = 0;
            return 3;
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

        /* ---- 面板模式（任务看板/事件流）：可操作浏览 ---- */
        if (t->mode != CLI_TUI_MODE_CHAT) {
            const cli_tui_panel_action_fn act = t->panel[t->mode].action;
            const void *ud = t->panel[t->mode].ud;
            size_t rows_page = tui_middle_rows(t) > 1 ? tui_middle_rows(t) - 1 : 1;

            /* 详情视图：Esc/Enter 返回列表 */
            if (t->detail_active) {
                if (key == 0x1b || key == '\n' || key == '\r') {
                    t->detail_active = 0;
                    t->detail_len = 0;
                    cli_tui_redraw(t);
                }
                fflush(stdout);
                continue;
            }

            if (t->mode == CLI_TUI_MODE_BOARD) {
                /* 看板计数：按键时重新拉取（同时刷新 entries 缓存） */
                size_t total = (t->panel[t->mode].count && ud)
                                   ? t->panel[t->mode].count((void *)ud)
                                   : 0;

                if (key == TUI_KEY_UP || key == TUI_KEY_DOWN ||
                    key == TUI_KEY_PGUP || key == TUI_KEY_PGDN ||
                    key == TUI_KEY_HOME || key == TUI_KEY_END) {
                    if (key == TUI_KEY_UP && t->sel > 0)
                        t->sel--;
                    else if (key == TUI_KEY_DOWN && total > 0 && t->sel + 1 < total)
                        t->sel++;
                    else if (key == TUI_KEY_PGUP)
                        t->sel = (t->sel > rows_page) ? t->sel - rows_page : 0;
                    else if (key == TUI_KEY_PGDN && total > 0)
                        t->sel = (t->sel + rows_page < total - 1) ? t->sel + rows_page
                                                                  : total - 1;
                    else if (key == TUI_KEY_HOME)
                        t->sel = 0;
                    else if (key == TUI_KEY_END && total > 0)
                        t->sel = total - 1;
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == '\n' || key == '\r') {
                    /* Enter：查看选中任务详情（DETAIL action 回填 detail）；
                     * 空看板时 Enter 回对话。 */
                    t->detail_len = 0;
                    if (act && ud && total > 0 && t->sel < total) {
                        t->detail[0] = '\0';
                        if (act((void *)ud, CLI_TUI_ACT_DETAIL, t->sel, t->detail,
                                sizeof(t->detail)))
                            t->detail_len = strlen(t->detail);
                        if (t->detail_len)
                            t->detail_active = 1;
                    } else if (total == 0) {
                        t->mode = CLI_TUI_MODE_CHAT;
                    }
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
                if (key == 'x' || key == 'X') {
                    /* x：请求取消选中任务（结果提示进标题栏） */
                    if (act && ud && total > 0 && t->sel < total)
                        act((void *)ud, CLI_TUI_ACT_CANCEL, t->sel, t->note,
                            sizeof(t->note));
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
                if (key == 0x1b) { /* Esc：返回对话 */
                    t->mode = CLI_TUI_MODE_CHAT;
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
            } else {
                /* 事件流：f 跟随 / c 过滤 / 方向键回放 */
                if (key == 'f' || key == 'F') {
                    t->follow = !t->follow;
                    t->scroll_off = 0;
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
                if (key == 'c' || key == 'C') {
                    if (act && ud)
                        act((void *)ud, CLI_TUI_ACT_CYCLE_FILTER, 0, t->note,
                            sizeof(t->note));
                    t->scroll_off = 0;
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
                if (key == TUI_KEY_UP) {
                    t->follow = 0;
                    t->scroll_off++;
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == TUI_KEY_DOWN) {
                    if (t->scroll_off > 0)
                        t->scroll_off--;
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == TUI_KEY_PGUP) {
                    t->follow = 0;
                    t->scroll_off += rows_page;
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == TUI_KEY_PGDN) {
                    t->follow = 0;
                    t->scroll_off = (t->scroll_off > rows_page)
                                        ? t->scroll_off - rows_page
                                        : 0;
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == TUI_KEY_HOME) {
                    t->follow = 0;
                    t->scroll_off = SIZE_MAX; /* 渲染时 clamp 到面板末尾 */
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == TUI_KEY_END) {
                    t->scroll_off = 0;
                    t->follow = 1; /* 回到尾部 = 恢复跟随 */
                    cli_tui_redraw(t);
                    continue;
                }
                if (key == 0x1b || key == '\n' || key == '\r') {
                    t->mode = CLI_TUI_MODE_CHAT;
                    cli_tui_redraw(t);
                    fflush(stdout);
                    continue;
                }
            }

            /* 其他键：返回对话模式，并把该击键送入输入框（"打字即
             * 退出浏览"的 Claude Code 风格）。若按键被吞掉，快速连
             * 击的命令（如 quit）会缺首字符。 */
            t->mode = CLI_TUI_MODE_CHAT;
            if (key >= 0x20 && key <= 0xFF) {
                tui_input_append(t, (char)key);
                tui_render_input(t);
            } else {
                cli_tui_redraw(t);
            }
            fflush(stdout);
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
            /* Remember the submitted line for Up/Down browsing (readline
             * semantics: history navigation + recall). Persist across
             * sessions so Up/Ctrl+R work after a restart. */
            tui_cmd_hist_push(t, buf);
            tui_cmd_hist_save(t);
            /* Do NOT echo the raw line here: the caller renders the
             * submission (e.g. cli_render_user_message for chat input), so
             * committing it now would duplicate it in the history. */
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
        if (key == 0x15) { /* Ctrl+U: kill before the caret (to line start) */
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
        if (key == 0x0b) { /* Ctrl+K: kill after the caret (to line end) */
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
                /* With typed text: browse the submitted-command history
                 * (readline Up convention). Preserve the in-progress draft
                 * so Down returns to it. */
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
            /* bracketed paste: insert raw bytes literally until ESC[201~.
             * Newlines fold to spaces (single-line input model), and no byte
             * is interpreted as a key, so pasted code never triggers editing
             * shortcuts or corrupts the line. */
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
                    if (tui_paste_read_end()) {
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
             * sequences (CJK input arrives byte-by-byte at 0x80..0xFF).
             * Control bytes and the escape-derived key codes are handled
             * above; anything else is literal text. */
            if (key >= 0x20 && key <= 0xFF) {
                tui_input_append(t, (char)key);
                tui_render_input(t);
                fflush(stdout);
            }
            break;
        }
    }
}

/* ---- lifecycle ---- */

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
    tui_get_size(t);
    if (t->rows <= 6 || t->cols <= 10) {
        t->active = 0;
        return -1;
    }

#ifdef _WIN32
    t->active = 0; /* POSIX-only full-screen mode */
    return -1;
#else
    /* Enter alternate screen + bracketed paste + raw mode. */
    fputs("\033[?1049h\033[?2004h\033[2J\033[H", stdout);
    fflush(stdout);

    g_tui_resize_pending = 0;
    signal(SIGWINCH, tui_sigwinch_handler);

    if (tcgetattr(STDIN_FILENO, &t->saved_termios) == 0) {
        /* Full raw mode (cfmakeraw semantics): the CLI owns every byte of
         * input. IXON must go so Ctrl+S (forward search) is not swallowed as
         * terminal flow control; ISIG goes so Ctrl+C is delivered to the
         * readline loop (0x03) instead of killing the process with SIGINT. */
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
    fputs("\033[?2004l\033[?1049l", stdout);
    fflush(stdout);
    t->active = 0;
    t->termios_saved = 0;
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
    if (g_default_tui == t)
        g_default_tui = NULL;
    AIRY_FREE(t);
}

int cli_tui_active(const cli_tui_t *t)
{
    return t && t->active;
}
