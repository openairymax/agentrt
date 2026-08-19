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

/* One-shot server mode switch (defined in main.c): suppress status chrome
 * and role chrome so -p output stays clean (Claude Code -p convention). */
extern int g_cli_print_mode;

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

/* ---- reply folding meter (2026-08-17) ----
 *
 * 计量器挂接在 cli_out*() 输出路径上：begin 之后所有渲染输出（含
 * markdown 拆行）逐字节喂入 meter，得到精确的逻辑/物理行数，供 TTY
 * 折叠擦除重绘与流式空回复判定。ANSI 转义序列（颜色等）被跳过，不
 * 计入宽度。 */

static cli_line_meter_t *g_active_meter;

static void cli_meter_feed(cli_line_meter_t *m, const char *s, size_t n)
{
    if (!m)
        return;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1B) { /* ANSI escape: skip until a letter terminator */
            m->in_esc = 1;
            continue;
        }
        if (m->in_esc) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                m->in_esc = 0;
            continue;
        }
        if (c == '\n') {
            /* settle the current row: soft-wrap = floor(width / cols) extra */
            if (m->cols > 0 && m->row_len > 0) {
                size_t w = cli_disp_width(m->row);
                if (w > m->cols)
                    m->phys_lines += (w - 1) / m->cols;
            }
            m->phys_lines += 1;
            m->lines += 1;
            m->row_len = 0;
            continue;
        }
        if (c == '\r') {
            m->row_len = 0;
            continue;
        }
        if (c < 0x20)
            continue; /* other control chars never appear in rendered text */
        /* accumulate the row; width settled on '\n' / phys() */
        if (m->row_len + 1 >= m->row_cap) {
            size_t new_cap = m->row_cap ? m->row_cap * 2 : 256;
            char *grown = (char *)AIRY_REALLOC(m->row, new_cap);
            if (!grown)
                continue;
            m->row = grown;
            m->row_cap = new_cap;
        }
        m->row[m->row_len++] = (char)c;
        m->row[m->row_len] = '\0';
    }
}

void cli_render_meter_begin(cli_line_meter_t *m)
{
    if (!m)
        return;
    AIRY_MEMSET(m, 0, sizeof(*m));
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    m->cols = cols > 0 ? (size_t)cols : 0;
    m->active = 1;
    g_active_meter = m;
}

void cli_render_meter_end(cli_line_meter_t *m)
{
    if (!m)
        return;
    if (g_active_meter == m)
        g_active_meter = NULL;
    m->active = 0;
    AIRY_FREE(m->row);
    m->row = NULL;
    m->row_len = 0;
    m->row_cap = 0;
}

size_t cli_render_meter_phys(cli_line_meter_t *m)
{
    if (!m)
        return 0;
    if (!m->done) {
        /* settle the trailing partial row (no '\n' at end) */
        if (m->cols > 0 && m->row_len > 0) {
            size_t w = cli_disp_width(m->row);
            if (w > m->cols)
                m->phys_lines += (w - 1) / m->cols;
        }
        if (m->row_len > 0)
            m->phys_lines += 1;
        m->done = 1;
    }
    return m->phys_lines;
}

void cli_outn(const char *s, size_t n)
{
    if (!s || n == 0)
        return;
    if (g_active_meter)
        cli_meter_feed(g_active_meter, s, n);
    if (g_cli_tui && cli_tui_active(g_cli_tui))
        cli_tui_emit(g_cli_tui, s, n);
    else
        fwrite(s, 1, n, stdout);
}

