// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_sse.h
 * @brief SSE 客户端契约：长连接行翻译 + chunked 解码（0.1.21 D-15）。
 *
 * gateway 的 /api/v1/agent/run/stream 逐行下发 `data: <json>`，HTTP/1.1
 * 未知长度响应经 MHD 走 chunked 传输。本模块只做字节层翻译：chunked →
 * plain → SSE 行 → 事件 JSON；不解析业务语义。
 */

#ifndef AIRY_CLI_SSE_H
#define AIRY_CLI_SSE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_SSE_EVMAX (256u * 1024u)  /* 单个事件 JSON 上限 */
#define CLI_SSE_HDR_MAX (16u * 1024u) /* 响应头上限 */

/* chunked 解码器：out 缓冲由自身持有，按行消费。 */
typedef struct {
    int chunked;
    int state;  /* 0=块长行 1=块数据 2=块尾 CRLF 3=结束 */
    int pad;    /* 块尾 CRLF 计数 */
    int done;
    unsigned long long remain;
    char sz[16];
    size_t szlen;
    char *buf;
    size_t len;
    size_t cap;
} cli_dc_t;

void cli_dc_init(cli_dc_t *dc, int chunked);
int cli_dc_feed(cli_dc_t *dc, const char *in, size_t n);
const char *cli_dc_plain(cli_dc_t *dc, size_t *len);
void cli_dc_take(cli_dc_t *dc, size_t n);
void cli_dc_free(cli_dc_t *dc);

typedef struct cli_sse cli_sse_t;

/* 连接 gateway 并发出 POST；NULL=失败（含 HTTP 非 200，见 cli_sse_http）。 */
cli_sse_t *cli_sse_open(const char *path, const char *body, int timeout_ms);

/* 取下一个事件：1=事件 JSON，0=暂无（可重试），-1=流结束/协议错误。 */
int cli_sse_next(cli_sse_t *s, char *out, size_t cap, int wait_ms);

int cli_sse_http(const cli_sse_t *s);
void cli_sse_close(cli_sse_t *s);

/* 纯解析：一行 → 事件 JSON；1=事件，0=忽略（空行/注释/其他字段），-1=超长。 */
int cli_sse_line(const char *line, size_t len, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_SSE_H */
