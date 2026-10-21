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
 *   - /health 可达性探测。
 */

#include "airy_memory.h"
#include "airy_defaults.h"
#include "cli_gw.h"
#include "error_codes.h" /* AIRY_ERR_CANCELED（S-02：取消语义与 sched.dag_cancel 对齐） */
#include "logger.h"
#include "platform.h"
#include "string_compat.h"

#include <cjson/cJSON.h>

#include <signal.h> /* sig_atomic_t（g_cli_cancel 类型 SSoT） */
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
/* 端口 SSoT：commons airy_defaults.h 为唯一权威（禁止复刻字面量）。 */
#define CLI_GW_DEFAULT_PORT AIRY_PORT_GATEWAY_HTTP
#define CLI_GW_RECV_CHUNK 8192
#define CLI_GW_MAX_BODY (64u * 1024u * 1024u) /* 64MiB 响应上限 */

/* 网关 RPC 客户端错误描述缓冲（本模块唯一权威；外部经 cli_gw_last_err /
 * cli_gw_err_take / cli_gw_err_set 访问，禁止跨 TU 直接引用）。 */
static char g_cli_gw_err[256] = "";

const char *cli_gw_last_err(void)
{
    return g_cli_gw_err;
}

const char *cli_gw_err_take(void)
{
    static char taken[256];
    snprintf(taken, sizeof(taken), "%s", g_cli_gw_err);
    g_cli_gw_err[0] = '\0';
    return taken;
}

void cli_gw_err_set(const char *msg)
{
    snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s", msg ? msg : "");
}

/* cli_gw_exchange 内部失败细分（S-02 超时可诊断/可取消）：
 * -1 传输失败（连接/发送/协议）；-2 用户取消；-3 接收超时。 */
#define CLI_GW_EXCH_CANCELED (-2)
#define CLI_GW_EXCH_TIMEOUT (-3)

/* SIGINT 取消标志（main.c 拥有，cli_internal.h SSoT 声明 extern）：cmd 域
 * 仅读，接收等待循环中命中即中断。 */
extern volatile sig_atomic_t g_cli_cancel;

/* 读取 $AIRY_HOME/run/gateway.port（完整启动器在端口漂移后固化实际端口）。
 * 0.1.6h：gateway 被占漂移后，CLI 回退默认端口若与监听不一致会误报
 * "网关不在线"；与 TUI/轻量启动器同源读取，根治端口漂移断连。 */
static int cli_gw_read_runtime_port(void)
{
    const char *home = getenv("AIRY_HOME");
    char path[512];
    if (!home || !*home)
        home = "$HOME"; /* 占位，实际下面展开 */
    if (!home || !*home || home[0] == '$') {
        const char *h = getenv("HOME");
        if (!h || !*h)
            return -1;
        snprintf(path, sizeof(path), "%s/.airymaxrt/run/gateway.port", h);
    } else {
        snprintf(path, sizeof(path), "%s/run/gateway.port", home);
    }
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    char buf[32] = "";
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    /* 仅接受纯数字端口 */
    int p = 0;
    for (char *s = buf; *s; s++) {
        if (*s == '\n' || *s == '\r')
            break;
        if (*s < '0' || *s > '9')
            return -1;
        p = p * 10 + (*s - '0');
    }
    return (p > 0 && p <= 65535) ? p : -1;
}

void cli_gw_endpoint(char *host, size_t host_len, int *port)
{
    *port = CLI_GW_DEFAULT_PORT;
    AIRY_STRNCPY_TERM(host, CLI_GW_DEFAULT_HOST, host_len);
    const char *gw = getenv("AIRY_GATEWAY_URL");
    if (!gw || !*gw) {
        /* 0.1.6h：无显式 URL 时读取运行时实际端口（端口漂移兼容） */
        int rp = cli_gw_read_runtime_port();
        if (rp > 0)
            *port = rp;
        return;
    }
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

/* 接收累积态：响应缓冲与解析进度随一次 exchange 存活，栈上持有。完成
 * 判定统一为 (len - body_off) >= content_len（旧 POSIX 腿用固定 4 近似
 * body_off，大响应头下会提前 break）。 */
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    size_t body_off;
    size_t content_len;
    long long budget;
    int header_done;
    int chunked;
} gw_rx_t;

