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

/* Frame width: v2 设计固定为 60 列，不再随终端宽度浮动——浮宽依赖每次
 * 终端查询与字符宽度计算，任一误差（CJK 渲染差异/字体连字）都会让竖线
 * 错位。固定宽保证任何终端渲染一致。终端不足时返回 0：调用方降级为
 * 无框纯文本（同样 6 行，CLI_HDR_LINES 不变），绝不错位。 */
#define CLI_HERO_FRAME_W 60u
static size_t cli_hero_frame_w(void)
{
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (cols <= 0)
        return CLI_HERO_FRAME_W; /* 宽度未知（非 TTY 重定向等）：按固定宽 */
    return (cols >= (int)CLI_HERO_FRAME_W + 6) ? CLI_HERO_FRAME_W : 0;
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

/* Top edge with the brand + version: ┌─ Airymax AgentRT · v0.1.6f ─┐
 * v2：品牌与版本合入顶框一行（原 4 行内容不变，行数仍为 6，CLI_HDR_LINES
 * 兼容）；窄终端（w==0）降级为无框纯文本，同样 6 行（含结尾空行）。 */
static void cli_hero_brand(const char *g)
{
    size_t w = cli_hero_frame_w();
    char title[96];
    snprintf(title, sizeof(title), "Airymax AgentRT · v%s", AIRY_CLI_VERSION);
    if (!w) {
        cli_out(g);
        cli_out(cli_c(CLR_BOLD));
        cli_out(cli_c(CLR_CYAN));
        cli_out("◆ ");
        cli_out(cli_c(CLR_RESET));
        cli_out("Airymax AgentRT");
        cli_out(cli_c(CLR_DIM));
        cli_outf(" · v%s", AIRY_CLI_VERSION);
        cli_out(cli_c(CLR_RESET));
        cli_outc('\n');
        return;
    }
    size_t used = 3; /* "┌─ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("┌─ ");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_BOLD));
    cli_out(cli_c(CLR_CYAN));
    char buf[96];
    /* 留一个 "─" 到 "┐"，顶框视觉收束 */
    size_t tmax = cli_hero_content_max(w);
    if (tmax > 1)
        tmax -= 1;
    size_t tw = cli_hero_clip(title, tmax, buf, sizeof(buf));
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

/* One quiet capabilities row: what the runtime does at a glance. v2 精简
 * 文本并去掉行首冗余空格（原 " 版本 v..." 双空格 + 版本已上移顶框）。 */
static void cli_hero_capabilities(const char *g)
{
    static const char *text = "对话 · 任务 · 蓝图调度 · 双思考 · GCCP · 工具执行";
    size_t w = cli_hero_frame_w();
    if (!w) {
        cli_out(g);
        cli_out(cli_c(CLR_DIM));
        cli_out(text);
        cli_out(cli_c(CLR_RESET));
        cli_outc('\n');
        return;
    }
    size_t budget = cli_hero_content_max(w);
    char out[96];
    size_t tw = cli_hero_clip(text, budget, out, sizeof(out));
    size_t used = 2 + tw; /* "│ " + content */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│ ");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out(out);
    cli_out(cli_c(CLR_RESET));
    cli_hero_line_end(used, w);
}

/* Four-role conversation legend. v2：去掉长英文名方括号（[For Thee]
 * [Super Agent]...），改短标签 + 角色色 + "·" 分隔——窄终端下信息保留率
 * 高（原版 60 列只剩一个角色），且视觉更轻。 */
static void cli_hero_roles(const char *g)
{
    static const struct {
        cli_theme_t color; /* 主题 token（CLR_* 宏为运行时函数，不可作静态初始化） */
        const char *name;
    } roles[] = {
        {CLI_TH_CYAN, "你"},
        {CLI_TH_GREEN, "agentrt"},
        {CLI_TH_YELLOW, "思考"},
        {CLI_TH_MAGENTA, "执行体"},
    };
    size_t w = cli_hero_frame_w();
    if (!w) {
        cli_out(g);
        for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
            if (i > 0) {
                cli_out(cli_c(CLR_DIM));
                cli_out(" · ");
                cli_out(cli_c(CLR_RESET));
            }
            cli_out(cli_c(cli_theme_seq(roles[i].color)));
            cli_out(roles[i].name);
            cli_out(cli_c(CLR_RESET));
        }
        cli_outc('\n');
        return;
    }
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│ ");
    cli_out(cli_c(CLR_RESET));
    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        size_t seg = cli_disp_width(roles[i].name);
        if (i > 0)
            seg += 3; /* " · " */
        if (used + seg > budget)
            break; /* 窄框：丢弃剩余角色 */
        if (i > 0) {
            cli_out(cli_c(CLR_DIM));
            cli_out(" · ");
            cli_out(cli_c(CLR_RESET));
            used += 3;
        }
        cli_out(cli_c(cli_theme_seq(roles[i].color)));
        cli_out(roles[i].name);
        cli_out(cli_c(CLR_RESET));
        used += cli_disp_width(roles[i].name);
    }
    cli_hero_line_end(used, w);
}

