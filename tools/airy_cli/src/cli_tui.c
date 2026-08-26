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
#include "airy_ime.h"     /* 内置拼音输入法词典（commons，2.2.3） */
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
/* Windows：全屏 alt-screen 模式仍为 POSIX-only，退化为行渲染；行渲染
 * readline 的逐键输入由 tui_wait_byte 的 ReadConsoleInputW 翻译层支持
 * （见 input 段），方向键/功能键/UTF-8 输入与 POSIX 行为一致。 */
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

/* P1 重绘节流窗口（毫秒）：两个相邻重绘的最小间隔（约 60fps 上限）。
 * 流式输出按此合并，视觉依然平滑。 */
#define TUI_REDRAW_MIN_MS 16

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

    /* 增量渲染状态（P1 重绘优化）：上次渲染的 viewport 窗口顶部 rel 索引。
     * cli_tui_emit 追加历史尾部时，若窗口未移动则只重绘最后可见行 +
     * 输入行，替代每次 chunk 全量重绘（实测流式输出每次全屏重绘产生
     * 1.5MB+ 字节流，弱终端卡顿闪烁）。 */
    size_t vp_start_rendered;
    int vp_start_valid;

    /* 上次渲染时的内容行数：滚动增量仅当旧窗口已填满（content >= rows）
     * 时安全——窗口未满时滚动会把未渲染的空洞滚上来，中间行缺失。 */
    size_t vp_content_rendered;

    /* P1 滚动区（DECSTBM）状态：窗口顶部前移时用终端滚动代替全量重绘
     * （滚动区 = viewport 区域，header/输入行固定在外）。 */
    size_t scr_top;
    size_t scr_bottom;
    int scr_set;

    /* P1 重绘节流：emit 侧按时间窗口合并相邻重绘（TUI_REDRAW_MIN_MS），
     * 避免逐字节/逐帧输出时每次触发一次重绘（实测一次会话累计产生
     * 1.5MB+ 字节流，弱终端卡顿闪烁）。 */
    long long redraw_last_ms; /* 上次实际执行重绘的时间戳 */
    int redraw_pending;       /* 节流窗口内被合并、待执行的重绘 */

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

    /* ---- 内置拼音输入法（airy_ime，2.2.3：无 IME 设备中文输入）----
     * F10 切换中/英（默认，可用 AIRY_IME_KEY=f9/f10 配置，F11 因终端
     * 全屏冲突不可用）。拼音模式字母进 ime_buf；数字/空格上屏候选，
     * 退格删拼音，,/. 或 PgUp/PgDn 翻页，←/→ 移动候选高亮，Enter 上屏
     * 高亮候选（无候选时提交拼音原文），Esc 取消拼音。词典加载失败时
     * ime==NULL 功能整体禁用，不影响既有英文输入。
     * 0.1.3 微信式交互（2026-08-23）：候选池扩至 27（3 页 × 9），
     * 分页渲染 + 高亮移动 + 页码指示。 */
    airy_ime_t *ime;         /* 词典句柄（加载失败=NULL） */
    int ime_active;          /* 拼音模式 */
    int ime_key;             /* 中/英切换主键（默认 TUI_KEY_F10，可配置） */
    int ime_key_alt;         /* 中/英切换备键（默认 TUI_KEY_F9：F10 常被
                              * GNOME/Windows 终端占用为菜单键，双键兜底） */
    char ime_buf[48];        /* 拼音缓冲（[a-z]，ü 以 v 表示） */
    size_t ime_buf_len;
    airy_ime_cand_t ime_cands[27]; /* 候选池（分页：每页 9 个） */
    int ime_cand_count;      /* 总候选数 */
    int ime_page;            /* 当前页（0 起） */
    int ime_pages;           /* 总页数（=ceil(count/9)） */
    int ime_sel;             /* 页内高亮候选下标（0-8） */

    /* 2.2.1.5 输入光标：黑白反显交替闪烁，半周期 ≈ Word 默认（500ms）。
     * caret_visible=1 时光标处字符反显（白底黑字/黑底白字按终端配色），
     * 0 时正常显示。由输入循环的轮询节拍翻转并局部重绘输入行。 */
    uint64_t caret_tick;     /* 上次翻转时间戳（cli_now_ms） */
    int caret_visible;

    /* 2.2.1.3：三区重建所需的 hero 模型名快照（main 启动时填充；
     * 终端 resize / F8 退出重建时用于重绘 hero，不依赖 main 局部量）。 */
    char hdr_t2[128];
    char hdr_t1f[128];
    char hdr_t1p[128];

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
    case CLI_TUI_MODE_HW:
        return "硬件信息";
    case CLI_TUI_MODE_MEM:
        return "记忆链";
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

/* P1 增量重绘（emit 场景）前向声明：定义在 emit 之后（尾部渲染段） */
static void tui_redraw_tail(cli_tui_t *t);
static void tui_schedule_redraw(cli_tui_t *t);

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

/* 内容行数：历史（去 header）+ 未提交的 partial 行。增量/全量渲染
 * 共用同一几何口径（与 tui_render_viewport 聊天分支一致）。 */
static size_t tui_content_lines(cli_tui_t *t)
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
    /* P1 重绘优化：不再每 chunk 全量重绘。
     *   - chat 跟随模式：增量重绘 viewport 尾部（窗口未移动时仅重绘
     *     最后可见行 + 输入行，字节量从 ~1.2KB/次降至 ~100B/次）；
     *   - 本次 emit 批量新增 >1 行（如 /daemons 整段输出）：全量逐行
     *     重绘——增量只画末行会导致中间新行不显示；
     *   - 面板模式（200ms 轮询兜底）/ 回放浏览（scroll_off>0，内容不在
     *     可视区）：只标记 dirty，由轮询或下次按键触发重绘。 */
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
    /* P1：flush 提交了部分行 → 立即增量重绘（跳过节流窗口，保证输出
     * 结束时末帧不丢；窗口移动时 tui_redraw_tail 内部回退全量）。 */
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