/* 平台读腿的返回细分：0=已读到（*out 为 0 表示结束），1=继续等待。 */
#define GW_RX_ERROR (-1)
#define GW_RX_CANCEL (-2)
#define GW_RX_MORE 1

/* 解析响应头（仅首次）：定位头结束、Content-Length 与 chunked 标记。 */
static void gw_rx_hdr(gw_rx_t *rx)
{
    char *he = memmem(rx->buf, rx->len, "\r\n\r\n", 4);
    size_t hlen;
    char *cl;

    if (!he)
        return;
    rx->header_done = 1;
    hlen = (size_t)(he - rx->buf) + 4;
    rx->body_off = hlen;
    cl = memmem(rx->buf, hlen, "Content-Length:", 15);
    if (!cl)
        cl = memmem(rx->buf, hlen, "content-length:", 15);
    if (cl)
        rx->content_len = (size_t)strtoull(cl + 15, NULL, 10);
    if (memmem(rx->buf, hlen, "Transfer-Encoding:", 18) &&
        memmem(rx->buf, hlen, "chunked", 7))
        rx->chunked = 1;
}

/* 追加 n 字节到接收缓冲，并按 Content-Length / chunked 判定是否收全。
 * 返回 1=响应完整；0=继续接收；-1=超过 CLI_GW_MAX_BODY 或扩容失败。 */
static int gw_recv_feed(gw_rx_t *rx, size_t n)
{
    rx->len += n;
    if (rx->len >= rx->cap) {
        size_t cap = rx->cap * 2;
        char *nb;

        if (cap > CLI_GW_MAX_BODY) {
            AIRY_FREE(rx->buf);
            rx->buf = NULL;
            return -1;
        }
        nb = (char *)AIRY_REALLOC(rx->buf, cap);
        if (!nb) {
            AIRY_FREE(rx->buf);
            rx->buf = NULL;
            return -1;
        }
        rx->buf = nb;
        rx->cap = cap;
    }

    if (!rx->header_done)
        gw_rx_hdr(rx);
    if (!rx->chunked && rx->content_len > 0 &&
        (rx->len - rx->body_off) >= rx->content_len)
        return 1;
    if (!rx->chunked && rx->content_len == 0 && rx->body_off &&
        rx->len > rx->body_off)
        return 1;
    return 0;
}

#ifdef _WIN32
/* Winsock 读腿：非阻塞 recv；WSAEWOULDBLOCK 以 10ms 步进扣减预算。 */
static int gw_rd_win(int fd, gw_rx_t *rx, ssize_t *out)
{
    *out = recv(fd, rx->buf + rx->len, (int)(rx->cap - rx->len), 0);
    if (*out >= 0)
        return 0;
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        *out = 0;
        return 0;
    }
    Sleep(10);
    rx->budget -= 10;
    return GW_RX_MORE;
}
#else
/* POSIX 读腿：poll ≤100ms 后 recv；空转窗口继续等待直至总预算耗尽
 * （0.1.14 修复：原实现 poll 返回 0 即 break，响应到达前的任何 >100ms
 * 空闲都会被提前判死，误报"网关不在线"）。 */
static int gw_rd_posix(int fd, gw_rx_t *rx, ssize_t *out)
{
    struct pollfd pfd = { fd, POLLIN, 0 };
    int wait = (int)(rx->budget > 100 ? 100 : rx->budget);
    int pr = poll(&pfd, 1, wait);

    if (pr < 0)
        return (errno == EINTR) ? GW_RX_MORE : 0;
    if (pr == 0) {
        rx->budget -= wait;
        return GW_RX_MORE;
    }
    *out = recv(fd, rx->buf + rx->len, rx->cap - rx->len, 0);
    if (*out >= 0)
        return 0;
    if (errno == EINTR)
        return GW_RX_MORE;
    *out = 0;
    return 0;
}
#endif

#ifdef _WIN32
#define GW_RD(fd, rx, out) gw_rd_win((fd), (rx), (out))
#else
#define GW_RD(fd, rx, out) gw_rd_posix((fd), (rx), (out))
#endif

