// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_gw_sse.c
 * @brief gateway SSE 客户端：HTTP 头握手、chunked 解码与事件行翻译。
 *
 * 只搬运字节，不解释业务语义：agent_d 的每个事件帧由 gateway 包成
 * `data: <json>\n\n`，本模块解出 json 交给事件桥。连接与请求构造复用
 * cli_gw.c 导出的传输薄面，避免第二套 HTTP 实现。
 */

#include "cli_sse.h"
#include "cli_gw.h"
#include "cli_gw_int.h"
#include "cli_render.h"
#include "airy_memory.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define SSE_RECV_CHUNK 8192

struct cli_sse {
    int fd;
    int http;
    int eof;
    size_t rawlen;
    char raw[CLI_SSE_HDR_MAX];
    cli_dc_t dc;
};

/* ── chunked 解码 ──────────────────────────────────────────────── */

void cli_dc_init(cli_dc_t *dc, int chunked)
{
    if (!dc)
        return;
    AIRY_MEMSET(dc, 0, sizeof(*dc));
    dc->chunked = chunked;
}

static int dc_app(cli_dc_t *dc, const char *p, size_t n)
{
    char *nb;
    size_t cap;

    if (n == 0)
        return 0;
    if (dc->len + n + 1 > dc->cap) {
        cap = dc->cap ? dc->cap : SSE_RECV_CHUNK;
        while (cap < dc->len + n + 1)
            cap *= 2;
        nb = (char *)AIRY_REALLOC(dc->buf, cap);
        if (!nb)
            return -1;
        dc->buf = nb;
        dc->cap = cap;
    }
    AIRY_MEMCPY(dc->buf + dc->len, p, n);
    dc->len += n;
    dc->buf[dc->len] = '\0';
    return 0;
}

const char *cli_dc_plain(cli_dc_t *dc, size_t *len)
{
    if (len)
        *len = dc ? dc->len : 0;
    return dc ? dc->buf : NULL;
}

void cli_dc_take(cli_dc_t *dc, size_t n)
{
    if (!dc || n == 0)
        return;
    if (n >= dc->len) {
        dc->len = 0;
        if (dc->buf)
            dc->buf[0] = '\0';
        return;
    }
    AIRY_MEMMOVE(dc->buf, dc->buf + n, dc->len - n);
    dc->len -= n;
    dc->buf[dc->len] = '\0';
}

void cli_dc_free(cli_dc_t *dc)
{
    if (!dc)
        return;
    AIRY_FREE(dc->buf);
    dc->buf = NULL;
    dc->len = 0;
    dc->cap = 0;
}

/* 块长行：十六进制（可带 ;ext），以 \n 收尾。 */
static int dc_size(cli_dc_t *dc, char c)
{
    unsigned long long v;

    if (c != '\n') {
        if (c != '\r' && dc->szlen + 1 < sizeof(dc->sz))
            dc->sz[dc->szlen++] = c;
        return 0;
    }
    dc->sz[dc->szlen] = '\0';
    v = strtoull(dc->sz, NULL, 16);
    dc->szlen = 0;
    dc->remain = v;
    dc->pad = 0;
    if (v == 0) {
        dc->done = 1;
        dc->state = 3;
    } else {
        dc->state = 1;
    }
    return 0;
}

int cli_dc_feed(cli_dc_t *dc, const char *in, size_t n)
{
    size_t i = 0;

    if (!dc || !in)
        return -1;
    if (!dc->chunked)
        return dc_app(dc, in, n);
    while (i < n && !dc->done) {
        if (dc->state == 0) {
            (void)dc_size(dc, in[i++]);
        } else if (dc->state == 1) {
            size_t take = n - i < dc->remain ? n - i : (size_t)dc->remain;

            if (dc_app(dc, in + i, take) < 0)
                return -1;
            i += take;
            dc->remain -= take;
            if (dc->remain == 0) {
                dc->state = 2;
                dc->pad = 0;
            }
        } else if (dc->state == 2) {
            char c = in[i++];

            if (c != '\r' && c != '\n')
                return -1;
            if (++dc->pad == 2) {
                dc->state = 0;
                dc->pad = 0;
            }
        } else {
            return 0;
        }
    }
    return 0;
}

/* ── SSE 行翻译 ────────────────────────────────────────────────── */

int cli_sse_line(const char *line, size_t len, char *out, size_t cap)
{
    if (!line || !out || cap == 0)
        return -1;
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        len--;
    if (len == 0 || line[0] == ':')
        return 0;
    if (len < 5 || memcmp(line, "data:", 5) != 0)
        return 0;
    line += 5;
    len -= 5;
    if (len > 0 && *line == ' ') {
        line++;
        len--;
    }
    if (len == 6 && memcmp(line, "[DONE]", 6) == 0)
        return 0;
    if (len == 0)
        return 0;
    if (len + 1 > cap)
        return -1;
    AIRY_MEMCPY(out, line, len);
    out[len] = '\0';
    return 1;
}

/* ── 传输 ──────────────────────────────────────────────────────── */

static int sse_wait(int fd, int wait_ms)
{
#ifdef _WIN32
    fd_set rf;
    struct timeval tv;

    FD_ZERO(&rf);
    FD_SET((SOCKET)fd, &rf);
    tv.tv_sec = wait_ms / 1000;
    tv.tv_usec = (long)(wait_ms % 1000) * 1000L;
    return select(0, &rf, NULL, NULL, &tv);
#else
    struct pollfd p = {fd, POLLIN, 0};

    return poll(&p, 1, wait_ms);
#endif
}

static char *sse_find(char *b, size_t n, const char *pat, size_t pl)
{
    size_t i;

    if (pl == 0 || n < pl)
        return NULL;
    for (i = 0; i + pl <= n; i++)
        if (memcmp(b + i, pat, pl) == 0)
            return b + i;
    return NULL;
}

