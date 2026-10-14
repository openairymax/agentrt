// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_render.c
 * @brief airy_cli 终端输出机制层：原语、色门、宽度、直印、状态辅助与
 *        结果/计划/看板呈现。
 *
 * 0.1.19 M5-3 清退收口：直印化完成后，体验渲染（role 卡片、spinner、
 * phase 指示）整体删除，排版体验由 agentrt-tui（sdk/tui）承接；原
 * cli_banner.c（启动头部）与 cli_display.c（结果/计划列表/看板行）
 * 收编于此，render 域收缩为单一机制文件。
 *
 * 面向流安全、行式输出：无 TTY 捕获、无 curses，所有函数直写 stdout，
 * 本地终端、SSH 与管道日志行为完全一致。
 */

#include "cli_render.h"
#include "cli_internal.h"

#include "airy_memory.h"
#include "cli_gw.h"
#include "platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#define CLI_GUTTER_MAX 64

/* ---- 输出原语 ---- */

void cli_out(const char *s)
{
    if (!s) return;
    cli_outn(s, strlen(s));
}

void cli_outn(const char *s, size_t n)
{
    if (!s || n == 0) return;
    fwrite(s, 1, n, stdout);
}

void cli_outc(char c)
{
    fputc(c, stdout);
}

void cli_outf(const char *fmt, ...)
{
    if (!fmt) return;
    va_list ap;
    va_start(ap, fmt);
    char stack_buf[1024];
    int need = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (need < 0) return;
    if ((size_t)need < sizeof(stack_buf)) {
        cli_outn(stack_buf, (size_t)need);
        return;
    }
    char *big = (char *)AIRY_MALLOC((size_t)need + 1);
    if (!big) return;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)need + 1, fmt, ap);
    va_end(ap);
    cli_outn(big, (size_t)need);
    AIRY_FREE(big);
}

/* ---- 错误描述 ---- */

const char *cli_err_desc(int err)
{
    /* 网关失败原因优先且一次性消费（cli_gw_err_take 内已清零） */
    const char *gw_desc = cli_gw_err_take();
    if (gw_desc[0] != '\0')
        return gw_desc;
    switch (err) {
    case AIRY_ERR_TIMEOUT:          return "请求超时，请检查网络或服务状态";
    case AIRY_ERR_NOT_FOUND:        return "目标不存在或未找到";
    case AIRY_ERR_GENERIC_FAIL:     return "操作失败，请稍后重试";
    case AIRY_ERR_IO:               return "读写错误，请检查文件或磁盘";
    case AIRY_ERR_PERMISSION_DENIED: return "权限不足，无法执行该操作";
    case AIRY_ERR_INVALID_PARAM:    return "参数无效，请检查输入";
    case AIRY_ERR_NOT_SUPPORTED:    return "暂不支持该操作";
    case AIRY_ERR_OUT_OF_MEMORY:    return "内存不足";
    case AIRY_ERR_CANCELED:         return "操作已取消";
    case AIRY_ERR_WOULD_BLOCK:      return "资源暂时不可用，请稍后重试";
    /* GCCP 两段式交互哨兵：正常流程在 CLI 内部闭环；漏出即交互未收敛，
     * 给出可读文案而非"发生错误"误导为引擎故障 */
    case AIRY_ERR_GCCP_INTERACTION: return "意图信息不完整，GCCP 确认未收敛，请补充任务描述";
    default:                        return "发生错误";
    }
}

/* ---- -p 模式进度通道（stderr） ---- */

