// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_live_board.c
 * @brief Live plan board: structured task progress with in-place redraw.
 *
 * 执行计划打印一次后，运行时按节点状态原位重绘：
 *
 *   ◇ 执行计划 (3 nodes, 2 deps)
 *     □ 1. analyze requirements  [agent_analyze]      ← 待处理
 *     ◐ 2. generate code         [agent_codegen]      ← 执行中
 *     ✓ 3. verify output         [agent_verify]       ← 完成
 *   ◐ 执行中 2/3 · ▓▓▓▓▓▓▓░░░ 67%                     ← footer 汇总
 *
 * 看板块（header + N 节点 + footer）打印后，spinner 紧跟其下；轮询循环在
 * spinner 暂停间隙按「相对行距」擦除并重绘整块，节点图标随状态翻转
 * （□ → ◐ → ✓/✗/⊘）。任务运行期间看板块下方不再有新行，相对几何稳定，
 * 即使区域滚动也只整体位移，不影响相对导航。非 TTY / TUI 激活时退化为
 * 静态计划（cli_print_plan_list）+ 追加看板行（cli_board_line）。
 *
 * 自 2026-08-27 起从 cli_display.c 拆出：本文件只承载看板会话状态与重绘；
 * 结果/计划列表/进度回调在 cli_display.c，hero 横幅在 cli_banner.c。
 */

#include "cli_internal.h"
#include "cli_display_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_LIVE_BOARD_MAX 64

typedef struct {
    int active;                      /* TTY 原位重绘会话进行中 */
    size_t n;                        /* 节点数 */
    size_t lines;                    /* 看板块总行数 = n + 2（header + footer） */
    size_t deps;                     /* 边数（header 展示） */
    size_t extra_below;              /* 看板块与 spinner 之间额外打印的行数 */
    char ids[CLI_LIVE_BOARD_MAX][48];
    char goals[CLI_LIVE_BOARD_MAX][96];
    char handlers[CLI_LIVE_BOARD_MAX][48];
    char states[CLI_LIVE_BOARD_MAX][16];    /* 运行时状态快照（回调线程写） */
    char last_rendered[CLI_LIVE_BOARD_MAX][16];
    char footer_state[16];
    double footer_progress;
} cli_live_board_t;

static cli_live_board_t g_live_board;

/* ---- live plan board implementation ---- */

static void cli_live_board_icon(const char *state, const char **icon, const char **col)
{
    if (state && (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0)) {
        *icon = CLI_ICON_CHECK;
        *col = CLR_GREEN;
    } else if (state && (strcmp(state, "failed") == 0 || strcmp(state, "canceled") == 0)) {
        *icon = CLI_ICON_CROSS;
        *col = CLR_RED;
    } else if (state && (strcmp(state, "running") == 0 || strcmp(state, "active") == 0 ||
                         strcmp(state, "queued") == 0 || strcmp(state, "retrying") == 0)) {
        *icon = CLI_ICON_HALF;
        *col = CLR_YELLOW;
    } else if (state && strcmp(state, "skipped") == 0) {
        *icon = CLI_ICON_CLOCK;
        *col = CLR_DIM;
    } else {
        *icon = CLI_ICON_TODO;
        *col = CLR_DIM;
    }
}

/* 单个节点行：图标 + 序号 + 目标（clip 到终端宽，防 wrap 破坏块几何）+
 * handler。 */
static void cli_live_board_node_line(size_t r)
{
    const char *icon, *col;
    cli_live_board_icon(g_live_board.states[r], &icon, &col);
    cli_outf("%s%s%s%s %s%zu.%s ", cli_gutter_pad(4), cli_c(col), icon, cli_c(CLR_RESET),
             cli_c(CLR_RESET), r + 1, cli_c(CLR_RESET));

    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t used = 4 + 1 + 1 + ((r + 1 >= 10) ? 3 : 2); /* gutter + icon + space + "N. " */
    size_t budget = (cols > (int)used + 8) ? (size_t)cols - used : 60;
    size_t handler_w = 6 + (size_t)strlen(g_live_board.handlers[r]); /* "  [..]" */
    if (budget > handler_w)
        budget -= handler_w;
    else
        budget = 8;

    char gclip[128];
    cli_hero_clip(g_live_board.goals[r][0] ? g_live_board.goals[r] : "(untitled)",
                  budget, gclip, sizeof(gclip));
    cli_out(gclip);
    cli_outf("  %s[%s]%s\n", cli_c(CLR_DIM), g_live_board.handlers[r], cli_c(CLR_RESET));
}

