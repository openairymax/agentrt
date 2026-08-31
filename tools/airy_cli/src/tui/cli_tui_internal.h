// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui_internal.h
 * @brief TUI 引擎内部共享头（域拆分：engine/keys/input/ime/history/render/
 *        readline/nav）。
 *
 * 原 cli_tui.c（3988 行）按功能域拆分后，跨文件共享的结构体定义与
 * 内部函数声明统一收敛于此，保持 cli_tui.h 公共 API 不变（对外仍为
 * 不透明 cli_tui_t）。2026-08-27 二轮拆分新增 readline 域
 * （tui_readline.c 主循环 / tui_readline_nav.c 导航键分派）。
 * 此头仅限 airy_cli/src 内部使用。
 */

#ifndef AIRY_CLI_TUI_INTERNAL_H
#define AIRY_CLI_TUI_INTERNAL_H

#include "cli_tui.h"

#include "airy_ime.h"
#include "cli_internal.h" /* CLI_COMMANDS (Tab 补全 SSoT) */
#include "cli_render.h"
#include "cli_term.h"
#include "airy_dirent.h" /* 跨平台 opendir/readdir/closedir（文件补全） */
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

/* SIGWINCH notification (engine 域拥有；keys/readline 读标志) */
#ifndef _WIN32
extern volatile sig_atomic_t g_tui_resize_pending;
void tui_sigwinch_handler(int sig);
#endif

/* 视图模式名（engine 域，render 面板标题使用） */
const char *tui_mode_name(cli_tui_mode_t m);

/* ---- 常量 ---- */
#define TUI_HIST_INIT_CAP 256
#define TUI_LINE_INIT_CAP 128
#define TUI_BOTTOM_INPUT_LINES 2 /* input line + one spare row */

#define TUI_TAB_CAND_MAX 64      /* Tab 补全候选上限 */
#define TUI_TAB_NAME_MAX 192     /* 文件/目录候选字符串长度上限 */
#define TUI_CMD_HIST_MAX 500
#define TUI_SEARCH_QUERY_MAX 256
#define TUI_INPUT_PREFIX "airy> " /* input prompt, width tracked in bytes */

/* P1 重绘节流窗口（毫秒） */
#define TUI_REDRAW_MIN_MS 16

#define CLI_CARET_BLINK_MS 265u /* 半周期 ≈ Word 默认光标闪烁频率（530ms 全周期） */

/* ---- 内部类型 ---- */
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

    /* 增量渲染状态（P1 重绘优化） */
    size_t vp_start_rendered;
    int vp_start_valid;
    size_t vp_content_rendered;

    /* P1 滚动区（DECSTBM）状态 */
    size_t scr_top;
    size_t scr_bottom;
    int scr_set;

    /* P1 重绘节流 */
    long long redraw_last_ms;
    int redraw_pending;

    /* ---- 阶段 4：视图模式（tab）+ 面板数据源 ---- */
    cli_tui_mode_t mode;
    struct {
        void *ud;
        cli_tui_panel_count_fn count;
        cli_tui_panel_line_fn line;
        cli_tui_panel_action_fn action;
    } panel[CLI_TUI_MODE_MAX];

    /* ---- 阶段 4：面板可操作状态 ---- */
    size_t sel;
    int detail_active;
    char detail[4096];
    size_t detail_len;
    int follow;
    char note[160];

    /* ---- input line state ---- */
    char *input;
    size_t input_len;
    size_t input_cap;
    size_t input_col;

    /* submitted-command history (Up/Down browse while typing) */
    tui_cmd_history_t cmd_hist;
    size_t cmd_hist_idx;
    char *cmd_hist_edit;
    size_t cmd_hist_edit_len;
    size_t cmd_hist_edit_cap;

    /* Ctrl+R reverse incremental search */
    int search_active;
    char search_query[TUI_SEARCH_QUERY_MAX];
    size_t search_query_len;
    ssize_t search_match;
    int search_wrapped;
    int search_forward;

    /* kill-ring */
    char kill_buf[4096];
    size_t kill_len;

    /* bracketed paste */
    int paste_active;

    /* Tab completion (SSoT: CLI_COMMANDS in main.c) */
    int tab_active;
    size_t tab_count;
    size_t tab_sel;
    int tab_kind;
    size_t tab_cands[TUI_TAB_CAND_MAX];
    char tab_cand_strs[TUI_TAB_CAND_MAX][TUI_TAB_NAME_MAX];

    char status[96];

    /* ---- 内置拼音输入法（airy_ime，2.2.3） ---- */
    airy_ime_t *ime;
    int ime_active;
    int ime_key;
    int ime_key_alt;
    char ime_buf[48];
    size_t ime_buf_len;
    airy_ime_cand_t ime_cands[27];
    int ime_cand_count;
    int ime_page;
    int ime_pages;
    int ime_sel;

    /* 2.2.1.5 输入光标：黑白反显交替闪烁 */
    uint64_t caret_tick;
    int caret_visible;

    /* 2.2.1.3：三区重建所需的 hero 模型名快照 */
    char hdr_t2[128];
    char hdr_t1f[128];
    char hdr_t1p[128];

