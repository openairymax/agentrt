// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_tui.c
 * @brief Full-screen TUI engine implementation.
 *
 * Layout (on an interactive terminal):
 *
 *   ┌──────────────────────────────────────────────┐
 *   │  pinned header  (banner + model panel)        │  fixed
 *   ├──────────────────────────────────────────────┤
 *   │  conversation viewport (scrollable history)  │  middle
 *   │                                              │
 *   ├──────────────────────────────────────────────┤
 *   │  > input line …                              │  bottom (fixed)
 *   └──────────────────────────────────────────────┘
 *
 * Emitted lines (cli_tui_emit) are folded into an in-memory history; the
 * visible viewport is the tail of that history (or a scroll offset into it
 * while browsing). Every redraw rewrites the header + viewport + input line
 * from the buffer, so scrolling is fully local to the CLI (no reliance on
 * the terminal's own scrollback).
 *
 * ANSI used: alt screen 1049, cursor home, erase line, cursor movement.
 * No curses — plain POSIX termios + ANSI, consistent with the project's
 * "no curses" rendering philosophy.
 */

#include "cli_tui.h"

#include "cli_render.h"
#include "cli_term.h"
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* TUI is POSIX-only: everything degrades to line-oriented stdout. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

/* ---- history / viewport model ---- */

/* Process-wide engine handle (accessor for GCCP etc.). */
static cli_tui_t *g_default_tui;

cli_tui_t *cli_tui_get_default(void)
{
    return g_default_tui;
}

#define TUI_HIST_INIT_CAP 256
#define TUI_LINE_INIT_CAP 128
#define TUI_BOTTOM_INPUT_LINES 2 /* input line + one spare row */

typedef struct {
    char **lines;    /* committed history lines (no trailing '\n') */
    size_t count;    /* number of committed lines */
    size_t cap;      /* allocated slots */
    size_t pinned;   /* lines [0, pinned) form the fixed header */
} tui_history_t;

struct cli_tui_s {
    int active;      /* full-screen page active */
    int dirty;       /* a redraw is pending */

    int rows;
    int cols;

    tui_history_t hist;
    char *cur;       /* partial line being accumulated */
    size_t cur_len;
    size_t cur_cap;

    size_t viewport_rows; /* rows usable by the middle viewport */
    size_t scroll_off;    /* history lines scrolled back (0 = live tail) */

    char *input;     /* input line being edited */
    size_t input_len;
    size_t input_cap;
    size_t input_col;   /* cursor column within input */

#ifndef _WIN32
    struct termios saved_termios;
    int termios_saved;
#endif
};

static void tui_grow_history(tui_history_t *h)
{
    if (h->count >= h->cap) {
        size_t new_cap = h->cap ? h->cap * 2 : TUI_HIST_INIT_CAP;
        char **grown = (char **)AIRY_REALLOC(h->lines, new_cap * sizeof(char *));
        if (!grown)
            return;
        h->lines = grown;
        h->cap = new_cap;
    }
}

static void tui_commit_line(cli_tui_t *t, char *line)
{
    tui_grow_history(&t->hist);
    if (t->hist.count < t->hist.cap)
        t->hist.lines[t->hist.count++] = line;
    else
        AIRY_FREE(line);
    /* Commit growth never resets the pin: header lines were committed
     * before pin_header() ran. */
}

static void tui_history_reset(tui_history_t *h)
{
    for (size_t i = 0; i < h->count; i++)
        AIRY_FREE(h->lines[i]);
    AIRY_FREE(h->lines);
    h->lines = NULL;
    h->count = 0;
    h->cap = 0;
    h->pinned = 0;
}

static void tui_append_byte(cli_tui_t *t, char c)
{
    if (t->cur_len + 2 > t->cur_cap) {
        size_t new_cap = t->cur_cap ? t->cur_cap * 2 : TUI_LINE_INIT_CAP;
        while (new_cap < t->cur_len + 2)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->cur, new_cap);
        if (!grown)
            return;
        t->cur = grown;
        t->cur_cap = new_cap;
    }
    t->cur[t->cur_len++] = c;
    t->cur[t->cur_len] = '\0';
}