static void cli_live_board_header_line(void)
{
    cli_outf("%s%s%s %s执行计划%s %s(%zu nodes, %zu deps)%s\n", cli_gutter_pad(2),
             cli_c(CLR_CYAN), CLI_ICON_DIAMOND, cli_c(CLR_RESET), cli_c(CLR_DIM),
             cli_c(CLR_DIM), g_live_board.n, g_live_board.deps, cli_c(CLR_RESET));
}

static void cli_live_board_footer_line(const char *state, double progress)
{
    if (progress < 0.0)
        progress = 0.0;
    if (progress > 1.0)
        progress = 1.0;
    const char *icon, *col;
    cli_live_board_icon(state, &icon, &col);
    size_t done = 0, fail = 0;
    for (size_t i = 0; i < g_live_board.n; i++) {
        if (strcmp(g_live_board.states[i], "completed") == 0)
            done++;
        else if (strcmp(g_live_board.states[i], "failed") == 0 ||
                 strcmp(g_live_board.states[i], "canceled") == 0)
            fail++;
    }
    char sbar[16];
    cli_compact_bar(sbar, sizeof(sbar), progress, 10);
    const char *cn = state ? cli_state_cn(state) : "执行中";
    int bright = (strcmp(state ? state : "", "completed") == 0);
    cli_outf("%s%s%s%s %s%s %zu/%zu%s · %s %s%3.0f%%%s\n", cli_gutter_pad(2),
             cli_c(col), icon, cli_c(CLR_RESET), cli_c(CLR_DIM), cn, done, g_live_board.n,
             cli_c(CLR_RESET), sbar, cli_c(bright ? CLR_GREEN : CLR_DIM), progress * 100.0,
             cli_c(CLR_RESET));
    (void)fail;
}

void cli_live_board_begin(const taskflow_workflow_t *wf)
{
    AIRY_MEMSET(&g_live_board, 0, sizeof(g_live_board));
    if (g_cli_print_mode || !wf || wf->node_count == 0)
        return;
    if (!cli_term_is_tty() || cli_tui_active(cli_tui_get_default())) {
        /* 退化：静态计划（原 cli_print_plan_list 语义），不开启原位重绘。 */
        cli_print_plan_list(wf);
        return;
    }

    g_live_board.n = (wf->node_count > CLI_LIVE_BOARD_MAX) ? CLI_LIVE_BOARD_MAX
                                                           : wf->node_count;
    g_live_board.lines = g_live_board.n + 2;
    g_live_board.deps = wf->edge_count;

    /* 拓扑序（与 cli_print_plan_list 共用 helper），节点行按计划顺序展示；
     * helper 内部把窗口外依赖视为已满足，截断场景无越界读。 */
    size_t *order = (size_t *)AIRY_MALLOC(g_live_board.n * sizeof(size_t));
    unsigned char *scratch = (unsigned char *)AIRY_CALLOC(g_live_board.n, 1);
    if (!order || !scratch ||
        !cli_plan_topology_build(wf, g_live_board.n, order, scratch)) {
        AIRY_FREE(order);
        AIRY_FREE(scratch);
        return;
    }

    for (size_t r = 0; r < g_live_board.n; r++) {
        const taskflow_node_t *nd = &wf->nodes[order[r]];
        AIRY_STRNCPY_TERM(g_live_board.ids[r], nd->id, sizeof(g_live_board.ids[r]));
        AIRY_STRNCPY_TERM(g_live_board.goals[r], nd->name[0] ? nd->name : "(untitled)",
                          sizeof(g_live_board.goals[r]));
        AIRY_STRNCPY_TERM(g_live_board.handlers[r],
                          nd->task_handler_name ? nd->task_handler_name : "?",
                          sizeof(g_live_board.handlers[r]));
        AIRY_STRNCPY_TERM(g_live_board.states[r], "pending",
                          sizeof(g_live_board.states[r]));
        AIRY_STRNCPY_TERM(g_live_board.last_rendered[r], "pending",
                          sizeof(g_live_board.last_rendered[r]));
    }
    AIRY_FREE(order);
    AIRY_FREE(scratch);

    g_live_board.active = 1;
    cli_live_board_header_line();
    for (size_t r = 0; r < g_live_board.n; r++)
        cli_live_board_node_line(r);
    cli_live_board_footer_line("pending", 0.0);
    AIRY_STRNCPY_TERM(g_live_board.footer_state, "pending",
                      sizeof(g_live_board.footer_state));
    fflush(stdout);
}

