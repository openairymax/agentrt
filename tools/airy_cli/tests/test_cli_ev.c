// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_ev.c
 * @brief 0.1.21 D-15 事件流出口单测：帧编码、seq 单调、环形续传、超限
 *        截断、chunked 分片解码与 SSE 行翻译。
 *
 * 输出写面经 cli_ev_io_set 注入内存 sink：被替换的是 I/O 目标，帧编码
 * 与状态机全部按真实路径执行，无桩数据面、不触网关。
 */

#include "cli_ev.h"
#include "cli_clock.h"
#include "cli_sse.h"
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond, what)                                                       \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what);                \
            g_fail = 1;                                                         \
        }                                                                       \
    } while (0)

#define CHECK_EQ(got, want, what)                                               \
    do {                                                                        \
        long g_ = (long)(got);                                                  \
        long w_ = (long)(want);                                                 \
        if (g_ != w_) {                                                         \
            printf("FAIL %s:%d %s (got=%ld want=%ld)\n", __FILE__, __LINE__,    \
                   what, g_, w_);                                               \
            g_fail = 1;                                                         \
        }                                                                       \
    } while (0)

static char g_sink[65536];
static size_t g_sink_len;

static int sink_wr(const void *s, size_t n, void *ud)
{
    (void)ud;
    if (!s || g_sink_len + n >= sizeof(g_sink))
        return 0;
    AIRY_MEMCPY(g_sink + g_sink_len, s, n);
    g_sink_len += n;
    g_sink[g_sink_len] = '\0';
    return (int)n;
}

static void sink_reset(void)
{
    g_sink_len = 0;
    g_sink[0] = '\0';
}

static int count_nl(void)
{
    int n = 0;
    const char *p;

    for (p = g_sink; *p; p++)
        if (*p == '\n')
            n++;
    return n;
}