void cli_trace(const char *tag, const char *fmt, ...)
{
    if (!g_cli_print_mode) return;
    if (!tag || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    char stack_buf[1024];
    int need = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (need < 0) return;
    fputs("[", stderr);
    fputs(tag, stderr);
    fputs("] ", stderr);
    if ((size_t)need < sizeof(stack_buf)) {
        fwrite(stack_buf, 1, (size_t)need, stderr);
    } else {
        char *big = (char *)AIRY_MALLOC((size_t)need + 1);
        if (big) {
            va_start(ap, fmt);
            vsnprintf(big, (size_t)need + 1, fmt, ap);
            va_end(ap);
            fwrite(big, 1, (size_t)need, stderr);
            AIRY_FREE(big);
        }
    }
    fputc('\n', stderr);
}

/* ---- 色门与缩进槽 ---- */

const char *cli_c(const char *seq)
{
    return cli_color_enabled() ? seq : "";
}

const char *cli_gutter(size_t indent)
{
    static char buf[CLI_GUTTER_MAX + 1];
    size_t n = indent > CLI_GUTTER_MAX ? CLI_GUTTER_MAX : indent;
    for (size_t i = 0; i < n; i++)
        buf[i] = ' ';
    buf[n] = '\0';
    return buf;
}

/* ---- UTF-8 显示宽度 ---- */

size_t cli_disp_width(const char *s)
{
    if (!s) return 0;
    return cli_disp_width_of(s, strlen(s));
}

size_t cli_disp_width_of(const char *s, size_t n)
{
    size_t w = 0;
    size_t i = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (i < n && p[i]) {
        if (p[i] < 0x80) { w += 1; i += 1; }
        else if ((p[i] & 0xE0) == 0xC0) { w += 1; i += 2; }
        else if ((p[i] & 0xF0) == 0xE0) {
            w += (p[i] == 0xE2) ? 1 : 2;
            i += 3;
        } else { w += 2; i += 4; }
    }
    return w;
}

size_t cli_utf8_safe_len(const char *s, size_t max_bytes)
{
    size_t len = strlen(s);
    if (max_bytes >= len) return len;
    size_t n = max_bytes;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    return n;
}

/* ---- plain 直印（0.1.19 统一出口） ---- */

void cli_print_plain(const char *text, size_t indent)
{
    if (!text) return;
    const char *g = cli_gutter(indent);
    const char *p = text;
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        cli_out(g);
        cli_outn(p, len);
        cli_outc('\n');
        p = nl ? nl + 1 : NULL;
    }
}

void cli_print_line(const char *header, const char *content)
{
    cli_out("  [");
    cli_out(header && header[0] ? header : "AgentRT");
    cli_out("] ");
    if (!content || !content[0]) {
        cli_outc('\n');
        return;
    }
    const char *nl = strchr(content, '\n');
    if (!nl) {
        cli_out(content);
        cli_outc('\n');
        return;
    }
    cli_outn(content, (size_t)(nl - content));
    cli_outc('\n');
    cli_print_plain(nl + 1, 2);
}

void cli_print_status(const char *header, const char *content)
{
    if (g_cli_print_mode)
        return;
    cli_print_line(header, content);
}

void cli_print_turn(uint64_t elapsed_ms, const char *metrics)
{
    uint64_t secs = elapsed_ms / 1000;
    char line[256];
    if (secs < 1)
        snprintf(line, sizeof(line), "完成");
    else if (secs < 60)
        snprintf(line, sizeof(line), "耗时 %llus", (unsigned long long)secs);
    else
        snprintf(line, sizeof(line), "耗时 %llum %02llus",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));
    if (metrics && metrics[0])
        snprintf(line + strlen(line), sizeof(line) - strlen(line), " · %s", metrics);
    cli_print_status("turn", line);
}

uint64_t cli_now_ms(void)
{
    return airy_time_ns() / 1000000ULL;
}

/* ---- 执行体状态辅助（单一事实源） ---- */

/* 终态串为 completed / semantic_failed / failed / canceled，其中
 * semantic_failed（节点进程成功但没有可交付产物）同样是终态，且必须按
 * 失败呈现，否则会把「无产出」渲染成绿勾。轮询、看板、快照与任务流
 * 多处共用本函数，避免新增状态时各写一遍而漂移。 */
int cli_state_terminal(const char *state)
{
    if (!state) return 0;
    return strcmp(state, "completed") == 0 || strcmp(state, "semantic_failed") == 0 ||
           strcmp(state, "failed") == 0 || strcmp(state, "canceled") == 0;
}

int cli_state_failed(const char *state)
{
    if (!state) return 0;
    return strcmp(state, "semantic_failed") == 0 || strcmp(state, "failed") == 0 ||
           strcmp(state, "canceled") == 0;
}

const char *cli_icon_for_state(const char *state)
{
    if (!state) return CLI_ICON_BULLET;
    if (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0 ||
        strcmp(state, "done") == 0) return CLI_ICON_CHECK;
    if (strcmp(state, "failed") == 0 || strcmp(state, "error") == 0) return CLI_ICON_CROSS;
    if (strcmp(state, "semantic_failed") == 0) return CLI_ICON_CROSS;
    if (strcmp(state, "running") == 0 || strcmp(state, "active") == 0 ||
        strcmp(state, "executing") == 0) return CLI_ICON_DIAMOND;
    if (strcmp(state, "pending") == 0 || strcmp(state, "queued") == 0 ||
        strcmp(state, "ready") == 0) return CLI_ICON_TODO;
    if (strcmp(state, "scheduled") == 0) return CLI_ICON_CLOCK;
    if (strcmp(state, "canceled") == 0) return CLI_ICON_CANCEL;
    return CLI_ICON_BULLET;
}

