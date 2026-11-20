// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev_frame.c
 * @brief 事件帧纯函数面：类型校验、字段提取、帧编码与超限截断。
 *
 * 无 I/O、无全局状态，单测直接覆盖。截断策略：整帧超 1MiB 时 payload
 * 收缩为 {truncated,bytes[,status]}，并保留 status/code 供终态判读。
 */

#include "cli_ev.h"
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EV_TYPE_MAX 24
#define EV_HEAD_MAX 192

/* 事件类型白名单字符：只接受 [a-z0-9_.]，杜绝帧头注入。 */
static int ev_type_ok(const char *t)
{
    size_t n = 0;

    if (!t || !*t)
        return 0;
    for (; t[n]; n++) {
        char c = t[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'))
            return 0;
    }
    return n < EV_TYPE_MAX;
}

/* 定位 key 的值起点（跳过空白与冒号）；未命中返回 NULL。 */
static const char *ev_find(const char *js, size_t n, const char *key)
{
    size_t kl = strlen(key);
    size_t i;

    for (i = 0; i + kl + 2 <= n; i++) {
        size_t j;

        if (js[i] != '"' || memcmp(js + i + 1, key, kl) != 0 ||
            js[i + 1 + kl] != '"')
            continue;
        j = i + kl + 2;
        while (j < n && (js[j] == ' ' || js[j] == '\t' || js[j] == '\n'))
            j++;
        if (j < n && js[j] == ':')
            return js + j + 1;
    }
    return NULL;
}

/* 读取字符串体（处理 \n/\t/\r 转义），返回写入字节数。 */
static size_t ev_str_body(const char *js, size_t n, const char *p, char *out, size_t cap)
{
    size_t o = 0;

    while (p < js + n && *p != '"') {
        char c = *p;
        if (c == '\\' && p + 1 < js + n) {
            p++;
            c = (*p == 'n') ? '\n' : (*p == 't') ? '\t' : (*p == 'r') ? '\r' : *p;
        }
        if (o + 1 < cap)
            out[o++] = c;
        p++;
    }
    out[o] = '\0';
    return o;
}

/* 取顶层字符串值；0=未命中（或非字符串）。 */
int cli_ev_str(const char *js, size_t n, const char *key, char *out, size_t cap)
{
    const char *p;

    if (!js || !out || cap == 0)
        return 0;
    out[0] = '\0';
    p = ev_find(js, n, key);
    if (!p)
        return 0;
    while (p < js + n && (*p == ' ' || *p == '\t'))
        p++;
    if (p >= js + n || *p != '"')
        return 0;
    return (int)ev_str_body(js, n, p + 1, out, cap);
}

/* 取顶层数值（无符号）；0=未命中。 */
int cli_ev_num(const char *js, size_t n, const char *key, unsigned long *out)
{
    const char *p;
    char *end = NULL;
    unsigned long v;

    if (!js || !out)
        return 0;
    p = ev_find(js, n, key);
    if (!p)
        return 0;
    while (p < js + n && (*p == ' ' || *p == '\t'))
        p++;
    if (p >= js + n || *p < '0' || *p > '9')
        return 0;
    v = strtoul(p, &end, 10);
    if (!end || end == p)
        return 0;
    *out = v;
    return 1;
}

int cli_ev_typeof(const char *json, size_t len, char *out, size_t cap)
{
    if (cli_ev_str(json, len, "type", out, cap) <= 0)
        return 0;
    return ev_type_ok(out) ? 1 : 0;
}

/* 从值起点扫描到配对结束括号：返回原文长度（含首尾），0=不完整。 */
static size_t ev_obj_span(const char *p, size_t n)
{
    size_t i;
    size_t depth = 0;
    int in_str = 0;

    for (i = 0; i < n; i++) {
        char c = p[i];

        if (in_str) {
            if (c == '\\') {
                i++;
                continue;
            }
            if (c == '"')
                in_str = 0;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            continue;
        }
        if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            if (depth == 0)
                return 0;
            if (--depth == 0)
                return i + 1;
        }
    }
    return 0;
}