/* Emit one "key=model" segment for the model row. v2：连接符由 " → "
 * 改 "="，间隔 2 空格——同语义下更紧凑，60 列固定框内三槽必然放下。 */
static int cli_hero_model_seg(const char *key, const char *model,
                              size_t *used, size_t budget, int lead_sep)
{
    size_t sep = lead_sep ? 2 : 0;
    size_t key_w = cli_disp_width(key);
    size_t model_w = cli_disp_width(model);
    if (*used + sep + key_w + 1 + model_w > budget)
        return 0;
    if (lead_sep) {
        cli_out("  ");
        *used += 2;
    }
    cli_out(cli_c(CLR_CYAN));
    cli_out(key);
    cli_out(cli_c(CLR_RESET));
    *used += key_w;
    cli_out(cli_c(CLR_DIM));
    cli_out("=");
    cli_out(cli_c(CLR_RESET));
    *used += 1;
    cli_out(cli_c(CLR_YELLOW));
    cli_out(model);
    cli_out(cli_c(CLR_RESET));
    *used += model_w;
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

    static const char *const keys[] = {"A·t2", "B·t1-f", "C·t1-p"};
    const char *models[] = {a, b, c};
    size_t w = cli_hero_frame_w();
    if (!w) {
        cli_out(g);
        for (size_t i = 0; i < 3; i++) {
            if (i > 0) {
                cli_out("  ");
            }
            cli_out(cli_c(CLR_CYAN));
            cli_out(keys[i]);
            cli_out(cli_c(CLR_RESET));
            cli_out(cli_c(CLR_DIM));
            cli_out("=");
            cli_out(cli_c(CLR_RESET));
            cli_out(cli_c(CLR_YELLOW));
            cli_out(models[i]);
            cli_out(cli_c(CLR_RESET));
        }
        cli_outc('\n');
        return;
    }
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│ ");
    cli_out(cli_c(CLR_RESET));
    for (size_t i = 0; i < 3; i++) {
        if (!cli_hero_model_seg(keys[i], models[i], &used, budget, i > 0))
            break;
    }
    cli_hero_line_end(used, w);
}

/* In-frame footer row: command hints + the project motto. v2：精简措辞，
 * motto 短版（"Agents, To the open air." 无引号包裹改直引），窄框 clip。 */
static void cli_hero_footer(const char *g)
{
    size_t w = cli_hero_frame_w();
    static const char *motto = "· \"Agents, To the open air.\"";
    if (!w) {
        cli_out(g);
        cli_out(cli_c(CLR_DIM));
        cli_out("? ");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_YELLOW));
        cli_out("/help");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_DIM));
        cli_out(" 帮助 · ");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_YELLOW));
        cli_out("quit");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_DIM));
        cli_out(" 退出 ");
        cli_out(cli_c(CLR_RESET));
        cli_out(cli_c(CLR_DIM));
        cli_out(motto);
        cli_out(cli_c(CLR_RESET));
        cli_outc('\n');
        return;
    }
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│ ");
    cli_out(cli_c(CLR_RESET));

    cli_out(cli_c(CLR_DIM));
    cli_out("? ");
    cli_out(cli_c(CLR_RESET));
    used += 2;
    cli_out(cli_c(CLR_YELLOW));
    cli_out("/help");
    cli_out(cli_c(CLR_RESET));
    used += 5;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 帮助 · ");
    cli_out(cli_c(CLR_RESET));
    used += cli_disp_width(" 帮助 · ");
    cli_out(cli_c(CLR_YELLOW));
    cli_out("quit");
    cli_out(cli_c(CLR_RESET));
    used += 4;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 退出 ");
    cli_out(cli_c(CLR_RESET));
    used += cli_disp_width(" 退出 ");

    /* The motto is clipped to whatever room remains (narrow frames). */
    size_t left = budget - used;
    if (left >= 2) {
        char buf[64];
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
 * v2：固定 60 列蓝框（终端过窄自动降级无框纯文本），品牌+版本入顶框，
 * 角色短标签，模型 "=" 紧凑格式。共 6 行（含顶/底框或结尾空行），
 * 与 CLI_HDR_LINES=6 严格对应，pin 永不偏移。
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
    cli_hero_roles(g);
    cli_model_line(g, t2, t1f, t1p);
    cli_hero_footer(g);
    if (cli_hero_frame_w())
        cli_hero_frame_bottom(g);
    else
        cli_outc('\n'); /* 无框降级：空行补齐第 6 行，pin 行数不变 */

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