const char *cli_state_cn(const char *state)
{
    if (!state) return "未知";
    if (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0 ||
        strcmp(state, "done") == 0) return "完成";
    if (strcmp(state, "failed") == 0) return "失败";
    if (strcmp(state, "semantic_failed") == 0) return "无产出";
    if (strcmp(state, "canceled") == 0) return "已取消";
    if (strcmp(state, "error") == 0) return "出错";
    if (strcmp(state, "running") == 0 || strcmp(state, "active") == 0 ||
        strcmp(state, "executing") == 0) return "执行中";
    if (strcmp(state, "queued") == 0) return "排队中";
    if (strcmp(state, "pending") == 0) return "待处理";
    if (strcmp(state, "scheduled") == 0) return "已调度";
    if (strcmp(state, "ready") == 0) return "就绪";
    if (strcmp(state, "waiting") == 0) return "等待中";
    if (strcmp(state, "skipped") == 0) return "已跳过";
    if (strcmp(state, "retrying") == 0) return "重试中";
    if (strcmp(state, "online") == 0) return "在线";
    if (strcmp(state, "offline") == 0) return "离线";
    return state;
}

void cli_compact_bar(char *out, size_t cap, double progress, size_t cells)
{
    if (!out || cap == 0) return;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    size_t filled = (size_t)(progress * (double)cells);
    if (filled > cells) filled = cells;
    size_t o = 0;
    if (o < cap - 1) out[o++] = '[';
    for (size_t i = 0; i < cells && o < cap - 1; i++)
        out[o++] = (i < filled) ? '#' : '-';
    if (o < cap - 1) out[o++] = ']';
    out[o] = '\0';
}

/* ---- 启动头部（原 cli_banner.c 收编） ---- */

/* 一个 "key=model" 段（紧凑头部模型槽行用）：青 key · dim "=" · 黄 model。
 * lead_sep 为 1 时前置两个空格分隔。 */
static void cli_hero_model_seg(const char *key, const char *model, int lead_sep)
{
    if (lead_sep) {
        cli_out("  ");
    }
    cli_out(cli_c(CLR_CYAN));
    cli_out(key);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out("=");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_YELLOW));
    cli_out(model);
    cli_out(cli_c(CLR_RESET));
}

/* 紧凑启动头部（3 行，普通滚动，不再 pin）。行数与 CLI_HDR_LINES=3 严格
 * 对应（cli_tui.c 退出全屏 TUI 重建三区时依赖该行数）。-p 单轮模式不打印
 * （stdout 保持纯净）。 */
void cli_print_system_header(const char *t2, const char *t1f, const char *t1p)
{
    if (g_cli_print_mode)
        return;

    const char *g = cli_gutter(2);
    /* 行 1：品牌 + 版本 */
    cli_out(g);
    cli_out(cli_c(CLR_BOLD));
    cli_out(cli_c(CLR_CYAN));
    cli_out("◆ Airymax AgentRT");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf(" · v%s", AIRY_CLI_VERSION);
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
    /* 行 2：三模型槽（空值回落"默认"） */
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";
    cli_out(g);
    cli_hero_model_seg("A·t2", a, 0);
    cli_hero_model_seg("B·t1-f", b, 1);
    cli_hero_model_seg("C·t1-p", c, 1);
    cli_outc('\n');
    /* 行 3：分界线（保持 CLI_HDR_LINES=3 契约） */
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    {
        char rule[192];
        size_t rw = 56, rn = 0;
        while (rn < rw && rn + 3 < sizeof(rule)) {
            rule[rn++] = '\xE2';
            rule[rn++] = '\x94';
            rule[rn++] = '\x80';   /* U+2500 ─（1 cell） */
        }
        rule[rn] = '\0';
        cli_out(rule);
    }
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
    fflush(stdout);
}

/* ---- 执行结果呈现 ---- */

void cli_print_result(const char *result)
{
    if (!result) {
        cli_print_line("Super Agent:result", "No result was produced.");
        return;
    }

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    if (root) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        cJSON *agent = cJSON_GetObjectItem(root, "agent_id");
        if (output && cJSON_IsString(output)) {
            /* -p 模式：stdout 保持纯净可管道，跳过角色行前缀，模型原始
             * 文本 plain 直印（0.1.19 直印化，不做显示期归一）。 */
            if (g_cli_print_mode) {
                cli_print_plain(output->valuestring, 0);
                cJSON_Delete(root);
                return;
            }
            if (agent && cJSON_IsString(agent)) {
                char line[160];
                snprintf(line, sizeof(line), "Execution finished by %s.", agent->valuestring);
                cli_print_line("Super Agent:result", line);
            } else {
                cli_print_line("Super Agent:result", "Execution finished.");
            }
            cli_print_plain(output->valuestring, 4);
            cJSON_Delete(root);
            return;
        }
        cJSON_Delete(root);
    }
