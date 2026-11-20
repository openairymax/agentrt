// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev_in.c
 * @brief stdin 输入帧：hello 续传、prompt/paste 入队、cancel 中断与
 *        control_response 裁决回送。
 *
 * 输入与事件流共用「一行一 JSON」的 NDJSON 形态；坏行或未知类型只回
 * notice，不断流。stdin 在事件模式下置非阻塞，运行期以切片轮询与
 * control_request 等待共用同一读面，避免两处争抢文件描述符。
 */

#include "cli_ev.h"
#include "cli_render.h"
#include "airy_memory.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#endif

#define IN_LINE_MAX 65536
#define IN_PR_MAX (1024u * 1024u)
#define IN_RID_MAX 24

static struct {
    int opened;
    int eof;
    int cancel;
    int pr_ready;
    int decision;
    int nonblock;
    int flags;
    unsigned next_id;
    char rid[IN_RID_MAX];
    char *buf;
    size_t cap;
    size_t len;
    char *pr;
    size_t prlen;
    size_t prcap;
    char *opts;
    size_t optslen;
} g_in;

#ifndef _WIN32
static void in_open(void)
{
    int fl = fcntl(0, F_GETFL, 0);

    g_in.cap = 8192;
    g_in.buf = (char *)AIRY_MALLOC(g_in.cap);
    if (fl >= 0 && !(fl & O_NONBLOCK)) {
        g_in.flags = fl;
        g_in.nonblock = 1;
        (void)fcntl(0, F_SETFL, fl | O_NONBLOCK);
    }
    g_in.opened = 1;
}

static int in_wait(int ms)
{
    struct pollfd pfd = { 0, POLLIN, 0 };
    int rc = poll(&pfd, 1, ms);

    return rc > 0 ? 1 : 0;
}

static long in_read(char *dst, size_t cap)
{
    ssize_t n = read(0, dst, cap);

    if (n > 0)
        return (long)n;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return 0;
    return -1;
}
#else
static void in_open(void)
{
    (void)_setmode(0, _O_BINARY);
    g_in.cap = 8192;
    g_in.buf = (char *)AIRY_MALLOC(g_in.cap);
    g_in.opened = 1;
}

/* 管道用 PeekNamedPipe 判可读，控制台用等待事件判可读；两者都是真实的
 * 就绪查询，保证非阻塞轮询在 Windows 上同样不吞输入。 */
static int in_wait(int ms)
{
    HANDLE h = (HANDLE)_get_osfhandle(0);
    DWORD type;

    if (h == INVALID_HANDLE_VALUE)
        return 0;
    type = GetFileType(h);
    if (type == FILE_TYPE_PIPE) {
        DWORD avail = 0;

        if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) && avail > 0)
            return 1;
        Sleep((DWORD)(ms > 0 ? ms : 1));
        return 0;
    }
    return WaitForSingleObject(h, (DWORD)ms) == WAIT_OBJECT_0;
}

static long in_read(char *dst, size_t cap)
{
    int n = _read(0, dst, (unsigned)cap);

    return n > 0 ? (long)n : (n == 0 ? -1 : 0);
}
#endif

static void in_push(const char *txt)
{
    size_t n = strlen(txt);
    size_t need;

    if (n == 0)
        return;
    need = g_in.prlen + n + 2;
    if (need > IN_PR_MAX) {
        (void)cli_ev_note("prompt buffer full; input frame dropped");
        return;
    }
    if (need > g_in.prcap) {
        size_t cap = g_in.prcap ? g_in.prcap : 4096;
        char *np;

        while (cap < need)
            cap *= 2;
        np = (char *)AIRY_REALLOC(g_in.pr, cap);
        if (!np)
            return;
        g_in.pr = np;
        g_in.prcap = cap;
    }
    if (g_in.prlen)
        g_in.pr[g_in.prlen++] = '\n';
    AIRY_MEMCPY(g_in.pr + g_in.prlen, txt, n);
    g_in.prlen += n;
    g_in.pr[g_in.prlen] = '\0';
    g_in.pr_ready = 1;
}

