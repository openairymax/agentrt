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
    printf(g_fail ? "FAILED\n" : "ALL PASSED\n");
    return g_fail;
}