#endif /* AIRY_HAS_CJSON */

    if (g_cli_print_mode) {
        cli_outf("%s\n", result ? result : "");
        return;
    }

    char truncated[640];
    /* 回落到 UTF-8 边界，长结果的截断预览不出现半个多字节字符 */
    size_t keep = cli_utf8_safe_len(result, 600);
    snprintf(truncated, sizeof(truncated), "%.*s%s", (int)keep, result,
             strlen(result) > keep ? "..." : "");
    cli_print_line("Super Agent:result", truncated);
}

/* ---- 静态执行计划列表 ---- */

/* 按 id 在计划内查找节点下标（不存在返回 -1）。 */
static int cli_plan_node_index(const airy_task_plan_t *plan, const char *id)
{
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        const airy_task_node_t *nd = plan->task_plan_nodes ? plan->task_plan_nodes[i] : NULL;
        if (nd && nd->task_node_id && strcmp(nd->task_node_id, id) == 0)
            return (int)i;
    }
    return -1;
}

/* 节点 idx 的全部依赖是否都来自已排序节点。计划外依赖、自环、空 id 或
 * 窗口 [0, count) 外的源视为已满足，保证排序必然终止；窗口守卫避免只排
 * 前 count 个节点的调用方越界读。 */
static int cli_plan_ready(const airy_task_plan_t *plan, size_t idx,
                          const unsigned char *ordered, size_t count)
{
    const airy_task_node_t *nd =
        plan->task_plan_nodes ? plan->task_plan_nodes[idx] : NULL;
    if (!nd || !nd->task_node_id)
        return 1;
    const char *target = nd->task_node_id;
    for (size_t d = 0; nd->task_node_depends_on && d < nd->task_node_depends_count; d++) {
        const char *src_id = nd->task_node_depends_on[d];
        if (!src_id || src_id[0] == '\0')
            continue;
        if (strcmp(src_id, target) == 0)
            continue;
        int src = cli_plan_node_index(plan, src_id);
        if (src >= 0 && (size_t)src < count && !ordered[src])
            return 0;
    }
    return 1;
}

/* 贪心拓扑排序：依赖在前，环/缺失源回落自然次序，渲染永不卡死。
 * scratch 为调用方提供的 count 字节零初始化工作区。返回 0 表示跳过渲染。 */
static int cli_plan_topo_build(const airy_task_plan_t *plan, size_t count, size_t *order,
                               unsigned char *scratch)
{
    if (!plan || !order || !scratch || count == 0 || plan->task_plan_node_count == 0)
        return 0;

    size_t placed = 0;
    for (size_t step = 0; step < count; step++) {
        size_t pick = (size_t)-1;
        for (size_t i = 0; i < count; i++) {
            if (scratch[i])
                continue;
            if (cli_plan_ready(plan, i, scratch, count)) {
                pick = i;
                break;
            }
        }
        if (pick == (size_t)-1)
            break;
        order[placed++] = pick;
        scratch[pick] = 1;
    }
    for (size_t i = 0; i < count && placed < count; i++) {
        if (!scratch[i]) {
            order[placed++] = i;
            scratch[i] = 1;
        }
    }
    return placed == count;
}