int cli_ev_obj(const char *js, size_t n, const char *key, char *out, size_t cap)
{
    const char *p;
    size_t len;

    if (!js || !out || cap == 0)
        return 0;
    out[0] = '\0';
    p = ev_find(js, n, key);
    if (!p)
        return 0;
    while (p < js + n && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    if (p >= js + n || (*p != '{' && *p != '['))
        return 0;
    len = ev_obj_span(p, (size_t)(js + n - p));
    if (len == 0 || len >= cap)
        return 0;
    AIRY_MEMCPY(out, p, len);
    out[len] = '\0';
    return (int)len;
}

size_t cli_ev_stream_body(char *dst, size_t cap, const char *goal,
                          const char *opts)
{
    size_t gn = strlen(goal ? goal : "");
    size_t xl = (opts && opts[0] == '{') ? strlen(opts) : 0;
    size_t inner = xl > 2 ? xl - 2 : 0;
    char mode[16];
    int has_mode = inner && cli_ev_str(opts, xl, "mode", mode, sizeof(mode)) > 0;
    char *esc;
    int m;

    esc = (char *)AIRY_MALLOC(2 * gn + 1);
    if (!esc)
        return 0;
    (void)cli_ev_esc(esc, 2 * gn + 1, goal);
    m = snprintf(dst, cap,
                 "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"task.submit_stream\","
                 "\"params\":{%s%.*s%s\"goal\":\"%s\",\"client\":\"cli\"}}",
                 has_mode ? "" : "\"mode\":\"auto\",",
                 inner ? (int)inner : 0, inner ? opts + 1 : "",
                 inner ? "," : "", esc);
    AIRY_FREE(esc);
    return m > 0 ? (size_t)m : 0;
}

/* 查询应答载荷：result 原样内嵌（调用方保证它是合法 JSON 原文），
 * id/error 走转义；ok=0 时不带 result 字段。 */
size_t cli_ev_query_payload(char *dst, size_t cap, const char *id, int ok,
                            const char *result, const char *error)
{
    char eid[512];
    char eerr[1024];
    int n;

    (void)cli_ev_esc(eid, sizeof(eid), id ? id : "");
    if (ok) {
        const char *res = (result && *result) ? result : "null";

        n = snprintf(dst, cap, "{\"id\":\"%s\",\"ok\":true,\"result\":%s}",
                     eid, res);
    } else {
        (void)cli_ev_esc(eerr, sizeof(eerr), error ? error : "");

        n = snprintf(dst, cap, "{\"id\":\"%s\",\"ok\":false,\"error\":\"%s\"}",
                     eid, eerr);
    }
    return n > 0 ? (size_t)n : 0;
}

int cli_ev_is_term(const char *type)
{
    return type && strcmp(type, "run_end") == 0;
}

/* JSON 字符串转义（不含首尾引号），返回写入长度。 */
int cli_ev_esc(char *dst, size_t cap, const char *src)
{
    size_t o = 0;

    if (!dst || cap == 0)
        return 0;
    if (!src)
        src = "";
    for (; *src && o + 2 < cap; src++) {
        unsigned char c = (unsigned char)*src;

        if (c == '"' || c == '\\') {
            dst[o++] = '\\';
            dst[o++] = (char)c;
        } else if (c == '\n') {
            dst[o++] = '\\';
            dst[o++] = 'n';
        } else if (c == '\t') {
            dst[o++] = '\\';
            dst[o++] = 't';
        } else if (c == '\r') {
            dst[o++] = '\\';
            dst[o++] = 'r';
        } else if (c >= 0x20) {
            dst[o++] = (char)c;
        }
    }
    dst[o] = '\0';
    return (int)o;
}

/* 帧头：返回完整帧头所需长度（与 snprintf 语义一致，cap 不足也返回真值）。 */
static int ev_head(char *b, size_t cap, unsigned seq, uint64_t ts,
                   const char *type, unsigned dropped, int trunc)
{
    char tail[48];
    int tl = 0;

    tail[0] = '\0';
    if (trunc)
        tl += snprintf(tail + tl, sizeof(tail) - (size_t)tl, ",\"truncated\":true");
    if (dropped)
        tl += snprintf(tail + tl, sizeof(tail) - (size_t)tl, ",\"dropped\":%u", dropped);
    return snprintf(b, cap,
                    "{\"schema\":%d,\"seq\":%u,\"ts\":%llu,\"type\":\"%s\"%s,\"payload\":",
                    CLI_EV_SCHEMA, seq, (unsigned long long)ts, type, tail);
}

size_t cli_ev_build(char *dst, size_t cap, unsigned seq, uint64_t ts,
                    const char *type, const char *payload, size_t plen,
                    unsigned dropped, int *trunc)
{
    char head[EV_HEAD_MAX];
    char vbuf[160];
    const char *pl = payload;
    size_t pl_len = payload ? plen : 0;
    int tr = 0;
    int hn;
    size_t need;

    if (!ev_type_ok(type))
        type = "notice";
    if (!pl) {
        pl = "{}";
        pl_len = 2;
    }
    hn = ev_head(head, sizeof(head), seq, ts, type, dropped, 0);
    /* 返回线上字节数（含行尾 '\n'，不含结尾 NUL）；调用方按 need+1 分配。 */
    need = (size_t)hn + pl_len + 2;
    if (need > CLI_EV_MAX) {
        char salv[64] = "";
        size_t sl;

        tr = 1;
        if (cli_ev_str(payload, plen, "status", salv, sizeof(salv)) <= 0)
            (void)cli_ev_str(payload, plen, "code", salv, sizeof(salv));
        if (salv[0])
            sl = (size_t)snprintf(vbuf, sizeof(vbuf),
                                  "{\"truncated\":true,\"bytes\":%lu,\"status\":\"%s\"}",
                                  (unsigned long)pl_len, salv);
        else
            sl = (size_t)snprintf(vbuf, sizeof(vbuf),
                                  "{\"truncated\":true,\"bytes\":%lu}",
                                  (unsigned long)pl_len);
        pl = vbuf;
        pl_len = sl;
        hn = ev_head(head, sizeof(head), seq, ts, type, dropped, tr);
        need = (size_t)hn + pl_len + 2;
    }
    if (trunc)
        *trunc = tr;
    if (dst && cap > need) {
        AIRY_MEMCPY(dst, head, (size_t)hn);
        AIRY_MEMCPY(dst + hn, pl, pl_len);
        AIRY_MEMCPY(dst + hn + pl_len, "}\n", 2);
        dst[need] = '\0';
    }
    return need;
}
