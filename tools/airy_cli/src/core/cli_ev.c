// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev.c
 * @brief 事件流出口：帧编码、序列号、环形续传缓冲与写背压。
 *
 * 帧信封 {"schema":1,"seq":N,"ts":ms,"type":T,"payload":{...}}；payload 原样
 * 承载 run-stream 事件对象，不新造语义。stdout 置非阻塞后按类型预算写出：
 * 增量帧 800ms 超时丢弃并计数，终态帧 30s 判死；EPIPE 置 closed 停流。
 * 环形缓冲保留最近 64 帧（单帧保留上限 64KiB）支撑 hello{last_seq} 续传。
 */

#include "cli_ev.h"
#include "cli_render.h"
#include "airy_memory.h"
#include "string_compat.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

#define EV_TYPE_MAX 24

typedef struct {
    unsigned seq;
    uint64_t ts;
    char type[EV_TYPE_MAX];
    char *pl;
    size_t plen;
    int trunc;
} ev_frame_t;

static struct {
    cli_ev_wr_t wr;
    void *ud;
    unsigned seq;
    unsigned dropped;
    uint64_t last_ms;
    int closed;
    int terminal;
    char state[16];
    int ring_n;
    int ring_head;
    ev_frame_t ring[CLI_EV_RING];
    int nonblock;
    int flags;
} g_ev;

/* 环形缓冲写入：单帧保留超 64KiB 时改存截断摘要，内存上界 64×64KiB。 */
static void ev_push(unsigned seq, uint64_t ts, const char *type,
                    const char *pl, size_t plen, int tr)
{
    ev_frame_t *f = &g_ev.ring[g_ev.ring_head];
    char vbuf[96];

    AIRY_FREE(f->pl);
    f->pl = NULL;
    f->seq = seq;
    f->ts = ts;
    f->trunc = tr;
    AIRY_STRNCPY_TERM(f->type, type, sizeof(f->type));
    if (plen > CLI_EV_KEEP) {
        plen = (size_t)snprintf(vbuf, sizeof(vbuf),
                                "{\"truncated\":true,\"bytes\":%lu}",
                                (unsigned long)plen);
        pl = vbuf;
        f->trunc = 1;
    }
    f->plen = plen;
    if (plen) {
        f->pl = (char *)AIRY_MALLOC(plen + 1);
        if (!f->pl) {
            f->plen = 0;
            return;
        }
        AIRY_MEMCPY(f->pl, pl, plen);
        f->pl[plen] = '\0';
    }
    g_ev.ring_head = (g_ev.ring_head + 1) % CLI_EV_RING;
    if (g_ev.ring_n < CLI_EV_RING)
        g_ev.ring_n++;
}

/* 环形缓冲所有权收敛：条目指针只能在这里释放并置空，避免跨会话重入时
 * 对已释放指针二次释放（D-15 实测：init/fini 多轮复用时命中）。 */
static void ev_ring_clear(void)
{
    int i;

    for (i = 0; i < CLI_EV_RING; i++) {
        AIRY_FREE(g_ev.ring[i].pl);
        g_ev.ring[i].pl = NULL;
        g_ev.ring[i].plen = 0;
        g_ev.ring[i].seq = 0;
    }
    g_ev.ring_n = 0;
    g_ev.ring_head = 0;
}

#ifndef _WIN32
/* POSIX 写腿：非阻塞 write + poll 切片，超预算返回 0（丢弃），EPIPE 返回 -1。 */
static int ev_write(const void *buf, size_t n, int budget)
{
    const char *p = (const char *)buf;
    size_t off = 0;
    uint64_t dl = cli_now_ms() + (uint64_t)budget;

    while (off < n) {
        ssize_t k = write(1, p + off, n - off);

        if (k > 0) {
            off += (size_t)k;
            continue;
        }
        if (k < 0 && errno == EINTR)
            continue;
        if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pfd = {1, POLLOUT, 0};
            uint64_t now = cli_now_ms();
            int wait;

            if (now >= dl)
                return 0;
            wait = (int)(dl - now);
            if (wait > 100)
                wait = 100;
            (void)poll(&pfd, 1, wait);
            continue;
        }
        return -1;
    }
    return 1;
}

