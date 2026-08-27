// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_banner.c
 * @brief Startup hero banner: blue-framed welcome panel and system header.
 *
 * The startup header is a blue box with the brand on the top edge:
 *
 *   ┌─ ◆ Airymax - Agent Runtime Platform Engineering ─┐
 *   │  版本 v0.1.5：对话 · 任务 · 蓝图调度 · 双思考 · … │
 *   │  [For Thee] 你  [Super Agent] agentrt  …         │
 *   │  A·t2 → …  B·t1-f → …  C·t1-p → …               │
 *   │  ? /help 查看命令 · quit/exit 退出  "Agents…"     │
 *   └───────────────────────────────────────────────────┘
 *
 * The frame is blue (CLR_BLUE); inner rows keep their own role colors so
 * the [For Thee]/[Super Agent]/[Dual Think]/[Sub Agent] scheme stays
 * visible. The whole block is pinned so conversation output scrolls below
 * it — a clear visual boundary between the system header and the dialogue.
 * Color gating uses the shared cli_c() (NO_COLOR / piped output renders
 * monochrome too, keeping the box geometry).
 *
 * Every content row is width-budgeted (cli_hero_content_max): segments
 * that would overflow the frame are dropped or clipped (UTF-8 safe, "…"),
 * so on narrow terminals the box stays intact instead of wrapping — a
 * wrapped row would shift the pinned line count and let the dialogue
 * overlap the header.
 *
 * 自 2026-08-27 起从 cli_display.c 拆出：本文件只承载 hero 横幅域。其中
 * cli_hero_clip（UTF-8 安全宽度截断）经 cli_display_internal.h 导出，
 * 由 cli_live_board.c 的节点行复用。
 */

#include "cli_internal.h"
#include "cli_display_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Frame width: adapts to the terminal, clamped for readability. */
static size_t cli_hero_frame_w(void)
{
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t w = (cols > 4) ? (size_t)cols - 4 : 74;
    if (w < 74)
        w = 74;
    if (w > 110)
        w = 110;
    return w;
}

/* Content budget inside the frame: each row is
 *   gutter + "│ " + content + padding + "│"
 * so content may use at most w-3 cells (one padding cell remains). */
static size_t cli_hero_content_max(size_t w)
{
    return (w > 3) ? w - 3 : 0;
}

/**
 * @brief Width-aware, UTF-8-safe truncation of `s` into buf (cap bytes) so
 *        it fits max_w cells; appends "…" (1 cell) when text was cut.
 *        Returns the display width of what was emitted.
 *
 * Exposed via cli_display_internal.h (see there for the cross-file usage).
 */
size_t cli_hero_clip(const char *s, size_t max_w, char *buf, size_t cap)
{
    size_t len = strlen(s);
    size_t n = 0, w = 0;
    while (n < len) {
        unsigned char c = (unsigned char)s[n];
        size_t cbytes = (c < 0x80) ? 1
                      : ((c & 0xE0) == 0xC0) ? 2
                      : ((c & 0xF0) == 0xE0) ? 3
                      : ((c & 0xF8) == 0xF0) ? 4 : 1;
        if (n + cbytes >= cap)
            break; /* keep room for the terminator */
        /* width of the current character only (not the rest of the
         * string): pass cbytes, otherwise the whole remaining tail is
         * counted and every row gets clipped to its first glyph */
        size_t cw = cli_disp_width_of(s + n, cbytes);
        if (w + cw > max_w)
            break;
        AIRY_MEMCPY(buf + n, s + n, cbytes);
        n += cbytes;
        w += cw;
    }
    if (n < len && w + 1 <= max_w && n + 3 < cap) {
        /* cut: append "…" (U+2026, 3 bytes, 1 cell) */
        buf[n] = '\xE2';
        buf[n + 1] = '\x80';
        buf[n + 2] = '\xA6';
        buf[n + 3] = '\0';
        w += 1;
    } else {
        buf[n] = '\0';
    }
    return w;
}

