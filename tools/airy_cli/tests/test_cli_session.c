// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_session.c
 * @brief --continue / --resume 会话恢复视图端到端单测（0.1.17 R5-G5）。
 *
 * 本进程内 mock 网关（真实 loopback socket，非桩函数）承接 mem.recent 派发，
 * 断言：
 *   - 出站契约：method=mem.recent，params.limit 与消息预算（cli_history_capacity）
 *     对齐——权威按 limit 裁剪，前端不臆造条数；
 *   - 还原顺序：权威返回新→旧，装配后 g_history_* 为旧→新（与 TUI 同序）；
 *   - 整轮拆分：CLI 线格式（CLI_TURN_*）拆为 user/assistant 两条，reasoning
 *     随 assistant 保留；前缀剥离按 8 字节（"用户: " UTF-8 宽度）而非历史上
 *     误记的 9 字节；
 *   - metadata.role 单条还原：仅 user/assistant 入历史，其余角色跳过；
 *     metadata 兼容字符串与对象两形态；
 *   - 空白/缺字段/畸形记录容忍，且不越界；
 *   - 超量记录按 FIFO 成对丢弃，条数不超预算上限；
 *   - 网关不可达、响应不可解析均降级为错误提示且不崩溃；
 *   - 视图完全由权威响应导出：重复装配（中间清空）结果逐字一致，前端不落
 *     任何本地会话状态。
 *
 * @owner: team-B
 */

#define _POSIX_C_SOURCE 200809L

#include "cli_internal.h"

#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* CLI 全局模式量与 TUI 附着面：正常由 main.c / cli_tui.c 拥有。本测试走
 * 无 TUI 的端到端路径（cli_render.c 的 TUI 句柄恒为 NULL），故只需符号
 * 闭合，语义与「未附着 TUI」的真实分支一致（非活跃 → 直写 stdout）。 */
volatile sig_atomic_t g_cli_cancel = 0;
int g_cli_print_mode = 0;

int cli_tui_active(const cli_tui_t *tui)
{
    (void)tui;
    return 0;
}

cli_tui_t *cli_tui_get_default(void)
{
    return NULL;
}

void cli_tui_emit(cli_tui_t *tui, const char *data, size_t len)
{
    (void)tui;
    (void)data;
    (void)len;
}

static int g_run = 0;
static int g_pass = 0;

#define CHECK(cond, name)                                                      \
    do {                                                                       \
        g_run++;                                                               \
        if (cond) {                                                            \
            printf("  [PASS] %s\n", name);                                     \
            g_pass++;                                                          \
        } else {                                                               \
            printf("  [FAIL] %s (line %d)\n", name, __LINE__);                 \
        }                                                                      \
    } while (0)

/* ── mock 网关 ─────────────────────────────────────────────────────── */

static int g_srv_fd = -1;
static pthread_t g_srv_thr;
static _Atomic int g_stop = 0;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int g_reqs = 0;
static char g_last_method[128] = "";
static char g_last_params[1024] = "";
static char g_body[16384] = "";
static char g_raw[256] = "";

static void mock_reset(void)
{
    pthread_mutex_lock(&g_lock);
    g_reqs = 0;
    g_last_method[0] = '\0';
    g_last_params[0] = '\0';
    pthread_mutex_unlock(&g_lock);
}

static void mock_send(int fd, const char *body)
{
    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      strlen(body));
    (void)send(fd, hdr, (size_t)hn, MSG_NOSIGNAL);
    (void)send(fd, body, strlen(body), MSG_NOSIGNAL);
}

