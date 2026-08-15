// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_panel.c
 * @brief TUI 面板数据源（阶段 4）：任务看板 + 事件流。
 *
 * 通过 cli_tui.h 的面板回调（count/line）为 TUI 提供两类数据视图：
 *   - 任务看板（BOARD）：work_hall 当前全部执行实例（id/状态/进度），
 *     count() 每次调用重新拉取 → TUI 200ms 轮询即得实时刷新。
 *   - 事件流（EVENTS）：hall_store 全局事件按 gseq 因果序合并（跨任务/
 *     类别），Up/Down 滚动即回放；复用 cli_cmds.c 的事件解析 helpers。
 *
 * 面板不持有任何引擎状态：ud 生命周期由 main.c 管理（create/destroy）。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 任务看板面板
 * ================================================================ */

typedef struct {
    airy_work_hall_t *hall;
    airy_work_hall_entry_t **entries;
    size_t count;
} cli_board_panel_t;

static const char *cli_panel_state_icon(const char *state)
{
    if (!state)
        return CLI_ICON_BULLET;
    if (strcmp(state, "completed") == 0)
        return CLI_ICON_CHECK;
    if (strcmp(state, "running") == 0 || strcmp(state, "pending") == 0 ||
        strcmp(state, "scheduled") == 0)
        return CLI_ICON_DIAMOND;
    if (strcmp(state, "failed") == 0)
        return CLI_ICON_CROSS;
    if (strcmp(state, "canceled") == 0)
        return CLI_ICON_ERR;
    return CLI_ICON_BULLET;
}

static const char *cli_panel_state_color(const char *state)
{
    if (!state)
        return "";
    if (strcmp(state, "completed") == 0)
        return cli_c(CLR_GREEN);
    if (strcmp(state, "failed") == 0 || strcmp(state, "canceled") == 0)
        return cli_c(CLR_RED);
    if (strcmp(state, "running") == 0 || strcmp(state, "pending") == 0 ||
        strcmp(state, "scheduled") == 0)
        return cli_c(CLR_YELLOW);
    return "";
}

/* 8 格迷你进度条（单行面板，不复用 cli_render_progress_bar 的宽条） */
static void cli_panel_bar(char *out, size_t cap, double prog)
{
    if (prog < 0)
        prog = 0;
    if (prog > 1)
        prog = 1;
    int filled = (int)(prog * 8);
    size_t i = 0;
    if (i + 1 < cap)
        out[i++] = '[';
    for (int k = 0; k < 8 && i + 1 < cap; k++)
        out[i++] = (k < filled) ? '#' : '-';
    if (i + 1 < cap)
        out[i++] = ']';
    out[i] = '\0';
}

void cli_panel_board_create(airy_work_hall_t *hall, void **out_ud)
{
    *out_ud = NULL;
    cli_board_panel_t *p = (cli_board_panel_t *)AIRY_CALLOC(1, sizeof(*p));
    if (!p)
        return;
    p->hall = hall;
    *out_ud = p;
}

void cli_panel_board_destroy(void *ud)
{
    cli_board_panel_t *p = (cli_board_panel_t *)ud;
    if (!p)
        return;
    if (p->entries)
        airy_work_hall_list_free(p->entries, p->count);
    AIRY_FREE(p);
}

size_t cli_panel_board_count(void *ud)
{
    cli_board_panel_t *p = (cli_board_panel_t *)ud;
    if (!p || !p->hall)
        return 0;
    /* 每次调用重新拉取：TUI 200ms 轮询即实时刷新 */
    if (p->entries)
        airy_work_hall_list_free(p->entries, p->count);
    p->entries = NULL;
    p->count = 0;
    if (airy_work_hall_list(p->hall, &p->entries, &p->count) != AIRY_SUCCESS)
        return 0;
    return p->count;
}

int cli_panel_board_line(void *ud, size_t idx, char *out, size_t cap)
{
    cli_board_panel_t *p = (cli_board_panel_t *)ud;
    if (!p || idx >= p->count || !p->entries)
        return 0;
    airy_work_hall_entry_t *e = p->entries[idx];
    char bar[12];
    cli_panel_bar(bar, sizeof(bar), e->progress);
    snprintf(out, cap, "%s  %s%-28s%s  %s%-10s%s  %s %3d%%",
             cli_panel_state_icon(e->state),
             cli_c(CLR_CYAN), e->execution_id,
             cli_c(CLR_RESET),
             cli_panel_state_color(e->state), e->state,
             cli_c(CLR_RESET), bar, (int)(e->progress * 100));
    return 1;
}