void cli_print_plan_list(const airy_task_plan_t *plan)
{
    if (g_cli_print_mode)
        return;
    if (!plan || plan->task_plan_node_count == 0) {
        cli_print_line("Sub plan Agent", "Empty plan, nothing to execute.");
        return;
    }

    const size_t n = plan->task_plan_node_count;
    const char *g = cli_gutter(2);

    cli_outf("%s%s%s %s执行计划%s %s(%zu nodes, %zu deps)%s\n",
             g, cli_c(CLR_CYAN), CLI_ICON_DIAMOND,
             cli_c(CLR_RESET), cli_c(CLR_DIM), cli_c(CLR_DIM),
             n, cli_plan_deps_count(plan), cli_c(CLR_RESET));

    size_t *order = (size_t *)AIRY_MALLOC(n * sizeof(size_t));
    unsigned char *scratch = (unsigned char *)AIRY_CALLOC(n, 1);
    if (!order || !scratch || !cli_plan_topo_build(plan, n, order, scratch)) {
        AIRY_FREE(order);
        AIRY_FREE(scratch);
        return;
    }

    const char *ig = cli_gutter(4);
    for (size_t r = 0; r < n; r++) {
        const airy_task_node_t *nd = plan->task_plan_nodes ? plan->task_plan_nodes[order[r]] : NULL;
        if (!nd)
            continue;
        const char *nid = (nd->task_node_id && nd->task_node_id[0]) ? nd->task_node_id : "node";
        char handler[96] = "";
        cli_node_handler(nd, handler, sizeof(handler));
        const char *goal = nd->task_node_goal ? nd->task_node_goal : "";

        cli_outf("%s%s%s%s %zu. %s%s%s  %s%s[%s]%s",
                 ig, cli_c(CLR_DIM), CLI_ICON_TODO, cli_c(CLR_RESET),
                 r + 1,
                 cli_c(CLR_RESET), goal, cli_c(CLR_DIM),
                 cli_c(CLR_DIM), handler[0] ? handler : "?", cli_c(CLR_DIM), cli_c(CLR_RESET));

        char deps[128];
        size_t dlen = 0;
        int has_dep = 0;
        for (size_t d = 0; nd->task_node_depends_on && d < nd->task_node_depends_count; d++) {
            const char *src_id = nd->task_node_depends_on[d];
            if (!src_id || src_id[0] == '\0')
                continue;
            if (strcmp(src_id, nid) == 0)
                continue;
            if (has_dep && dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, ", ");
            if (!has_dep) {
                dlen = (size_t)snprintf(deps, sizeof(deps), "  %s←%s ",
                                        cli_c(CLR_DIM), cli_c(CLR_RESET));
                has_dep = 1;
            }
            if (dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, "%s%s%s",
                                         cli_c(CLR_CYAN), src_id, cli_c(CLR_DIM));
        }
        cli_outf("%s\n", has_dep ? deps : "");
    }

    AIRY_FREE(order);
    AIRY_FREE(scratch);
}

/* ---- 看板行 ---- */

void cli_board_line(const char *tag, const char *id, const char *state, double progress)
{
    if (g_cli_print_mode)
        return;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    size_t filled = (size_t)(progress * 10.0);
    if (filled > 10) filled = 10;
    char bar[32];
    size_t bo = 0;
    for (size_t b = 0; b < 10; b++) {
        bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", b < filled ? "█" : "░");
    }

    const char *st = state ? state : "?";
    const char *st_col = cli_c(CLR_DIM);
    const char *st_icon = CLI_ICON_BULLET;
    int bar_bright = 0;
    if (strcmp(st, "completed") == 0) {
        st_col = cli_c(CLR_GREEN); st_icon = CLI_ICON_CHECK; bar_bright = 1;
    } else if (strcmp(st, "failed") == 0) {
        st_col = cli_c(CLR_RED); st_icon = CLI_ICON_CROSS; bar_bright = 0;
    } else if (strcmp(st, "semantic_failed") == 0) {
        st_col = cli_c(CLR_YELLOW); st_icon = CLI_ICON_CROSS; bar_bright = 0;
    } else if (strcmp(st, "running") == 0 || strcmp(st, "active") == 0 ||
               strcmp(st, "queued") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_DIAMOND;
    } else if (strcmp(st, "pending") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_TODO;
    } else if (strcmp(st, "scheduled") == 0) {
        st_col = cli_c(CLR_CYAN); st_icon = CLI_ICON_CLOCK;
    } else if (strcmp(st, "canceled") == 0) {
        st_col = cli_c(CLR_DIM); st_icon = CLI_ICON_CANCEL;
    }

    const char *g = cli_gutter(2);
    if (tag && tag[0])
        cli_outf("%s%s[%s]%s ", g, cli_c(CLR_DIM), tag, cli_c(CLR_RESET));
    cli_outf("%s%s %s%s%s ", st_col, st_icon, cli_c(CLR_RESET), cli_c(CLR_DIM),
             id ? id : "?");
    cli_outf("%s%s%s ", st_col, cli_state_cn(st), cli_c(CLR_RESET));
    cli_outf("%s[%s]%s %s%3.0f%%%s\n", cli_c(bar_bright ? CLR_GREEN : CLR_DIM), bar,
             cli_c(CLR_RESET), cli_c(bar_bright ? CLR_GREEN : CLR_DIM), progress * 100.0,
             cli_c(CLR_RESET));
}
