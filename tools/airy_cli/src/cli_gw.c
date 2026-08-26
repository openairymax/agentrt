/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw.c
 * @brief 轻量 HTTP/1.1 JSON-RPC 客户端（统一经 gateway 访问微核心服务）。
 *
 * 架构约束（2026-08-25）：所有客户端必须经 gateway 派发，禁止直连 daemon
 * socket。airy_cli 不依赖 curl（保持零外部 HTTP 依赖），本模块用 POSIX /
 * Winsock 原生 socket 实现 HTTP/1.1：
 *   - 非流式：POST /（JSON-RPC），解析 Content-Length 读完整响应；
 *   - 流式对话：POST /api/v1/chat/stream（SSE），逐 data: 行回调；
 *   - /health 可达性探测。
 */

#include "airy_memory.h"
#include "cli_gw.h"
#include "logger.h"
#include "platform.h"
#include "string_compat.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define CLI_GW_DEFAULT_HOST "127.0.0.1"
#define CLI_GW_DEFAULT_PORT 8080
#define CLI_GW_RECV_CHUNK 8192
#define CLI_GW_MAX_BODY (64u * 1024u * 1024u) /* 64MiB 响应上限 */

void cli_gw_endpoint(char *host, size_t host_len, int *port)
{
    *port = CLI_GW_DEFAULT_PORT;
    AIRY_STRNCPY_TERM(host, CLI_GW_DEFAULT_HOST, host_len);
    const char *gw = getenv("AIRY_GATEWAY_URL");
    if (!gw || !*gw)
        return;
    /* 支持 http://host:port / host:port 两种形态 */
    const char *h = strstr(gw, "://");
    const char *start = h ? h + 3 : gw;
    const char *colon = strrchr(start, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - start);
        if (hlen > 0 && hlen < host_len)
            AIRY_STRNCPY_TERM(host, start, hlen + 1);
        else
            AIRY_STRNCPY_TERM(host, start, host_len);
        if (colon[1])
            *port = atoi(colon + 1);
    } else {
        AIRY_STRNCPY_TERM(host, start, host_len);
    }
    if (*port <= 0 || *port > 65535)
        *port = CLI_GW_DEFAULT_PORT;
}

/* ── 底层 socket 连接（带超时）────────────────────────────────────── */
static int cli_gw_connect(const char *host, int port, int timeout_ms)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return -1;
    }
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    struct sockaddr_in sa;
    __builtin_memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        closesocket(s);
        WSACleanup();
        return -1;
    }
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
        return (int)s;
    }
    fd_set wf;
    FD_ZERO(&wf);
    FD_SET(s, &wf);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (long)(timeout_ms % 1000) * 1000L;
    if (select(0, NULL, &wf, NULL, &tv) > 0) {
        int soerr = 0;
        int slen = sizeof(soerr);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &slen);
        if (soerr == 0)
            return (int)s;
    }
    closesocket(s);
    WSACleanup();
    return -1;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    /* P2-2：F_GETFL 失败时 flags=-1，F_SETFL 会破坏 fd 标志位 */
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(s, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in sa;
    __builtin_memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        close(s);
        return -1;
    }
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
        int fl2 = fcntl(s, F_GETFL, 0);
        if (fl2 >= 0)
            (void)fcntl(s, F_SETFL, fl2 & ~O_NONBLOCK);
        return s;
    }
    if (errno == EINPROGRESS) {
        struct pollfd pfd;
        pfd.fd = s;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, timeout_ms) > 0) {
            int soerr = 0;
            socklen_t slen = sizeof(soerr);
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &slen) == 0 && soerr == 0) {
                int fl2 = fcntl(s, F_GETFL, 0);
                if (fl2 >= 0)
                    (void)fcntl(s, F_SETFL, fl2 & ~O_NONBLOCK);
                return s;
            }
        }
    }
    close(s);
    return -1;
#endif
}