static void mock_handle(int fd)
{
    char buf[8192];
    size_t n = 0;
    while (n + 1 < sizeof(buf)) {
        ssize_t r = recv(fd, buf + n, sizeof(buf) - n - 1, 0);
        if (r <= 0)
            break;
        n += (size_t)r;
        buf[n] = '\0';
        if (strstr(buf, "\r\n\r\n"))
            break;
    }
    if (n == 0)
        return;

    char *he = strstr(buf, "\r\n\r\n");
    if (!he)
        return;
    size_t hlen = (size_t)(he - buf) + 4;

    size_t clen = 0;
    char *cl = strstr(buf, "Content-Length:");
    if (!cl)
        cl = strstr(buf, "content-length:");
    if (cl)
        clen = (size_t)strtoul(cl + 15, NULL, 10);

    while (n < hlen + clen) {
        if (n + 1 >= sizeof(buf))
            break;
        ssize_t r = recv(fd, buf + n, sizeof(buf) - n - 1, 0);
        if (r <= 0)
            break;
        n += (size_t)r;
        buf[n] = '\0';
    }
    char *body = (buf + hlen < buf + n) ? (buf + hlen) : (buf + n);

    pthread_mutex_lock(&g_lock);
    if (body && *body) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            cJSON *m = cJSON_GetObjectItem(root, "method");
            cJSON *p = cJSON_GetObjectItem(root, "params");
            if (cJSON_IsString(m))
                snprintf(g_last_method, sizeof(g_last_method), "%s", m->valuestring);
            if (p) {
                char *ps = cJSON_PrintUnformatted(p);
                if (ps) {
                    snprintf(g_last_params, sizeof(g_last_params), "%s", ps);
                    free(ps);
                }
            }
            cJSON_Delete(root);
        }
    }
    g_reqs++;
    char raw[256];
    snprintf(raw, sizeof(raw), "%s", g_raw);
    pthread_mutex_unlock(&g_lock);

    if (raw[0]) {
        mock_send(fd, raw);
        return;
    }

    pthread_mutex_lock(&g_lock);
    size_t cap = strlen(g_body) + 64;
    char *resp = malloc(cap);
    if (resp)
        snprintf(resp, cap, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":%s}", g_body);
    pthread_mutex_unlock(&g_lock);
    if (resp) {
        mock_send(fd, resp);
        free(resp);
    }
}

static void *mock_thread(void *arg)
{
    (void)arg;
    while (!g_stop) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int fd = accept(g_srv_fd, (struct sockaddr *)&cli, &clen);
        if (fd < 0) {
            if (g_stop)
                break;
            continue;
        }
        if (g_stop) {
            close(fd);
            break;
        }
        mock_handle(fd);
        close(fd);
    }
    return NULL;
}

static int mock_start(void)
{
    g_srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_srv_fd < 0)
        return -1;
    int one = 1;
    (void)setsockopt(g_srv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(g_srv_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -1;
    if (listen(g_srv_fd, 8) != 0)
        return -1;

    socklen_t alen = sizeof(addr);
    if (getsockname(g_srv_fd, (struct sockaddr *)&addr, &alen) != 0)
        return -1;
    char url[64];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    setenv("AIRY_GATEWAY_URL", url, 1);
    return pthread_create(&g_srv_thr, NULL, mock_thread, NULL);
}

static void mock_stop(void)
{
    if (g_srv_fd < 0)
        return;
    g_stop = 1;
    /* 自连接唤醒 accept（macOS shutdown 不唤醒阻塞中的 accept）。 */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        char host[64];
        int port = 0;
        cli_gw_endpoint(host, sizeof(host), &port);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((uint16_t)port);
        (void)connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        close(fd);
    }
    pthread_join(g_srv_thr, NULL);
    close(g_srv_fd);
    g_srv_fd = -1;
}

/* ── 夹具 ─────────────────────────────────────────────────────────── */

static void body_clear(void)
{
    g_body[0] = '\0';
    g_raw[0] = '\0';
    cli_history_clear();
}

static long req_limit(void)
{
    cJSON *root = cJSON_Parse(g_last_params);
    if (!root)
        return -1;
    cJSON *v = cJSON_GetObjectItem(root, "limit");
    long n = cJSON_IsNumber(v) ? (long)v->valuedouble : -1;
    cJSON_Delete(root);
    return n;
}

static int role_at(size_t i, const char *want)
{
    return i < g_history_count && g_history_roles[i] && strcmp(g_history_roles[i], want) == 0;
}

static int content_at(size_t i, const char *want)
{
    return i < g_history_count && g_history_contents[i] &&
           strcmp(g_history_contents[i], want) == 0;
}

