// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_output.c
 * @brief Conversation output: role lines, tool cards, progress, separators.
 *
 * Extracted from cli_render.c.  Owns the role-tagged conversation line
 * renderer (role_line / super_agent / user_message / sub_agent), the tool
 * invocation/result cards（0.1.19 自 airy_cli_markdown.c 归位，机制层），
 * the progress bar and task line for the work-hall board, and the turn
 * separator that marks the end of each agent turn.
 *
 * 0.1.19 直印化：内容输出收口为 cli_print_plain（plain 直印），markdown
 * 排版体验由 agentrt-tui（sdk/tui）承接；airy_cli_markdown.c 随之清退。
 */

#include "cli_render.h"
#include "cli_tui.h"

#include "airy_memory.h"
#include "logger.h"

extern int g_cli_print_mode;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- role-tagged conversation line ----
 * v2：角色头宽度 24 → 20，内容起始列由 28 缩进至 24——消息流更紧凑，
 * 长对话下可视信息密度更高。 */

#define CLI_ROLE_HDR_W 20

static void cli_pad_role_header(const char *hdr)
{
    size_t w = cli_disp_width(hdr);
    for (size_t i = w; i < CLI_ROLE_HDR_W; i++)
        cli_outc(' ');
}

static void cli_build_role_header(char *out, size_t cap, const char *name, const char *tag)
{
    if (!tag || !tag[0]) {
        snprintf(out, cap, "[%s]", name);
        return;
    }
    size_t name_len = strlen(name);
    size_t tag_budget =
        CLI_ROLE_HDR_W > (name_len + 3) ? (CLI_ROLE_HDR_W - name_len - 3) : 0;
    size_t tag_show = strlen(tag);
    if (tag_show > tag_budget)
        tag_show = cli_utf8_safe_len(tag, tag_budget);
    snprintf(out, cap, "[%s:%.*s]", name, (int)tag_show, tag);
}

void cli_render_role_line(cli_role_t role, cli_actor_t actor, const char *tag,
                          const char *content)
{
    if (g_cli_print_mode &&
        (role == CLI_ROLE_TRACE || role == CLI_ROLE_STATUS || role == CLI_ROLE_SUB_AGENT))
        return;
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(role);
    const char *name = cli_render_actor_name(actor);
    char hdr[CLI_ROLE_HDR_W + 1];

    cli_build_role_header(hdr, sizeof(hdr), name, tag);

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);

    if (content)
        cli_print_plain(content, 2);
    else
        cli_outc('\n');
}

void cli_render_super_agent(const char *content)
{
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL, content);
}

void cli_render_user_message(const char *content)
{
    if (g_cli_print_mode)
        return;
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(CLI_ROLE_USER);
    const char *name = cli_render_actor_name(CLI_ACTOR_USER);
    char hdr[CLI_ROLE_HDR_W + 1];

    snprintf(hdr, sizeof(hdr), "[%s]", name);

    /* v2：去掉 CLR_BG_GRAY 全行背景块（视觉厚重、多行内容像补丁），
     * 改 "›" 前缀 + 角色色——与助手消息同构（同为 [头] + 内容），
     * 靠角色色与图标区分身份，消息流整体更干净。 */
    cli_outc('\n');
    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
    cli_out(col);
    cli_out(CLI_ICON_USER " ");
    cli_out(cli_c(CLR_RESET));
    if (content && content[0]) {
        cli_out(content);
    }
    cli_outc('\n');
    cli_outc('\n');
}

void cli_render_sub_agent_line(cli_role_t role, const char *tag, const char *content)
{
    if (g_cli_print_mode && role != CLI_ROLE_ERROR)
        return;
    char hdr[CLI_ROLE_HDR_W + 1];
    const char *t = (tag && tag[0]) ? tag : "exec";

    size_t tag_budget = CLI_ROLE_HDR_W > 12 ? (CLI_ROLE_HDR_W - 12) : 0;
    size_t tag_show = strlen(t);
    if (tag_show > tag_budget)
        tag_show = tag_budget;
    snprintf(hdr, sizeof(hdr), "[Sub %.*s Agent]", (int)tag_show, t);

    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(role);
    const char *icon = (role == CLI_ROLE_ERROR) ? CLI_ICON_CROSS : CLI_ICON_DIAMOND;

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
    cli_out(col);
    cli_out(icon);
    cli_out(" ");
    cli_out(cli_c(CLR_RESET));

    if (content)
        cli_print_plain(content, 2);
    else
        cli_outc('\n');
}

void cli_render_sub_agent(const char *tag, const char *content)
{
    cli_render_sub_agent_line(CLI_ROLE_SUB_AGENT, tag, content);
}

/* ---- turn separator ---- */