static void t_emit(void)
{
    static const char f1[] = "{\"type\":\"run_start\"}";
    static const char f2[] = "{\"type\":\"text_delta\",\"text\":\"hi\"}";
    unsigned base;

    printf("  t_emit...\n");
    cli_ev_io_set(sink_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    base = cli_ev_seq();
    sink_reset();
    CHECK_EQ(cli_ev_emit("run_start", f1, strlen(f1)), 0, "emit run_start");
    CHECK_EQ(cli_ev_emit("text_delta", f2, strlen(f2)), 0, "emit text_delta");
    CHECK_EQ(cli_ev_seq(), base + 2, "seq advances by one per frame");
    CHECK(strstr(g_sink, "\"schema\":1") != NULL, "frame carries schema");
    CHECK(strstr(g_sink, "\"type\":\"run_start\"") != NULL, "frame carries type");
    CHECK(strstr(g_sink, "\"payload\":{") != NULL,
          "payload is keyed (frames stay valid JSON)");
    CHECK(strstr(g_sink, f1) != NULL, "payload passed through verbatim");
    CHECK_EQ(count_nl(), 2, "one NDJSON line per frame");
    cli_ev_fini();
}

static void t_bridge(void)
{
    static const char start[] = "{\"type\":\"run_start\",\"run_id\":\"r1\"}";
    static const char end[] = "{\"type\":\"run_end\",\"status\":\"completed\"}";
    static const char cancel[] = "{\"status\":\"cancelled\"}";
    static const char bad[] = "not-json";

    printf("  t_bridge...\n");
    cli_ev_io_set(sink_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    sink_reset();
    CHECK_EQ(cli_ev_bridge(start, strlen(start)), 0, "run_start is not terminal");
    CHECK_EQ(cli_ev_terminal(), 0, "terminal flag clear");
    CHECK_EQ(cli_ev_bridge(end, strlen(end)), 1, "run_end is terminal");
    CHECK_EQ(cli_ev_terminal(), 1, "terminal flag set");
    CHECK_EQ(cli_ev_status(end, strlen(end)), 0, "completed -> 0");
    CHECK_EQ(cli_ev_status(cancel, strlen(cancel)), 130, "cancelled -> 130");
    CHECK_EQ(cli_ev_bridge(bad, strlen(bad)), 0, "malformed frame does not kill stream");
    cli_ev_fini();
}

static void t_finish(void)
{
    printf("  t_finish...\n");
    cli_ev_io_set(sink_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    sink_reset();
    CHECK_EQ(cli_ev_finish("stream_lost"), 1, "finish synthesizes terminal");
    CHECK_EQ(cli_ev_terminal(), 1, "terminal after finish");
    CHECK(strstr(g_sink, "\"type\":\"error\"") != NULL, "error frame first");
    CHECK(strstr(g_sink, "\"status\":\"failed\"") != NULL, "failed run_end");
    CHECK_EQ(cli_ev_finish("again"), 0, "exactly one terminal per turn");
    cli_ev_fini();
}

/* 环形续传：hello(last_seq) 先出 snapshot，再补发 seq>last_seq 的帧；
 * snapshot 自身不进入补发窗口。 */
static void t_resume(void)
{
    static const char f1[] = "{\"type\":\"task_state\",\"state\":\"running\"}";
    static const char f2[] = "{\"type\":\"text_delta\",\"text\":\"a\"}";
    static const char f3[] = "{\"type\":\"text_delta\",\"text\":\"b\"}";
    unsigned base;
    char want[32];
    const char *first;

    printf("  t_resume...\n");
    cli_ev_io_set(sink_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    base = cli_ev_seq();
    sink_reset();
    CHECK_EQ(cli_ev_emit("task_state", f1, strlen(f1)), 0, "emit 1");
    CHECK_EQ(cli_ev_emit("text_delta", f2, strlen(f2)), 0, "emit 2");
    CHECK_EQ(cli_ev_emit("text_delta", f3, strlen(f3)), 0, "emit 3");
    sink_reset();
    cli_ev_snap(base + 2);
    first = strstr(g_sink, "\n");
    CHECK(first != NULL, "snapshot followed by resumed frames");
    CHECK(strstr(g_sink, "\"type\":\"snapshot\"") != NULL, "snapshot frame emitted");
    snprintf(want, sizeof(want), "\"seq\":%u", base + 3);
    CHECK(strstr(g_sink, want) != NULL, "frame after last_seq is resent");
    snprintf(want, sizeof(want), "\"seq\":%u", base + 1);
    CHECK(strstr(g_sink, want) == NULL, "frame before last_seq is not resent");
    cli_ev_fini();
}

static void t_trunc(void)
{
    size_t n = (size_t)CLI_EV_MAX + 8;
    char *big = (char *)AIRY_MALLOC(n);
    char *out;
    size_t need;
    int trunc = 0;

    printf("  t_trunc...\n");
    CHECK(big != NULL, "alloc payload");
    if (!big)
        return;
    AIRY_MEMSET(big, 'a', n);
    /* 与第二次调用同参（dropped=3）取长度，否则帧头变长会撑破预算。 */
    need = cli_ev_build(NULL, 0, 9, 0, "text_delta", big, n, 3, &trunc);
    CHECK(need > 0 && need <= CLI_EV_MAX, "truncated frame fits the cap");
    out = (char *)AIRY_MALLOC(need + 1);
    CHECK(out != NULL, "alloc frame");
    if (out) {
        trunc = 0;
        (void)cli_ev_build(out, need + 1, 9, 0, "text_delta", big, n, 3, &trunc);
        CHECK_EQ(trunc, 1, "over-cap payload flagged");
        CHECK(strstr(out, "\"truncated\":true") != NULL, "stub payload present");
        CHECK(strstr(out, "\"bytes\":") != NULL, "original size reported");
        AIRY_FREE(out);
    }
    AIRY_FREE(big);
}

static void t_chunked(void)
{
    static const char body[] = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    cli_dc_t dc;
    size_t len = 0;
    const char *p;

    printf("  t_chunked...\n");
    cli_dc_init(&dc, 1);
    CHECK_EQ(cli_dc_feed(&dc, body, 10), 0, "partial chunk accepted");
    p = cli_dc_plain(&dc, &len);
    CHECK(len == 5 && p && memcmp(p, "hello", 5) == 0, "first chunk decoded");
    CHECK_EQ(cli_dc_feed(&dc, body + 10, sizeof(body) - 1 - 10), 0, "rest accepted");
    p = cli_dc_plain(&dc, &len);
    CHECK(len == 11 && p && memcmp(p, "hello world", 11) == 0, "all chunks decoded");
    cli_dc_free(&dc);

    cli_dc_init(&dc, 0);
    CHECK_EQ(cli_dc_feed(&dc, "plain", 5), 0, "identity mode accepted");
    p = cli_dc_plain(&dc, &len);
    CHECK(len == 5 && p && memcmp(p, "plain", 5) == 0, "identity mode passthrough");
    cli_dc_free(&dc);
}

static void t_opts(void)
{
    static const char frame[] =
        "{\"type\":\"input\",\"kind\":\"prompt\",\"text\":\"hi\","
        "\"opts\":{\"model\":\"m1\",\"history\":[{\"role\":\"user\",\"text\":\"a}b\"}]}}";
    char out[512];
    char body[1024];
    size_t need;

    printf("  t_opts...\n");
    CHECK_EQ(cli_ev_obj(frame, sizeof(frame) - 1, "opts", out, sizeof(out)),
             (int)strlen("{\"model\":\"m1\",\"history\":[{\"role\":\"user\",\"text\":\"a}b\"}]}"),
             "opts object extracted whole");
    CHECK(strstr(out, "\"a}b\"") != NULL, "brace inside string not a boundary");
    CHECK_EQ(cli_ev_obj(frame, sizeof(frame) - 1, "text", out, sizeof(out)), 0,
             "non-object value rejected");
    CHECK_EQ(cli_ev_obj("{\"opts\":{\"a\":1", 13, "opts", out, sizeof(out)), 0,
             "unbalanced object rejected");
    CHECK_EQ(cli_ev_obj("{\"opts\":{}}", 11, "opts", out, 2), 0,
             "insufficient buffer rejected");

    need = cli_ev_stream_body(body, sizeof(body), "goal \"q\"", "{\"model\":\"m1\"}");
    CHECK(need > 0 && need < sizeof(body), "body fits");
    CHECK(strstr(body, "\"params\":{\"mode\":\"auto\",\"model\":\"m1\","
                       "\"goal\":\"goal \\\"q\\\"\"") != NULL,
             "opts merged after default mode");
    CHECK(strstr(body, "\"client\":\"cli\"") != NULL,
             "authoritative fields traced");

    (void)cli_ev_stream_body(body, sizeof(body), "g", "{}");
    CHECK(strstr(body, "\"params\":{\"mode\":\"auto\",\"goal\":\"g\"") != NULL,
             "empty opts falls back to auto mode");

    (void)cli_ev_stream_body(body, sizeof(body), "g", NULL);
    CHECK(strstr(body, "\"params\":{\"mode\":\"auto\",\"goal\":\"g\"") != NULL,
             "absent opts falls back to auto mode");

    (void)cli_ev_stream_body(body, sizeof(body), "g", "{\"mode\":\"task\"}");
    CHECK(strstr(body, "\"params\":{\"mode\":\"task\",\"goal\":\"g\"") != NULL,
             "caller mode overrides without duplication");
    CHECK(strstr(body, "\"mode\":\"auto\"") == NULL, "no duplicate mode key");
}

static void t_sse_line(void)
{
    char out[256];

    printf("  t_sse_line...\n");
    CHECK_EQ(cli_sse_line("data: {\"type\":\"run_start\"}\n", 27, out, sizeof(out)), 1,
             "data line is an event");
    CHECK(strcmp(out, "{\"type\":\"run_start\"}") == 0, "payload unwrapped");
    CHECK_EQ(cli_sse_line(": keep-alive", 12, out, sizeof(out)), 0, "comment ignored");
    CHECK_EQ(cli_sse_line("", 0, out, sizeof(out)), 0, "empty line ignored");
    CHECK_EQ(cli_sse_line("event: ping", 11, out, sizeof(out)), 0, "other field ignored");
    CHECK_EQ(cli_sse_line("data: [DONE]", 12, out, sizeof(out)), 0, "DONE sentinel ignored");
}

/* 查询应答载荷：ok 分支内嵌 result 原文，失败分支转义 error。 */
static void t_query_payload(void)
{
    char out[256];
    size_t n;

    printf("  t_query_payload...\n");
    n = cli_ev_query_payload(out, sizeof(out), "q7", 1,
                             "{\"agents\":[{\"id\":\"a1\"}]}", NULL);
    CHECK(n > 0 && n < sizeof(out), "ok payload fits");
    CHECK(strcmp(out, "{\"id\":\"q7\",\"ok\":true,\"result\":"
                      "{\"agents\":[{\"id\":\"a1\"}]}}") == 0,
          "result embedded verbatim");
    n = cli_ev_query_payload(out, sizeof(out), "q8", 1, NULL, NULL);
    CHECK(strstr(out, "\"result\":null") != NULL, "absent result is null");
    n = cli_ev_query_payload(out, sizeof(out), "q9", 0, NULL,
                             "gateway \"offline\"");
    CHECK(strstr(out, "\"ok\":false") != NULL, "failure marked");
    CHECK(strstr(out, "\"error\":\"gateway \\\"offline\\\"\"") != NULL,
          "error escaped");
    CHECK(strstr(out, "result") == NULL, "no result on failure");
    {
        char tmp[64];
        size_t want = cli_ev_query_payload(NULL, 0, "q7", 1, "{\"a\":1}", NULL);

        CHECK_EQ(cli_ev_query_payload(tmp, sizeof(tmp), "q7", 1, "{\"a\":1}", NULL),
                 want, "dry run reports the filled length");
        CHECK(strcmp(tmp, "{\"id\":\"q7\",\"ok\":true,\"result\":{\"a\":1}}") == 0,
              "dry run and fill agree");
    }
}

#ifndef _WIN32

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* 10k 事件/s 压测（D-15-23 判据⑤）：真实编码路径配内存 sink，逐帧核对
 * seq 单调与总字节；环形缓冲只留最近 64 帧，常驻内存不随帧数增长。 */
#define LOAD_N 10000
#define LOAD_RSS_KB 8192

static unsigned g_load_seq;
static int g_load_order;
static size_t g_load_bytes;

static int load_wr(const void *s, size_t n, void *ud)
{
    const char *p = (const char *)s;
    size_t i = 0;
    unsigned v;

    (void)ud;
    while (i + 6 <= n && memcmp(p + i, "\"seq\":", 6) != 0)
        i++;
    if (i + 6 > n) {
        g_load_order = 1;
        return (int)n;
    }
    v = (unsigned)strtoul(p + i + 6, NULL, 10);
    if (v != g_load_seq)
        g_load_order = 1;
    g_load_seq = v + 1;
    g_load_bytes += n;
    return (int)n;
}

#ifdef __linux__
static long rss_kb(void)
{
    char buf[128];
    char *p = buf;
    FILE *f = fopen("/proc/self/statm", "r");
    long res;

    if (!f)
        return -1;
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    (void)strtoul(p, &p, 10);
    res = (long)strtoul(p, &p, 10);
    return res < 0 ? -1 : res * (sysconf(_SC_PAGESIZE) / 1024);
}
#endif

static void t_load(void)
{
    char pl[2048];
    uint64_t t0;
    uint64_t dt;
    unsigned base;
    long rss0 = -1;
    int i;

    printf("  t_load...\n");
#ifdef __linux__
    rss0 = rss_kb();
#endif
    cli_ev_io_set(load_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    base = cli_ev_seq();
    g_load_seq = base + 1;
    g_load_order = 0;
    g_load_bytes = 0;
    t0 = cli_now_ms();
    for (i = 0; i < LOAD_N; i++) {
        int n = snprintf(pl, sizeof(pl),
                         "{\"type\":\"text_delta\",\"i\":%d,\"text\":\"%01900d\"}",
                         i, i);

        if (cli_ev_emit("text_delta", pl, (size_t)n) != 0) {
            CHECK(0, "emit under load");
            break;
        }
    }
    dt = cli_now_ms() - t0;
    printf("    %d frames in %lu ms (%lu ev/s, %lu bytes)\n", LOAD_N,
           (unsigned long)dt, (unsigned long)(dt ? LOAD_N * 1000 / dt : 0),
           (unsigned long)g_load_bytes);
    CHECK_EQ(cli_ev_seq(), base + LOAD_N, "seq continuous under load");
    CHECK_EQ(g_load_order, 0, "frames arrive in seq order");
    CHECK_EQ(cli_ev_dropped(), 0, "no frame dropped under load");
    CHECK(dt <= 1000, "10k frames within one second");
#ifdef __linux__
    if (rss0 >= 0) {
        long rss1 = rss_kb();

        CHECK(rss1 < 0 || rss1 - rss0 < LOAD_RSS_KB, "resident set stays bounded");
    }
#endif
    cli_ev_fini();
}

/* 取一个刚释放的本机端口：保证 connect 被立即拒绝（非桩服务端）。 */
static int t_dead_port(void)
{
    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int port = 0;

    if (fd < 0)
        return 0;
    AIRY_MEMSET(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0 &&
        getsockname(fd, (struct sockaddr *)&sa, &sl) == 0)
        port = ntohs(sa.sin_port);
    close(fd);
    return port;
}

/* 输入帧端到端：stdin 真管道喂 query 帧，网关指向已释放端口，
 * 断言回帧是 ok:false 的 query_result（解析 → 调用 → 载荷 → 写出全真跑）。 */
static void t_query_frame(void)
{
    static const char q[] =
        "{\"type\":\"input\",\"kind\":\"query\",\"id\":\"q7\","
        "\"method\":\"hall.board\",\"params\":{}}\n";
    char url[64];
    int pfd[2];
    int saved;
    int port = t_dead_port();

    printf("  t_query_frame...\n");
    CHECK(port > 0, "dead port picked");
    if (port <= 0)
        return;
    snprintf(url, sizeof(url), "127.0.0.1:%d", port);
    (void)setenv("AIRY_GATEWAY_URL", url, 1);
    CHECK_EQ(pipe(pfd), 0, "stdin pipe");
    saved = dup(0);
    (void)write(pfd[1], q, sizeof(q) - 1);
    (void)dup2(pfd[0], 0);
    close(pfd[0]);
    close(pfd[1]);
    cli_ev_io_set(sink_wr, NULL);
    CHECK_EQ(cli_ev_init(), 0, "init");
    cli_ev_begin();
    sink_reset();
    (void)cli_ev_in_poll();
    CHECK(strstr(g_sink, "\"type\":\"query_result\"") != NULL,
          "query answered as a result frame");
    CHECK(strstr(g_sink, "\"id\":\"q7\"") != NULL, "id echoed");
    CHECK(strstr(g_sink, "\"ok\":false") != NULL, "unreachable gateway fails");
    CHECK(strstr(g_sink, "\"error\":\"") != NULL, "failure carries a reason");
    (void)dup2(saved, 0);
    close(saved);
    cli_ev_fini();
    (void)unsetenv("AIRY_GATEWAY_URL");
}

#endif /* !_WIN32 */

int main(void)
{
    printf("test_cli_ev:\n");
    t_emit();
    t_bridge();
    t_finish();
    t_resume();
    t_trunc();
    t_chunked();
    t_sse_line();
    t_opts();
    t_query_payload();
#ifndef _WIN32
    t_load();
    t_query_frame();
#endif
    printf(g_fail ? "FAILED\n" : "ALL PASSED\n");
    return g_fail;
}