/* 两轮整轮记录，权威口径新→旧（r2 在前）。 */
static void body_two_turns(void)
{
    snprintf(g_body, sizeof(g_body),
             "{\"records\":["
             "{\"record_id\":\"r2\",\"created_at\":1789566001,"
             "\"data\":\"用户: 第二个问题\\nAgentRT: 第二个回答\\n[reasoning] 第二段思考\","
             "\"metadata\":\"{\\\"kind\\\":\\\"chat\\\"}\"},"
             "{\"record_id\":\"r1\",\"created_at\":1789566000,"
             "\"data\":\"用户: 第一个问题\\nAgentRT: 第一个回答\","
             "\"metadata\":\"{\\\"kind\\\":\\\"chat\\\"}\"}],\"total\":2}");
}

/* n 条整轮记录，权威口径新→旧。 */
static void body_many_turns(int n)
{
    size_t off = (size_t)snprintf(g_body, sizeof(g_body), "{\"records\":[");
    for (int i = n - 1; i >= 0 && off < sizeof(g_body) - 128; i--) {
        off += (size_t)snprintf(g_body + off, sizeof(g_body) - off,
                                "%s{\"data\":\"用户: Q%d\\nAgentRT: A%d\"}",
                                (i == n - 1) ? "" : ",", i, i);
    }
    snprintf(g_body + off, sizeof(g_body) - off, "],\"total\":%d}", n);
}

/* ── 用例 ─────────────────────────────────────────────────────────── */

static void test_request_contract(void)
{
    body_two_turns();
    mock_reset();
    cli_session_restore();
    CHECK(g_reqs == 1 && strcmp(g_last_method, "mem.recent") == 0,
          "request: dispatches mem.recent");
    CHECK(req_limit() == (long)(cli_history_capacity() / 2),
          "request: limit matches half of message budget");

    /* 预算随 AIRY_CHAT_HISTORY_ROUNDS 变化，出站 limit 同源跟随。 */
    setenv("AIRY_CHAT_HISTORY_ROUNDS", "6", 1);
    body_clear();
    body_two_turns();
    mock_reset();
    cli_session_restore();
    CHECK(req_limit() == 6, "request: limit follows AIRY_CHAT_HISTORY_ROUNDS");
    unsetenv("AIRY_CHAT_HISTORY_ROUNDS");
}

static void test_turn_split(void)
{
    body_clear();
    body_two_turns();
    mock_reset();
    cli_session_restore();

    CHECK(g_history_count == 4, "split: two records expand to four messages");
    CHECK(role_at(0, "user") && content_at(0, "第一个问题"), "split: oldest user first");
    CHECK(role_at(1, "assistant") && content_at(1, "第一个回答"), "split: reply follows");
    CHECK(g_history_reasonings[1] == NULL, "split: absent reasoning stays NULL");
    CHECK(role_at(2, "user") && content_at(2, "第二个问题"), "split: order is oldest to newest");
    CHECK(role_at(3, "assistant") && content_at(3, "第二个回答"), "split: newest reply last");
    CHECK(g_history_reasonings[3] && strcmp(g_history_reasonings[3], "第二段思考") == 0,
          "split: reasoning carried with assistant");
}

static void test_role_records(void)
{
    body_clear();
    snprintf(g_body, sizeof(g_body),
             "{\"records\":["
             "{\"data\":\"系统内部记录\",\"metadata\":\"{\\\"role\\\":\\\"system\\\"}\"},"
             "{\"data\":\"助手侧记录\",\"metadata\":\"{\\\"role\\\":\\\"assistant\\\","
             "\\\"reasoning\\\":\\\"思考X\\\"}\"},"
             "{\"data\":\"用户侧记录\",\"metadata\":{\"role\":\"user\"}}],\"total\":3}");
    mock_reset();
    cli_session_restore();

    CHECK(g_history_count == 2, "role: non user/assistant roles skipped");
    CHECK(content_at(0, "用户侧记录") && role_at(0, "user"), "role: user record restored");
    CHECK(content_at(1, "助手侧记录") && role_at(1, "assistant"),
          "role: object-form metadata honoured");
    CHECK(g_history_reasonings[1] && strcmp(g_history_reasonings[1], "思考X") == 0,
          "role: reasoning from metadata kept");
}