static void in_resp(const char *js, size_t n)
{
    char rid[IN_RID_MAX];
    char opt[32];

    if (cli_ev_str(js, n, "request_id", rid, sizeof(rid)) <= 0)
        (void)cli_ev_str(js, n, "id", rid, sizeof(rid));
    if (rid[0] && strcmp(rid, g_in.rid) != 0)
        return;
    if (cli_ev_str(js, n, "behavior", opt, sizeof(opt)) <= 0 &&
        cli_ev_str(js, n, "decision", opt, sizeof(opt)) <= 0)
        return;
    g_in.decision = (strncmp(opt, "allow", 5) == 0) ? 1 : -1;
}

static void in_opts_set(const char *js, size_t n)
{
    char *buf;

    AIRY_FREE(g_in.opts);
    g_in.opts = NULL;
    g_in.optslen = 0;
    buf = (char *)AIRY_MALLOC(n + 1);
    if (!buf)
        return;
    if (cli_ev_obj(js, n, "opts", buf, n + 1) > 0) {
        g_in.opts = buf;
        g_in.optslen = strlen(buf);
    } else {
        AIRY_FREE(buf);
    }
}

static void in_input(const char *js, size_t n)
{
    char kind[16];
    char *txt;

    if (cli_ev_str(js, n, "kind", kind, sizeof(kind)) <= 0) {
        (void)cli_ev_note("input frame without kind");
        return;
    }
    if (strcmp(kind, "cancel") == 0) {
        g_in.cancel = 1;
        return;
    }
    if (strcmp(kind, "confirm") == 0) {
        in_resp(js, n);
        return;
    }
    if (strcmp(kind, "prompt") != 0 && strcmp(kind, "paste") != 0) {
        (void)cli_ev_note("input frame kind ignored");
        return;
    }
    /* 选项与文本同帧到达：先落 opts 再入队，保证消费方取到同一轮的行 */
    in_opts_set(js, n);
    txt = (char *)AIRY_MALLOC(n + 1);
    if (!txt)
        return;
    if (cli_ev_str(js, n, "text", txt, n + 1) > 0)
        in_push(txt);
    else
        (void)cli_ev_note("input frame without text");
    AIRY_FREE(txt);
}

static void in_line(const char *js, size_t n)
{
    char type[24];
    unsigned long seq = 0;

    if (n == 0)
        return;
    if (!cli_ev_typeof(js, n, type, sizeof(type))) {
        (void)cli_ev_note("input frame rejected: bad or missing type");
        return;
    }
    if (strcmp(type, "hello") == 0) {
        if (cli_ev_num(js, n, "last_seq", &seq))
            cli_ev_snap((unsigned)seq);
        return;
    }
    if (strcmp(type, "control_response") == 0) {
        in_resp(js, n);
        return;
    }
    if (strcmp(type, "input") == 0) {
        in_input(js, n);
        return;
    }
    (void)cli_ev_note("input frame type not supported");
}

static void in_lines(void)
{
    size_t off = 0;

    while (off < g_in.len) {
        char *nl = (char *)memchr(g_in.buf + off, '\n', g_in.len - off);
        size_t l;

        if (!nl)
            break;
        l = (size_t)(nl - (g_in.buf + off));
        if (l > 0 && g_in.buf[off + l - 1] == '\r')
            l--;
        in_line(g_in.buf + off, l);
        off += (size_t)(nl - (g_in.buf + off)) + 1;
    }
    if (off > 0) {
        AIRY_MEMMOVE(g_in.buf, g_in.buf + off, g_in.len - off);
        g_in.len -= off;
    }
    if (g_in.len >= g_in.cap) {
        (void)cli_ev_note("input frame overlong; buffer reset");
        g_in.len = 0;
    }
}

