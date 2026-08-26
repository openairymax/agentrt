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
#include "cli_gw.h"

#include <cjson/cJSON.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    if (strcmp(state, "running") == 0 || strcmp(state, "pending") == 0)
        return CLI_ICON_DIAMOND;
    if (strcmp(state, "scheduled") == 0)
        return CLI_ICON_CLOCK;
    if (strcmp(state, "failed") == 0)
        return CLI_ICON_CROSS;
    if (strcmp(state, "canceled") == 0)
        return CLI_ICON_CANCEL;
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

/* 任务看板可操作动作（2026-08-19）：DETAIL 查看选中任务详情 /
 * CANCEL 请求取消选中任务。动作期间重新拉取列表解析 sel（事件处理时
 * 引擎刚重绘过，entries 缓存有效，但拉取最新状态更稳）。 */
/* P3-3：钳制追加（snprintf 返回"应写长度"，盲累加会越过容量下溢越界写） */
static size_t panel_append(char *out, size_t cap, size_t o, const char *fmt, ...)
{
    if (!out || cap == 0 || o >= cap - 1)
        return o;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(out + o, cap - o, fmt, ap);
    va_end(ap);
    if (w < 0)
        return o;
    size_t written = (size_t)w;
    if (written >= cap - o)
        written = cap - o - 1;
    return o + written;
}

int cli_panel_board_action(void *ud, int action, size_t sel, char *out, size_t cap)
{
    cli_board_panel_t *p = (cli_board_panel_t *)ud;
    if (!p || !p->hall || !out || cap == 0)
        return 0;
    out[0] = '\0';

    airy_work_hall_entry_t **entries = NULL;
    size_t n = 0;
    if (airy_work_hall_list(p->hall, &entries, &n) != AIRY_SUCCESS)
        return 0;
    int ok = 0;

    if (sel < n) {
        airy_work_hall_entry_t *e = entries[sel];
        if (action == CLI_TUI_ACT_DETAIL) {
            size_t o = 0;
            o = panel_append(out, cap, o, "◆ 执行 ID  %s\n", e->execution_id);
            o = panel_append(out, cap, o, "  工作流    %s\n",
                             e->workflow_name[0] ? e->workflow_name : e->workflow_id);
            o = panel_append(out, cap, o, "  状态      %s %.0f%%\n", e->state,
                             e->progress * 100.0);
            o = panel_append(out, cap, o, "  复核      %s\n",
                             e->review_verdict[0] ? e->review_verdict : "-");
            o = panel_append(out, cap, o, "  输入      %.*s\n",
                             (int)(e->input_json ? strlen(e->input_json) : 0),
                             e->input_json ? e->input_json : "");
            ok = 1;
        } else if (action == CLI_TUI_ACT_CANCEL) {
            airy_err_t err = airy_work_hall_cancel(p->hall, e->execution_id);
            if (err == AIRY_SUCCESS)
                snprintf(out, cap, "已请求取消 %s", e->execution_id);
            else
                snprintf(out, cap, "取消失败 %s（%s）", e->execution_id,
                         cli_err_desc((int)err));
            ok = 1;
        }
    }
    airy_work_hall_list_free(entries, n);
    return ok;
}

/* ================================================================
 * 事件流面板（hall_store 全局 gseq 因果序回放）
 * ================================================================ */

#define CLI_PANEL_EV_MAX 512