void cli_tui_emit(cli_tui_t *t, const char *data, size_t len)
{
    if (!t || !data) {
        /* NULL engine: stream-safe passthrough (non-TTY callers). */
        if (data && len)
            fwrite(data, 1, len, stdout);
        return;
    }
    if (!t->active) {
        if (len)
            fwrite(data, 1, len, stdout);
        return;
    }

    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\r') {
            /* Rewind the partial line: spinner / status redraws replace the
             * current line instead of appending. */
            t->cur_len = 0;
            if (t->cur)
                t->cur[0] = '\0';
            continue;
        }
        if (c == '\n') {
            if (t->cur_len > 0) {
                tui_commit_line(t, t->cur);
                t->cur = NULL;
            } else {
                /* Blank line: commit an empty marker line. */
                char *blank = (char *)AIRY_STRDUP("");
                if (blank)
                    tui_commit_line(t, blank);
            }
            t->cur_len = 0;
            t->cur_cap = 0;
            continue;
        }
        tui_append_byte(t, c);
    }
    /* Redraw immediately so streamed chunks appear live (typewriter effect).
     * Each cli_outn() call is one render tick — chunk-granular, cheap enough. */
    t->dirty = 1;
    cli_tui_redraw(t);
}

void cli_tui_emit_flush(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    if (t->cur_len > 0) {
        tui_commit_line(t, t->cur);
        t->cur = NULL;
        t->cur_len = 0;
        t->cur_cap = 0;
        t->dirty = 1;
    }
}

void cli_tui_pin_header(cli_tui_t *t)
{
    if (!t)
        return;
    /* Commit any partial header line, then fix the header boundary. */
    if (t->active && t->cur_len > 0) {
        tui_commit_line(t, t->cur);
        t->cur = NULL;
        t->cur_len = 0;
        t->cur_cap = 0;
    }
    t->hist.pinned = t->hist.count;
    t->dirty = 1;
}

/* ---- terminal helpers ---- */

static void tui_get_size(cli_tui_t *t)
{
#ifdef _WIN32
    (void)t;
    t->rows = 24;
    t->cols = 80;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        t->rows = (int)ws.ws_row;
        t->cols = (int)ws.ws_col;
    } else {
        t->rows = 24;
        t->cols = 80;
    }
#endif
}

static void tui_write_literal(const char *s)
{
    if (s && *s)
        fputs(s, stdout);
}

/* ---- viewport geometry ---- */

static size_t tui_middle_rows(cli_tui_t *t)
{
    size_t avail = t->rows > 0 ? (size_t)t->rows : 24;
    size_t header = t->hist.pinned;
    size_t input_rows = TUI_BOTTOM_INPUT_LINES;
    if (avail <= header + input_rows)
        return 0;
    return avail - header - input_rows;
}

/* ---- full redraw ---- */

static void tui_clear_line(void)
{
    tui_write_literal("\033[2K");
}

static void tui_render_header(cli_tui_t *t)
{
    for (size_t i = 0; i < t->hist.pinned && i < t->hist.count; i++) {
        tui_write_literal("\033[");
        /* 1-based row */
        char num[16];
        snprintf(num, sizeof(num), "%zu", i + 1);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(t->hist.lines[i], stdout);
    }
}

static void tui_render_viewport(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    size_t start_row = t->hist.pinned + 1;
    size_t total = t->hist.count;
    int have_partial = t->cur && t->cur_len > 0;

    if (rows == 0)
        return;

    /* Viewport window over the content (history minus header), plus the
     * in-progress partial line (streamed text not yet committed). */
    size_t content = (total > t->hist.pinned ? total - t->hist.pinned : 0) +
                     (have_partial ? 1 : 0);
    size_t live = content > rows ? rows : content;
    size_t start = 0;
    if (t->scroll_off > 0) {
        size_t max_off = content > rows ? content - rows : 0;
        if (t->scroll_off > max_off)
            t->scroll_off = max_off;
        start = content - t->scroll_off - live;
    } else {
        start = content > rows ? content - rows : 0;
    }

    for (size_t r = 0; r < rows; r++) {
        tui_write_literal("\033[");
        char num[16];
        snprintf(num, sizeof(num), "%zu", start_row + r);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        size_t rel = start + r;
        if (rel < content) {
            if (rel < content - (have_partial ? 1 : 0)) {
                size_t idx = t->hist.pinned + rel;
                if (idx < t->hist.count)
                    fputs(t->hist.lines[idx], stdout);
            } else if (have_partial) {
                fputs(t->cur, stdout);
            }
        }
    }
}