/* ================================================================
 * 事件流面板（hall_store 全局 gseq 因果序回放）
 * ================================================================ */

#define CLI_PANEL_EV_MAX 512

typedef struct {
    airy_hall_store_t *hs;
    struct {
        uint64_t gseq;
        int cat;
        char task[48];
        char label[128];
        char content[192];
    } evs[CLI_PANEL_EV_MAX];
    size_t nev;
} cli_events_panel_t;

void cli_panel_events_create(airy_hall_store_t *hs, void **out_ud)
{
    *out_ud = NULL;
    cli_events_panel_t *p = (cli_events_panel_t *)AIRY_CALLOC(1, sizeof(*p));
    if (!p)
        return;
    p->hs = hs;
    *out_ud = p;
}

void cli_panel_events_destroy(void *ud)
{
    AIRY_FREE(ud);
}

size_t cli_panel_events_count(void *ud)
{
    cli_events_panel_t *p = (cli_events_panel_t *)ud;
    if (!p || !p->hs)
        return 0;
    p->nev = 0;

    char **tasks = NULL;
    size_t nt = 0;
    if (airy_hall_store_list_tasks(p->hs, "default", &tasks, &nt) != AIRY_SUCCESS || nt == 0)
        return 0;

    /* 跨任务 + 跨类别合并收集（最新 512 条，超出取新弃旧） */
    for (size_t ti = 0; ti < nt && p->nev < CLI_PANEL_EV_MAX; ti++) {
        for (int cat = 0; cat < AIRY_HALL_CAT_MAX && p->nev < CLI_PANEL_EV_MAX; cat++) {
            char **jsons = NULL;
            size_t cnt = 0;
            if (airy_hall_store_replay(p->hs, "default", tasks[ti],
                                       (airy_hall_category_t)cat, &jsons, &cnt) != AIRY_SUCCESS ||
                cnt == 0)
                continue;
            for (size_t i = 0; i < cnt && p->nev < CLI_PANEL_EV_MAX; i++) {
                char content[192];
                cli_chain_extract_content(jsons[i], content, sizeof(content));
                p->evs[p->nev].gseq = cli_chain_extract_gseq(jsons[i]);
                p->evs[p->nev].cat = cat;
                AIRY_STRNCPY_TERM(p->evs[p->nev].task, tasks[ti],
                                  sizeof(p->evs[p->nev].task));
                cli_chain_label(cat, content, p->evs[p->nev].label,
                                sizeof(p->evs[p->nev].label));
                AIRY_STRNCPY_TERM(p->evs[p->nev].content, content,
                                  sizeof(p->evs[p->nev].content));
                p->nev++;
            }
            airy_hall_store_free_strings(jsons, cnt);
        }
    }
    airy_hall_store_free_strings(tasks, nt);

    /* 按 gseq 升序（全局因果序，事件流回放顺序） */
    for (size_t i = 1; i < p->nev; i++) {
        size_t j = i;
        while (j > 0 && p->evs[j - 1].gseq > p->evs[j].gseq) {
            __typeof__(p->evs[0]) tmp = p->evs[j - 1];
            p->evs[j - 1] = p->evs[j];
            p->evs[j] = tmp;
            j--;
        }
    }
    return p->nev;
}

int cli_panel_events_line(void *ud, size_t idx, char *out, size_t cap)
{
    cli_events_panel_t *p = (cli_events_panel_t *)ud;
    if (!p || idx >= p->nev)
        return 0;
    const char *catn = airy_hall_category_name((airy_hall_category_t)p->evs[idx].cat);
    snprintf(out, cap, "%s[%s:%05llu]%s %s%-10s %s%s%s  %s%s%s%s",
             cli_c(CLR_CYAN), catn, (unsigned long long)p->evs[idx].gseq,
             cli_c(CLR_RESET),
             cli_c(CLR_DIM), p->evs[idx].task, cli_c(CLR_RESET),
             cli_c(CLR_BOLD), p->evs[idx].label, cli_c(CLR_RESET),
             cli_c(CLR_DIM), p->evs[idx].content, cli_c(CLR_RESET));
    return 1;
}