int cli_ev_init(void)
{
    int fl = fcntl(1, F_GETFL, 0);

    if (fl >= 0 && !(fl & O_NONBLOCK)) {
        g_ev.flags = fl;
        g_ev.nonblock = 1;
        (void)fcntl(1, F_SETFL, fl | O_NONBLOCK);
    }
    g_ev.last_ms = cli_now_ms();
    AIRY_STRNCPY_TERM(g_ev.state, "idle", sizeof(g_ev.state));
    ev_ring_clear();
    return 0;
}

void cli_ev_fini(void)
{
    if (g_ev.nonblock)
        (void)fcntl(1, F_SETFL, g_ev.flags);
    g_ev.nonblock = 0;
    ev_ring_clear();
}
#else
/* Windows 控制台/管道无 O_NONBLOCK 语义：退化为阻塞写，对端关闭即失败。 */
static int ev_write(const void *buf, size_t n, int budget)
{
    (void)budget;
    return _write(1, buf, (unsigned)n) == (int)n ? 1 : -1;
}

int cli_ev_init(void)
{
    (void)_setmode(1, _O_BINARY);
    g_ev.last_ms = cli_now_ms();
    AIRY_STRNCPY_TERM(g_ev.state, "idle", sizeof(g_ev.state));
    ev_ring_clear();
    return 0;
}

void cli_ev_fini(void)
{
    ev_ring_clear();
}
#endif

void cli_ev_io_set(cli_ev_wr_t wr, void *ud)
{
    g_ev.wr = wr;
    g_ev.ud = ud;
}

void cli_ev_begin(void)
{
    g_ev.terminal = 0;
    g_ev.dropped = 0;
    g_ev.last_ms = cli_now_ms();
    g_ev.closed = 0;
    AIRY_STRNCPY_TERM(g_ev.state, "running", sizeof(g_ev.state));
}

void cli_ev_state(const char *st)
{
    if (st && *st)
        AIRY_STRNCPY_TERM(g_ev.state, st, sizeof(g_ev.state));
}

int cli_ev_closed(void) { return g_ev.closed; }
unsigned cli_ev_seq(void) { return g_ev.seq; }
unsigned cli_ev_dropped(void) { return g_ev.dropped; }
int cli_ev_terminal(void) { return g_ev.terminal; }

/* 写出一帧：类型决定预算；超时丢弃计入 dropped，终态超时判死。 */
static int ev_out(const char *type, const char *pl, size_t plen)
{
    uint64_t ts = cli_now_ms();
    unsigned seq = g_ev.seq + 1;
    unsigned drop = cli_ev_is_term(type) ? g_ev.dropped : 0;
    int tr = 0;
    size_t need;
    char *buf;
    int rc;

    if (g_ev.closed)
        return -1;
    need = cli_ev_build(NULL, 0, seq, ts, type, pl, plen, drop, &tr);
    if (need == 0 || need >= CLI_EV_MAX + 4096)
        return -1;
    buf = (char *)AIRY_MALLOC(need + 1);
    if (!buf)
        return -1;
    cli_ev_build(buf, need + 1, seq, ts, type, pl, plen, drop, &tr);
    g_ev.seq = seq;
    ev_push(seq, ts, type, pl, plen, tr);
    rc = g_ev.wr ? g_ev.wr(buf, need, g_ev.ud)
                 : ev_write(buf, need, cli_ev_is_term(type) ? CLI_EV_TERM_MS
                                                            : CLI_EV_DROP_MS);
    AIRY_FREE(buf);
    g_ev.last_ms = cli_now_ms();
    if (rc < 0) {
        g_ev.closed = 1;
        return -1;
    }
    if (rc == 0) {
        if (cli_ev_is_term(type)) {
            g_ev.closed = 1;
            return -1;
        }
        g_ev.dropped++;
    }
    if (cli_ev_is_term(type))
        g_ev.terminal = 1;
    return 0;
}

int cli_ev_emit(const char *type, const char *payload, size_t plen)
{
    return ev_out(type, payload, payload ? plen : 0);
}

