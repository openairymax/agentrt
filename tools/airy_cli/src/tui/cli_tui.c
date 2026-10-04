// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.c
 * @brief TUI 引擎骨架（域拆分后，2026-08-27）。
 *
 * 2026-08-27 域拆分（3988 行 → 7 文件）：
 *   - cli_tui.c              引擎骨架：状态、面板/模式 API、生命周期
 *   - tui_keys.c             按键读取域（POSIX/Windows + ESC 序列解析）
 *   - tui_input.c            输入编辑域（光标/编辑/Tab 补全/行模式 readline）
 *   - tui_ime.c              内置拼音输入法域
 *   - tui_history.c          历史与搜索域
 *   - tui_render.c           渲染域（行模式终端原语）
 * 跨文件共享结构体与内部声明见 cli_tui_internal.h；公共 API 见
 * cli_tui.h（对外不透明 cli_tui_t 不变）。
 *
 * 0.1.17 R5-G2 冻结退役：备用屏全屏形态（pinned header + 会话视口 + 底部
 * 输入条的三区布局）不再可进入，全屏渲染唯一由 Rust 侧控制台渲染层
 * （`airy_cli --tui` 调起的 agentrt-tui）提供。0.1.19 t143 物理删除全屏
 * 渲染引擎、emit 流式重绘与引擎死 API，本文件仅保留行式渲染所需的骨架、
 * 状态与视图查询。
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

/* ---- 阶段 4：视图模式（tab）+ 面板数据源 ---- */

void cli_tui_set_panel(cli_tui_t *t, cli_tui_mode_t mode, void *ud,
                       cli_tui_panel_count_fn count, cli_tui_panel_line_fn line)
{
    if (!t || mode < 0 || mode >= CLI_TUI_MODE_MAX)
        return;
    t->panel[mode].ud = ud;
    t->panel[mode].count = count;
    t->panel[mode].line = line;
}

void cli_tui_set_panel_action(cli_tui_t *t, cli_tui_mode_t mode,
                              cli_tui_panel_action_fn fn)
{
    if (!t || mode < 0 || mode >= CLI_TUI_MODE_MAX)
        return;
    t->panel[mode].action = fn;
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

/* 2.2.1.2/2.2.1.3 → 0.1.8：终端尺寸变化后重建行渲染视图。
 * 0.1.7 已声明弃用「固定滚动区 + 底部输入条」三区布局（cli_banner.c），
 * 但本函数此前仍 cli_term_header_pin 重设 DECSTBM——终端被强加固定
 * 滚动区，滚轮失效、hero 钉死，与默认 REPL 行为不一致（社区反馈
 * 「CLI 页面操作混乱」根因）。现与启动路径对齐：只重绘头部与历史，
 * 保持普通滚动（unpin 状态）。0.1.17 R5-G2 后唯一调用方为行式
 * readline 的 SIGWINCH 处理。 */
void cli_tui_rebuild_three_zone(cli_tui_t *t)
{
    if (!t || !cli_term_is_tty())
        return;
    cli_term_header_unpin();
    cli_out("\033[2J\033[H");
    cli_print_system_header(t->hdr_t2[0] ? t->hdr_t2 : NULL,
                            t->hdr_t1f[0] ? t->hdr_t1f : NULL,
                            t->hdr_t1p[0] ? t->hdr_t1p : NULL);
    for (size_t i = 0; i < g_history_count; i++) {
        if (strcmp(g_history_roles[i], "user") == 0)
            cli_render_user_message(g_history_contents[i]);
        else
            cli_render_super_agent(g_history_contents[i]);
    }
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
     * 0.1.17 R5-G2：C 侧全屏页面已冻结退役，全屏渲染唯一由 `--tui`
     * 模式调起 agentrt-tui 提供。 */
    return 0;
}

void cli_tui_destroy(cli_tui_t *t)
{
    if (!t)
        return;
    tui_cmd_hist_reset(t);
    AIRY_FREE(t->input);
    airy_ime_destroy(t->ime); /* 可为 NULL */
    if (g_default_tui == t)
        g_default_tui = NULL;
    AIRY_FREE(t);
}
