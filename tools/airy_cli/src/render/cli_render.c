// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_render.c
 * @brief airy_cli rendering core: output primitives, color gate, meter, spinner.
 *
 * Split layout (2026-08-27):
 *   cli_render.c         — output primitives, color, meter, status helpers, spinner
 *   airy_cli_markdown.c  — markdown parser, inline formatting, tool rendering
 *   airy_cli_output.c    — role lines, progress bars, turn separators
 *
 * A stream-safe, line-oriented renderer. No TTY capture and no curses: every
 * function prints directly to stdout so the CLI behaves identically on a
 * local terminal, over SSH or when output is piped to a log file.
 */

#include "cli_render.h"
#include "cli_tui.h"

#include "airy_memory.h"

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

/* Attached full-screen TUI engine (NULL = plain stdout streaming). */
static struct cli_tui_s *g_cli_tui;

void cli_render_set_tui(struct cli_tui_s *tui)
{
    g_cli_tui = tui;
}

void cli_out(const char *s)
{
    if (!s) return;
    cli_outn(s, strlen(s));
}

/* ---- reply folding meter ---- */

static cli_line_meter_t *g_active_meter;

static void cli_meter_feed(cli_line_meter_t *m, const char *s, size_t n)
{
    if (!m) return;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1B) { m->in_esc = 1; continue; }
        if (m->in_esc) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                m->in_esc = 0;
            continue;
        }
        if (c == '\n') {
            if (m->cols > 0 && m->row_len > 0) {
                size_t w = cli_disp_width(m->row);
                if (w > m->cols) m->phys_lines += (w - 1) / m->cols;
            }
            m->phys_lines += 1;
            m->lines += 1;
            m->row_len = 0;
            continue;
        }
        if (c == '\r') { m->row_len = 0; continue; }
        if (c < 0x20) continue;
        if (m->row_len + 1 >= m->row_cap) {
            size_t new_cap = m->row_cap ? m->row_cap * 2 : 256;
            char *grown = (char *)AIRY_REALLOC(m->row, new_cap);
            if (!grown) continue;
            m->row = grown;
            m->row_cap = new_cap;
        }
        m->row[m->row_len++] = (char)c;
        m->row[m->row_len] = '\0';
    }
}

void cli_render_meter_begin(cli_line_meter_t *m)
{
    if (!m) return;
    AIRY_MEMSET(m, 0, sizeof(*m));
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    m->cols = cols > 0 ? (size_t)cols : 0;
    m->active = 1;
    g_active_meter = m;
}

void cli_render_meter_end(cli_line_meter_t *m)
{
    if (!m) return;
    if (g_active_meter == m) g_active_meter = NULL;
    m->active = 0;
    AIRY_FREE(m->row);
    m->row = NULL;
    m->row_len = 0;
    m->row_cap = 0;
}

size_t cli_render_meter_phys(cli_line_meter_t *m)
{
    if (!m) return 0;
    if (!m->done) {
        if (m->cols > 0 && m->row_len > 0) {
            size_t w = cli_disp_width(m->row);
            if (w > m->cols) m->phys_lines += (w - 1) / m->cols;
        }
        if (m->row_len > 0) m->phys_lines += 1;
        m->done = 1;
    }
    return m->phys_lines;
}

void cli_outn(const char *s, size_t n)
{
    if (!s || n == 0) return;
    if (g_active_meter) cli_meter_feed(g_active_meter, s, n);
    if (g_cli_tui && cli_tui_active(g_cli_tui))
        cli_tui_emit(g_cli_tui, s, n);
    else
        fwrite(s, 1, n, stdout);
}

void cli_outc(char c)
{
    if (g_active_meter) cli_meter_feed(g_active_meter, &c, 1);
    if (g_cli_tui && cli_tui_active(g_cli_tui))
        cli_tui_emit(g_cli_tui, &c, 1);
    else
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

/* ---- error description ---- */

const char *cli_err_desc(int err)
{
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
    default:                        return "发生错误";
    }
}

/* ---- trace (server -p mode progress channel) ---- */

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

/* ---- color gate ---- */

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

const char *cli_gutter_pad(size_t indent)
{
    return cli_gutter(indent);
}

/* ---- UTF-8 width helpers ---- */

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

/* ---- role / actor names ---- */

