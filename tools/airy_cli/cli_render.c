// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_render.c
 * @brief airy_cli rendering layer: roles, colors, markdown and status display.
 *
 * A stream-safe, line-oriented renderer. No TTY capture and no curses: every
 * function prints directly to stdout so the CLI behaves identically on a
 * local terminal, over SSH or when output is piped to a log file. When
 * stdout is not a TTY (or NO_COLOR is set) all color sequences are dropped
 * and the animated status line degrades to a static line.
 *
 * The markdown subset is deliberately small (headings, lists, GFM task
 * checkboxes, quotes, code blocks, bold/emph/code, pipe tables degraded to
 * key/value rows) — enough to read agent output comfortably without pulling
 * in a full parser.
 */

#include "cli_render.h"
#include "cli_tui.h"

#include "airy_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define CLI_GUTTER_MAX 64
#define CLI_DEFAULT_WIDTH 100

/* Attached full-screen TUI engine (NULL = plain stdout streaming). Set by
 * cli_render_set_tui before the header renders; every cli_out*() call below
 * routes through it so output lands in the TUI line history. */
static struct cli_tui_s *g_cli_tui;

void cli_render_set_tui(struct cli_tui_s *tui)
{
    g_cli_tui = tui;
}

void cli_out(const char *s)
{
    if (!s)
        return;
    cli_outn(s, strlen(s));
}

void cli_outn(const char *s, size_t n)
{
    if (!s || n == 0)
        return;
    if (g_cli_tui && cli_tui_active(g_cli_tui))
        cli_tui_emit(g_cli_tui, s, n);
    else
        fwrite(s, 1, n, stdout);
}

void cli_outc(char c)
{
    if (g_cli_tui && cli_tui_active(g_cli_tui))
        cli_tui_emit(g_cli_tui, &c, 1);
    else
        fputc(c, stdout);
}

void cli_outf(const char *fmt, ...)
{
    if (!fmt)
        return;
    va_list ap;
    va_start(ap, fmt);
    char stack_buf[1024];
    int need = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (need < 0)
        return;
    if ((size_t)need < sizeof(stack_buf)) {
        cli_outn(stack_buf, (size_t)need);
        return;
    }
    char *big = (char *)AIRY_MALLOC((size_t)need + 1);
    if (!big)
        return;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)need + 1, fmt, ap);
    va_end(ap);
    cli_outn(big, (size_t)need);
    AIRY_FREE(big);
}

/* Runtime color gate (shared): every ANSI sequence flows through cli_c() so
 * NO_COLOR / piped output produces clean monochrome text (server-grade).
 * Defined here, declared in cli_render.h, used across all CLI translation
 * units — one gate, no per-file duplicates. */
const char *cli_c(const char *seq)
{
    return cli_color_enabled() ? seq : "";
}

static const char *cli_gutter(size_t indent)
{
    static char buf[CLI_GUTTER_MAX + 1];
    size_t n = indent > CLI_GUTTER_MAX ? CLI_GUTTER_MAX : indent;
    for (size_t i = 0; i < n; i++)
        buf[i] = ' ';
    buf[n] = '\0';
    return buf;
}

const char *cli_gutter_pad(size_t indent)
{
    return cli_gutter(indent);
}

/* Terminal display width of a UTF-8 string. CJK ideographs and full-width
 * symbols occupy two cells; ASCII, box drawing and "·" occupy one. Shared
 * with the banner so all columns account for wide glyphs. */
size_t cli_disp_width(const char *s)
{
    size_t w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        if (*p < 0x80) { /* ASCII: 1 cell */
            w += 1;
            p += 1;
        } else if ((*p & 0xE0) == 0xC0) { /* 2-byte (e.g. "·"): 1 cell */
            w += 1;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) { /* 3-byte */
            w += (*p == 0xE2) ? 1 : 2;     /* 0xE2: box drawing / arrows */
            p += 3;
        } else { /* 4-byte: 2 cells */
            w += 2;
            p += 4;
        }
    }
    return w;
}