/* 读一次可用字节：0=暂无数据，-1=对端关闭。 */
static int in_fill(void)
{
    long n;

    if (!g_in.opened)
        in_open();
    if (!g_in.buf)
        return -1;
    for (;;) {
        if (g_in.cap - g_in.len < 1024) {
            char *nb;
            size_t cap = g_in.cap * 2;

            if (cap > IN_LINE_MAX * 2) {
                g_in.len = 0;
                return 0;
            }
            nb = (char *)AIRY_REALLOC(g_in.buf, cap);
            if (!nb)
                return -1;
            g_in.buf = nb;
            g_in.cap = cap;
        }
        n = in_read(g_in.buf + g_in.len, g_in.cap - g_in.len);
        if (n < 0) {
            g_in.eof = 1;
            return -1;
        }
        if (n == 0)
            return 0;
        g_in.len += (size_t)n;
        in_lines();
        if (g_in.pr_ready || g_in.cancel)
            return 1;
    }
}

int cli_ev_in_poll(void)
{
    if (in_fill() < 0)
        return 0;
    if (g_in.cancel)
        return -1;
    return 1;
}

int cli_ev_in_prompt(char *out, size_t cap)
{
    for (;;) {
        if (g_in.cancel)
            return -1;
        if (g_in.pr_ready) {
            size_t n = g_in.prlen < cap - 1 ? g_in.prlen : cap - 1;

            AIRY_MEMCPY(out, g_in.pr, n);
            out[n] = '\0';
            g_in.prlen = 0;
            g_in.pr_ready = 0;
            return 1;
        }
        if (g_in.eof)
            return 0;
        if (in_fill() < 0)
            return 0;
        if (!g_in.pr_ready && !g_in.cancel)
            (void)in_wait(200);
    }
}

int cli_ev_in_cancel(void)
{
    return g_in.cancel;
}

int cli_ev_in_opts(char *out, size_t cap)
{
    if (!out || cap == 0)
        return 0;
    out[0] = '\0';
    if (!g_in.opts || g_in.optslen == 0 || g_in.optslen >= cap)
        return 0;
    AIRY_MEMCPY(out, g_in.opts, g_in.optslen + 1);
    AIRY_FREE(g_in.opts);
    g_in.opts = NULL;
    g_in.optslen = 0;
    return (int)(strlen(out));
}

int cli_ev_ask(const char *tool, const char *target, int timeout_ms)
{
    char esc1[96];
    char esc2[256];
    char pl[512];
    uint64_t dl;
    int n;

    if (cli_ev_closed())
        return 0;
    g_in.decision = 0;
    g_in.next_id++;
    (void)cli_ev_esc(esc1, sizeof(esc1), tool ? tool : "");
    (void)cli_ev_esc(esc2, sizeof(esc2), target ? target : "");
    n = snprintf(pl, sizeof(pl),
                 "{\"type\":\"control_request\",\"request_id\":\"c%u\","
                 "\"subtype\":\"can_use_tool\",\"tool\":\"%s\","
                 "\"input\":\"%s\",\"timeout_ms\":%d}",
                 g_in.next_id, esc1, esc2, timeout_ms);
    if (n <= 0 || (size_t)n >= sizeof(pl))
        return 0;
    snprintf(g_in.rid, sizeof(g_in.rid), "c%u", g_in.next_id);
    if (cli_ev_emit("control_request", pl, (size_t)n) != 0)
        return 0;
    dl = cli_now_ms() + (uint64_t)timeout_ms;
    while (cli_now_ms() < dl) {
        if (g_in.decision)
            return g_in.decision > 0 ? 1 : 0;
        if (cli_ev_closed())
            return 0;
        (void)cli_ev_in_poll();
        (void)cli_ev_beat();
#ifndef _WIN32
        {
            struct timespec ts = { 0, 20 * 1000 * 1000 };

            (void)nanosleep(&ts, NULL);
        }
#else
        Sleep(20);
#endif
    }
    (void)cli_ev_note("tool approval timed out; denied (fail-closed)");
    return 0;
}