static void test_prefix_variants(void)
{
    body_clear();
    /* 无前缀 → 原样 user 单条；有前缀无回复分隔 → 剥离 8 字节前缀后 user 单条。 */
    snprintf(g_body, sizeof(g_body),
             "{\"records\":[{\"data\":\"用户: 只有提问\"},{\"data\":\"随手一句\"}]}");
    mock_reset();
    cli_session_restore();

    CHECK(g_history_count == 2, "prefix: two single-user records");
    CHECK(content_at(0, "随手一句") && role_at(0, "user"),
          "prefix: missing prefix passes through verbatim");
    CHECK(content_at(1, "只有提问"), "prefix: 8-byte prefix stripped exactly");
}

static void test_malformed_records(void)
{
    body_clear();
    snprintf(g_body, sizeof(g_body),
             "{\"records\":["
             "{\"data\":\"   \"},{\"data\":123},{\"record_id\":\"x\"},"
             "{\"data\":\"用户: 有效记录\\nAgentRT: 有效回复\"}]}");
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 2 && content_at(0, "有效记录"),
          "malformed: blank/non-string/absent data skipped");

    /* records 缺失 / 非数组 / 空数组 → 无历史可恢复，不崩溃。 */
    body_clear();
    snprintf(g_body, sizeof(g_body), "{\"total\":0}");
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 0 && g_reqs == 1, "malformed: missing records tolerated");

    body_clear();
    snprintf(g_body, sizeof(g_body), "{\"records\":\"nope\"}");
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 0, "malformed: non-array records tolerated");

    body_clear();
    snprintf(g_body, sizeof(g_body), "{\"records\":[]}");
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 0, "malformed: empty records tolerated");
}

static void test_budget_clamp(void)
{
    body_clear();
    body_many_turns(20);
    mock_reset();
    cli_session_restore();

    /* 40 条消息灌入 30 条预算：FIFO 成对丢最旧，最终恰好 30 条，保留
     * 第 11..40 条（Q5/A5 .. Q19/A19）。 */
    CHECK(g_history_count == CLI_HISTORY_MAX_MSGS / 2,
          "budget: capped at message budget");
    CHECK(role_at(0, "user") && content_at(0, "Q5"), "budget: oldest pairs dropped");
    CHECK(role_at(g_history_count - 1, "assistant") && content_at(g_history_count - 1, "A19"),
          "budget: newest pair retained");
}

static void test_no_local_state(void)
{
    body_clear();
    body_two_turns();
    mock_reset();
    cli_session_restore();
    size_t first_count = g_history_count;
    char first_content[64];
    snprintf(first_content, sizeof(first_content), "%s",
             g_history_count ? g_history_contents[0] : "");

    /* 清空后按同一权威响应再装配：结果逐字一致 ⇒ 视图完全由权威导出。 */
    cli_history_clear();
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == first_count && g_history_count > 0 &&
              strcmp(g_history_contents[0], first_content) == 0 && g_reqs == 1,
          "state: view fully derived from authority, no local session state");
}

static void test_degraded_paths(void)
{
    char saved[128];
    const char *cur = getenv("AIRY_GATEWAY_URL");
    snprintf(saved, sizeof(saved), "%s", cur ? cur : "");
    setenv("AIRY_GATEWAY_URL", "http://airymaxrt.invalid:1", 1);

    body_clear();
    body_two_turns();
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 0, "offline: no history assembled, no crash");

    setenv("AIRY_GATEWAY_URL", saved, 1);

    /* result 非对象（字符串）→ 解析失败降级。 */
    body_clear();
    snprintf(g_raw, sizeof(g_raw),
             "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"not-an-object\"}");
    mock_reset();
    cli_session_restore();
    CHECK(g_history_count == 0, "unparsable: degrades without crash");
    g_raw[0] = '\0';
}

int main(void)
{
    /* 预算口径固定为缺省 30 条，避免宿主环境变量影响断言。 */
    unsetenv("AIRY_CHAT_HISTORY_ROUNDS");

    if (mock_start() != 0) {
        printf("test_cli_session: mock gateway start failed\n");
        return 1;
    }

    test_request_contract();
    test_turn_split();
    test_role_records();
    test_prefix_variants();
    test_malformed_records();
    test_budget_clamp();
    test_no_local_state();
    test_degraded_paths();

    mock_stop();

    printf("test_cli_session: %d/%d passed\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