int cli_ev_beat(void)
{
    char pl[48];

    if (g_ev.closed)
        return -1;
    if (g_ev.last_ms && cli_now_ms() - g_ev.last_ms < CLI_EV_BEAT_MS)
        return 0;
    snprintf(pl, sizeof(pl), "{\"state\":\"%s\"}", g_ev.state);
    return cli_ev_emit("heartbeat", pl, strlen(pl)) == 0 ? 1 : -1;
}

int cli_ev_note(const char *msg)
{
    char pl[1024];
    char esc[900];
    size_t o = 0;

    if (!msg)
        msg = "";
    for (; *msg && o + 2 < sizeof(esc); msg++) {
        unsigned char c = (unsigned char)*msg;

        if (c == '"' || c == '\\') {
            esc[o++] = '\\';
            esc[o++] = (char)c;
        } else if (c == '\n' || c == '\t' || c == '\r') {
            esc[o++] = ' ';
        } else if (c < 0x20) {
            continue;
        } else {
            esc[o++] = (char)c;
        }
    }
    esc[o] = '\0';
    snprintf(pl, sizeof(pl), "{\"message\":\"%s\"}", esc);
    return cli_ev_emit("notice", pl, strlen(pl));
}

/* 快照帧：当前状态 + 最近 8 帧 payload（时序升序），随后补发 seq>last_seq
 * 的缓冲帧（原 seq/ts，保证续传不重不漏）。 */
void cli_ev_snap(unsigned last_seq)
{
    const ev_frame_t *recent[8];
    const char *pl;
    size_t cap = CLI_EV_KEEP + 8192;
    char *buf;
    unsigned upto = g_ev.seq;
    int nr = 0;
    int n;
    int i;

    buf = (char *)AIRY_MALLOC(cap);
    if (!buf)
        return;
    for (i = 0; i < g_ev.ring_n && nr < 8; i++) {
        const ev_frame_t *f = &g_ev.ring[(g_ev.ring_head - 1 - i + CLI_EV_RING * 2) %
                                        CLI_EV_RING];
        recent[nr++] = f;
    }
    n = snprintf(buf, cap, "{\"state\":\"%s\",\"last_seq\":%u,\"recent\":[",
                 g_ev.state, last_seq);
    for (i = nr - 1; i >= 0 && n > 0 && (size_t)n + 2 < cap; i--) {
        size_t fl = recent[i]->pl ? recent[i]->plen : 2;

        if ((size_t)n + fl + 2 >= cap)
            break;
        if (n > 0 && buf[n - 1] != '[')
            buf[n++] = ',';
        AIRY_MEMCPY(buf + n, recent[i]->pl ? recent[i]->pl : "{}", fl);
        n += (int)fl;
    }
    if (n > 0 && (size_t)n + 2 < cap) {
        buf[n++] = ']';
        buf[n++] = '}';
        buf[n] = '\0';
        (void)cli_ev_emit("snapshot", buf, (size_t)n);
    }
    for (i = 0; i < g_ev.ring_n && !g_ev.closed; i++) {
        const ev_frame_t *f = &g_ev.ring[(g_ev.ring_head - g_ev.ring_n + i +
                                          CLI_EV_RING * 2) % CLI_EV_RING];
        size_t need;
        int tr = 0;
        char *fb;

        if (f->seq <= last_seq || f->seq > upto)
            continue;
        pl = f->pl ? f->pl : "{}";
        need = cli_ev_build(NULL, 0, f->seq, f->ts, f->type, pl, f->plen, 0, &tr);
        if (need == 0)
            continue;
        fb = (char *)AIRY_MALLOC(need + 1);
        if (!fb)
            continue;
        cli_ev_build(fb, need + 1, f->seq, f->ts, f->type, pl, f->plen, 0, &tr);
        if (g_ev.wr)
            (void)g_ev.wr(fb, need, g_ev.ud);
        else if (ev_write(fb, need, CLI_EV_DROP_MS) < 0)
            g_ev.closed = 1;
        AIRY_FREE(fb);
    }
    AIRY_FREE(buf);
}