const char *cli_render_role_color(cli_role_t role)
{
    if (!cli_color_enabled())
        return "";
    switch (role) {
    case CLI_ROLE_USER:
        return CLR_CYAN;
    case CLI_ROLE_SUPER_AGENT:
        return CLR_GREEN;
    case CLI_ROLE_SUPER_THINK:
        return CLR_YELLOW;
    case CLI_ROLE_SUB_AGENT:
        return CLR_MAGENTA;
    case CLI_ROLE_STATUS:
        return CLR_DIM;
    case CLI_ROLE_TRACE:
        return CLR_DIM;
    case CLI_ROLE_ERROR:
        return CLR_RED;
    default:
        return CLR_RESET;
    }
}

const char *cli_render_actor_name(cli_actor_t actor)
{
    switch (actor) {
    case CLI_ACTOR_USER:
        return "For Thee";
    case CLI_ACTOR_SUPER_AGENT:
        return "Super Agent";
    case CLI_ACTOR_SUPER_THINK:
        return "Super Think";
    case CLI_ACTOR_SUB_AGENT:
        return "Sub Agent";
    default:
        return "AgentRT";
    }
}

/* ---- inline markdown: **bold**, *emph*, `code` ---- */

/* Emphasis markers only bind when the enclosed text contains at least one
 * ASCII letter, the opener is immediately followed by a letter and the
 * closer is immediately preceded by a letter.  This keeps math like
 * "6*7=42" / "cross-check with 7*6" intact (CommonMark would otherwise
 * pair the stray asterisks as emphasis) while *real* emphasis like *step*
 * still renders bold. */
static int cli_emph_has_letter(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))
            return 1;
    }
    return 0;
}

static int cli_emph_is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void cli_emit_inline(const char *text)
{
    const char *p = text;
    while (*p) {
        if (p[0] == '*' && p[1] == '*') {
            const char *e = strstr(p + 2, "**");
            if (e && cli_emph_has_letter(p + 2, (size_t)(e - (p + 2)))) {
                cli_out(cli_c(CLR_BOLD));
                cli_outn(p + 2, (size_t)(e - (p + 2)));
                cli_out(cli_c(CLR_RESET));
                p = e + 2;
                continue;
            }
        }
        if (p[0] == '*' && cli_emph_is_letter(p[1])) {
            const char *e = strchr(p + 1, '*');
            if (e && e > p + 1 && cli_emph_is_letter(e[-1]) &&
                cli_emph_has_letter(p + 1, (size_t)(e - (p + 1)))) {
                cli_out(cli_c(CLR_BOLD));
                cli_outn(p + 1, (size_t)(e - (p + 1)));
                cli_out(cli_c(CLR_RESET));
                p = e + 1;
                continue;
            }
        }
        if (p[0] == '`') {
            const char *e = strchr(p + 1, '`');
            if (e) {
                cli_out(cli_c(CLR_BG_GRAY));
                cli_outn(p + 1, (size_t)(e - (p + 1)));
                cli_out(cli_c(CLR_RESET));
                p = e + 1;
                continue;
            }
        }
        cli_outc(*p);
        p++;
    }
}

/* ---- GFM task checkbox: "- [ ]" / "- [x]" ---- */

static int cli_checkbox(const char *content)
{
    if (strncmp(content, "- [", 3) != 0 && strncmp(content, "* [", 3) != 0)
        return 0;
    char mark = content[3];
    if (mark != ' ' && mark != 'x' && mark != 'X')
        return 0;
    if (content[4] != ']')
        return 0;
    return 1;
}

/* ---- pipe table row: split on unescaped '|' into cell strings ---- */

#define CLI_TABLE_MAX_CELLS 16
#define CLI_TABLE_MAX_COLS 8

typedef struct {
    char *cells[CLI_TABLE_MAX_CELLS];
    size_t cell_count;
} cli_table_row_t;

static void cli_table_row_free(cli_table_row_t *r)
{
    for (size_t i = 0; i < r->cell_count; i++)
        AIRY_FREE(r->cells[i]);
    r->cell_count = 0;
}