static void tui_render_input(cli_tui_t *t)
{
    size_t row = (size_t)t->rows;
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    fputs(cli_c(CLR_CYAN), stdout);
    fputs("airy> ", stdout);
    fputs(cli_c(CLR_RESET), stdout);
    if (t->input_len > 0)
        fwrite(t->input, 1, t->input_len, stdout);
    /* Place the cursor after the input text, using the display width (CJK
     * chars occupy two columns) so the caret never drifts on CJK input. */
    size_t col = 7 + cli_disp_width(t->input ? t->input : "");
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", col);
    tui_write_literal(num);
    tui_write_literal("H");
    fflush(stdout);
}

void cli_tui_redraw(cli_tui_t *t)
{
    if (!t || !t->active)
        return;
    tui_render_header(t);
    tui_render_viewport(t);
    tui_render_input(t);
    t->dirty = 0;
    fflush(stdout);
}

/* ---- input ---- */

static int tui_read_byte(cli_tui_t *t, char *out)
{
#ifdef _WIN32
    (void)t;
    (void)out;
    return 0;
#else
    (void)t;
    ssize_t n = read(STDIN_FILENO, out, 1);
    if (n == 1)
        return 1;
    if (n < 0 && errno == EINTR)
        return tui_read_byte(t, out);
    return 0;
#endif
}

/* Read an escape-sequence remainder (following ESC); returns a key code. */
enum {
    TUI_KEY_UP = 0x1001,
    TUI_KEY_DOWN,
    TUI_KEY_PGUP,
    TUI_KEY_PGDN,
    TUI_KEY_HOME,
    TUI_KEY_END,
    TUI_KEY_UNKNOWN,
};

static int tui_read_key(cli_tui_t *t)
{
    char c;
    if (!tui_read_byte(t, &c))
        return 0;
    if (c == 0x1b) {
        char b;
        if (!tui_read_byte(t, &b))
            return 0x1b; /* lone ESC */
        if (b == '[') {
            char x;
            if (!tui_read_byte(t, &x))
                return TUI_KEY_UNKNOWN;
            switch (x) {
            case 'A': return TUI_KEY_UP;
            case 'B': return TUI_KEY_DOWN;
            case 'H': return TUI_KEY_HOME;
            case 'F': return TUI_KEY_END;
            case '5': /* page up: ESC [ 5 ~ */
                if (tui_read_byte(t, &b) && b == '~')
                    return TUI_KEY_PGUP;
                return TUI_KEY_UNKNOWN;
            case '6': /* page down: ESC [ 6 ~ */
                if (tui_read_byte(t, &b) && b == '~')
                    return TUI_KEY_PGDN;
                return TUI_KEY_UNKNOWN;
            case '<': /* SGR mouse: ESC [ < b ; r ; c M  (ignore) */
                return TUI_KEY_UNKNOWN;
            default:
                return TUI_KEY_UNKNOWN;
            }
        }
        return TUI_KEY_UNKNOWN;
    }
    return (unsigned char)c;
}

static void tui_input_append(cli_tui_t *t, char c)
{
    if (t->input_len + 2 > t->input_cap) {
        size_t new_cap = t->input_cap ? t->input_cap * 2 : 256;
        while (new_cap < t->input_len + 2)
            new_cap *= 2;
        char *grown = (char *)AIRY_REALLOC(t->input, new_cap);
        if (!grown)
            return;
        t->input = grown;
        t->input_cap = new_cap;
    }
    t->input[t->input_len++] = c;
    t->input[t->input_len] = '\0';
}

static void tui_input_backspace(cli_tui_t *t)
{
    if (t->input_len == 0)
        return;
    /* Delete one full UTF-8 character: rewind trailing continuation bytes
     * (0x80..0xBF) plus the leading byte, so CJK input is not torn apart. */
    size_t n = t->input_len;
    while (n > 1 && ((unsigned char)t->input[n - 1] & 0xC0) == 0x80)
        n--;
    if (n > 0)
        n--;
    t->input_len = n;
    t->input[t->input_len] = '\0';
}

