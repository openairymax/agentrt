// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_markdown.c
 * @brief Markdown subset parser and tool-use rendering.
 *
 * Extracted from cli_render.c.  Owns the inline markdown emitter
 * (bold/emph/code/link), the block-level markdown renderer (headings,
 * lists, checkboxes, quotes, code blocks, pipe tables), and the tool
 * invocation/result card renderer.
 */

#include "cli_render.h"
#include "cli_tui.h"

#include "airy_memory.h"

extern int g_cli_print_mode;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- inline markdown: **bold**, *emph*, `code` ---- */

static int cli_emph_has_letter(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        unsigned char u = (unsigned char)s[i];
        if (u >= 0x80 || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z'))
            return 1;
    }
    return 0;
}

static int cli_emph_is_letter(char c)
{
    unsigned char u = (unsigned char)c;
    if (u >= 0x80)
        return 1;
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
}

static int s_inline_bold_open = 0;

static void cli_emit_inline(const char *text)
{
    const char *p = text;

    if (s_inline_bold_open) {
        const char *close = strstr(text, "**");
        if (close) {
            cli_outn(text, (size_t)(close - text));
            cli_out(cli_c(CLR_RESET));
            s_inline_bold_open = 0;
            p = close + 2;
        } else {
            cli_out(text);
            return;
        }
    }

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
            if (cli_emph_has_letter(p + 2, strlen(p + 2))) {
                cli_out(cli_c(CLR_BOLD));
                cli_outn(p + 2, strlen(p + 2));
                s_inline_bold_open = 1;
                return;
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
        if (p[0] == '[') {
            const char *close_bracket = strchr(p + 1, ']');
            if (close_bracket && close_bracket[1] == '(') {
                const char *close_paren = strchr(close_bracket + 2, ')');
                if (close_paren) {
                    size_t label_len = (size_t)(close_bracket - (p + 1));
                    if (label_len > 0) {
                        cli_out(cli_c(CLR_CYAN));
                        cli_out(cli_c(CLR_UNDERLINE));
                        cli_outn(p + 1, label_len);
                        cli_out(cli_c(CLR_RESET));
                        if (g_cli_print_mode) {
                            size_t url_len = (size_t)(close_paren - (close_bracket + 2));
                            cli_out(cli_c(CLR_DIM));
                            cli_out(" (");
                            cli_outn(close_bracket + 2, url_len);
                            cli_out(")");
                            cli_out(cli_c(CLR_RESET));
                        }
                        p = close_paren + 1;
                        continue;
                    }
                }
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

/* ---- pipe table row ---- */

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
        if (p[0] == ' ') { p++; len--; }
        if (len > 0 && p[len - 1] == ' ') { len--; }
    }
    if (len < 3 || p[0] != '|' || p[len - 1] != '|')
        return 0;

    const char *s = p + 1;
    const char *end = p + len - 1;
    for (;;) {
        const char *bar = NULL;
        for (const char *q = s; q < end; q++) {
            if (*q == '|') { bar = q; break; }
        }
        size_t clen = bar ? (size_t)(bar - s) : (size_t)(end - s);
        while (clen > 0 && s[0] == ' ') s++, clen--;
        while (clen > 0 && s[clen - 1] == ' ') clen--;
        char *cell = (char *)AIRY_MALLOC(clen + 1);
        if (!cell) return 0;
        AIRY_MEMCPY(cell, s, clen);
        cell[clen] = '\0';
        if (row->cell_count < CLI_TABLE_MAX_CELLS)
            row->cells[row->cell_count++] = cell;
        else
            AIRY_FREE(cell);
        if (!bar) break;
        s = bar + 1;
    }
    return row->cell_count > 0;
}

static int cli_table_sep(const cli_table_row_t *row)
{
    for (size_t i = 0; i < row->cell_count; i++) {
        const char *c = row->cells[i];
        if (!c || c[0] == '\0') continue;
        size_t j = 0;
        while (c[j] == '-' || c[j] == ':' || c[j] == ' ') j++;
        if (c[j] != '\0') return 0;
    }
    return 1;
}

static void cli_table_flush(cli_table_row_t *table, size_t *table_rows, const char *g)
{
    size_t rows = *table_rows;
    if (rows == 0) return;

    size_t cols = 0;
    for (size_t t = 0; t < rows; t++) {
        if (table[t].cell_count > cols)
            cols = table[t].cell_count;
    }
    size_t *widths = (size_t *)AIRY_CALLOC(cols ? cols : 1, sizeof(size_t));
    if (widths) {
        for (size_t t = 0; t < rows; t++) {
            for (size_t c = 0; c < table[t].cell_count; c++) {
                size_t cl = cli_disp_width(table[t].cells[c]);
                if (cl > widths[c]) widths[c] = cl;
            }
        }
        for (size_t t = 0; t < rows; t++) {
            if (cli_table_sep(&table[t])) continue;
            cli_out(g);
            for (size_t c = 0; c < table[t].cell_count; c++) {
                const char *cell = table[t].cells[c];
                cli_out(cli_c(CLR_DIM));
                cli_out("│ ");
                cli_out(cli_c(CLR_RESET));
                if (t == 0) cli_out(cli_c(CLR_BOLD));
                cli_out(cell);
                if (t == 0) cli_out(cli_c(CLR_RESET));
                size_t pad = widths[c] - cli_disp_width(cell);
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
    if (!text) return;
    const char *g = cli_gutter(indent);
    const char *p = text;
    int in_code = 0;
    int in_table = 0;
    cli_table_row_t table[CLI_TABLE_MAX_COLS];
    size_t table_rows = 0;
    s_inline_bold_open = 0;

    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        char *line = (char *)AIRY_MALLOC(len + 1);
        if (!line) return;
        AIRY_MEMCPY(line, p, len);
        line[len] = '\0';
        p = nl ? nl + 1 : NULL;

        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] == '\r')
            line[--llen] = '\0';

        if (strncmp(line, "```", 3) == 0 || strncmp(line, "[code]", 6) == 0 ||
            strncmp(line, "[/code]", 7) == 0) {
            if (in_table) { cli_table_flush(table, &table_rows, g); in_table = 0; }
            in_code = !in_code;
            cli_out(g);
            if (g_cli_print_mode) {
                cli_out("```");
            } else {
                cli_out(cli_c(CLR_DIM));
                cli_out(in_code ? "[code]" : "[/code]");
                cli_out(cli_c(CLR_RESET));
            }
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

        size_t s = 0;
        while (s < llen && (line[s] == ' ' || line[s] == '\t')) s++;
        if (s == llen) {
            if (in_table) { cli_table_flush(table, &table_rows, g); in_table = 0; }
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }
        const char *content = line + s;

        if (content[0] == '|') {
            cli_table_row_t row;
            if (cli_table_parse_row(content, &row)) {
                if (!in_table && cli_table_sep(&row)) {
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
        if (in_table) { cli_table_flush(table, &table_rows, g); in_table = 0; }

        if (content[0] == '#') {
            int level = 0;
            while (content[level] == '#') level++;
            if (level <= 4 && (content[level] == ' ' || content[level] == '\0')) {
                const char *h = content[level] ? content + level + 1 : content + level;
                cli_out(g);
                cli_out(cli_c(CLR_BOLD));
                if (level == 1) cli_out(cli_c(CLR_UNDERLINE));
                cli_out(h);
                cli_out(cli_c(CLR_RESET));
                cli_outc('\n');
                AIRY_FREE(line);
                continue;
            }
        }

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

        if (cli_checkbox(content)) {
            char mark = content[3];
            const char *rest = content + 5;
            if (rest[0] == ' ') rest++;
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

        {
            const char *d = content;
            while (*d >= '0' && *d <= '9') d++;
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

        if (strncmp(content, "---", 3) == 0) {
            cli_out(g);
            cli_out(cli_c(CLR_DIM));
            cli_out("────────────────────────────");
            cli_out(cli_c(CLR_RESET));
            cli_outc('\n');
            AIRY_FREE(line);
            continue;
        }

        cli_out(g);
        cli_emit_inline(line + s);
        cli_outc('\n');
        AIRY_FREE(line);
    }
    if (in_table) { cli_table_flush(table, &table_rows, g); in_table = 0; }
}

/* ---- tool invocation / result ---- */

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
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].tool) == 0)
            return map[i].action;
    }
    return name;
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