static int sse_has(const char *h, size_t n, const char *k)
{
    size_t kl = strlen(k);
    size_t i;
    size_t j;

    if (kl == 0 || n < kl)
        return 0;
    for (i = 0; i + kl <= n; i++) {
        for (j = 0; j < kl; j++)
            if (tolower((unsigned char)h[i + j]) != (unsigned char)k[j])
                break;
        if (j == kl)
            return 1;
    }
    return 0;
}

/* 状态行 "HTTP/1.x NNN"：返回 100~599，无法识别返回 0。 */
static int sse_status(const char *h, size_t n)
{
    const char *sp;
    const char *c;

    if (!h || n < 12 || memcmp(h, "HTTP/", 5) != 0)
        return 0;
    sp = (const char *)memchr(h, ' ', n);
    if (!sp || (size_t)(sp - h) + 4 > n)
        return 0;
    c = sp + 1;
    if (c[0] < '1' || c[0] > '5' || c[1] < '0' || c[1] > '9' ||
        c[2] < '0' || c[2] > '9')
        return 0;
    return (c[0] - '0') * 100 + (c[1] - '0') * 10 + (c[2] - '0');
}

/* 接收响应头直到空行；剩余字节留给 chunked 解码。 */
static int sse_hdr(cli_sse_t *s, int timeout_ms)
{
    uint64_t dl = cli_now_ms() + (uint64_t)timeout_ms;

    for (;;) {
        char *he = sse_find(s->raw, s->rawlen, "\r\n\r\n", 4);
        size_t used;
        size_t left;

        if (he) {
            used = (size_t)(he - s->raw) + 4;
            left = s->rawlen - used;
            s->http = sse_status(s->raw, used);
            s->dc.chunked = sse_has(s->raw, used, "chunked");
            if (left)
                AIRY_MEMMOVE(s->raw, s->raw + used, left);
            s->rawlen = left;
            return 0;
        }
        if (s->rawlen >= CLI_SSE_HDR_MAX)
            return -1;
        {
            uint64_t now = cli_now_ms();
            int w;

            if (now >= dl)
                return -1;
            w = (int)(dl - now);
            if (w > 100)
                w = 100;
            w = sse_wait(s->fd, w);
            if (w < 0)
                return -1;
            if (w == 0)
                continue;
        }
        {
            int n = (int)recv(s->fd, s->raw + s->rawlen,
                              (int)(sizeof(s->raw) - s->rawlen), 0);

            if (n <= 0)
                return -1;
            s->rawlen += (size_t)n;
        }
    }
}

cli_sse_t *cli_sse_open(const char *path, const char *body, int timeout_ms)
{
    char host[256];
    int port = 0;
    int rn;
    char req[1024];
    cli_sse_t *s;

    if (!path || !body)
        return NULL;
    cli_gw_endpoint(host, sizeof(host), &port);
    s = (cli_sse_t *)AIRY_CALLOC(1, sizeof(*s));
    if (!s)
        return NULL;
    s->fd = cli_gw_conn_open(host, port, timeout_ms);
    if (s->fd < 0)
        goto fail;
    rn = cli_gw_req_build("POST", host, port, path, body, req, sizeof(req));
    if (rn < 0 || cli_gw_req_send(s->fd, req, (size_t)rn, body) != 0)
        goto fail;
    if (sse_hdr(s, timeout_ms) != 0)
        goto fail;
    {
        int chunked = s->dc.chunked;

        cli_dc_init(&s->dc, chunked);
    }
    if (s->rawlen) {
        if (cli_dc_feed(&s->dc, s->raw, s->rawlen) < 0)
            goto fail;
        s->rawlen = 0;
    }
    if (s->http != 200)
        s->eof = 1;
    return s;
fail:
    if (s->fd >= 0)
        cli_gw_conn_close(s->fd);
    cli_dc_free(&s->dc);
    AIRY_FREE(s);
    return NULL;
}

/* 从已解码字节中取一行事件；1=事件，0=暂无完整行，-1=超长。 */
static int sse_take(cli_sse_t *s, char *out, size_t cap)
{
    for (;;) {
        size_t len = 0;
        const char *p = cli_dc_plain(&s->dc, &len);
        const char *nl;
        size_t ll;
        int r;

        if (!p || len == 0)
            return 0;
        nl = (const char *)memchr(p, '\n', len);
        if (!nl)
            return 0;
        ll = (size_t)(nl - p);
        r = cli_sse_line(p, ll, out, cap);
        cli_dc_take(&s->dc, ll + 1);
        if (r != 0)
            return r;
    }
}

int cli_sse_next(cli_sse_t *s, char *out, size_t cap, int wait_ms)
{
    int r;

    if (!s || !out)
        return -1;
    r = sse_take(s, out, cap);
    if (r != 0)
        return r;
    if (s->eof)
        return -1;
    {
        int w = sse_wait(s->fd, wait_ms);

        if (w < 0)
            return -1;
        if (w == 0)
            return 0;
    }
    {
        char tmp[SSE_RECV_CHUNK];
        int n = (int)recv(s->fd, tmp, sizeof(tmp), 0);

        if (n <= 0) {
            s->eof = 1;
            return -1;
        }
        if (cli_dc_feed(&s->dc, tmp, (size_t)n) < 0)
            return -1;
    }
    return sse_take(s, out, cap);
}

int cli_sse_http(const cli_sse_t *s)
{
    return s ? s->http : 0;
}

void cli_sse_close(cli_sse_t *s)
{
    if (!s)
        return;
    if (s->fd >= 0)
        cli_gw_conn_close(s->fd);
    cli_dc_free(&s->dc);
    AIRY_FREE(s);
}