/* P1：把 viewport 区域设为终端滚动区（DECSTBM），几何变化时才重设。
 * 窗口前移时终端滚动比逐行全量重绘省一个数量级的字节。 */
static void tui_scroll_region_set(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    if (rows == 0)
        return;
    size_t top = t->hist.pinned + 1;
    size_t bottom = t->hist.pinned + rows; /* viewport 最后一行 */
    if (t->scr_set && t->scr_top == top && t->scr_bottom == bottom)
        return;
    tui_write_literal("\033[");
    char num[24];
    snprintf(num, sizeof(num), "%zu", top);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", bottom);
    tui_write_literal(num);
    tui_write_literal("r");
    t->scr_top = top;
    t->scr_bottom = bottom;
    t->scr_set = 1;
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

/* 架构字符串（编译期判定，跨平台）：与 install.sh detect_arch 同口径。 */
static const char *tui_arch_name(void)
{
#if defined(_WIN32) || defined(_WIN64)
# if defined(_M_ARM64)
    return "arm64";
# elif defined(_M_X64)
    return "x86_64";
# else
    return "windows";
# endif
#elif defined(__APPLE__)
# if defined(__aarch64__)
    return "arm64";
# else
    return "x86_64";
# endif
#else
# if defined(__riscv)
    return "riscv64";
# elif defined(__aarch64__)
    return "aarch64";
# elif defined(__arm__)
    return "armv7l";
# else
    return "x86_64";
# endif
#endif
}

/* ---- 硬件信息面板（F2，2026-08-25）----
 * 静态渲染本机实时环境：OS/版本/主机名、CPU 型号/核心数、内存总量/可用、
 * 架构、统一数据根路径。airy_get_sysinfo 跨平台采集（Linux/macOS/Windows）。 */

/* 面板行写入：定位到视口第 row 行（相对 start_row）→ 清行 → 打印格式化文本。 */
static void tui_hw_line(size_t row, size_t start_row, const char *fmt, ...)
{
    va_list ap;
    char buf[512];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", start_row + row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    fputs(buf, stdout);
}

static void tui_render_hw_panel(cli_tui_t *t, size_t rows, size_t start_row)
{
    airy_sysinfo_t si;
    int ok = (airy_get_sysinfo(&si) == AIRY_SUCCESS);

    size_t r = 0;
    tui_hw_line(r++, start_row, "%s%s%s  %s%s%s", cli_c(CLR_BOLD),
                cli_c(CLR_CYAN), tui_mode_name(t->mode), cli_c(CLR_DIM),
                "· 本机实时环境 · F2/Esc 返回", cli_c(CLR_RESET));

    if (!ok) {
        tui_hw_line(r++, start_row, "%s%s 硬件信息采集失败（airy_get_sysinfo）%s",
                    cli_c(CLR_RED), cli_c(CLR_RESET), cli_c(CLR_RESET));
        for (; r < rows; r++)
            tui_hw_line(r, start_row, " ");
        return;
    }

    char mem_total[32], mem_free[32];
    {
        uint64_t mt = si.memory_total, mf = si.memory_free;
        const char *unit = "B";
        if (mt >= (1024ULL * 1024 * 1024)) {
            mt /= (1024 * 1024 * 1024);
            mf /= (1024 * 1024 * 1024);
            unit = "GiB";
        } else if (mt >= 1024 * 1024) {
            mt /= (1024 * 1024);
            mf /= (1024 * 1024);
            unit = "MiB";
        }
        snprintf(mem_total, sizeof(mem_total), "%llu %s",
                 (unsigned long long)mt, unit);
        snprintf(mem_free, sizeof(mem_free), "%llu %s",
                 (unsigned long long)mf, unit);
    }

    tui_hw_line(r++, start_row, "%sOS%s       %s%s%s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), cli_c(CLR_BOLD),
                si.os_name[0] ? si.os_name : "unknown", cli_c(CLR_RESET));
    if (si.os_version[0])
        tui_hw_line(r++, start_row, "%s版本%s     %s", cli_c(CLR_DIM),
                    cli_c(CLR_RESET), si.os_version);
    tui_hw_line(r++, start_row, "%s主机名%s   %s%s%s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), cli_c(CLR_BOLD),
                si.hostname[0] ? si.hostname : "unknown", cli_c(CLR_RESET));
    tui_hw_line(r++, start_row, "%sCPU%s      %s（%u 核）", cli_c(CLR_DIM),
                cli_c(CLR_RESET),
                si.cpu_model[0] ? si.cpu_model : "unknown", si.cpu_count);
    tui_hw_line(r++, start_row, "%s内存%s     %s（可用 %s）", cli_c(CLR_DIM),
                cli_c(CLR_RESET), mem_total, mem_free);
    tui_hw_line(r++, start_row, "%s架构%s     %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), tui_arch_name());
    tui_hw_line(r++, start_row, "%s数据根%s   %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), airy_data_dir());
    tui_hw_line(r++, start_row, "%s运行时%s   %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), airy_runtime_dir());

    for (r = r + 1; r < rows; r++)
        tui_hw_line(r, start_row, " ");
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

        /* ---- 硬件信息面板（F2，2026-08-25）：静态渲染本机环境，不走
         * 面板回调（无动态列表）。airy_get_sysinfo 跨平台采集 CPU/内存/
         * OS/主机名；追加 daemon 在线概览与统一数据根路径。 */
        if (t->mode == CLI_TUI_MODE_HW) {
            tui_render_hw_panel(t, rows, start_row);
            return;
        }

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
                    const char *hint;
                    if (t->mode == CLI_TUI_MODE_MEM)
                        hint = "· 尾部实时刷新  ↑↓ 回放  Esc 返回";
                    else
                        hint = t->follow ? "· 跟随中(f 关)" : "· 回放浏览(f 跟随)";
                    snprintf(buf, sizeof(buf), "%s%s%s  %s%zu 条%s  %s%s%s",
                             cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                             tui_mode_name(t->mode), cli_c(CLR_DIM), total,
                             cli_c(CLR_RESET), cli_c(CLR_DIM),
                             hint,
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
    /* 全量渲染完成 → 记录窗口顶部，供 cli_tui_emit 增量重绘检测；
     * 同步 viewport 滚动区，供窗口前移时终端滚动增量。 */
    t->vp_start_rendered = start;
    t->vp_start_valid = 1;
    t->vp_content_rendered = content;
    tui_scroll_region_set(t);

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

/* 拼音候选条（定义在输入法辅助段，此处前置声明供 tui_render_input 用）。 */
static int tui_ime_draw_cands(cli_tui_t *t, int input_row);
/* 中/英切换键命中判定（定义在输入法辅助段，前置声明供按键循环用）。 */
static int tui_ime_key_hit(const cli_tui_t *t, int key);
/* 反显光标打印（定义在光标辅助段，前置声明供输入行渲染用）。 */
static size_t tui_caret_print(cli_tui_t *t);

static void tui_render_input(cli_tui_t *t)
{
    size_t row = (size_t)t->rows;
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();

    /* 多候选 Tab 补全：在输入行上方显示候选列表（当前候选高亮）。
     * 2.2.3 拼音候选条优先：IME 拼音态时占用该行，回落原绘制路径。 */
    if (!tui_ime_draw_cands(t, (int)row) && t->tab_active && t->tab_count > 1) {
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
    /* Session status (model · msgs · elapsed), right-aligned dim; hidden
     * when the typed line would overlap it. Drawn before the caret is
     * repositioned, so cursor math stays untouched. */
    if (t->status[0] && t->mode == CLI_TUI_MODE_CHAT) {
        size_t st_w = cli_disp_width(t->status) + 2;
        size_t used_w = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) +
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
    size_t col = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) + input_w;
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
    tui_write_literal(num);
    tui_write_literal("H");
    fflush(stdout);
}

/* P1 增量重绘（emit 场景）：chat 跟随模式下仅重绘 viewport 尾部变化行
 * （最后可见内容行 + 输入行），替代每次 chunk 全量重绘。窗口移动（内容
 * 溢出导致 start 前移）或首次进入时回退全量。 */
static void tui_redraw_tail(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    if (rows == 0) {
        cli_tui_redraw(t);
        return;
    }
    size_t content = tui_content_lines(t);
    int have_partial = t->cur && t->cur_len > 0;
    if (content == 0) {
        cli_tui_redraw(t);
        return;
    }
    size_t live = content > rows ? rows : content;
    size_t start = content > rows ? content - rows : 0;

    /* 窗口前移（尾部溢出）：滚动区增量——终端滚动 k 行 + 重绘新进
     * 视口的 k 行，避免全量逐行重绘。仅当旧窗口已填满（上次渲染时
     * 内容行数 >= 视口行数）时安全；首次进入或窗口未满（中间行从未
     * 渲染，滚动会带出空洞）时回退全量。 */
    if (start != t->vp_start_rendered) {
        size_t k = start > t->vp_start_rendered
                       ? start - t->vp_start_rendered
                       : 0;
        if (!t->vp_start_valid || k == 0 || k > rows || !t->scr_set ||
            t->vp_content_rendered < rows) {
            cli_tui_redraw(t);
            return;
        }
        /* 光标到滚动区底部，k 次换行触发终端上滚；滚动后底部 k 行空。 */
        char num[16];
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", t->scr_bottom);
        tui_write_literal(num);
        tui_write_literal(";1H");
        for (size_t i = 0; i < k; i++)
            tui_write_literal("\n");
        /* 重绘新进入视口的 k 行（滚动后的空行区） */
        for (size_t r = rows - k; r < rows; r++) {
            size_t rel = start + r;
            if (rel >= content)
                break;
            tui_write_literal("\033[");
            snprintf(num, sizeof(num), "%zu", t->hist.pinned + 1 + r);
            tui_write_literal(num);
            tui_write_literal(";1H");
            tui_clear_line();
            if (have_partial && rel == content - 1) {
                fputs(t->cur, stdout);
            } else {
                size_t idx = t->hist.pinned + rel;
                if (idx < t->hist.count)
                    fputs(t->hist.lines[idx], stdout);
            }
        }
        t->vp_start_rendered = start;
        t->vp_content_rendered = content;
        tui_render_input(t);
        t->dirty = 0;
        fflush(stdout);
        return;
    }
    /* 窗口未移动：只重绘最后可见内容行（含未提交的部分行）与输入行。
     * 字节量从每次全屏 ~1.2KB 降至 ~100B。超宽行触发终端自动换行时，
     * 画出内容后清掉其占用的 wrap 溢出行，避免 partial 变短/commit 后
     * 上一帧的溢出内容残留（显示宽度用 cli_disp_width_of，CJK 双宽正确）。 */
    size_t cols = t->cols > 0 ? (size_t)t->cols : 80;
    size_t row = t->hist.pinned + live; /* start_row + live - 1 */
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    size_t disp = 0;
    if (have_partial) {
        fputs(t->cur, stdout);
        disp = cli_disp_width_of(t->cur, t->cur_len);
    } else {
        size_t idx = t->hist.pinned + start + live - 1;
        if (idx < t->hist.count) {
            fputs(t->hist.lines[idx], stdout);
            disp = cli_disp_width_of(t->hist.lines[idx],
                                     strlen(t->hist.lines[idx]));
        }
    }
    /* 清 wrap 溢出区（占用物理行数 - 1）；不越过输入行（输入行由
     * tui_render_input 覆盖）。 */
    size_t phys = disp > 0 ? (disp + cols - 1) / cols : 1;
    for (size_t i = 1; i < phys && row + i < (size_t)t->rows; i++) {
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", row + i);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
    }
    tui_render_input(t);
    t->dirty = 0;
    fflush(stdout);
}

/* P1 重绘节流入口（emit 侧）：距上次重绘 >= TUI_REDRAW_MIN_MS 才实际
 * 重绘，否则仅挂起 redraw_pending（由下一次 emit / flush / readline 轮询
 * 消费）。逐字节流式输出因此合并为 ~60fps 的整帧刷新。 */
static void tui_schedule_redraw(cli_tui_t *t)
{
    long long now = cli_now_ms();
    if (now - t->redraw_last_ms >= TUI_REDRAW_MIN_MS) {
        t->redraw_last_ms = now;
        t->redraw_pending = 0;
        tui_redraw_tail(t);
    } else {
        t->redraw_pending = 1;
    }
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

/* ---- Windows 输入：控制台键事件 → 伪 VT 字节流 ----
 *
 * tui_read_key 解析 ESC 序列，但 Windows 控制台不产生 raw 字节流。
 * 这里把 ReadConsoleInputW 的 KEY_EVENT 翻译为字节：普通字符按 UTF-8
 * （含代理对），方向/功能/修饰键翻译为 xterm VT 序列，使 POSIX 的
 * 按键解析代码在 Windows 上零改动复用（Windows 10+ ConPTY 与现代
 * 终端行为一致，老控制台经 cli_term_init 降级为行渲染）。
 */
#ifdef _WIN32

static char g_win_key_buf[16];    /* 翻译后的字节序列缓存 */
static size_t g_win_key_len = 0;
static size_t g_win_key_off = 0;
static WCHAR g_win_high_surrogate = 0; /* UTF-16 高代理暂存（跨事件） */

static void tui_win_flush_buf(const char *s, size_t n)
{
    if (n > sizeof(g_win_key_buf))
        n = sizeof(g_win_key_buf);
    memcpy(g_win_key_buf, s, n);
    g_win_key_len = n;
    g_win_key_off = 0;
}

/* UTF-16 单字符 → UTF-8（含代理对组合）。无内容时不动缓存。 */
static void tui_win_enqueue_wchar(WCHAR wc)
{
    char buf[4];
    size_t n;
    if (wc < 0x80) {
        buf[0] = (char)wc;
        n = 1;
    } else if (wc < 0x800) {
        buf[0] = (char)(0xC0 | (wc >> 6));
        buf[1] = (char)(0x80 | (wc & 0x3F));
        n = 2;
    } else if (wc < 0xD800 || wc > 0xDFFF) {
        /* BMP 非代理（中文等 3 字节字符） */
        buf[0] = (char)(0xE0 | (wc >> 12));
        buf[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (wc & 0x3F));
        n = 3;
    } else if (wc >= 0xDC00) {
        /* 低代理：与暂存的高代理组合成 4 字节（无高代理则丢弃） */
        if (g_win_high_surrogate == 0)
            return;
        unsigned long cp = 0x10000 +
                (((unsigned long)(g_win_high_surrogate - 0xD800)) << 10) +
                (wc - 0xDC00);
        g_win_high_surrogate = 0;
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    } else {
        /* 高代理：暂存，等待随后的低代理事件 */
        g_win_high_surrogate = wc;
        return;
    }
    tui_win_flush_buf(buf, n);
}

static void tui_win_enqueue_key(WORD vk, WCHAR wc, DWORD ctl)
{
    const int ctrl = (ctl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    const int alt = (ctl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    const char *seq = NULL;

    switch (vk) {
    case VK_UP:     seq = "\x1b[A";      break;
    case VK_DOWN:   seq = "\x1b[B";      break;
    case VK_RIGHT:  seq = ctrl ? "\x1b[1;5C" : alt ? "\x1b[1;3C" : "\x1b[C"; break;
    case VK_LEFT:   seq = ctrl ? "\x1b[1;5D" : alt ? "\x1b[1;3D" : "\x1b[D"; break;
    case VK_HOME:   seq = "\x1b[H";      break;
    case VK_END:    seq = "\x1b[F";      break;
    case VK_PRIOR:  seq = "\x1b[5~";     break;
    case VK_NEXT:   seq = "\x1b[6~";     break;
    case VK_DELETE: seq = "\x1b[3~";     break;
    case VK_F6:     seq = "\x1b[17~";    break;
    case VK_F7:     seq = "\x1b[18~";    break;
    case VK_F8:     seq = "\x1b[19~";    break;
    case VK_TAB:    seq = "\t";          break;
    case VK_RETURN: seq = "\r";          break;
    case VK_BACK:   seq = "\x7f";        break; /* termios DEL，与 POSIX 一致 */
    case VK_ESCAPE: seq = "\x1b";        break;
    default:
        break;
    }
    if (seq) {
        tui_win_flush_buf(seq, strlen(seq));
        return;
    }
    if (alt && (vk == 'B' || vk == 'F')) {
        /* Alt+B / Alt+F：词左/右移（xterm 的 ESC b / ESC f） */
        const char *ab = (vk == 'B') ? "\x1bb" : "\x1bf";
        tui_win_flush_buf(ab, 2);
        return;
    }
    if (ctrl && vk >= 'A' && vk <= 'Z') {
        /* Ctrl+letter → 控制字节 0x01-0x1A（Ctrl+C=0x03 等）。Windows
         * 的 uChar 对 Ctrl 组合常为 0，按虚拟键码自行合成。 */
        char b = (char)((vk - 'A') + 1);
        tui_win_flush_buf(&b, 1);
        return;
    }
    if (wc) {
        tui_win_enqueue_wchar(wc);
        return;
    }
    /* 无字符可翻译（Shift 等纯修饰键）：缓存保持空，调用方重试。 */
}

#endif /* _WIN32 */

/* 阶段 4：带超时的按键等待。timeout_ms < 0 无限等待（原阻塞语义）。
 * 返回 1 有数据（*out 有效）；0 = 超时（*eof=0）或 EOF（*eof=1）。
 * 看板模式用它做 200ms 轮询节拍实现"实时刷新"。 */
static int tui_wait_byte(cli_tui_t *t, char *out, int timeout_ms, int *eof)
{
#ifdef _WIN32
    (void)t;
    /* 优先消耗上一个键事件翻译出的字节序列。 */
    if (g_win_key_off < g_win_key_len) {
        *out = g_win_key_buf[g_win_key_off++];
        *eof = 0;
        return 1;
    }
    g_win_key_len = g_win_key_off = 0;

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (hin == INVALID_HANDLE_VALUE || hin == NULL) {
        *eof = 1;
        return 0;
    }
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    if (WaitForSingleObject(hin, ms) != WAIT_OBJECT_0) {
        *eof = 0;
        return 0;
    }
    if (GetFileType(hin) != FILE_TYPE_CHAR) {
        /* 重定向的管道/文件：逐字节读（与 POSIX read 语义一致）。 */
        char c;
        DWORD n = 0;
        if (ReadFile(hin, &c, 1, &n, NULL) && n == 1) {
            *out = c;
            *eof = 0;
            return 1;
        }
        *eof = 1;
        return 0;
    }
    /* 控制台：ReadConsoleInputW 取 KEY_EVENT，跳过鼠标/窗口尺寸等
     * 非键事件后翻译为字节。 */
    for (;;) {
        INPUT_RECORD rec;
        DWORD n = 0;
        if (!ReadConsoleInputW(hin, &rec, 1, &n) || n == 0) {
            /* 缓冲已空：非键事件只占单个记录，通常 KEY_EVENT 紧随；
             * 再等待一轮，避免 resize/鼠标事件被误判为 EOF。 */
            if (WaitForSingleObject(hin, ms) != WAIT_OBJECT_0) {
                *eof = 0;
                return 0;
            }
            continue;
        }
        if (rec.EventType == KEY_EVENT &&
            rec.Event.KeyEvent.bKeyDown) {
            tui_win_enqueue_key(rec.Event.KeyEvent.wVirtualKeyCode,
                                rec.Event.KeyEvent.uChar.UnicodeChar,
                                rec.Event.KeyEvent.dwControlKeyState);
            if (g_win_key_len > 0) {
                *out = g_win_key_buf[g_win_key_off++];
                *eof = 0;
                return 1;
            }
        }
    }
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
    TUI_KEY_F9,          /* ESC [ 2 0 ~：内置输入法快捷键可选项（AIRY_IME_KEY=f9） */
    TUI_KEY_F10,         /* ESC [ 2 1 ~：内置输入法 中/英 切换（默认，可配置） */
    TUI_KEY_F4,          /* ESC O S / ESC [ 1 4 ~：F4（linux console/xterm 应用键区） */
    TUI_KEY_F2,          /* ESC O Q / ESC [ 1 2 ~：F2（硬件信息面板，2026-08-25） */
    TUI_KEY_F5,          /* ESC O T / ESC [ 1 5 ~：F5（记忆链面板，2026-08-25） */
    TUI_KEY_PASTE_START, /* ESC [ 200 ~：bracketed paste 开始 */
    TUI_KEY_UNKNOWN,
};

/* ---- bracketed paste (ESC [ 200 ~ ... ESC [ 201 ~) ---- */

/* 读取 bracketed-paste 结束序列的剩余字节（ESC 已被调用方消费，
 * 这里只读 "[201~" 5 字节）。返回 1 = 完整结束序列；0 = 不匹配。
 * 经 tui_wait_byte 读取，Windows 键翻译层与原样字节流均适用。 */
static int tui_paste_read_end(cli_tui_t *t)
{
    char want[] = {'[', '2', '0', '1', '~'};
    char got[sizeof(want)];
    for (size_t i = 0; i < sizeof(want); i++) {
        int eof = 0;
        if (!tui_wait_byte(t, &got[i], 50, &eof))
            return 0;
        if (got[i] != want[i])
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
            case '1': /* ESC[19~ = F8；ESC[14~ = F4；ESC[1;5D 等 = Ctrl/Alt+Left/Right */
            {
                char semi, mod, dir;
                if (!tui_wait_byte(t, &semi, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (semi == '9') { /* F8: ESC [ 1 9 ~ */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F8;
                }
                if (semi == '4') { /* F4 (linux console): ESC [ 1 4 ~ */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F4;
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
                if (semi == '2') { /* F2: ESC [ 1 2 ~（xterm 标准 F2 序列） */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F2;
                }
                if (semi == '5') { /* F5: ESC [ 1 5 ~（xterm 标准 F5 序列） */
                    if (!tui_wait_byte(t, &dir, 50, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F5;
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
            case '2': /* ESC[20~ = F9；ESC[21~ = F10；ESC[200~ = paste start */
                if (!tui_wait_byte(t, &b, 50, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == '0') { /* F9: ESC [ 2 0 ~；paste: ESC [ 2 0 0 ~ */
                    if (!tui_wait_byte(t, &b, 50, eof))
                        return TUI_KEY_UNKNOWN;
                    if (b == '~')
                        return TUI_KEY_F9;
                    if (b != '0')
                        return TUI_KEY_UNKNOWN;
                    if (!tui_wait_byte(t, &b, 50, eof) || b != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_PASTE_START;
                }
                if (b == '1') { /* F10: ESC [ 2 1 ~ */
                    if (!tui_wait_byte(t, &b, 50, eof) || b != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F10;
                }
                return TUI_KEY_UNKNOWN;
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
        /* Application cursor mode (smkx): ESC O A/B/C/D = 方向键；
         * ESC O S = F4（xterm 标准）。 */
        if (b == 'O') {
            char x;
            if (!tui_wait_byte(t, &x, 50, eof))
                return TUI_KEY_UNKNOWN;
            switch (x) {
            case 'A': return TUI_KEY_UP;
            case 'B': return TUI_KEY_DOWN;
            case 'C': return TUI_KEY_RIGHT;
            case 'D': return TUI_KEY_LEFT;
            case 'Q': return TUI_KEY_F2;  /* F2（应用键区 smkx：ESC O Q） */
            case 'S': return TUI_KEY_F4;
            case 'T': return TUI_KEY_F5;  /* F5（应用键区 smkx：ESC O T） */
            default:  return TUI_KEY_UNKNOWN;
            }
        }
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

/* ==================== 输入光标（2.2.1.5 黑白交替闪烁） ==================== */

#define CLI_CARET_BLINK_MS 265u /* 半周期 ≈ Word 默认光标闪烁频率（530ms 全周期） */

/* 推进闪烁状态机：到达半周期翻转反显状态。输入循环每轮调用（轮询节拍）。 */
static void tui_caret_tick(cli_tui_t *t)
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
static size_t tui_caret_print(cli_tui_t *t)
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

/* ==================== 内置拼音输入法（2.2.3，commons/utils/ime） ============
 * 无 IME 设备的中文输入路径：F10 中/英切换（默认，AIRY_IME_KEY=f9/f10
 * 可配置），拼音模式字母进 ime_buf 并实时查候选，数字 1-9 / 空格上屏，
 * 退格删拼音，Enter 提交拼音原文，其他键先提交拼音原文再按正常路径
 * 处理。词典加载失败（ime==NULL）时整体降级禁用，英文输入路径完全不
 * 受影响。 */

/* 拼音原文上屏：逐字节插入输入行光标处，清空拼音缓冲。 */
static void tui_ime_commit_raw(cli_tui_t *t)
{
    for (size_t i = 0; i < t->ime_buf_len; i++)
        tui_input_append(t, t->ime_buf[i]);
    t->ime_buf_len = 0;
    t->ime_buf[0] = '\0';
    t->ime_cand_count = 0;
    t->ime_page = 0;
    t->ime_sel = 0;
}

/* 上屏候选：UTF-8 逐字节插入光标处（tui_input_append 支持中插），清空
 * 拼音缓冲并保持拼音模式，连续词组输入不中断（翻页/高亮归零）。 */
static void tui_ime_commit_cand(cli_tui_t *t, const char *text)
{
    if (!text)
        return;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        tui_input_append(t, (char)*p);
    t->ime_buf_len = 0;
    t->ime_buf[0] = '\0';
    t->ime_cand_count = 0;
    t->ime_page = 0;
    t->ime_sel = 0;
}

/* 以当前拼音缓冲刷新候选列表（微信式分页：候选池 27，每页 9 个）。
 * 拼音变化后重置页码与高亮（新输入上下文从第一页首候选开始）。 */
static void tui_ime_refresh(cli_tui_t *t)
{
    t->ime_cand_count =
        airy_ime_query(t->ime, t->ime_buf, t->ime_cands,
                       (int)(sizeof(t->ime_cands) / sizeof(t->ime_cands[0])));
    t->ime_pages = (t->ime_cand_count + 8) / 9;
    if (t->ime_pages < 1)
        t->ime_pages = 1;
    if (t->ime_page >= t->ime_pages)
        t->ime_page = t->ime_pages - 1;
    if (t->ime_page < 0)
        t->ime_page = 0;
    if (t->ime_sel > 8)
        t->ime_sel = 8;
    if (t->ime_sel < 0)
        t->ime_sel = 0;
}

/* 当前高亮候选在候选池中的绝对下标（-1=无候选）。 */
static int tui_ime_sel_index(const cli_tui_t *t)
{
    int idx = t->ime_page * 9 + t->ime_sel;
    if (idx < 0 || idx >= t->ime_cand_count)
        return -1;
    return idx;
}

/* 翻页（微信式，,/. 与 PgUp/PgDn）：越界回绕。 */
static void tui_ime_page_flip(cli_tui_t *t, int dir)
{
    if (t->ime_pages <= 1)
        return;
    t->ime_page += dir;
    if (t->ime_page < 0)
        t->ime_page = t->ime_pages - 1;
    if (t->ime_page >= t->ime_pages)
        t->ime_page = 0;
}

/* 绘制拼音候选条（输入行上方一行，微信式分页）：拼音高亮 + 当前页
 * 数字键候选（页内高亮以蓝底标记）+ 页码指示（多页时显示 ‹1/2›）。
 * 返回 1=已绘制（占用该行）；0=无拼音态（调用方继续画分隔线/tab 候选等）。 */
static int tui_ime_draw_cands(cli_tui_t *t, int input_row)
{
    if (!t->ime || !t->ime_active || t->ime_buf_len == 0)
        return 0;
    char num[16];
    size_t brow = input_row > 1 ? (size_t)input_row - 1 : 1;
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", brow);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    fputs(cli_c(CLR_BOLD), stdout);
    fputs(cli_c(CLR_CYAN), stdout);
    fwrite(t->ime_buf, 1, t->ime_buf_len, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    fputs(" ", stdout);
    size_t used = cli_disp_width(t->ime_buf) + 1;
    /* 当前页切片：page*9 .. min(page*9+9, count) */
    int start = t->ime_page * 9;
    int end = start + 9;
    if (end > t->ime_cand_count)
        end = t->ime_cand_count;
    for (int i = start; i < end; i++) {
        const char *txt = t->ime_cands[i].text;
        char tag[4];
        snprintf(tag, sizeof(tag), "%d", (i - start) + 1);
        size_t w = cli_disp_width(txt) + strlen(tag) + 1;
        if (used + w > (size_t)t->cols)
            break;
        if (i == t->ime_page * 9 + t->ime_sel) {
            /* 页内高亮（微信式：蓝底反显当前选中） */
            fputs(cli_c(CLR_BG_BLUE), stdout);
            fputs(cli_c(CLR_BOLD), stdout);
        } else {
            fputs(cli_c(CLR_DIM), stdout);
        }
        fputs(tag, stdout);
        fputs(txt, stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fputs(" ", stdout);
        used += w;
    }
    /* 页码指示：多页时尾部显示 ‹cur/total›（微信式翻页反馈） */
    if (t->ime_pages > 1) {
        char pgbuf[32];
        snprintf(pgbuf, sizeof(pgbuf), " ‹%d/%d›", t->ime_page + 1, t->ime_pages);
        if (used + (size_t)strlen(pgbuf) + 2 <= (size_t)t->cols) {
            fputs(cli_c(CLR_DIM), stdout);
            fputs(pgbuf, stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
    }
    fflush(stdout);
    return 1;
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

static int tui_readline_line_mode(cli_tui_t *t, char *buf, size_t cap,
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
    /* 2.2.1.1 环境突变自适应：行模式也监听 SIGWINCH，resize 后重建
     * 三区（hero 固定、不重叠）。仅 raw mode 成功时安装。 */
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
        /* 2.2.3 内置拼音输入法：中/英切换（F10 主键 + F9 备键，
         * AIRY_IME_KEY 可配）。词典加载失败（ime==NULL）时无效果，
         * 英文路径不变。 */
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
                /* 数字选字：选中当前页内第 N 个候选（微信式分页） */
                size_t i = (size_t)(key - '1');
                int idx = t->ime_page * 9 + (int)i;
                if (i < 9 && idx >= 0 && idx < t->ime_cand_count)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                tui_line_redraw(t);
                continue;
            }
            if (key == ' ') {
                /* 空格：上屏高亮候选（微信式，默认高亮第一个） */
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
                /* 翻页（微信式：,/. 与 PgUp/PgDn）；单页时标点走正常路径 */
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
                /* 页内高亮移动（微信式：←/→ 选中候选） */
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
                /* Esc：取消拼音（微信语义：清空缓冲，放弃组合） */
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
                /* Enter：有候选时上屏高亮候选（微信语义），无候选时提交
                 * 拼音原文；随后退出拼音态，由下方 Enter 提交整个输入行 */
                int idx = tui_ime_sel_index(t);
                if (idx >= 0)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                else
                    tui_ime_commit_raw(t);
                t->ime_active = 0;
                tui_line_redraw(t);
            } else if (key >= 0x20 && key <= 0xFF) {
                /* 标点/数字等：先提交拼音原文并退出拼音模式，按键继续
                 * 走正常输入路径（如 "." 直接出英文句号）。 */
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
            /* UTF-8 完整序列到达才重绘：中文输入攒齐 2/3/4 字节后一次
             * 刷新，避免逐字节渲染的 � 中间乱码帧（输入过程保持干净）。 */
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

int cli_tui_readline(cli_tui_t *t, char *buf, size_t cap, size_t *out_len)
{
    if (!buf || cap < 2)
        return 0;
    if (out_len)
        *out_len = 0;

    if (!t || !t->active) {
        /* Non-TUI. 2.3.7：F8 转义序列 (ESC[19~) 出现在行输入中 →
         * 请求进入全屏页面（返回 2）。交互 TTY 走字节级 readline
         * （方向键/PgUp 翻历史、无乱码）；管道/日志走 fgets。
         * Windows：行渲染 readline 由 tui_wait_byte 的键翻译层支持，
         * 与 POSIX 同路径（无 _WIN32 降级）。 */
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
         * 对话模式按 100ms 轮询：P1 节流兜底——输出暂停且未 flush 时
         * 消费 redraw_pending，保证末帧不丢。 */
        int timeout = (t->mode != CLI_TUI_MODE_CHAT) ? CLI_TUI_PANEL_POLL_MS
                                                     : CLI_TUI_CHAT_POLL_MS;
        int key = tui_read_key(t, timeout, &eof);
        if (eof)
            return 0;
        if (key == 0)
            return 0; /* EOF */
        if (key == -1) {
            /* 轮询超时：面板实时刷新（count 回调重建缓存 → 重绘）。
             * SIGWINCH 到达：刷新几何尺寸并全量重绘。对话模式下消费
             * 节流挂起的增量重绘。 */
            if (t->mode != CLI_TUI_MODE_CHAT) {
                cli_tui_redraw(t);
            }
#ifndef _WIN32
            else if (g_tui_resize_pending) {
                g_tui_resize_pending = 0;
                tui_get_size(t);
                /* 2.2.2 环境突变自适应：终端窗口缩得过小（<7 行或 <11 列）
                 * 全屏 TUI 无法可用渲染——自动退出全屏回到行渲染流式模式，
                 * 而非留在不可用的全屏页面上（resize 触发时实时生效）。 */
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
        /* 2.3.7：全屏 F8 → 请求退出回行渲染流式模式（返回 3）。放在
         * 模式/搜索处理之前：任何视图下 F8 都能切换，语义单一。 */
        if (key == TUI_KEY_F8) {
            if (out_len)
                *out_len = 0;
            return 3;
        }
        /* 2.2.3 内置拼音输入法：中/英切换（F10 主键 + F9 备键，
         * AIRY_IME_KEY 可配；chat 视图；词典未加载时 ime==NULL 无
         * 效果）。面板视图按切换键会落入"打字即退出浏览"分支返回
         * 对话，无拼音态残留。 */
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
                /* 数字选字：当前页内第 N 个候选（微信式分页） */
                size_t i = (size_t)(key - '1');
                int idx = t->ime_page * 9 + (int)i;
                if (i < 9 && idx >= 0 && idx < t->ime_cand_count)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                tui_render_input(t);
                fflush(stdout);
                continue;
            }
            if (key == ' ') {
                /* 空格：上屏高亮候选（微信式，默认高亮第一个） */
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
                /* 翻页（微信式：,/. 与 PgUp/PgDn）；单页时标点走正常路径 */
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
                /* 页内高亮移动（微信式：←/→ 选中候选） */
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
                /* Esc：取消拼音（微信语义：清空缓冲，放弃组合） */
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
                /* Enter：有候选时上屏高亮候选（微信语义），无候选时提交
                 * 拼音原文；随后退出拼音态，由下方 Enter 提交整个输入行 */
                int idx = tui_ime_sel_index(t);
                if (idx >= 0)
                    tui_ime_commit_cand(t, t->ime_cands[idx].text);
                else
                    tui_ime_commit_raw(t);
                t->ime_active = 0;
                tui_render_input(t);
                fflush(stdout);
            } else if (key >= 0x20 && key <= 0xFF) {
                /* 标点/数字等：先提交拼音原文并退出拼音模式，按键继续
                 * 走正常输入路径（如 "." 直接出英文句号）。 */
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
        /* 硬件信息面板：静态视图，Esc 返回对话，其余按键不进入面板动作。 */
        if (t->mode == CLI_TUI_MODE_HW) {
            if (key == 0x1b) {
                cli_tui_mode_set(t, CLI_TUI_MODE_CHAT);
                cli_tui_redraw(t);
                fflush(stdout);
            }
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
                /* UTF-8 完整序列才重绘（与行模式 readline 一致），
                 * 避免中文逐字节渲染的 � 中间乱码帧 */
                if (tui_input_utf8_complete(t->input, t->input_len))
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
             * sequences (CJK input arrives byte-by-byte at 0x80..0xFF).
             * Control bytes and the escape-derived key codes are handled
             * above; anything else is literal text. */
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

/* 返回可执行文件所在目录（无尾分隔符），失败返回 -1。用于二进制包
 * 解压即用布局：词典在 <exe>/../share/agentrt/ime/airy_ime.dat。 */
static int tui_exe_dir(char *buf, size_t cap)
{
    if (!buf || cap < 2)
        return -1;
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
    if (n == 0 || n >= (DWORD)cap)
        return -1;
#else
    ssize_t n = readlink("/proc/self/exe", buf, cap - 1);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
#endif
    char *slash = strrchr(buf, TUI_PATH_SEP);
#ifdef _WIN32
    char *bslash = strrchr(buf, '/');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
#endif
    if (!slash)
        return -1;
    *slash = '\0';
    return (int)strlen(buf);
}

/* 加载内置拼音词典（2.2.3）。路径优先级：AIRY_IME_DICT 环境变量 →
 * $AIRY_HOME/share/agentrt/ime/airy_ime.dat（安装布局）→ 可执行文件
 * 同级上溯 share/agentrt/ime/airy_ime.dat（二进制包解压即用）→ 当前
 * 目录 share/agentrt/ime/airy_ime.dat（开发/便携布局）。airy_ime_load
 * 对不存在/损坏文件 fail-closed 返回 NULL；这里顺次尝试，全部失败
 * 返回 NULL（输入法整体降级禁用，英文输入不受影响）。 */
static airy_ime_t *tui_ime_load_dict(void)
{
    const char *dict = getenv("AIRY_IME_DICT");
    if (dict && dict[0]) {
        airy_ime_t *ime = airy_ime_load(dict);
        if (ime)
            return ime;
    }
    const char *home = airy_home_dir();
    if (home && home[0]) {
        char path[AIRY_PATH_MAX];
        snprintf(path, sizeof(path), "%s/share/agentrt/ime/airy_ime.dat", home);
        airy_ime_t *ime = airy_ime_load(path);
        if (ime)
            return ime;
    }
    /* 二进制包解压即用：bin/ 与 share/ 平级，跳过 $AIRY_HOME 缺失时
     * 用户直接从包解压运行 airy_cli（未安装）也能用内置输入法。 */
    {
        char exe_dir[AIRY_PATH_MAX];
        if (tui_exe_dir(exe_dir, sizeof(exe_dir)) > 0) {
            char path[AIRY_PATH_MAX];
            snprintf(path, sizeof(path),
                     "%s/../share/agentrt/ime/airy_ime.dat", exe_dir);
            airy_ime_t *ime = airy_ime_load(path);
            if (ime)
                return ime;
        }
    }
    return airy_ime_load("share/agentrt/ime/airy_ime.dat");
}

/* 解析中/英切换键（2.2.3 可配置）：环境变量 AIRY_IME_KEY=f9/f10/both，
 * 默认 F10 主键 + F9 备键。F11 因终端模拟器全屏冲突（GNOME Terminal /
 * Windows Terminal / iTerm2 均默认 F11 全屏）不可用，显式拒绝。F10 在
 * GNOME/Windows 终端默认绑定"激活菜单"，字节到不了程序——保留 F9 为
 * 备键（用户按 F9 同样切换），降低"按了没反应"的观感。 */
static int tui_ime_key_resolve(void)
{
    const char *key = getenv("AIRY_IME_KEY");
    if (key && key[0]) {
        if (strcmp(key, "f9") == 0)
            return TUI_KEY_F9;
        if (strcmp(key, "f10") == 0)
            return TUI_KEY_F10;
        if (strcmp(key, "both") == 0)
            return TUI_KEY_F10;
        /* 未知值回落默认，不阻断启动 */
    }
    return TUI_KEY_F10;
}

/* 备键解析：AIRY_IME_KEY=f9/f10 时备键为对侧；both/默认时备键 F9。
 * 主键与备键不相等（f9 配置下主键 F9 备键 F10，反之亦然）。 */
static int tui_ime_key_alt_resolve(void)
{
    const char *key = getenv("AIRY_IME_KEY");
    if (key && key[0]) {
        if (strcmp(key, "f9") == 0)
            return TUI_KEY_F10;
        if (strcmp(key, "f10") == 0)
            return TUI_KEY_F9;
        /* both / 未知值：备键 F9 */
    }
    return TUI_KEY_F9;
}

/* 中/英切换键命中判定：主键或备键任一即命中。 */
static int tui_ime_key_hit(const cli_tui_t *t, int key)
{
    if (!t->ime)
        return 0;
    return key == t->ime_key || key == t->ime_key_alt;
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
     * 启动；但必须明确提示，避免"按 F10/F9 没反应"的静默困惑。
     * 提示走 stderr（启动早期，未进 alt-screen）。 */
    t->ime = tui_ime_load_dict();
    if (!t->ime) {
        fprintf(stderr,
                "airy_cli: 警告 内置输入法词典未加载，F10/F9 中文输入不可用\n"
                "          （可用 AIRY_IME_DICT=/path/to/airy_ime.dat 指定词典）\n");
    } else if (cli_term_is_tty()) {
        /* 2026-08-25：词典就绪时给出明确提示，避免"按 F10/F9 没反应"的
         * 静默困惑。F10 在 VS Code/GNOME Terminal 等常被占用（调试单步/
         * 菜单键），明确告知 F9 备键。 */
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
 * 供 main.c 的 cli_task_poll_input 在 TUI 全屏下开放中断能力（此前
 * TUI 激活时任务等待段 stdin 无人读取，Ctrl+C 因 raw mode 关闭 ISIG
 * 也不产生 SIGINT，任务无法通过快捷键退出）。 */
int cli_tui_poll_key(cli_tui_t *t, int *eof)
{
    if (!t || !cli_tui_active(t)) {
        if (eof)
            *eof = 0;
        return 0;
    }
    return tui_read_key(t, 0, eof);
}