#ifndef _WIN32
    struct termios saved_termios;
    int termios_saved;
#endif
};

/* ---- 按键码（keys 域） ---- */
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
    TUI_KEY_CTRL_LEFT,
    TUI_KEY_CTRL_RIGHT,
    TUI_KEY_ALT_LEFT,
    TUI_KEY_ALT_RIGHT,
    TUI_KEY_ALT_B,
    TUI_KEY_ALT_F,
    TUI_KEY_F8,
    TUI_KEY_F6,
    TUI_KEY_F7,
    TUI_KEY_F9,
    TUI_KEY_F10,
    TUI_KEY_F4,
    TUI_KEY_F2,
    TUI_KEY_F5,
    TUI_KEY_F1,
    TUI_KEY_F3,
    TUI_KEY_PASTE_START,
    TUI_KEY_UNKNOWN,
};

/* ---- 跨文件内部函数声明（按域） ---- */

/* engine 域（cli_tui.c） */
void tui_commit_line(cli_tui_t *t, char *line);
size_t tui_content_lines(cli_tui_t *t);
void tui_append_byte(cli_tui_t *t, char c);
void tui_render_input(cli_tui_t *t);      /* 实现在 render 域，供 engine/readline 调用 */

/* render 域（tui_render.c） */
void tui_get_size(cli_tui_t *t);
void tui_write_literal(const char *s);
size_t tui_middle_rows(cli_tui_t *t);
void tui_clear_line(void);
void tui_render_header(cli_tui_t *t);
void tui_render_viewport(cli_tui_t *t);
void tui_redraw_tail(cli_tui_t *t);
void tui_schedule_redraw(cli_tui_t *t);

/* keys 域（tui_keys.c） */
int tui_wait_byte(cli_tui_t *t, char *out, int timeout_ms, int *eof);
int tui_paste_read_end(cli_tui_t *t);
int tui_read_key(cli_tui_t *t, int timeout_ms, int *eof);

/* history 域（tui_history.c） */
void tui_history_reset(tui_history_t *h);
void tui_cmd_hist_push(cli_tui_t *t, const char *line);
void tui_cmd_hist_reset(cli_tui_t *t);
void tui_cmd_hist_load(cli_tui_t *t);
void tui_cmd_hist_save(cli_tui_t *t);
void tui_cmd_hist_save_draft(cli_tui_t *t);
void tui_cmd_hist_apply(cli_tui_t *t, size_t idx);
void tui_search_step(cli_tui_t *t);
void tui_search_step_forward(cli_tui_t *t);

/* input 域（tui_input.c） */
void tui_input_append(cli_tui_t *t, char c);
void tui_input_backspace(cli_tui_t *t);
void tui_input_delete_fwd(cli_tui_t *t);
void tui_input_back_word(cli_tui_t *t);
void tui_input_word_left(cli_tui_t *t);
void tui_input_word_right(cli_tui_t *t);
void tui_input_transpose(cli_tui_t *t);
void tui_input_kill_save(cli_tui_t *t, const char *text, size_t n);
void tui_input_yank(cli_tui_t *t);
int tui_tab_complete(cli_tui_t *t);
int tui_input_utf8_complete(const char *s, size_t len);
size_t tui_caret_print(cli_tui_t *t);
void tui_caret_tick(cli_tui_t *t);
void tui_line_redraw(cli_tui_t *t);
int tui_readline_line_mode(cli_tui_t *t, char *buf, size_t cap, size_t *out_len);

/* readline 域（tui_readline.c / tui_readline_nav.c）：全屏 readline 主循环。
 * tui_readline_arrow_keys 返回 0 = 正常处理继续；-1 = 请求终止 readline
 * （粘贴内 EOF），调用方应 return 0。 */
int tui_readline_arrow_keys(cli_tui_t *t, int key);

/* ime 域（tui_ime.c） */
void tui_ime_commit_raw(cli_tui_t *t);
void tui_ime_commit_cand(cli_tui_t *t, const char *text);
void tui_ime_refresh(cli_tui_t *t);
int tui_ime_sel_index(const cli_tui_t *t);
void tui_ime_page_flip(cli_tui_t *t, int dir);
int tui_ime_draw_cands(cli_tui_t *t, int input_row);
int tui_ime_key_hit(const cli_tui_t *t, int key);
airy_ime_t *tui_ime_load_dict(void);
int tui_ime_key_resolve(void);
int tui_ime_key_alt_resolve(void);

/* lifecycle 域（cli_tui.c）：rebuild_three_zone 依赖行渲染历史（g_history） */
void cli_tui_rebuild_three_zone(cli_tui_t *tui);

/* panel dispatch 域（tui_panel_dispatch.c）：面板模式按键分派 */
int tui_panel_dispatch(cli_tui_t *t, int key);

#endif /* AIRY_CLI_TUI_INTERNAL_H */