/* 接收主循环：平台读腿只负责一次等待+读取，完成判定与预算语义集中在此。 */
static int gw_rx_loop(int fd, gw_rx_t *rx)
{
    while (rx->budget > 0) {
        ssize_t n = 0;
        int rc;

        if (g_cli_cancel)
            return GW_RX_CANCEL;
        if (GW_RD(fd, rx, &n) == GW_RX_MORE)
            continue;
        if (n == 0)
            return 0;
        rc = gw_recv_feed(rx, (size_t)n);
        if (rc < 0)
            return GW_RX_ERROR;
        if (rc > 0)
            return 0;
    }
    return 0;
}

/* 解析状态行 "HTTP/1.x NNN ..."（仅头部区间，resp 未必以 NUL 结尾）。
 * 返回 100~599；无法识别返回 0（由调用方按未知处理）。 */
static int cli_gw_status_line(const char *resp, size_t hlen)
{
    if (!resp || hlen < 12 || memcmp(resp, "HTTP/", 5) != 0)
        return 0;
    const char *sp = (const char *)memchr(resp, ' ', hlen);
    if (!sp || (size_t)(sp - resp) + 4 > hlen)
        return 0;
    const char *c = sp + 1;
    if (c[0] < '1' || c[0] > '5' || c[1] < '0' || c[1] > '9' || c[2] < '0' || c[2] > '9')
        return 0;
    return (c[0] - '0') * 100 + (c[1] - '0') * 10 + (c[2] - '0');
}

/* 构造 HTTP/1.1 请求头（含 Content-Length）。返回头部字节数，超长返回 -1。 */
static int gw_build_req(const char *method, const char *host, int port,
                        const char *path, const char *body, char *req, size_t reqsz)
{
    int reqn = snprintf(req, reqsz,
                        "%s %s HTTP/1.1\r\n"
                        "Host: %s:%d\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "Accept: text/event-stream\r\n"
                        "\r\n",
                        method, path, host, port, body ? strlen(body) : 0);

    /* P2: snprintf 返回的是应写长度；超出栈缓冲即表示头部已被截断。 */
    if (reqn < 0 || (size_t)reqn >= reqsz) {
        fprintf(stderr, "cli_gw: request header too long (path/host)\n");
        return -1;
    }
    return reqn;
}

/* 发送请求头 + body（body 可为 NULL）；未发完即判传输失败。 */
static int gw_send_req(int fd, const char *req, size_t reqn, const char *body)
{
    size_t blen = body ? strlen(body) : 0;
    size_t to_send = reqn + blen;
    char *buf = (char *)AIRY_MALLOC(to_send + 1);
    size_t off = 0;

    if (!buf)
        return -1;
    __builtin_memcpy(buf, req, reqn);
    if (body)
        __builtin_memcpy(buf + reqn, body, blen);
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
    return (off < to_send) ? -1 : 0;
}

/* 接收态初始化：缓冲初值 CLI_GW_RECV_CHUNK，预算为 timeout_ms。 */
static int gw_rx_init(gw_rx_t *rx, int timeout_ms)
{
    AIRY_MEMSET(rx, 0, sizeof(*rx));
    rx->buf = (char *)AIRY_MALLOC(CLI_GW_RECV_CHUNK);
    if (!rx->buf)
        return -1;
    rx->cap = CLI_GW_RECV_CHUNK;
    rx->budget = (long long)timeout_ms;
    return 0;
}

/* 关闭连接（Winsock 腿需配平 WSAStartup）。 */
static void gw_close(int fd)
{
#ifdef _WIN32
    closesocket(fd);
    WSACleanup();
#else
    close(fd);
#endif
}