static int cli_table_parse_row(const char *line, cli_table_row_t *row)
{
    AIRY_MEMSET(row, 0, sizeof(*row));
    const char *p = line;
    size_t len = strlen(line);
    while (len > 0 && (p[0] == ' ' || p[len - 1] == ' ')) {
        if (p[0] == ' ') {
            p++;
            len--;
        }
        if (len > 0 && p[len - 1] == ' ') {
            len--;
        }
    }
    if (len < 3 || p[0] != '|' || p[len - 1] != '|')
        return 0;

    const char *s = p + 1;
    const char *end = p + len - 1;
    for (;;) {
        const char *bar = NULL;
        for (const char *q = s; q < end; q++) {
            if (*q == '|') {
                bar = q;
                break;
            }
        }
        size_t clen = bar ? (size_t)(bar - s) : (size_t)(end - s);
        while (clen > 0 && s[0] == ' ')
            s++, clen--;
        while (clen > 0 && s[clen - 1] == ' ')
            clen--;
        char *cell = (char *)AIRY_MALLOC(clen + 1);
        if (!cell)
            return 0;
        AIRY_MEMCPY(cell, s, clen);
        cell[clen] = '\0';
        if (row->cell_count < CLI_TABLE_MAX_CELLS)
            row->cells[row->cell_count++] = cell;
        else
            AIRY_FREE(cell);
        if (!bar)
            break;
        s = bar + 1;
    }
    return row->cell_count > 0;
}

/* is a markdown pipe-table separator row (| --- | --- |)? */
static int cli_table_is_separator(const cli_table_row_t *row)
{
    for (size_t i = 0; i < row->cell_count; i++) {
        const char *c = row->cells[i];
        if (!c || c[0] == '\0')
            continue;
        size_t j = 0;
        while (c[j] == '-' || c[j] == ':' || c[j] == ' ')
            j++;
        if (c[j] != '\0')
            return 0;
    }
    return 1;
}

/* render + free a collected pipe table, then reset the collector state */
static void cli_table_flush(cli_table_row_t *table, size_t *table_rows, const char *g)
{
    size_t rows = *table_rows;
    if (rows == 0)
        return;

    size_t cols = 0;
    for (size_t t = 0; t < rows; t++) {
        if (table[t].cell_count > cols)
            cols = table[t].cell_count;
    }
    size_t *widths = (size_t *)AIRY_CALLOC(cols ? cols : 1, sizeof(size_t));
    if (widths) {
        for (size_t t = 0; t < rows; t++) {
            for (size_t c = 0; c < table[t].cell_count; c++) {
                size_t cl = strlen(table[t].cells[c]);
                if (cl > widths[c])
                    widths[c] = cl;
            }
        }
        for (size_t t = 0; t < rows; t++) {
            if (cli_table_is_separator(&table[t]))
                continue;
            cli_out(g);
            for (size_t c = 0; c < table[t].cell_count; c++) {
                const char *cell = table[t].cells[c];
                cli_out(cli_c(CLR_DIM));
                cli_out("│ ");
                cli_out(cli_c(CLR_RESET));
                if (t == 0)
                    cli_out(cli_c(CLR_BOLD));
                cli_out(cell);
                if (t == 0)
                    cli_out(cli_c(CLR_RESET));
                size_t pad = widths[c] - strlen(cell);
                for (size_t q = 0; q < pad + 1; q++)
                    cli_outc(' ');
            }
            cli_outc('\n');
        }
        AIRY_FREE(widths);
    }
    for (size_t t = 0; t < rows; t++)
        cli_table_row_free(&table[t]);
    *table_rows = 0;
}

/* ---- line-oriented markdown block renderer ---- */