void cli_live_board_set_node(const char *node_id, const char *state)
{
    if (!g_live_board.active || !node_id || !state)
        return;
    for (size_t r = 0; r < g_live_board.n; r++) {
        if (strcmp(g_live_board.ids[r], node_id) == 0) {
            AIRY_STRNCPY_TERM(g_live_board.states[r], state,
                              sizeof(g_live_board.states[r]));
            return;
        }
    }
}

int cli_live_board_refresh(const char *agg_state, double agg_progress)
{
    if (!g_live_board.active || g_live_board.n == 0)
        return 0; /* 退化模式：调用方回退 cli_board_line */

    int changed = (strcmp(g_live_board.footer_state, agg_state ? agg_state : "") != 0) ||
                  (g_live_board.footer_progress - agg_progress) >= 0.01 ||
                  (agg_progress - g_live_board.footer_progress) >= 0.01;
    for (size_t r = 0; r < g_live_board.n && !changed; r++) {
        if (strcmp(g_live_board.states[r], g_live_board.last_rendered[r]) != 0)
            changed = 1;
    }
    if (!changed)
        return 1; /* 无变化：不重绘（也避免 200ms 刷屏） */

    AIRY_STRNCPY_TERM(g_live_board.footer_state, agg_state ? agg_state : "",
                      sizeof(g_live_board.footer_state));
    g_live_board.footer_progress = agg_progress;

    /* 原位重绘：光标在 spinner 行，看板块在其上方 lines 行（中间可能
     * 隔着 extra_below 行附加输出，如 "still running" 回退行）。
     * 擦除整块 → 回退到块首 → 重绘 → 回到 spinner 行。 */
    const size_t T = g_live_board.lines;
    cli_outf("\033[%zuA", T + g_live_board.extra_below);
    for (size_t i = 0; i < T; i++) {
        cli_out("\033[2K");
        if (i + 1 < T)
            cli_out("\033[1B");
    }
    cli_outf("\033[%zuA", T - 1);
    cli_live_board_header_line();
    for (size_t r = 0; r < g_live_board.n; r++)
        cli_live_board_node_line(r);
    cli_live_board_footer_line(agg_state, agg_progress);
    for (size_t r = 0; r < g_live_board.n; r++)
        AIRY_STRNCPY_TERM(g_live_board.last_rendered[r], g_live_board.states[r],
                          sizeof(g_live_board.last_rendered[r]));
    cli_outf("\033[%zuB", g_live_board.extra_below + 1);
    fflush(stdout);
    return 1;
}

/* 看板块与 spinner 之间将额外打印一行（如 "still running" 回退行），
 * 通知看板调整原位重绘的相对行距。 */
void cli_live_board_extra(void)
{
    g_live_board.extra_below++;
}

int cli_live_board_active(void)
{
    return g_live_board.active;
}

void cli_live_board_done(void)
{
    g_live_board.active = 0;
}