/* Close a framed content row: pad spaces to the frame width then "│". */
static void cli_hero_line_end(size_t used, size_t w)
{
    cli_out(cli_c(CLR_BLUE));
    while (used + 1 < w) {
        cli_out(" ");
        used++;
    }
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* Top edge with the brand in the title area: ┌─ ◆ Airymax - Agent Runtime
 * Platform Engineering ──────────────────────────┐ */
static void cli_hero_brand(const char *g)
{
    size_t w = cli_hero_frame_w();
    size_t used = 3; /* "┌─ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("┌─ ");
    cli_out(cli_c(CLR_BOLD));
    cli_out(cli_c(CLR_CYAN));
    char buf[96];
    /* leave one cell for a "─" filler before "┐" */
    size_t tmax = cli_hero_content_max(w);
    if (tmax > 1)
        tmax -= 1;
    size_t tw = cli_hero_clip("◆ Airymax - Agent Runtime Platform Engineering",
                              tmax, buf, sizeof(buf));
    cli_out(buf);
    cli_out(cli_c(CLR_RESET));
    used += tw;
    cli_out(cli_c(CLR_BLUE));
    while (used + 1 < w) {
        cli_out("─");
        used++;
    }
    cli_out("┐");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* Bottom edge: └─────┘ */
static void cli_hero_frame_bottom(const char *g)
{
    size_t w = cli_hero_frame_w();
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("└");
    for (size_t i = 1; i + 1 < w; i++)
        cli_out("─");
    cli_out("┘");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* One quiet capabilities row inside the frame, carrying the version and
 * what the runtime does at a glance: 版本 v0.1.5：对话 · 任务 · … */
static void cli_hero_capabilities(const char *g)
{
    char text[160];
    /* 1 leading space matches the other hero rows ("│ [For Thee]",
     * "│ A·t2", "│ ? /help") so all content is left-aligned inside
     * the frame. */
    snprintf(text, sizeof(text), " 版本 v%s：对话 · 任务 · 蓝图调度 · 双思考 · GCCP · 工具执行",
             AIRY_CLI_VERSION);
    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    char out[160];
    size_t tw = cli_hero_clip(text, budget, out, sizeof(out));
    size_t used = 2 + tw; /* "│ " + content */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");
    cli_out(cli_c(CLR_DIM));
    cli_out(out);
    cli_out(cli_c(CLR_RESET));
    cli_hero_line_end(used, w);
}

/* Four-role conversation legend. Each bracket keeps its role color so the
 * scheme [For Thee] / [Super Agent] / [Dual Think] / [Sub Agent] is visible
 * at a glance on startup and stays consistent with every conversation line.
 * On narrow terminals later roles are dropped before the frame wraps. */
static void cli_banner_legend(const char *g)
{
    static const struct {
        cli_theme_t color; /* 主题 token（CLR_* 宏为运行时函数，不可作静态初始化） */
        const char *name;
        const char *label;
    } roles[] = {
        {CLI_TH_CYAN, "For Thee", "你"},
        {CLI_TH_GREEN, "Super Agent", "agentrt"},
        {CLI_TH_YELLOW, "Dual Think", "思考"},
        {CLI_TH_MAGENTA, "Sub Agent", "执行体"},
    };

    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");
    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        size_t seg = 2 + cli_disp_width(roles[i].name) +
                     1 + cli_disp_width(roles[i].label);
        if (i > 0)
            seg += 3;
        if (used + seg > budget)
            break; /* narrow terminal: drop the remaining roles */
        if (i > 0) {
            cli_out("   ");
            used += 3;
        }
        cli_out("[");
        cli_out(cli_c(cli_theme_seq(roles[i].color)));
        cli_out(roles[i].name);
        cli_out(cli_c(CLR_RESET));
        cli_out("]");
        used += 2 + cli_disp_width(roles[i].name);
        cli_outf(" %s", roles[i].label);
        used += 1 + cli_disp_width(roles[i].label);
    }
    cli_out(cli_c(CLR_RESET));
    cli_hero_line_end(used, w);
}

/* Emit one "key → model" segment for the model row. The model name is
 * clipped to the remaining budget (UTF-8 safe) so a narrow terminal keeps
 * the frame intact. Returns 0 when not even the key + arrow fit (stop the
 * row). */
static int cli_hero_model_seg(const char *key, const char *model,
                              size_t *used, size_t budget, int lead_sep)
{
    size_t sep = lead_sep ? 3 : 0;
    size_t key_w = cli_disp_width(key);
    size_t model_w = cli_disp_width(model);
    if (*used + sep + key_w + 3 + model_w <= budget) {
        if (lead_sep) {
            cli_out("   ");
            *used += 3;
        }
        cli_out(cli_c(CLR_CYAN));
        cli_out(key);
        cli_out(cli_c(CLR_RESET));
        *used += key_w;
        cli_out(cli_c(CLR_DIM));
        cli_out(" → ");
        cli_out(cli_c(CLR_RESET));
        *used += 3;
        cli_out(cli_c(CLR_YELLOW));
        cli_out(model);
        cli_out(cli_c(CLR_RESET));
        *used += model_w;
        return 1;
    }
    size_t left = (*used + sep + key_w + 3 <= budget)
                      ? budget - *used - sep - key_w - 3
                      : 0;
    if (left < 2)
        return 0;
    char buf[64];
    size_t mw = cli_hero_clip(model, left, buf, sizeof(buf));
    if (lead_sep) {
        cli_out("   ");
        *used += 3;
    }
    cli_out(cli_c(CLR_CYAN));
    cli_out(key);
    cli_out(cli_c(CLR_RESET));
    *used += key_w;
    cli_out(cli_c(CLR_DIM));
    cli_out(" → ");
    cli_out(cli_c(CLR_RESET));
    *used += 3;
    cli_out(cli_c(CLR_YELLOW));
    cli_out(buf);
    cli_out(cli_c(CLR_RESET));
    *used += mw;
    return 1;
}

/* One compact model-config row: the three GRAD roles (A·t2 generator,
 * B·t1-f arbiter, C·t1-p verifier). Empty env/yaml values fall back to
 * the provider default ("默认"). */
static void cli_model_line(const char *g, const char *t2, const char *t1f,
                           const char *t1p)
{
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";

    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");

    static const char *const keys[] = {"A·t2", "B·t1-f", "C·t1-p"};
    const char *models[] = {a, b, c};
    for (size_t i = 0; i < 3; i++) {
        if (!cli_hero_model_seg(keys[i], models[i], &used, budget, i > 0))
            break;
    }
    cli_hero_line_end(used, w);
}

/* In-frame footer row: command hints + the project motto. Lives inside the
 * frame so the whole system header reads as one pinned block above the
 * dialogue. */
static void cli_hero_footer(const char *g)
{
    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");

    cli_out(cli_c(CLR_DIM));
    cli_out("? ");
    cli_out(cli_c(CLR_RESET));
    used += 2;
    cli_out(cli_c(CLR_YELLOW));
    cli_out("/help");
    cli_out(cli_c(CLR_RESET));
    used += 5;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 查看命令 · ");
    cli_out(cli_c(CLR_RESET));
    used += cli_disp_width(" 查看命令 · ");
    cli_out(cli_c(CLR_YELLOW));
    cli_out("quit");
    cli_out(cli_c(CLR_RESET));
    used += 4;
    cli_out(cli_c(CLR_DIM));
    cli_out("/exit");
    cli_out(cli_c(CLR_RESET));
    used += 5;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 退出");
    cli_out(cli_c(CLR_RESET));
    used += 1 + cli_disp_width("退出");

    /* The motto is clipped to whatever room remains (narrow terminals). */
    size_t left = budget - used;
    if (left >= 2) {
        char buf[64];
        static const char *motto = "  \"Agents, To the open air.\"";
        size_t mw = cli_hero_clip(motto, left, buf, sizeof(buf));
        cli_out(cli_c(CLR_DIM));
        cli_out(buf);
        cli_out(cli_c(CLR_RESET));
        used += mw;
    }
    cli_hero_line_end(used, w);
}

/* ---- unified blue-framed system header ----
 *
 * Title edge + version/capabilities + role legend + model config + footer
 * hints, all enclosed in a blue frame (6 pinned lines including the two
 * edges). The whole block is pinned so conversation output scrolls below
 * it (Terminal feedback: "system header must stay fixed"); the frame marks
 * the boundary between the system header and the dialogue.
 */
void cli_print_system_header(const char *t2, const char *t1f, const char *t1p)
{
    /* One-shot server mode (-p): no startup hero/panel, output is the
     * single-turn result only (Claude Code -p / Codex exec convention). */
    if (g_cli_print_mode)
        return;

    const char *g = cli_gutter_pad(2);
    cli_hero_brand(g);
    cli_hero_capabilities(g);
    cli_banner_legend(g);
    cli_model_line(g, t2, t1f, t1p);
    cli_hero_footer(g);
    cli_hero_frame_bottom(g);

    /* Full-screen TUI page pins its own header boundary (history-based)
     * after this; non-TTY output just scrolls. Only interactive plain
     * TTYs pin the 6-line block; the bottom two rows are reserved as the
     * fixed input zone (three-zone layout: hero / dialogue / input with a
     * dim separator line between dialogue and input).
     *
     * 注意：TUI active 时 hero 仍要打印——它会被重定向进 TUI 历史
     * （cli_render_set_tui 已切换渲染目标），供全屏页面 pin 头部；
     * 此处只跳过行渲染模式的 scroll-region pin。 */
    if (!cli_term_is_tty())
        return;
    if (!cli_tui_active(cli_tui_get_default()))
        cli_term_header_pin(CLI_HDR_LINES, 2);
    fflush(stdout);
}