/* ── HTTP 请求发送 + 响应体读取（返回 malloc'd body，调用方 AIRY_FREE）── */
static int cli_gw_exchange(const char *host, int port, const char *path, const char *body,
                           int timeout_ms, char **out_body, size_t *out_body_len)
{
    *out_body = NULL;
    *out_body_len = 0;
    int fd = cli_gw_connect(host, port, timeout_ms);
    if (fd < 0)
        return -1;

    char req[512];
    int reqn = snprintf(req, sizeof(req),
                        "POST %s HTTP/1.1\r\n"
                        "Host: %s:%d\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "Accept: text/event-stream\r\n"
                        "\r\n",
                        path, host, port, body ? strlen(body) : 0);
    /* P2: snprintf returns the would-be length; if it exceeds the stack
     * buffer the header was truncated and the memcpy below would over-read. */
    if (reqn < 0 || (size_t)reqn >= sizeof(req)) {
        fprintf(stderr, "cli_gw: request header too long (path/host)\n");
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return -1;
    }

    /* 发送（先请求头 + body） */
    size_t to_send = (size_t)reqn + (body ? strlen(body) : 0);
    size_t sent = 0;
    if (reqn > 0)
        sent += (size_t)reqn;
    if (body)
        sent += strlen(body);
    char *buf = (char *)AIRY_MALLOC(to_send + 1);
    if (!buf) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return -1;
    }
    __builtin_memcpy(buf, req, (size_t)reqn);
    if (body)
        __builtin_memcpy(buf + reqn, body, strlen(body));
    size_t off = 0;
    while (off < to_send) {
#ifdef _WIN32
        int n = send(fd, buf + off, (int)(to_send - off), 0);
#else
        ssize_t n = send(fd, buf + off, to_send - off, 0);
#endif
        if (n <= 0)
            break;
        off += (size_t)n;
    }
    AIRY_FREE(buf);
    if (off < to_send) {
#ifdef _WIN32
        closesocket(fd);
        WSACleanup();
#else
        close(fd);
#endif
        return -1;
    }

    /* 接收（先收头，找 Content-Length / chunked，再收 body） */
    size_t cap = CLI_GW_RECV_CHUNK;
    char *resp = (char *)AIRY_MALLOC(cap);
    if (!resp) {
#ifdef _WIN32
        closesocket(fd);
        WSACleanup();
#else
        close(fd);
#endif
        return -1;
    }
    size_t rlen = 0;
    int header_done = 0;
    size_t content_len = 0;
    int is_chunked = 0;
    long long total_timeout = (long long)timeout_ms;
#ifdef _WIN32
    while (total_timeout > 0) {
        int n = recv(fd, resp + rlen, (int)(cap - rlen), 0);
        if (n == 0)
            break;
        if (n < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(10);
                total_timeout -= 10;
                continue;
            }
            break;
        }
        rlen += (size_t)n;
        if (rlen >= cap) {
            cap *= 2;
            if (cap > CLI_GW_MAX_BODY) {
                AIRY_FREE(resp);
                closesocket(fd);
                WSACleanup();
                return -1;
            }
            char *nb = (char *)AIRY_REALLOC(resp, cap);
            if (!nb) {
                AIRY_FREE(resp);
                closesocket(fd);
                WSACleanup();
                return -1;
            }
            resp = nb;
        }
        if (!header_done) {
            char *he = memmem(resp, rlen, "\r\n\r\n", 4);
            if (he) {
                header_done = 1;
                size_t hlen = (size_t)(he - resp) + 4;
                /* 解析 Content-Length */
                char *cl = memmem(resp, hlen, "Content-Length:", 15);
                if (!cl)
                    cl = memmem(resp, hlen, "content-length:", 15);
                if (cl) {
                    content_len = (size_t)strtoull(cl + 15, NULL, 10);
                }
                if (memmem(resp, hlen, "Transfer-Encoding:", 18) &&
                    memmem(resp, hlen, "chunked", 7))
                    is_chunked = 1;
                /* 剩余可读量判断完成 */
                if (!is_chunked && content_len > 0 && (rlen - hlen) >= content_len)
                    break;
                if (!is_chunked && content_len == 0 && (rlen - hlen) > 0)
                    break;
            }
        } else {
            if (!is_chunked && content_len > 0 && (rlen - (rlen > 0 ? 1 : 0)) >= content_len)
                break;
        }
    }
#else
    while (total_timeout > 0) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, (int)(total_timeout > 100 ? 100 : total_timeout));
        if (pr <= 0)
            break;
        ssize_t n = recv(fd, resp + rlen, cap - rlen, 0);
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        rlen += (size_t)n;
        if (rlen >= cap) {
            cap *= 2;
            if (cap > CLI_GW_MAX_BODY) {
                AIRY_FREE(resp);
                close(fd);
                return -1;
            }
            char *nb = (char *)AIRY_REALLOC(resp, cap);
            if (!nb) {
                AIRY_FREE(resp);
                close(fd);
                return -1;
            }
            resp = nb;
        }
        if (!header_done) {
            char *he = memmem(resp, rlen, "\r\n\r\n", 4);
            if (he) {
                header_done = 1;
                size_t hlen = (size_t)(he - resp) + 4;
                char *cl = memmem(resp, hlen, "Content-Length:", 15);
                if (!cl)
                    cl = memmem(resp, hlen, "content-length:", 15);
                if (cl)
                    content_len = (size_t)strtoull(cl + 15, NULL, 10);
                if (memmem(resp, hlen, "Transfer-Encoding:", 18) &&
                    memmem(resp, hlen, "chunked", 7))
                    is_chunked = 1;
                if (!is_chunked && content_len > 0 && (rlen - hlen) >= content_len)
                    break;
                if (!is_chunked && content_len == 0 && (rlen - hlen) > 0)
                    break;
            }
        } else if (!is_chunked && content_len > 0 && rlen >= 4 && (rlen - 4) >= content_len) {
            break;
        }
    }
#endif
#ifdef _WIN32
    closesocket(fd);
    WSACleanup();
#else
    close(fd);
