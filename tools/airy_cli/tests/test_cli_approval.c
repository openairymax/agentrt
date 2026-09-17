// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_approval.c
 * @brief /pending 与 /approve 端到端单测（0.1.17 R5-G4）。
 *
 * 本进程内 mock 网关（真实 loopback socket，非桩函数）承接 CLI 侧派发，
 * 断言：
 *   - /approve 出站契约：method=tool.approve，params.request_id /
 *     params.decision 与用户输入逐字一致（allow / always / deny 三值）；
 *   - 本地自检：缺参、单 token、非法决议一律不出站（不把非法决议推给
 *     网关换回英文报错）；
 *   - /pending 只读视图：method=tool.pending，result.pending 数组遍历
 *     渲染（空列表 / 双项列表 / 数组直出三形态）不崩溃；
 *   - 网关不可达：两条命令均降级为错误提示并返回 0。
 *
 * @owner: team-B
 */

#include "daemon_cmds.h"
#include "cli_gw.h"
#include "cli_render.h"
#include "cli_tui.h"

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
static char g_last_params[512] = "";
static char g_pending_body[1024] = "";

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
    char method[128];
    snprintf(method, sizeof(method), "%s", g_last_method);
    char pending_body[1024];
    snprintf(pending_body, sizeof(pending_body), "%s", g_pending_body);
    pthread_mutex_unlock(&g_lock);

    char resp[1200];
    if (strcmp(method, "tool.pending") == 0) {
        snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":%s}",
                 pending_body);
    } else {
        snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":"
                                     "{\"resolved\":true,\"request_id\":\"req_x\","
                                     "\"decision\":\"allow\"}}");
    }
    mock_send(fd, resp);
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

static int req_params_is(const char *key, const char *want)
{
    cJSON *root = cJSON_Parse(g_last_params);
    if (!root)
        return 0;
    cJSON *v = cJSON_GetObjectItem(root, key);
    int ok = cJSON_IsString(v) && v->valuestring && strcmp(v->valuestring, want) == 0;
    cJSON_Delete(root);
    return ok;
}

/* ── 用例 ─────────────────────────────────────────────────────────── */

static void test_approve_roundtrip(void)
{
    static const char *decisions[] = {"allow", "always", "deny"};
    for (size_t i = 0; i < sizeof(decisions) / sizeof(decisions[0]); i++) {
        char arg[64];
        snprintf(arg, sizeof(arg), "req_1_%zu %s", i, decisions[i]);
        char want_id[32];
        snprintf(want_id, sizeof(want_id), "req_1_%zu", i);

        mock_reset();
        CHECK(cmd_approve(arg, NULL) == 0, "approve: returns 0");
        CHECK(g_reqs == 1 && strcmp(g_last_method, "tool.approve") == 0,
              "approve: dispatches tool.approve");
        CHECK(req_params_is("request_id", want_id), "approve: request_id verbatim");
        CHECK(req_params_is("decision", decisions[i]), "approve: decision verbatim");
    }

    /* 决议后带尾随 token（交互输入尾随空格）：仍按首 token 判定。 */
    mock_reset();
    CHECK(cmd_approve("req_2_0 deny ", NULL) == 0 && g_reqs == 1 &&
              req_params_is("decision", "deny"),
          "approve: trailing token ignored");
}

static void test_approve_selfcheck(void)
{
    mock_reset();
    CHECK(cmd_approve(NULL, NULL) == 0 && g_reqs == 0, "approve: no arg -> no dispatch");
    CHECK(cmd_approve("", NULL) == 0 && g_reqs == 0, "approve: empty arg -> no dispatch");
    CHECK(cmd_approve("req_3_0", NULL) == 0 && g_reqs == 0,
          "approve: missing decision -> no dispatch");
    CHECK(cmd_approve("req_3_0 bogus", NULL) == 0 && g_reqs == 0,
          "approve: invalid decision -> no dispatch");
    CHECK(cmd_approve(" req_3_0 allow", NULL) == 0 && g_reqs == 0,
          "approve: empty request_id -> no dispatch");
}

static void test_pending_view(void)
{
    snprintf(g_pending_body, sizeof(g_pending_body),
             "{\"pending\":["
             "{\"request_id\":\"req_9_1\",\"tool\":\"shell_run\",\"agent_id\":\"agent-1\","
             "\"params\":\"{\\\"cmd\\\":\\\"ls\\\"}\",\"created_at\":1789566000000},"
             "{\"request_id\":\"req_9_2\",\"tool\":\"git_push\",\"agent_id\":\"agent-2\","
             "\"params\":\"{}\",\"created_at\":1789566001000}]}");

    mock_reset();
    CHECK(cmd_pending(NULL, NULL) == 0, "pending: returns 0");
    CHECK(g_reqs == 1 && strcmp(g_last_method, "tool.pending") == 0,
          "pending: dispatches tool.pending");

    /* 空列表 */
    snprintf(g_pending_body, sizeof(g_pending_body), "{\"pending\":[]}");
    mock_reset();
    CHECK(cmd_pending(NULL, NULL) == 0 && g_reqs == 1, "pending: empty list tolerated");

    /* result 直接是数组（与 TUI 客户端同容错） */
    snprintf(g_pending_body, sizeof(g_pending_body),
             "[{\"request_id\":\"req_9_3\",\"tool\":\"shell_run\","
             "\"agent_id\":\"agent-1\",\"params\":\"{}\",\"created_at\":1789566002000}]");
    mock_reset();
    CHECK(cmd_pending(NULL, NULL) == 0 && g_reqs == 1, "pending: bare array tolerated");

    /* 字段缺失：不得崩溃 */
    snprintf(g_pending_body, sizeof(g_pending_body), "{\"pending\":[{}]}");
    mock_reset();
    CHECK(cmd_pending(NULL, NULL) == 0 && g_reqs == 1, "pending: missing fields tolerated");
}

static void test_gateway_offline(void)
{
    char saved[64];
    const char *cur = getenv("AIRY_GATEWAY_URL");
    snprintf(saved, sizeof(saved), "%s", cur ? cur : "");
    setenv("AIRY_GATEWAY_URL", "http://airymaxrt.invalid:1", 1);

    CHECK(cmd_pending(NULL, NULL) == 0, "pending: offline degrades to error line");
    CHECK(cmd_approve("req_4_0 allow", NULL) == 0, "approve: offline degrades to error line");

    setenv("AIRY_GATEWAY_URL", saved, 1);
}

int main(void)
{
    if (mock_start() != 0) {
        printf("test_cli_approval: mock gateway start failed\n");
        return 1;
    }

    test_approve_roundtrip();
    test_approve_selfcheck();
    test_pending_view();
    test_gateway_offline();

    mock_stop();

    printf("test_cli_approval: %d/%d passed\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