typedef struct {
    airy_hall_store_t *hs;
    int filter_cat; /* 类别过滤（-1 = 全部；CYCLE_FILTER 循环切换） */
    struct {
        uint64_t gseq; /* 进程内单调序（仅展示用） */
        char ts[24];   /* ts_utc（跨进程稳定序，固定宽度可字典序比较） */
        uint32_t seq;  /* 事件文件序号 */
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
    p->filter_cat = -1; /* 默认全部类别 */
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

    /* 2.8c：实时可见——事件流由多进程写入（gateway/sched/tool/runtime），
     * 本进程索引是 create() 时的会话快照；刷新面板前 rescan 一次，让
     * 跨进程事件可见、gseq 续接到磁盘最大号（失败仅降级为快照）。 */
    (void)airy_hall_store_rescan(p->hs);

    char **tasks = NULL;
    size_t nt = 0;
    if (airy_hall_store_list_tasks(p->hs, "default", &tasks, &nt) != AIRY_SUCCESS || nt == 0)
        return 0;

    /* 跨任务 + 跨类别合并收集（最新 512 条，超出取新弃旧；启用类别
     * 过滤时只收集 filter_cat 匹配的事件） */
    for (size_t ti = 0; ti < nt && p->nev < CLI_PANEL_EV_MAX; ti++) {
        for (int cat = 0; cat < AIRY_HALL_CAT_MAX && p->nev < CLI_PANEL_EV_MAX; cat++) {
            if (p->filter_cat >= 0 && cat != p->filter_cat)
                continue;
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
                p->evs[p->nev].seq = cli_chain_extract_seq(jsons[i]);
                (void)cli_chain_str_field(jsons[i], "ts_utc", p->evs[p->nev].ts,
                                          sizeof(p->evs[p->nev].ts));
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

    /* 跨进程稳定全局序 (ts_utc, seq)：gseq 是进程内单调，跨进程会撞号，
     * 不能作为全局排序键（2.8c）。ts_utc 定宽字典序即时间序。 */
    for (size_t i = 1; i < p->nev; i++) {
        size_t j = i;
        while (j > 0 && (strcmp(p->evs[j - 1].ts, p->evs[j].ts) > 0 ||
                         (strcmp(p->evs[j - 1].ts, p->evs[j].ts) == 0 &&
                          p->evs[j - 1].seq > p->evs[j].seq))) {
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

/* 事件流可操作动作（2026-08-19）：CYCLE_FILTER 循环切换类别过滤
 * （全部 → cat0 → cat1 → ... → 全部），out 返回当前过滤名。 */
int cli_panel_events_action(void *ud, int action, size_t sel, char *out, size_t cap)
{
    cli_events_panel_t *p = (cli_events_panel_t *)ud;
    (void)sel;
    if (!p || !out || cap == 0 || action != CLI_TUI_ACT_CYCLE_FILTER)
        return 0;
    p->filter_cat++;
    if (p->filter_cat >= AIRY_HALL_CAT_MAX)
        p->filter_cat = -1;
    if (p->filter_cat < 0)
        snprintf(out, cap, "过滤：全部类别");
    else
        snprintf(out, cap, "过滤：%s",
                 airy_hall_category_name((airy_hall_category_t)p->filter_cat));
    return 1;
}

/* ================================================================
 * 记忆链面板（2026-08-25，F5）
 *
 * 数据源：gateway → syscall → mem_d 的 mem.recent（最近记录倒序，
 * 含完整内容）。count() 秒级节流拉取并缓存渲染行（TUI 200ms 轮询
 * 时不会打爆 gateway）；line() 从缓存输出。不直连 daemon socket。
 * ================================================================ */

#define CLI_PANEL_MEM_LIMIT 12
#define CLI_PANEL_MEM_GW_TIMEOUT_MS 5000

typedef struct {
    char *lines[CLI_PANEL_MEM_LIMIT]; /* 渲染后的行（1 行 / 条记忆） */
    size_t count;
    time_t last_fetch;                /* 上次拉取时刻（秒级节流） */
} cli_mem_panel_t;

/* Unix 时间戳 → "MM-DD HH:MM"（本地时区；线程安全 localtime_r） */
static void cli_panel_mem_time(uint64_t ts, char *out, size_t cap)
{
    time_t t = (time_t)ts;
    struct tm ltm;
#if defined(_WIN32)
    if (localtime_s(&ltm, &t) == 0) {
        strftime(out, cap, "%m-%d %H:%M", &ltm);
    } else {
        snprintf(out, cap, "%llu", (unsigned long long)ts);
    }
#else
    if (localtime_r(&t, &ltm)) {
        strftime(out, cap, "%m-%d %H:%M", &ltm);
    } else {
        snprintf(out, cap, "%llu", (unsigned long long)ts);
    }
#endif
}

/* 记录内容摘要（首行，去 user: 前缀，UTF-8 安全截断） */
static void cli_panel_mem_summary(const char *data, char *out, size_t cap)
{
    const char *p = data ? data : "";
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    if (strncmp(p, "user: ", 6) == 0) {
        p += 6;
        len = (len > 6) ? len - 6 : 0;
    }
    if (len > 56)
        len = 56;
    while (len > 0 && ((unsigned char)p[len] & 0xC0) == 0x80)
        len--;
    if (len >= cap)
        len = cap - 1;
    __builtin_memcpy(out, p, len);
    out[len] = '\0';
}

static void cli_panel_mem_fetch(cli_mem_panel_t *p)
{
    for (size_t i = 0; i < p->count; i++)
        AIRY_FREE(p->lines[i]);
    p->count = 0;

    char *res = NULL;
    if (cli_gw_call("mem.recent", "{\"limit\":12}", CLI_PANEL_MEM_GW_TIMEOUT_MS, &res) != 0 ||
        !res) {
        AIRY_FREE(res);
        return;
    }
    cJSON *root = cJSON_Parse(res);
    AIRY_FREE(res);
    if (!root)
        return;
    cJSON *records = cJSON_GetObjectItem(root, "records");
    size_t n = cJSON_IsArray(records) ? (size_t)cJSON_GetArraySize(records) : 0;
    if (n > CLI_PANEL_MEM_LIMIT)
        n = CLI_PANEL_MEM_LIMIT;
    for (size_t i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(records, (int)i);
        cJSON *rid = cJSON_GetObjectItem(item, "record_id");
        cJSON *ts = cJSON_GetObjectItem(item, "created_at");
        cJSON *data = cJSON_GetObjectItem(item, "data");
        cJSON *lenj = cJSON_GetObjectItem(item, "len");
        const char *id = cJSON_IsString(rid) ? rid->valuestring : "";
        uint64_t created = cJSON_IsNumber(ts) ? (uint64_t)ts->valuedouble : 0;
        const char *d = cJSON_IsString(data) ? data->valuestring : "";
        long dlen = cJSON_IsNumber(lenj) ? (long)lenj->valuedouble : (long)strlen(d);

        char timestr[32], sum[64];
        cli_panel_mem_time(created, timestr, sizeof(timestr));
        cli_panel_mem_summary(d, sum, sizeof(sum));

        char *line = (char *)AIRY_MALLOC(192);
        if (!line)
            break;
        snprintf(line, 192, "%s[%s]%s %s%s%s%s  %s%ldB %.*s%s",
                 cli_c(CLR_DIM), timestr, cli_c(CLR_RESET),
                 cli_c(CLR_CYAN), sum, dlen > 56 ? "…" : "", cli_c(CLR_RESET),
                 cli_c(CLR_DIM), dlen, 8, id, cli_c(CLR_RESET));
        p->lines[p->count++] = line;
    }
    cJSON_Delete(root);
}

void cli_panel_mem_create(void **out_ud)
{
    *out_ud = NULL;
    cli_mem_panel_t *p = (cli_mem_panel_t *)AIRY_CALLOC(1, sizeof(*p));
    if (!p)
        return;
    *out_ud = p;
}

void cli_panel_mem_destroy(void *ud)
{
    cli_mem_panel_t *p = (cli_mem_panel_t *)ud;
    if (!p)
        return;
    for (size_t i = 0; i < p->count; i++)
        AIRY_FREE(p->lines[i]);
    AIRY_FREE(p);
}

size_t cli_panel_mem_count(void *ud)
{
    cli_mem_panel_t *p = (cli_mem_panel_t *)ud;
    if (!p)
        return 0;
    time_t now = time(NULL);
    if (now != p->last_fetch) { /* 秒级节流刷新 */
        p->last_fetch = now;
        cli_panel_mem_fetch(p);
    }
    return p->count;
}

int cli_panel_mem_line(void *ud, size_t idx, char *out, size_t cap)
{
    cli_mem_panel_t *p = (cli_mem_panel_t *)ud;
    if (!p || idx >= p->count || !p->lines[idx])
        return 0;
    snprintf(out, cap, "%s", p->lines[idx]);
    return 1;
}