#endif

    if (rlen == 0) {
        AIRY_FREE(resp);
        return -1;
    }
    /* 定位 header 结束，body = 其后内容 */
    char *he = memmem(resp, rlen, "\r\n\r\n", 4);
    if (!he) {
        AIRY_FREE(resp);
        return -1;
    }
    size_t hlen = (size_t)(he - resp) + 4;
    char *body_start = resp + hlen;
    size_t body_len = rlen - hlen;
    if (is_chunked) {
        /* 简单 chunked 解码（CLI 侧 SSE 场景） */
        char *out = (char *)AIRY_MALLOC(body_len + 1);
        if (!out) {
            AIRY_FREE(resp);
            return -1;
        }
        size_t o = 0;
        char *p = body_start;
        char *end = body_start + body_len;
        while (p < end) {
            char *nl = memmem(p, (size_t)(end - p), "\r\n", 2);
            if (!nl)
                break;
            size_t sz = (size_t)strtoull(p, NULL, 16);
            if (sz == 0)
                break;
            if ((size_t)(end - (nl + 2)) < sz)
                break;
            __builtin_memcpy(out + o, nl + 2, sz);
            o += sz;
            p = nl + 2 + (ssize_t)sz + 2;
        }
        out[o] = '\0';
        AIRY_FREE(resp);
        *out_body = out;
        *out_body_len = o;
        return 0;
    }
    char *out = (char *)AIRY_MALLOC(body_len + 1);
    if (!out) {
        AIRY_FREE(resp);
        return -1;
    }
    __builtin_memcpy(out, body_start, body_len);
    out[body_len] = '\0';
    AIRY_FREE(resp);
    *out_body = out;
    *out_body_len = body_len;
    return 0;
}

/* ── JSON-RPC over HTTP POST / ─────────────────────────────────────── */
int cli_gw_call(const char *method, const char *params_json, int timeout_ms, char **out_result)
{
    if (!method || !out_result)
        return -1;
    *out_result = NULL;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);

    char body[4096];
    char params_buf[512] = "";
    if (params_json && *params_json)
        __builtin_snprintf(params_buf, sizeof(params_buf), "%s", params_json);
    int bn = snprintf(body, sizeof(body), "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"%s\",\"params\":%s}",
                      method, params_json ? params_json : "{}");
    if (bn <= 0 || bn >= (int)sizeof(body))
        return -1;

    char *resp = NULL;
    size_t rlen = 0;
    if (cli_gw_exchange(host, port, "/", body, timeout_ms, &resp, &rlen) != 0) {
        AIRY_FREE(resp);
        AIRY_LOG_WARN("cli_gw: gateway unreachable at %s:%d (method=%s)", host, port, method);
        return -1;
    }
    if (!resp) {
        return -1;
    }
    /* 提取 JSON-RPC result（忽略 id 字段） */
    cJSON *root = cJSON_Parse(resp);
    AIRY_FREE(resp);
    if (!root)
        return -1;
    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (err) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!result) {
        cJSON_Delete(root);
        return -1;
    }
    char *out = cJSON_PrintUnformatted(result);
    cJSON_Delete(root);
    if (!out)
        return -1;
    *out_result = out;
    return 0;
}

/* ── SSE 流式对话（POST /api/v1/chat/stream） ─────────────────────── */
int cli_gw_stream(const char *payload, int timeout_ms, cli_gw_line_cb cb, void *ud)
{
    if (!payload)
        return -1;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);

    char *resp = NULL;
    size_t rlen = 0;
    if (cli_gw_exchange(host, port, "/api/v1/chat/stream", payload, timeout_ms, &resp, &rlen) !=
        0) {
        AIRY_FREE(resp);
        return -1;
    }
    if (!resp)
        return -1;

    /* SSE：逐 data: 行解析 */
    char *p = resp;
    char *end = resp + rlen;
    int done = 0;
    while (p < end && !done) {
        char *nl = memmem(p, (size_t)(end - p), "\n", 1);
        size_t linelen = nl ? (size_t)(nl - p) : (size_t)(end - p);
        char *line = (char *)AIRY_MALLOC(linelen + 1);
        if (!line)
            break;
        __builtin_memcpy(line, p, linelen);
        line[linelen] = '\0';
        /* 去掉行尾 \r */
        if (linelen > 0 && line[linelen - 1] == '\r')
            line[linelen - 1] = '\0';
        if (strncmp(line, "data:", 5) == 0) {
            const char *val = line + 5;
            while (*val == ' ')
                val++;
            if (strcmp(val, "[DONE]") == 0) {
                done = 1;
            } else if (cb) {
                cb(val, ud);
            }
        }
        AIRY_FREE(line);
        if (!nl)
            break;
        p = nl + 1;
    }
    AIRY_FREE(resp);
    return done ? 0 : -1;
}

/* ── /health 可达性 ───────────────────────────────────────────────── */
int cli_gw_health(int timeout_ms)
{
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    char *resp = NULL;
    size_t rlen = 0;
    if (cli_gw_exchange(host, port, "/health", NULL, timeout_ms, &resp, &rlen) != 0) {
        AIRY_FREE(resp);
        return 0;
    }
    int ok = (resp && strstr(resp, "\"healthy\"")) ? 1 : 0;
    AIRY_FREE(resp);
    return ok;
}