/* chunked 正文解码（CLI 侧 SSE 场景），返回 malloc'd 缓冲。 */
static char *gw_chunked_plain(const char *body, size_t body_len, size_t *out_len)
{
    char *out = (char *)AIRY_MALLOC(body_len + 1);
    size_t o = 0;
    char *p = (char *)body;
    char *end = (char *)body + body_len;

    if (!out)
        return NULL;
    while (p < end) {
        char *nl = memmem(p, (size_t)(end - p), "\r\n", 2);
        size_t sz;

        if (!nl)
            break;
        sz = (size_t)strtoull(p, NULL, 16);
        if (sz == 0)
            break;
        if ((size_t)(end - (nl + 2)) < sz)
            break;
        __builtin_memcpy(out + o, nl + 2, sz);
        o += sz;
        p = nl + 2 + sz + 2;
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

/* 非 chunked 正文复制（补 NUL 结尾）。 */
static char *gw_body_dup(const char *body, size_t body_len, size_t *out_len)
{
    char *out = (char *)AIRY_MALLOC(body_len + 1);

    if (!out)
        return NULL;
    __builtin_memcpy(out, body, body_len);
    out[body_len] = '\0';
    *out_len = body_len;
    return out;
}

/* 从接收态切出正文：定位头部、填状态码、按 chunked 解码。
 * 返回 0=成功；CLI_GW_EXCH_TIMEOUT=无完整头部且预算耗尽；-1=失败。 */
static int gw_body_take(gw_rx_t *rx, char **out_body, size_t *out_len, int *out_status)
{
    char *he;
    size_t hlen;
    size_t olen = 0;
    char *out;

    if (rx->len == 0)
        return (rx->budget <= 0) ? CLI_GW_EXCH_TIMEOUT : -1;
    he = memmem(rx->buf, rx->len, "\r\n\r\n", 4);
    if (!he)
        return (rx->budget <= 0) ? CLI_GW_EXCH_TIMEOUT : -1;
    hlen = (size_t)(he - rx->buf) + 4;
    if (out_status)
        *out_status = cli_gw_status_line(rx->buf, (size_t)(he - rx->buf));
    if (rx->chunked)
        out = gw_chunked_plain(rx->buf + hlen, rx->len - hlen, &olen);
    else
        out = gw_body_dup(rx->buf + hlen, rx->len - hlen, &olen);
    if (!out)
        return -1;
    *out_body = out;
    *out_len = olen;
    return 0;
}

static int cli_gw_exchange(const char *method, const char *host, int port, const char *path,
                           const char *body, int timeout_ms, char **out_body, size_t *out_body_len,
                           int *out_status)
{
    char req[512];
    int reqn;
    int fd;
    gw_rx_t rx;
    int rc;

    *out_body = NULL;
    *out_body_len = 0;
    if (out_status)
        *out_status = 0;
    reqn = gw_build_req(method, host, port, path, body, req, sizeof(req));
    if (reqn < 0)
        return -1;
    fd = cli_gw_connect(host, port, timeout_ms);
    if (fd < 0)
        return -1;
    if (gw_send_req(fd, req, (size_t)reqn, body) != 0 ||
        gw_rx_init(&rx, timeout_ms) != 0) {
        gw_close(fd);
        return -1;
    }
    rc = gw_rx_loop(fd, &rx);
    gw_close(fd);
    if (rc == GW_RX_CANCEL) {
        AIRY_FREE(rx.buf);
        return CLI_GW_EXCH_CANCELED;
    }
    if (rc == GW_RX_ERROR) {
        AIRY_FREE(rx.buf);
        return -1;
    }
    rc = gw_body_take(&rx, out_body, out_body_len, out_status);
    AIRY_FREE(rx.buf);
    return rc;
}

/* N-3（0.1.16）：provider 400/422（请求体非法，典型是消息含无效 UTF-8 →
 * "invalid unicode code point"）在 llm_d 已归入 AIRY_ERR_LLM_BAD_REQUEST，
 * 但网关转发时把 daemon 错误码统一折叠为 -32603、只透传 message 文本，故
 * CLI 侧对 error.message 做特征分诊，避免把 provider 400 误报为"读写错误/
 * 网络错误"（R-1 已处理 401/403，R-5 已处理超时/连接失败，此处补齐 400/422）。
 * 返回：2=请求体被明确拒绝；1=provider 请求失败（请求体非法或不可达）；
 *       0=与 provider 无关。 */
static int cli_gw_provider_state(const char *m)
{
    if (!m || !m[0])
        return 0;
    static const char *const body_reject[] = {
        "Provider rejected the request body",
        "invalid unicode",
        "Failed to parse the request body",
        "request body as JSON",
        "invalid_request_error",
    };
    for (size_t i = 0; i < sizeof(body_reject) / sizeof(body_reject[0]); i++) {
        if (strstr(m, body_reject[i]))
            return 2;
    }
    if (strstr(m, "Network request to provider failed"))
        return 1;
    return 0;
}

/* ── JSON-RPC over HTTP POST / ─────────────────────────────────────── */
/* 失败返回真实 AIRY_ERR_*（0.1.19 §3.7.2 N1）：呈现层据此按 class 分诊，
 * 不再靠字符串嗅探；g_cli_gw_err 保留可读原文，二者互补。全部调用面均按
 * 非 0 判失败（cli_err_desc 消费文本），返回码细化不改变控制流。 */
int cli_gw_call(const char *method, const char *params_json, int timeout_ms, char **out_result)
{
    if (!method || !out_result) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关调用参数非法（method%s，out_result%s）",
                 method ? " 已给出" : " 为空", out_result ? " 已给出" : " 为空");
        return AIRY_ERR_INVALID_PARAM;
    }
    *out_result = NULL;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);

    /* 请求体按实际长度动态构造：固定缓冲会在工具参数（如 fs_write 大内容）
     * 超过容量时静默截断，产生非法 JSON 导致调用失败（0.1.13 实机回归）。 */
    size_t envelope_len = 64 + strlen(method);
    size_t params_len = (params_json && *params_json) ? strlen(params_json) : 2;
    size_t blen = envelope_len + params_len + 1;
    char *body = (char *)AIRY_MALLOC(blen);
    if (!body) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "请求缓冲分配失败（method=%s，%zu 字节）", method, blen);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    int bn = snprintf(body, blen, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"%s\",\"params\":%s}",
                      method, (params_json && *params_json) ? params_json : "{}");
    if (bn <= 0 || (size_t)bn >= blen) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "请求信封构造失败（method=%s），参数过长", method);
        AIRY_FREE(body);
        return AIRY_ERR_INVALID_PARAM;
    }

    char *resp = NULL;
    size_t rlen = 0;
    int status = 0;
    int exchange_err = cli_gw_exchange("POST", host, port, "/", body, timeout_ms, &resp, &rlen,
                                       &status);
    AIRY_FREE(body);
    if (exchange_err == CLI_GW_EXCH_CANCELED) {
        AIRY_FREE(resp);
        /* S-02：用户取消返回 AIRY_ERR_CANCELED，与 sched.dag_cancel /
         * cli_dag_wait_remote 语义对齐；描述由 cli_err_desc 统一渲染
         * （"操作已取消"），不占 g_cli_gw_err。31 处调用面均按非 0 判
         * 失败，新码不破坏既有调用方。 */
        return AIRY_ERR_CANCELED;
    }
    if (exchange_err != 0) {
        AIRY_FREE(resp);
        if (exchange_err == CLI_GW_EXCH_TIMEOUT) {
            /* S-02 超时可诊断：socket 已建立，区分于"网关不在线"——
             * 模型思考超预算时不再误导用户去启动服务。 */
            snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                     "网关已连接但响应超时（%d 秒，method=%s）：服务端可能仍在推理，"
                     "可重试或排查模型负载",
                     timeout_ms / 1000, method);
            AIRY_LOG_WARN("cli_gw: response timeout after %dms (method=%s) at %s:%d",
                          timeout_ms, method, host, port);
            return AIRY_ERR_TIMEOUT;
        }
        /* 0.1.6h 友好化：网关不可达给出可执行提示（原仅日志 WARN） */
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关不在线（%s:%d），请运行 airymaxrt 启动服务", host, port);
        AIRY_LOG_WARN("cli_gw: gateway unreachable at %s:%d (method=%s)", host, port, method);
        return AIRY_ERR_SYS_SOCKET;
    }
    if (!resp) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应为空，请检查 %s 服务状态", host);
        return AIRY_ERR_PROTOCOL;
    }
    /* 上游 5xx：网关自身或其后端不可用（典型为 daemon 重启期间的空体
     * 503）。此前状态行被丢弃，空体降级为"响应不是合法 JSON"，用户无法
     * 分辨"后端重启"与"协议损坏"；此处按 N1 传输层具名。 */
    if (status >= 500) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关上游不可用（HTTP %d，method=%s）：服务可能正在重启，请稍后重试",
                 status, method);
        AIRY_LOG_WARN("cli_gw: upstream unavailable (HTTP %d, method=%s) at %s:%d", status, method,
                      host, port);
        AIRY_FREE(resp);
        return AIRY_ERR_SVC_NOT_READY;
    }
    /* 提取 JSON-RPC result（忽略 id 字段） */
    cJSON *root = cJSON_Parse(resp);
    AIRY_FREE(resp);
    if (!root) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应不是合法 JSON（method=%s），请查看 %s/logs/gateway_d.out",
                 method, getenv("AIRY_HOME") ? getenv("AIRY_HOME") : "~/.airymaxrt");
        AIRY_LOG_WARN("cli_gw: malformed json response (method=%s) at %s:%d", method, host, port);
        return AIRY_ERR_PROTOCOL;
    }
    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (err) {
        /* 0.1.6h 友好化：解析 error 详情，避免裸 JSON-RPC 刷屏 */
        cJSON *code = cJSON_GetObjectItem(err, "code");
        cJSON *msg = cJSON_GetObjectItem(err, "message");
        int c = code && cJSON_IsNumber(code) ? (int)code->valuedouble : 0;
        const char *m = msg && cJSON_IsString(msg) ? msg->valuestring : "";
        int pr = cli_gw_provider_state(m);
        /* 网关已应答即链路可达：错误对象按 JSON-RPC 码保持语义，归类为
         * 业务/参数/不支持，绝不外溢为 transport（N1 三层责任）。 */
        int rc = AIRY_ERR_GENERIC_FAIL;
        if (pr > 0)
            /* N-3：provider 请求被拒（400/422）不再表现为"读写/网络错误"，
             * 并保留网关 error.message 原文供自助定位。 */
            snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s；网关原文：%s",
                     (pr == 2) ? "请求被模型服务拒绝（请求体非法，如消息含无效 UTF-8 字符）"
                               : "请求被模型服务拒绝或模型服务不可达（请检查消息内容与网络连通性）",
                     m);
        else if (c == -32603)
            snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                     "网关服务内部错误（%s），请查看 %s/logs/gateway_d.out",
                     m[0] ? m : "Internal error", getenv("AIRY_HOME") ? getenv("AIRY_HOME") : "~/.airymaxrt");
        else if (c == -32601) {
            snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                     "网关不支持该方法（%s）", method);
            rc = AIRY_ERR_NOT_SUPPORTED;
        } else {
            if (c == -32600 || c == -32602)
                rc = AIRY_ERR_INVALID_PARAM;
            snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                     "网关服务异常（code=%d%s%s）", c, m[0] ? " " : "", m);
        }
        cJSON_Delete(root);
        return rc;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!result) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应缺少 result 字段（method=%s），服务面实现异常", method);
        AIRY_LOG_WARN("cli_gw: response without result (method=%s) at %s:%d", method, host, port);
        cJSON_Delete(root);
        return AIRY_ERR_PROTOCOL;
    }
    char *out = cJSON_PrintUnformatted(result);
    cJSON_Delete(root);
    if (!out) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应序列化失败（method=%s）", method);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    *out_result = out;
    return 0;
}

/* ── /health 可达性 + 传输代次 ────────────────────────────────────── */
int cli_gw_probe(int timeout_ms, unsigned long long *epoch_out)
{
    if (epoch_out)
        *epoch_out = 0;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    char *resp = NULL;
    size_t rlen = 0;
    int status = 0;
    /* /health 仅支持 GET；此前误用 POST 得 404，导致网关健康时仍误报
     * "gateway unreachable"（v0.1.16 现场缺陷根因）。 */
    if (cli_gw_exchange("GET", host, port, "/health", NULL, timeout_ms, &resp, &rlen, &status) != 0) {
        AIRY_FREE(resp);
        return 0;
    }
    int ok = (status == 200 && resp && strstr(resp, "\"healthy\"")) ? 1 : 0;
    if (ok && epoch_out) {
        const char *p = strstr(resp, "\"epoch\":");
        if (p)
            *epoch_out = strtoull(p + 8, NULL, 10);
    }
    AIRY_FREE(resp);
    return ok;
}

int cli_gw_health(int timeout_ms)
{
    return cli_gw_probe(timeout_ms, NULL);
}