int cli_tui_readline(cli_tui_t *t, char *buf, size_t cap, size_t *out_len)
{
    if (!buf || cap < 2)
        return 0;
    if (out_len)
        *out_len = 0;

    if (!t || !t->active) {
        /* Non-TUI: classic fgets semantics. */
        if (!fgets(buf, (int)cap, stdin))
            return 0;
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
        if (out_len)
            *out_len = n;
        return 1;
    }

    t->input_len = 0;
    if (t->input)
        t->input[0] = '\0';
    t->scroll_off = 0;
    tui_render_input(t);
    fflush(stdout);

    for (;;) {
        int key = tui_read_key(t);
        if (key == 0)
            return 0; /* EOF */
        if (key == '\n' || key == '\r') {
            size_t n = t->input_len;
            if (n >= cap)
                n = cap - 1;
            if (n > 0)
                AIRY_MEMCPY(buf, t->input, n);
            buf[n] = '\0';
            if (out_len)
                *out_len = n;
            /* Do NOT echo the raw line here: the caller renders the
             * submission (e.g. cli_render_user_message for chat input), so
             * committing it now would duplicate it in the history. */
            t->input_len = 0;
            t->scroll_off = 0;
            return 1;
        }
        if (key == 0x03 || key == 0x04) { /* Ctrl+C / Ctrl+D */
            return 0;
        }
        if (key == 0x7f || key == 0x08) { /* Backspace */
            tui_input_backspace(t);
            tui_render_input(t);
            fflush(stdout);
            continue;
        }
        switch (key) {
        case TUI_KEY_UP:
            if (t->scroll_off < t->hist.count)
                t->scroll_off++;
            cli_tui_redraw(t);
            break;
        case TUI_KEY_DOWN:
            if (t->scroll_off > 0)
                t->scroll_off--;
            cli_tui_redraw(t);
            break;
        case TUI_KEY_PGUP:
            t->scroll_off += tui_middle_rows(t) - 1;
            cli_tui_redraw(t);
            break;
        case TUI_KEY_PGDN:
            if (t->scroll_off > tui_middle_rows(t) - 1)
                t->scroll_off -= tui_middle_rows(t) - 1;
            else
                t->scroll_off = 0;
            cli_tui_redraw(t);
            break;
        case TUI_KEY_HOME:
            t->scroll_off = t->hist.count;
            cli_tui_redraw(t);
            break;
        case TUI_KEY_END:
            t->scroll_off = 0;
            cli_tui_redraw(t);
            break;
        default:
            /* Accept every byte >= 0x20, including UTF-8 multi-byte
             * sequences (CJK input arrives byte-by-byte at 0x80..0xFF).
             * Control bytes and the escape-derived key codes are handled
             * above; anything else is literal text. */
            if (key >= 0x20 && key <= 0xFF) {
                tui_input_append(t, (char)key);
                tui_render_input(t);
                fflush(stdout);
            }
            break;
        }
    }
}

/* ---- lifecycle ---- */

int cli_tui_create(cli_tui_t **out_tui)
{
    if (!out_tui)
        return -1;

    cli_tui_t *t = (cli_tui_t *)AIRY_CALLOC(1, sizeof(cli_tui_t));
    if (!t)
        return -1;
    *out_tui = t;
    if (!g_default_tui)
        g_default_tui = t;
    if (!cli_term_is_tty()) {
        /* Non-TTY: keep the handle but stay inactive (stream-safe). */
        return 0;
    }

    t->active = 1;
    tui_get_size(t);
    if (t->rows <= 6 || t->cols <= 10) {
        t->active = 0;
        return 0;
    }

#ifdef _WIN32
    t->active = 0; /* POSIX-only full-screen mode */
    return 0;
#else
    /* Enter alternate screen + raw mode. */
    fputs("\033[?1049h\033[2J\033[H", stdout);
    fflush(stdout);

    if (tcgetattr(STDIN_FILENO, &t->saved_termios) == 0) {
        struct termios raw = t->saved_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            t->termios_saved = 1;
    }
    return 0;
#endif
}

void cli_tui_destroy(cli_tui_t *t)
{
    if (!t)
        return;
    if (t->active) {
#ifndef _WIN32
        if (t->termios_saved)
            tcsetattr(STDIN_FILENO, TCSANOW, &t->saved_termios);
#endif
        fputs("\033[?1049l", stdout);
        fflush(stdout);
    }
    tui_history_reset(&t->hist);
    AIRY_FREE(t->cur);
    AIRY_FREE(t->input);
    if (g_default_tui == t)
        g_default_tui = NULL;
    AIRY_FREE(t);
}

int cli_tui_active(const cli_tui_t *t)
{
    return t && t->active;
}