void cli_render_turn_separator(uint64_t elapsed_ms, const char *metrics)
{
    if (g_cli_print_mode)
        return;
    cli_outc('\n');
    uint64_t secs = elapsed_ms / 1000;
    char label[128];
    if (secs < 1)
        snprintf(label, sizeof(label), "完成");
    else if (secs < 60)
        snprintf(label, sizeof(label), "耗时 %llus", (unsigned long long)secs);
    else
        snprintf(label, sizeof(label), "耗时 %llum %02llus",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));

    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t dash_left = 8; /* v2：12 → 8，分隔线更轻，不喧宾夺主 */
    size_t dash_right = 8;
    (void)rows;
    (void)cols;

    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    for (size_t i = 0; i < dash_left; i++)
        cli_out("─");
    cli_out(" ");
    cli_out(cli_c(CLR_RESET));
    cli_out(label);
    if (metrics && metrics[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(" · ");
        cli_out(cli_c(CLR_RESET));
        cli_out(metrics);
    }
    cli_out(cli_c(CLR_DIM));
    cli_out(" ");
    for (size_t i = 0; i < dash_right; i++)
        cli_out("─");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* ---- compact task line for the work-hall board ---- */

void cli_render_task_line(const char *tag, const char *id, const char *state,
                          double progress)
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

/* ---- tool invocation / result ---- */

/* 0.1.18 B3（V3.4）：未登记工具标识符的用户面显示名。
 *
 * 内部工具标识符不得越过渲染边界进入用户面：白名单外一律显示本串并记日志，
 * 禁止原样透传。与 TUI 侧 app/dispatch.rs::tool_action 同源同语义——同一份
 * 白名单内容、同一个兜底串，两处渲染点收口口径一致。 */
static const char CLI_TOOL_HIDDEN[] = "未知工具（已隐藏）";

static const char *cli_tool_action(const char *name)
{
    static const struct { const char *tool; const char *action; } map[] = {
        {"web_search", "搜索网络"}, {"web_fetch", "抓取网页"},
        {"fs_read", "读取文件"},   {"fs_write", "写入文件"},
        {"fs_list", "列出目录"},   {"fs_ls", "列出目录"},
        {"fs_info", "查看文件信息"}, {"fs_mkdir", "创建目录"},
        {"fs_rm", "删除文件"},     {"agent.spawn", "派生智能体"},
        {"agent.invoke", "调用智能体"}, {"think.depth", "深度思考"},
        {"memory.get", "读取记忆"}, {"memory.put", "写入记忆"},
    };
    if (!name || !name[0])
        return CLI_TOOL_HIDDEN;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].tool) == 0)
            return map[i].action;
    }
    /* 只记长度、不记标识符本身：日志不应成为第二处泄漏面（与 TUI 侧一致）。 */
    AIRY_LOG_WARN("airy_cli: 未登记工具标识符已按白名单隐藏 (len=%zu)", strlen(name));
    return CLI_TOOL_HIDDEN;
}

void cli_render_tool_use(const char *name, const char *args)
{
    (void)args;
    if (!name) return;
    const char *action = cli_tool_action(name);

    if (g_cli_print_mode) {
        fputs(cli_c(CLR_MAGENTA), stderr);
        fputs(CLI_ICON_TOOL, stderr);
        fputs(" ", stderr);
        fputs(cli_c(CLR_RESET), stderr);
        fputs(cli_c(CLR_CYAN), stderr);
        fputs(action, stderr);
        fputs(cli_c(CLR_RESET), stderr);
        fputs("…\n", stderr);
        return;
    }

    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(cli_c(CLR_MAGENTA));
    cli_out(CLI_ICON_TOOL);
    cli_out(" ");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_CYAN));
    cli_out(action);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out("…");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

static void cli_tool_error_summary(const char *raw, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!raw || !raw[0]) return;
    const char *p = raw;
    if (strncmp(p, "Web fetch failed", 16) == 0) {
        const char *close = strchr(p, ')');
        if (close && close[1] == ':') p = close + 2;
        while (*p == ' ') p++;
    }
    if (strstr(p, "timed out") || strstr(p, "Timeout") || strstr(p, "timeout")) {
        snprintf(out, cap, "连接超时"); return;
    }
    if (strstr(p, "refused") || strstr(p, "Failed to connect") || strstr(p, "couldn't connect")) {
        snprintf(out, cap, "连接被拒绝"); return;
    }
    if (strstr(p, "Could not resolve") || strstr(p, "getaddrinfo") || strstr(p, "resolve host")) {
        snprintf(out, cap, "无法解析域名"); return;
    }
    if (strstr(p, "404") || strstr(p, "Not Found")) {
        snprintf(out, cap, "页面不存在 (404)"); return;
    }
    size_t o = 0;
    for (; *p && o + 1 < cap; p++) {
        if (*p == '\n' || *p == '\r') break;
        out[o++] = *p;
    }
    out[o] = '\0';
}

void cli_render_tool_result(const char *name, const char *text, int ok)
{
    if (!name) return;
    const char *action = cli_tool_action(name);

    char err[128];
    err[0] = '\0';
    if (!ok && text && text[0]) {
        size_t o = 0;
        for (const char *p = text; *p && o + 1 < sizeof(err); p++) {
            if (*p == '\n' || *p == '\r') break;
            err[o++] = *p;
        }
        err[o] = '\0';
        char plain[128];
        cli_tool_error_summary(err, plain, sizeof(plain));
        if (plain[0]) AIRY_STRNCPY_TERM(err, plain, sizeof(err));
    }

    if (g_cli_print_mode) {
        fputs(cli_c(ok ? CLR_GREEN : CLR_RED), stderr);
        fputs(ok ? CLI_ICON_CHECK : CLI_ICON_CROSS, stderr);
        fputs(cli_c(CLR_RESET), stderr);
        fputs(" ", stderr);
        fputs(cli_c(CLR_CYAN), stderr);
        fputs(action, stderr);
        fputs(cli_c(CLR_RESET), stderr);
        if (err[0]) { fputs(" — ", stderr); fputs(err, stderr); }
        fputc('\n', stderr);
        return;
    }

    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(ok ? cli_c(CLR_GREEN) : cli_c(CLR_RED));
    cli_out(ok ? CLI_ICON_CHECK : CLI_ICON_CROSS);
    cli_out(" ");
    cli_out(cli_c(CLR_CYAN));
    cli_out(action);
    cli_out(cli_c(CLR_RESET));
    if (err[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(" — ");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_RED));
        cli_out(err);
        cli_out(cli_c(CLR_RESET));
    } else {
        cli_out(ok ? cli_c(CLR_DIM) : cli_c(CLR_RESET));
        cli_out(ok ? " 完成" : "");
        cli_out(cli_c(CLR_RESET));
    }
    cli_outc('\n');
}