void cli_render_markdown(const char *text, size_t indent)
{
    if (!text)
        return;
    const char *g = cli_gutter(indent);
    const char *p = text;
    int in_code = 0;
    int in_table = 0;
    cli_table_row_t table[CLI_TABLE_MAX_COLS];
    size_t table_rows = 0;

    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        char *line = (char *)AIRY_MALLOC(len + 1);
        if (!line)
            return;
        AIRY_MEMCPY(line, p, len);
        line[len] = '\0';
        p = nl ? nl + 1 : NULL;

        /* strip a trailing \r (CRLF input) */
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\r')
            line[--llen] = '\0';

        /* fenced code block toggle */
        if (strncmp(line, "```", 3) == 0) {
            if (in_table) {
                cli_table_flush(table, &table_rows, g);
                in_table = 0;
            }
            in_code = !in_code;
            cli_out(g);
            cli_out(cli_c(CLR_DIM));
            cli_out(in_code ? "[code]" : "[/code]");
            cli_out(cli_c(CLR_RESET));
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }
        if (in_code) {
            cli_out(g);
            cli_out(cli_c(CLR_BG_GRAY));
            cli_out(line);
            cli_out(cli_c(CLR_RESET));
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        /* skip empty lines */
        size_t s = 0;
        while (s < llen && (line[s] == ' ' || line[s] == '\t'))
            s++;
        if (s == llen) {
            if (in_table) {
                cli_table_flush(table, &table_rows, g);
                in_table = 0;
            }
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }
        const char *content = line + s;

        /* pipe table: starts with '|' and the next non-empty line is a
         * separator -> collect rows and render aligned key/value pairs */
        if (content[0] == '|') {
            cli_table_row_t row;
            if (cli_table_parse_row(content, &row)) {
                if (!in_table && cli_table_is_separator(&row)) {
                    cli_table_row_free(&row);
                    AIRY_FREE(line);
                    continue;
                }
                if (table_rows < CLI_TABLE_MAX_COLS) {
                    table[table_rows++] = row;
                } else {
                    cli_table_row_free(&row);
                }
                in_table = 1;
                AIRY_FREE(line);
                continue;
            }
        }
        if (in_table) {
            /* render the collected table then fall through for this line */
            cli_table_flush(table, &table_rows, g);
            in_table = 0;
        }

        /* headings */
        if (content[0] == '#') {
            int level = 0;
            while (content[level] == '#')
                level++;
            if (level <= 4 && (content[level] == ' ' || content[level] == '\0')) {
                const char *h = content[level] ? content + level + 1 : content + level;
                cli_out(g);
                cli_out(cli_c(CLR_BOLD));
                if (level == 1)
                    cli_out(cli_c(CLR_UNDERLINE));
                cli_out(h);
                cli_out(cli_c(CLR_RESET));
                cli_outc('\n');
                AIRY_FREE(line);
                continue;
            }
        }

        /* blockquote */
        if (content[0] == '>') {
            cli_out(g);
            cli_out(cli_c(CLR_DIM));
            cli_out("│ ");
            cli_out(cli_c(CLR_RESET));
            cli_emit_inline(content + 1);
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        /* GFM task checkbox: "- [x] text" -> "✓ text" / "□ text" */
        if (cli_checkbox(content)) {
            char mark = content[3];
            const char *rest = content + 5;
            if (rest[0] == ' ')
                rest++;
            cli_out(g);
            if (mark == 'x' || mark == 'X') {
                cli_out(cli_c(CLR_GREEN));
                cli_out(CLI_ICON_DONE " ");
                cli_out(cli_c(CLR_RESET));
                cli_out(cli_c(CLR_DIM));
                cli_emit_inline(rest);
                cli_out(cli_c(CLR_RESET));
            } else {
                cli_out(cli_c(CLR_CYAN));
                cli_out(CLI_ICON_TODO " ");
                cli_out(cli_c(CLR_RESET));
                cli_emit_inline(rest);
            }
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        /* unordered list */
        if ((content[0] == '-' || content[0] == '*') &&
            (content[1] == ' ' || content[1] == '\0')) {
            cli_out(g);
            cli_out(cli_c(CLR_GREEN));
            cli_out(CLI_ICON_BULLET " ");
            cli_out(cli_c(CLR_RESET));
            cli_emit_inline(content + (content[1] == ' ' ? 2 : 1));
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        /* ordered list: "1. text" */
        {
            const char *d = content;
            while (*d >= '0' && *d <= '9')
                d++;
            if (d > content && *d == '.' && (d[1] == ' ' || d[1] == '\0')) {
                char num[16];
                size_t nlen = (size_t)(d - content);
                if (nlen < sizeof(num)) {
                    AIRY_MEMCPY(num, content, nlen);
                    num[nlen] = '\0';
                    cli_out(g);
                    cli_out(cli_c(CLR_GREEN));
                    cli_out(num);
                    cli_out(". ");
                    cli_out(cli_c(CLR_RESET));
                    cli_emit_inline(d + (d[1] == ' ' ? 2 : 1));
                    cli_outc('\n');
                    AIRY_FREE(line);
                    continue;
                }
            }
        }

        /* horizontal rule */
        if (strncmp(content, "---", 3) == 0) {
            cli_out(g);
            cli_out(cli_c(CLR_DIM));
            cli_out("────────────────────────────");
            cli_out(cli_c(CLR_RESET));
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        /* plain paragraph */
        cli_out(g);
        cli_emit_inline(line + s);
        cli_outc('\n');
        AIRY_FREE(line);
    }
    if (in_table) {
        cli_table_flush(table, &table_rows, g);
        in_table = 0;
    }
}

/* ---- progressive disclosure: long text collapsed to N lines ---- */

void cli_render_collapsed(const char *text, size_t indent, size_t max_lines, int weak)
{
    if (!text)
        return;
    if (max_lines < 1)
        max_lines = 1;

    /* Byte offset just past the max_lines-th newline (or the end of text). */
    const char *cut = text;
    size_t shown = 0;
    while (shown < max_lines && *cut) {
        const char *nl = strchr(cut, '\n');
        if (!nl) {
            cut = cut + strlen(cut);
            shown++;
            break;
        }
        cut = nl + 1;
        shown++;
    }

    /* Count remaining lines for the trailer. */
    size_t total = shown;
    if (*cut) {
        for (const char *q = cut; *q; q++)
            if (*q == '\n')
                total++;
        if (*cut)
            total++;
    }

    /* Internal traces (weak) render dim so they read as background context
     * and never compete with the actual reply. */
    if (weak)
        cli_out(cli_c(CLR_DIM));
    if (cut > text) {
        size_t plen = (size_t)(cut - text);
        char *prefix = (char *)AIRY_MALLOC(plen + 1);
        if (!prefix)
            return;
        AIRY_MEMCPY(prefix, text, plen);
        prefix[plen] = '\0';
        cli_render_markdown(prefix, indent);
        AIRY_FREE(prefix);
    }

    if (total > shown) {
        char trailer[96];
        snprintf(trailer, sizeof(trailer), "└ … %zu more lines (full text in logs)",
                 total - shown);
        const char *g = cli_gutter(indent);
        cli_out(g);
        cli_out(trailer);
        cli_outc('\n');
    }
    if (weak)
        cli_out(cli_c(CLR_RESET));
}

/* ---- role-tagged conversation line ---- */

/* Fixed role-header column so every conversation line starts its content at
 * the same column (Claude Code convention): "[For Thee]" and "[Sub xxx Agent]"
 * pad to the same width, keeping the chat column readable and left-aligned. */
#define CLI_ROLE_HDR_W 24

static void cli_pad_role_header(const char *hdr)
{
    size_t w = strlen(hdr);
    for (size_t i = w; i < CLI_ROLE_HDR_W; i++)
        cli_outc(' ');
}

/* Build a fixed-width role header, always keeping the closing "]" intact.
 * When the tag makes the header exceed CLI_ROLE_HDR_W, the tag is truncated
 * (not the bracket), so the gutter stays aligned and never renders a dangling
 * header like "[Sub Agent:tool.list_too". */
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
        tag_show = tag_budget;
    snprintf(out, cap, "[%s:%.*s]", name, (int)tag_show, tag);
}

void cli_render_role_line(cli_role_t role, cli_actor_t actor, const char *tag,
                          const char *content)
{
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
        cli_render_markdown(content, 2);
    else
        cli_outc('\n');
}

void cli_render_super_agent(const char *content)
{
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL, content);
}

void cli_render_super_agent_begin(void)
{
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(CLI_ROLE_SUPER_AGENT);
    const char *name = cli_render_actor_name(CLI_ACTOR_SUPER_AGENT);
    char hdr[CLI_ROLE_HDR_W + 1];

    cli_build_role_header(hdr, sizeof(hdr), name, NULL);

    cli_out(g);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
}

void cli_render_stream_fold_trailer(size_t more_lines)
{
    if (more_lines == 0)
        return;
    char trailer[96];
    snprintf(trailer, sizeof(trailer), "└ … %zu more lines (full text in logs)",
             more_lines);
    const char *g = cli_gutter(2 + CLI_ROLE_HDR_W);
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    cli_out(trailer);
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

void cli_render_user_message(const char *content)
{
    const char *g = cli_gutter(2);
    const char *col = cli_render_role_color(CLI_ROLE_USER);
    const char *bg = cli_c(CLR_BG_GRAY);
    const char *name = cli_render_actor_name(CLI_ACTOR_USER);
    char hdr[CLI_ROLE_HDR_W + 1];

    snprintf(hdr, sizeof(hdr), "[%s]", name);

    /* The user message renders as one visual block: a leading blank line, a
     * background-tinted header, the content also on the background, then a
     * trailing blank line (Codex user-message grouping). */
    cli_outc('\n');
    cli_out(g);
    cli_out(bg);
    cli_out(col);
    cli_out(hdr);
    cli_out(cli_c(CLR_RESET));
    cli_pad_role_header(hdr);
    cli_out(bg);
    cli_out(col);
    cli_out(CLI_ICON_USER " ");
    cli_out(cli_c(CLR_RESET));
    if (content && content[0]) {
        cli_out(bg);
        cli_out(content);
        cli_out(cli_c(CLR_RESET));
    }
    cli_outc('\n');
    cli_outc('\n');
}

void cli_render_sub_agent_line(cli_role_t role, const char *tag, const char *content)
{
    char hdr[CLI_ROLE_HDR_W + 1];
    const char *t = (tag && tag[0]) ? tag : "exec";

    /* "[Sub <tag> Agent]": fixed frame is "[Sub " (5) + " Agent]" (7) = 12;
     * truncate the tag to keep the closing " Agent]" intact at 24 cols. */
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
        cli_render_markdown(content, 2);
    else
        cli_outc('\n');
}

void cli_render_sub_agent(const char *tag, const char *content)
{
    cli_render_sub_agent_line(CLI_ROLE_SUB_AGENT, tag, content);
}

void cli_render_turn_separator(uint64_t elapsed_ms, const char *metrics)
{
    uint64_t secs = elapsed_ms / 1000;
    char label[128];
    if (secs < 1)
        snprintf(label, sizeof(label), "Worked");
    else if (secs < 60)
        snprintf(label, sizeof(label), "Worked for %llus", (unsigned long long)secs);
    else
        snprintf(label, sizeof(label), "Worked for %llum %02llus", (unsigned long long)(secs / 60),
                 (unsigned long long)(secs % 60));

    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    cli_out("──────────────── ");
    cli_out(cli_c(CLR_RESET));
    cli_out(label);
    if (metrics && metrics[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(" · ");
        cli_out(metrics);
        cli_out(cli_c(CLR_RESET));
    }
    cli_out(cli_c(CLR_DIM));
    cli_out(" ─────────────────");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

void cli_render_footer_hint(void)
{
    /* TTY only: keep piped / logged output free of UI chrome. The full-screen
     * TUI already carries this hint in its pinned header, so skip it there. */
    if (!cli_term_is_tty() || cli_tui_active(cli_tui_get_default()))
        return;
    const char *g = cli_gutter(2);
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    cli_out("? ");
    cli_out(cli_c(CLR_YELLOW));
    cli_out("/help");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out(" 查看命令 · ");
    cli_out(cli_c(CLR_YELLOW));
    cli_out("quit");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out("/exit");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out(" 退出");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* ---- progress bar ---- */

void cli_render_progress_bar(double progress, size_t width, const char *label)
{
    if (width < 4)
        width = 4;
    if (progress < 0.0)
        progress = 0.0;
    if (progress > 1.0)
        progress = 1.0;

    size_t filled = (size_t)(progress * (double)width);
    if (filled > width)
        filled = width;

    const char *g = cli_gutter(4);
    if (label && label[0]) {
        cli_out(g);
        cli_out(cli_c(CLR_DIM));
        cli_out(label);
        cli_out(cli_c(CLR_RESET));
        cli_out(": ");
    } else {
        cli_out(g);
    }

    cli_out("[");
    for (size_t i = 0; i < width; i++)
        cli_out(i < filled ? "█" : "░");
    cli_outf("] %3.0f%%\n", progress * 100.0);
}

/* ---- compact task line for the work-hall board ---- */

void cli_render_task_line(const char *tag, const char *id, const char *state,
                          double progress)
{
    if (progress < 0.0)
        progress = 0.0;
    if (progress > 1.0)
        progress = 1.0;

    size_t filled = (size_t)(progress * 10.0);
    if (filled > 10)
        filled = 10;
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
        st_col = cli_c(CLR_GREEN);
        st_icon = CLI_ICON_CHECK;
        bar_bright = 1;
    } else if (strcmp(st, "failed") == 0 || strcmp(st, "canceled") == 0) {
        st_col = cli_c(CLR_RED);
        st_icon = CLI_ICON_CROSS;
    } else if (strcmp(st, "running") == 0 || strcmp(st, "active") == 0 ||
               strcmp(st, "queued") == 0) {
        /* In-flight board lines are background trace (dim): the running state
         * is already visible through the spinner, a bright line per progress
         * change would flood the terminal. Completion stays green. */
        st_col = cli_c(CLR_DIM);
        st_icon = CLI_ICON_DIAMOND;
    } else if (strcmp(st, "pending") == 0) {
        st_col = cli_c(CLR_DIM);
        st_icon = CLI_ICON_TODO;
    }

    const char *g = cli_gutter(2);
    if (tag && tag[0])
        cli_outf("%s%s[%s]%s ", g, cli_c(CLR_DIM), tag, cli_c(CLR_RESET));
    cli_outf("%s%s %s%s%s ", st_col, st_icon, cli_c(CLR_RESET), cli_c(CLR_DIM),
           id ? id : "?");
    cli_outf("%s%s%s ", st_col, st, cli_c(CLR_RESET));
    cli_outf("%s[%s]%s %s%3.0f%%%s\n", cli_c(bar_bright ? CLR_GREEN : CLR_DIM), bar,
           cli_c(CLR_RESET), cli_c(bar_bright ? CLR_GREEN : CLR_DIM), progress * 100.0,
           cli_c(CLR_RESET));
}

/* ---- one-line status indicator (spinner) ---- */

static struct {
    int active;
    int degraded;
    char title[96];
    uint64_t start_ns;
    unsigned int frame;
    int printed;
} g_spinner;

static const char *const CLI_SPINNER_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏",
};
#define CLI_SPINNER_FRAMES_COUNT (sizeof(CLI_SPINNER_FRAMES) / sizeof(CLI_SPINNER_FRAMES[0]))

/* Claude Code amber: a long-running activity (>= 10s) turns the indicator
 * amber so the user still perceives work is in progress. */
#define CLI_SPINNER_AMBER_MS 10000
#define CLR_AMBER "\033[33;1m"

static uint64_t cli_time_ns(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) - 116444736000000000ULL) * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t cli_now_ms(void)
{
    return cli_time_ns() / 1000000ULL;
}

static void cli_spinner_erase(void)
{
    if (g_spinner.printed) {
        cli_out("\r\033[K");
        fflush(stdout);
    }
}

int cli_spinner_start(const char *title)
{
    AIRY_MEMSET(&g_spinner, 0, sizeof(g_spinner));
    if (!title || !title[0])
        return 0;
    AIRY_STRNCPY_TERM(g_spinner.title, title, sizeof(g_spinner.title));
    g_spinner.start_ns = cli_time_ns();

    if (!cli_term_is_tty() || cli_tui_active(cli_tui_get_default())) {
        /* Non-TTY, or full-screen TUI (which repaints itself): print a static
         * line and keep the elapsed text; no \r animation in either mode. */
        g_spinner.degraded = 1;
        g_spinner.active = 1;
        const char *g = cli_gutter(2);
        cli_outf("%s%s%s %s\n", g, cli_c(CLR_DIM), CLI_ICON_BULLET, title);
        return 0;
    }

    g_spinner.active = 1;
    g_spinner.frame = 0;
    cli_spinner_tick();
    return 1;
}

void cli_spinner_tick(void)
{
    if (!g_spinner.active || g_spinner.degraded)
        return;

    uint64_t elapsed = (cli_time_ns() - g_spinner.start_ns) / 1000000ULL;
    const char *frame = CLI_SPINNER_FRAMES[g_spinner.frame % CLI_SPINNER_FRAMES_COUNT];
    g_spinner.frame++;

    char elapsed_s[32];
    uint64_t secs = elapsed / 1000;
    if (secs < 1)
        snprintf(elapsed_s, sizeof(elapsed_s), "0s");
    else if (secs < 60)
        snprintf(elapsed_s, sizeof(elapsed_s), "%llus", (unsigned long long)secs);
    else
        snprintf(elapsed_s, sizeof(elapsed_s), "%llum %02llus", (unsigned long long)(secs / 60),
                 (unsigned long long)(secs % 60));

    const char *g = cli_gutter(2);
    cli_spinner_erase();
    cli_outf("%s%s%s %s%s %s(%s)%s", g,
           cli_c(elapsed >= CLI_SPINNER_AMBER_MS ? CLR_AMBER : CLR_YELLOW), frame,
           cli_c(CLR_RESET), g_spinner.title, cli_c(CLR_DIM), elapsed_s, cli_c(CLR_RESET));
    fflush(stdout);
    g_spinner.printed = 1;
}

void cli_spinner_pause(void)
{
    if (!g_spinner.active || g_spinner.degraded)
        return;
    cli_spinner_erase();
    g_spinner.printed = 0;
}

void cli_spinner_resume(void)
{
    if (!g_spinner.active || g_spinner.degraded)
        return;
    g_spinner.frame = 0;
    cli_spinner_tick();
}

void cli_spinner_stop(int ok, const char *detail)
{
    if (!g_spinner.active)
        return;

    uint64_t elapsed = (cli_time_ns() - g_spinner.start_ns) / 1000000ULL;
    uint64_t secs = elapsed / 1000;
    char elapsed_s[32];
    if (secs < 1)
        snprintf(elapsed_s, sizeof(elapsed_s), "0s");
    else if (secs < 60)
        snprintf(elapsed_s, sizeof(elapsed_s), "%llus", (unsigned long long)secs);
    else
        snprintf(elapsed_s, sizeof(elapsed_s), "%llum %02llus", (unsigned long long)(secs / 60),
                 (unsigned long long)(secs % 60));

    /* Both modes print the same completion line; in static (non-TTY) mode the
     * erase is a no-op because nothing was animated. */
    cli_spinner_erase();
    cli_outf("%s%s%s%s %s", cli_gutter(2), ok ? cli_c(CLR_GREEN) : cli_c(CLR_RED),
           ok ? CLI_ICON_CHECK : CLI_ICON_CROSS, cli_c(CLR_RESET), g_spinner.title);
    cli_outf(" %s(%s)%s", cli_c(CLR_DIM), elapsed_s, cli_c(CLR_RESET));
    if (detail && detail[0])
        cli_outf(" %s%s%s", cli_c(CLR_DIM), detail, cli_c(CLR_RESET));
    cli_outc('\n');
    fflush(stdout);
    AIRY_MEMSET(&g_spinner, 0, sizeof(g_spinner));
}

void cli_spinner_cancel(void)
{
    if (!g_spinner.active)
        return;
    cli_spinner_erase();
    AIRY_MEMSET(&g_spinner, 0, sizeof(g_spinner));
}