const char *cli_render_role_color(cli_role_t role)
{
    if (!cli_color_enabled()) return "";
    switch (role) {
    case CLI_ROLE_USER:         return CLR_CYAN;
    case CLI_ROLE_SUPER_AGENT:  return CLR_GREEN;
    case CLI_ROLE_DUAL_THINK:   return CLR_YELLOW;
    case CLI_ROLE_SUB_AGENT:    return CLR_MAGENTA;
    case CLI_ROLE_STATUS:       return CLR_DIM;
    case CLI_ROLE_TRACE:        return CLR_DIM;
    case CLI_ROLE_ERROR:        return CLR_RED;
    default:                    return CLR_RESET;
    }
}

const char *cli_render_actor_name(cli_actor_t actor)
{
    switch (actor) {
    case CLI_ACTOR_USER:            return "For Thee";
    case CLI_ACTOR_SUPER_AGENT:     return "Super Agent";
    case CLI_ACTOR_DUAL_THINK:      return "Dual Think";
    case CLI_ACTOR_DUAL_SLOW_THINK: return "Dual Slow Think";
    case CLI_ACTOR_DUAL_FAST_THINK: return "Dual Fast Think";
    case CLI_ACTOR_DUAL_PROF_THINK: return "Dual Prof Think";
    case CLI_ACTOR_SUB_AGENT:       return "Sub Agent";
    default:                        return "AgentRT";
    }
}

/* ---- progressive disclosure ---- */

void cli_render_collapsed(const char *text, size_t indent, size_t max_lines, int weak)
{
    if (!text) return;
    if (max_lines < 1) max_lines = 1;

    const char *cut = text;
    size_t shown = 0;
    while (shown < max_lines && *cut) {
        const char *nl = strchr(cut, '\n');
        if (!nl) { cut = cut + strlen(cut); shown++; break; }
        cut = nl + 1;
        shown++;
    }

    size_t total = shown;
    if (*cut) {
        for (const char *q = cut; *q; q++)
            if (*q == '\n') total++;
        if (*cut) total++;
    }

    if (weak) cli_out(cli_c(CLR_DIM));
    if (cut > text) {
        size_t plen = (size_t)(cut - text);
        char *prefix = (char *)AIRY_MALLOC(plen + 1);
        if (!prefix) return;
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
    if (weak) cli_out(cli_c(CLR_RESET));
}

/* ---- status helpers ---- */

const char *cli_icon_for_state(const char *state)
{
    if (!state) return CLI_ICON_BULLET;
    if (strcmp(state, "completed") == 0 || strcmp(state, "success") == 0 ||
        strcmp(state, "done") == 0) return CLI_ICON_CHECK;
    if (strcmp(state, "failed") == 0 || strcmp(state, "error") == 0) return CLI_ICON_CROSS;
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

/* ---- spinner ---- */

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
    if (g_cli_print_mode) return 0;
    AIRY_MEMSET(&g_spinner, 0, sizeof(g_spinner));
    if (!title || !title[0]) return 0;
    AIRY_STRNCPY_TERM(g_spinner.title, title, sizeof(g_spinner.title));
    g_spinner.start_ns = cli_time_ns();

    if (!cli_term_is_tty() || cli_tui_active(cli_tui_get_default())) {
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
    if (!g_spinner.active || g_spinner.degraded) return;

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
        snprintf(elapsed_s, sizeof(elapsed_s), "%llum %02llus",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));

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
    if (!g_spinner.active || g_spinner.degraded) return;
    cli_spinner_erase();
    g_spinner.printed = 0;
}

void cli_spinner_resume(void)
{
    if (!g_spinner.active || g_spinner.degraded) return;
    g_spinner.frame = 0;
    cli_spinner_tick();
}

void cli_spinner_stop(int ok, const char *detail)
{
    if (!g_spinner.active) return;

    uint64_t elapsed = (cli_time_ns() - g_spinner.start_ns) / 1000000ULL;
    uint64_t secs = elapsed / 1000;
    char elapsed_s[32];
    if (secs < 1)
        snprintf(elapsed_s, sizeof(elapsed_s), "0s");
    else if (secs < 60)
        snprintf(elapsed_s, sizeof(elapsed_s), "%llus", (unsigned long long)secs);
    else
        snprintf(elapsed_s, sizeof(elapsed_s), "%llum %02llus",
                 (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));

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
    if (!g_spinner.active) return;
    cli_spinner_erase();
    AIRY_MEMSET(&g_spinner, 0, sizeof(g_spinner));
}

/* ---- phase indicator ---- */

void cli_render_phase(const char *label)
{
    if (!label || !label[0] || g_cli_print_mode) return;
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
    if (dash > 50) dash = 50;
    for (size_t i = 0; i < dash; i++)
        cli_out("─");
    cli_outf("%s\n", cli_c(CLR_RESET));
}