void cli_outc(char c)
{
    if (g_active_meter)
        cli_meter_feed(g_active_meter, &c, 1);
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

/* 用户可读的错误描述：内部 AIRY_ERR_* 错误码（负值）对用户无意义，
 * 统一映射为可理解的中文描述；未识别码返回通用描述。 */
const char *cli_err_desc(int err)
{
    switch (err) {
    case AIRY_ERR_TIMEOUT:
        return "请求超时，请检查网络或服务状态";
    case AIRY_ERR_NOT_FOUND:
        return "目标不存在或未找到";
    case AIRY_ERR_GENERIC_FAIL:
        return "操作失败，请稍后重试";
    case AIRY_ERR_IO:
        return "读写错误，请检查文件或磁盘";
    case AIRY_ERR_PERMISSION_DENIED:
        return "权限不足，无法执行该操作";
    case AIRY_ERR_INVALID_PARAM:
        return "参数无效，请检查输入";
    case AIRY_ERR_NOT_SUPPORTED:
        return "暂不支持该操作";
    case AIRY_ERR_OUT_OF_MEMORY:
        return "内存不足";
    case AIRY_ERR_CANCELED:
        return "操作已取消";
    case AIRY_ERR_WOULD_BLOCK:
        return "资源暂时不可用，请稍后重试";
    default:
        return "发生错误";
    }
}

void cli_trace(const char *tag, const char *fmt, ...)
{
    /* One-shot server mode (-p) progress channel: a "[tag] message" line on
     * stderr so stdout carries only the final answer (Claude Code -p
     * convention). No-op in interactive mode, where the normal role chrome
     * already shows progress. */
    if (!g_cli_print_mode)
        return;
    if (!tag || !fmt)
        return;
    va_list ap;
    va_start(ap, fmt);
    char stack_buf[1024];
    int need = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (need < 0)
        return;
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
    if (!s)
        return 0;
    return cli_disp_width_of(s, strlen(s));
}

size_t cli_disp_width_of(const char *s, size_t n)
{
    size_t w = 0;
    size_t i = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (i < n && p[i]) {
        if (p[i] < 0x80) { /* ASCII: 1 cell */
            w += 1;
            i += 1;
        } else if ((p[i] & 0xE0) == 0xC0) { /* 2-byte (e.g. "·"): 1 cell */
            w += 1;
            i += 2;
        } else if ((p[i] & 0xF0) == 0xE0) { /* 3-byte */
            w += (p[i] == 0xE2) ? 1 : 2;     /* 0xE2: box drawing / arrows */
            i += 3;
        } else { /* 4-byte: 2 cells */
            w += 2;
            i += 4;
        }
    }
    return w;
}

/* Back off a byte-wise truncation point to the nearest UTF-8 sequence
 * boundary so a multi-byte character is never cut in half (which would
 * print a stray continuation byte and garble CJK tags/headers).
 * Returns a safe byte count <= max_bytes. */
size_t cli_utf8_safe_len(const char *s, size_t max_bytes)
{
    size_t len = strlen(s);
    if (max_bytes >= len)
        return len;
    size_t n = max_bytes;
    /* Walk back while byte n is a continuation byte of a multi-byte char */
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80)
        n--;
    return n;
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
    case CLI_ROLE_DUAL_THINK:
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
    case CLI_ACTOR_DUAL_THINK:
        return "Dual Think";
    case CLI_ACTOR_DUAL_SLOW_THINK:
        return "Dual Slow Think";
    case CLI_ACTOR_DUAL_FAST_THINK:
        return "Dual Fast Think";
    case CLI_ACTOR_DUAL_PROF_THINK:
        return "Dual Prof Think";
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
        /* Markdown link: [text](url) — cyan + underline for the label;
         * URL hidden in interactive mode (kept in -p for pipe consumers). */
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
                            /* -p: expose URL for pipe consumers */
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
        /* Column width in display columns (CJK chars occupy 2 columns), not
         * bytes: strlen-based padding misaligns every mixed ASCII/CJK table. */
        for (size_t t = 0; t < rows; t++) {
            for (size_t c = 0; c < table[t].cell_count; c++) {
                size_t cl = cli_disp_width(table[t].cells[c]);
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

        /* fenced code block toggle: ``` 或 LLM 常用替代分隔符 [code]/[/code]
         * （2026-08-16：部分模型以 [code] 包裹代码，若不识别则代码行以
         * 普通文本直出，可读性差；统一按代码围栏渲染） */
        if (strncmp(line, "```", 3) == 0 || strncmp(line, "[code]", 6) == 0 ||
            strncmp(line, "[/code]", 7) == 0) {
            if (in_table) {
                cli_table_flush(table, &table_rows, g);
                in_table = 0;
            }
            in_code = !in_code;
            cli_out(g);
            /* One-shot server mode (-p)：输出标准 markdown 围栏 ``` 而非
             * [code] 字样，保证 stdout 可被下游 markdown 渲染器直接消费；
             * 交互模式保留 [code] 视觉围栏（灰底代码块上下文更醒目）。 */
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
    size_t w = cli_disp_width(hdr);
    for (size_t i = w; i < CLI_ROLE_HDR_W; i++)
        cli_outc(' ');
}

/* Build a fixed-width role header, always keeping the closing "]" intact.
 * When the tag makes the header exceed CLI_ROLE_HDR_W, the tag is truncated
 * (not the bracket), so the gutter stays aligned and never renders a dangling
 * header like "[Sub Agent:tool.list_too".  Truncation is UTF-8 safe: the cut
 * falls back to the nearest character boundary so a CJK tag (对话/思考/状态)
 * never renders as a broken half-width glyph (乱码). */
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
    /* One-shot server mode (-p): execution detail (trace / status / sub-agent
     * reports) is suppressed so the output carries only the final result and
     * real errors (Claude Code -p keeps execution chrome out of the reply). */
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

/* 截取文本前 max_lines 行（按 '\n' 分割；保留行内内容），供折叠重绘。
 * UTF-8 安全：末字节落在多字节序列中间时回退到字符边界，避免切断
 * 中文 → 折叠展示出现乱码。 */
static size_t cli_take_first_lines(const char *text, size_t max_lines, char *out,
                                   size_t cap)
{
    if (!out || cap == 0)
        return 0;
    size_t o = 0, lines = 0;
    const char *p = text ? text : "";
    while (*p && lines < max_lines && o + 1 < cap) {
        if (*p == '\n') {
            lines++;
            if (o + 1 < cap)
                out[o++] = '\n';
            p++;
            continue;
        }
        out[o++] = *p++;
    }
    if (o > 0)
        o = cli_utf8_safe_len(out, o);
    if (o == 0)
        out[o++] = '\0';
    else
        out[o] = '\0';
    return o;
}

void cli_render_super_agent_truncated(const char *content)
{
    const char *src = content ? content : "";
    char keep_buf[8192];
    cli_take_first_lines(src, CLI_REPLY_FOLD_KEEP, keep_buf, sizeof(keep_buf));
    cli_render_super_agent(keep_buf);

    /* 折叠尾行数 = 原文逻辑行数 - KEEP（渲染形态与原文行数可能有细微
     * 差异，尾数近似即可——只影响折叠提示文案）。 */
    size_t total_lines = 0;
    for (const char *p = src; *p; p++) {
        if (*p == '\n')
            total_lines++;
    }
    total_lines++; /* 末行 */
    if (total_lines > CLI_REPLY_FOLD_KEEP)
        cli_render_stream_fold_trailer(total_lines - CLI_REPLY_FOLD_KEEP);
}

void cli_render_super_agent_folded(const char *content)
{
    /* 折叠依赖 ANSI 上移擦除：仅交互 TTY 可用；-p（管道）与 TUI 由
     * 调用方自行分支（-p 完整输出、TUI 折叠区）。 */
    if (!cli_term_is_tty())
        return;

    cli_line_meter_t m;
    AIRY_MEMSET(&m, 0, sizeof(m));
    cli_render_meter_begin(&m);
    cli_render_super_agent(content ? content : "");
    size_t lines = m.lines;
    size_t phys = cli_render_meter_phys(&m);
    cli_render_meter_end(&m);

    if (lines <= CLI_REPLY_FOLD_MAX)
        return; /* 短回复：已完整渲染，无需折叠 */

    /* 长回复：上移擦除已渲染的物理行，重绘为前 KEEP 行 + 折叠尾。
     * \r 先回行首再 \033[J（CUU 只移行不移列，直接清会残留列尾内容）。
     * 全量文本保留在日志/消息历史中（full text in logs 约定）。 */
    char erase[32];
    int en = snprintf(erase, sizeof(erase), "\033[%zuA\r\033[J", phys);
    if (en > 0)
        fwrite(erase, 1, (size_t)en, stdout);

    cli_render_super_agent_truncated(content ? content : "");
}

void cli_render_stream_fold_trailer(size_t more_lines)
{
    if (more_lines == 0)
        return;
    /* 折叠尾：中文提示 + 省略号图标，视觉轻盈不抢眼 */
    char trailer[128];
    snprintf(trailer, sizeof(trailer), "└ … 省略 %zu 行（完整内容见日志）",
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
    /* One-shot server mode (-p): no user-input echo; the reply is the only
     * output (Claude Code -p / Codex exec convention). */
    if (g_cli_print_mode)
        return;
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
    /* One-shot server mode (-p): suppress sub-agent reports; keep real errors. */
    if (g_cli_print_mode && role != CLI_ROLE_ERROR)
        return;
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

/* ---- tool invocation / result (process-only, Claude Code tool-use) ---- */

/* 动作短语映射：对话只展示"正在做什么"（过程），不暴露工具参数与
 * 返回内容（代码/URL/文件内容等操作细节保留在日志与模型上下文里）。 */
static const char *cli_tool_action(const char *name)
{
    static const struct {
        const char *tool;
        const char *action;
    } map[] = {
        {"web_search", "搜索网络"},
        {"web_fetch", "抓取网页"},
        {"fs_read", "读取文件"},
        {"fs_write", "写入文件"},
        {"fs_list", "列出目录"},
        {"fs_ls", "列出目录"},
        {"fs_info", "查看文件信息"},
        {"fs_mkdir", "创建目录"},
        {"fs_rm", "删除文件"},
        {"agent.spawn", "派生智能体"},
        {"agent.invoke", "调用智能体"},
        {"think.depth", "深度思考"},
        {"memory.get", "读取记忆"},
        {"memory.put", "写入记忆"},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].tool) == 0)
            return map[i].action;
    }
    return name; /* 未知工具保留原名（过程性名词，非参数） */
}

void cli_render_tool_use(const char *name, const char *args)
{
    (void)args; /* 参数不进对话：只展示过程（2026-08-17） */
    if (!name)
        return;
    const char *action = cli_tool_action(name);

    if (g_cli_print_mode) {
        /* One-shot server mode (-p): tool progress goes to stderr so stdout
         * stays a pure, pipeable reply. */
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

void cli_render_tool_result(const char *name, const char *text, int ok)
{
    if (!name)
        return;
    const char *action = cli_tool_action(name);

    /* 首行错误摘要（失败时附于结果行末尾） */
    char err[128];
    err[0] = '\0';
    if (!ok && text && text[0]) {
        size_t o = 0;
        for (const char *p = text; *p && o + 1 < sizeof(err); p++) {
            if (*p == '\n' || *p == '\r')
                break;
            err[o++] = *p;
        }
        err[o] = '\0';
    }

    if (g_cli_print_mode) {
        /* -p: stderr 一行 ✓/✗ + 动作 + 可选错误 */
        fputs(cli_c(ok ? CLR_GREEN : CLR_RED), stderr);
        fputs(ok ? CLI_ICON_CHECK : CLI_ICON_CROSS, stderr);
        fputs(cli_c(CLR_RESET), stderr);
        fputs(" ", stderr);
        fputs(cli_c(CLR_CYAN), stderr);
        fputs(action, stderr);
        fputs(cli_c(CLR_RESET), stderr);
        if (err[0]) {
            fputs(" — ", stderr);
            fputs(err, stderr);
        }
        fputc('\n', stderr);
        return;
    }

    /* 交互 TTY：与工具调用卡片对齐的简洁结果行
     *   ✓ 搜索完成
     *   ✗ 抓取 — 连接超时
     * 不暴露内部角色名（[Sub xxx Agent]）或工具参数。 */
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

void cli_render_turn_separator(uint64_t elapsed_ms, const char *metrics)
{
    /* One-shot server mode (-p): the result is the only output, no turn chrome. */
    if (g_cli_print_mode)
        return;
    /* Start on a fresh line: the previous turn's content (or a folded
     * reply tail) may not end with '\n', and writing the separator right
     * after it would leave "…─" debris on the prompt line (reported
     * overlap). This also adds breathing room between turns. */
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
    size_t label_len = strlen(label);
    size_t metrics_len = (metrics && metrics[0]) ? strlen(metrics) + 3 : 0; /* " · " + metrics */
    size_t center = label_len + metrics_len + 2; /* 2 spaces padding */
    size_t ccols = (size_t)(cols > 0 ? cols : 80);
    size_t dash_total = ccols > center + 6 ? ccols - center - 6 : 10;
    size_t dash_left = dash_total / 2;
    size_t dash_right = dash_total - dash_left;
    if (dash_left > 40)
        dash_left = 40;
    if (dash_right > 40)
        dash_right = 40;

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

/* ---- progress bar ---- */

void cli_render_progress_bar(double progress, size_t width, const char *label)
{
    /* One-shot server mode (-p): no progress chrome, only the final result. */
    if (g_cli_print_mode)
        return;
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
    cli_out(g);
    if (label && label[0]) {
        cli_out(cli_c(CLR_DIM));
        cli_out(CLI_ICON_BRANCH " "); /* └ 节点层级分支符号 */
        cli_out(label);
        cli_out(cli_c(CLR_RESET));
        cli_out(": ");
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
    /* One-shot server mode (-p): no task board chrome, only the final result. */
    if (g_cli_print_mode)
        return;
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
    } else if (strcmp(st, "failed") == 0) {
        st_col = cli_c(CLR_RED);
        st_icon = CLI_ICON_CROSS;
        bar_bright = 0;
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
    } else if (strcmp(st, "scheduled") == 0) {
        st_col = cli_c(CLR_CYAN);
        st_icon = CLI_ICON_CLOCK;
    } else if (strcmp(st, "canceled") == 0) {
        st_col = cli_c(CLR_DIM);
        st_icon = CLI_ICON_CANCEL;
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

/* ---- 非 TTY 状态行辅助（-p 模式的 cli_trace 行） ---- */

const char *cli_icon_for_state(const char *state)
{
    if (!state)
        return CLI_ICON_BULLET;
    if (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0 ||
        strcmp(state, "done") == 0)
        return CLI_ICON_CHECK;
    if (strcmp(state, "failed") == 0 || strcmp(state, "error") == 0)
        return CLI_ICON_CROSS;
    if (strcmp(state, "running") == 0 || strcmp(state, "active") == 0 ||
        strcmp(state, "executing") == 0)
        return CLI_ICON_DIAMOND;
    if (strcmp(state, "pending") == 0 || strcmp(state, "queued") == 0 ||
        strcmp(state, "ready") == 0)
        return CLI_ICON_TODO;
    if (strcmp(state, "scheduled") == 0)
        return CLI_ICON_CLOCK;
    if (strcmp(state, "canceled") == 0)
        return CLI_ICON_CANCEL;
    return CLI_ICON_BULLET;
}

const char *cli_state_cn(const char *state)
{
    if (!state)
        return "未知";
    if (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0 ||
        strcmp(state, "done") == 0)
        return "完成";
    if (strcmp(state, "failed") == 0)
        return "失败";
    if (strcmp(state, "canceled") == 0)
        return "已取消";
    if (strcmp(state, "error") == 0)
        return "出错";
    if (strcmp(state, "running") == 0 || strcmp(state, "active") == 0 ||
        strcmp(state, "executing") == 0)
        return "执行中";
    if (strcmp(state, "queued") == 0)
        return "排队中";
    if (strcmp(state, "pending") == 0)
        return "待处理";
    if (strcmp(state, "scheduled") == 0)
        return "已调度";
    if (strcmp(state, "ready") == 0)
        return "就绪";
    if (strcmp(state, "waiting") == 0)
        return "等待中";
    if (strcmp(state, "skipped") == 0)
        return "已跳过";
    if (strcmp(state, "retrying") == 0)
        return "重试中";
    if (strcmp(state, "online") == 0)
        return "在线";
    if (strcmp(state, "offline") == 0)
        return "离线";
    return state;
}

void cli_compact_bar(char *out, size_t cap, double progress, size_t cells)
{
    if (!out || cap == 0)
        return;
    if (progress < 0.0)
        progress = 0.0;
    if (progress > 1.0)
        progress = 1.0;
    size_t filled = (size_t)(progress * (double)cells);
    if (filled > cells)
        filled = cells;
    size_t o = 0;
    if (o < cap - 1)
        out[o++] = '[';
    for (size_t i = 0; i < cells && o < cap - 1; i++)
        out[o++] = (i < filled) ? '#' : '-';
    if (o < cap - 1)
        out[o++] = ']';
    out[o] = '\0';
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
    /* One-shot server mode (-p): no status lines at all; the final result is
     * the only output (Claude Code -p / Codex exec convention). */
    if (g_cli_print_mode)
        return 0;
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

/* ---- 阶段指示器（task execution phase headers） ---- */

void cli_render_phase(const char *label)
{
    if (!label || !label[0] || g_cli_print_mode)
        return;
    const char *g = cli_gutter(2);
    cli_outf("\n%s%s%s %s%s%s %s",
             g, cli_c(CLR_CYAN), CLI_ICON_DIAMOND,
             cli_c(CLR_CYAN), label, cli_c(CLR_RESET),
             cli_c(CLR_DIM));
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t used = 4 + 2 + strlen(label);
    size_t ccols = (size_t)(cols > 0 ? cols : 80);
    size_t dash = ccols > used + 4 ? ccols - used - 4 : 20;
    if (dash > 50)
        dash = 50;
    for (size_t i = 0; i < dash; i++)
        cli_out("─");
    cli_outf("%s\n", cli_c(CLR_RESET));
}
