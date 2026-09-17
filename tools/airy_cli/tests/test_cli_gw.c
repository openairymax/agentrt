// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_gw.c
 * @brief cli_gw（统一网关客户端）端到端单测（0.1.16 审计 E2E）。
 *
 * 覆盖 CLI 组织面访问微核心服务的唯一合规通路：JSON-RPC over HTTP POST
 * → gateway。测试用本进程内 mock HTTP/1.1 服务端（AF_INET 127.0.0.1:0
 * 真实 socket，非桩函数）验证：
 *   - 端点发现：默认 127.0.0.1:8080 / AIRY_GATEWAY_URL 两形态（http://
 *     host:port 与裸 host:port）/ 运行时端口文件 $AIRY_HOME/run/gateway.port；
 *   - 调用往返：envelope（jsonrpc=2.0 / id=1 / method / params）逐字段
 *     抵达服务端，result 原文返回（OWNER AIRY_FREE）；
 *   - 失败分诊：JSON-RPC -32601 → "不支持该方法"；连接拒绝 → "网关不在线"；
 *     接收预算耗尽 → "响应超时"（与离线区分）；SIGINT → AIRY_ERR_CANCELED；
 *   - 可达性：/health body 含 "healthy" → 1，否则 0。
 *
 * @owner: team-B
 */

#include "cli_gw.h"

#include "error_codes.h"

#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/* cli_gw.c 依赖的跨 TU 符号：SIGINT 取消标志由 main.c 拥有（S-02），本测试
 * 作为独立可执行体定义它；失败原因经 cli_gw_last_err() 读取断言。 */
volatile sig_atomic_t g_cli_cancel = 0;

/* ── 极简测试框架 ─────────────────────────────────────────────────── */

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define CHECK(cond, name)                                                                          \
    do {                                                                                           \
        g_tests_run++;                                                                             \
        if (cond) {                                                                                \
            printf("  [PASS] %s\n", name);                                                         \
            g_tests_passed++;                                                                      \
        } else {                                                                                   \
            printf("  [FAIL] %s (line %d)\n", name, __LINE__);                                     \
        }                                                                                          \
    } while (0)

/* ── mock HTTP/1.1 服务端 ─────────────────────────────────────────── */

#define MOCK_MODE_OK     0 /* POST / → result OK；/health → healthy:true */
#define MOCK_MODE_ERROR  1 /* POST / → JSON-RPC error -32601 */
#define MOCK_MODE_SILENT 2 /* 收完请求后保持静默，用于驱动客户端超时 */
#define MOCK_MODE_BADJSON 3 /* POST / → 响应体非合法 JSON */
#define MOCK_MODE_NORESULT 4 /* POST / → 合法 JSON 但既无 result 也无 error */

static int g_srv_fd = -1;
static int g_srv_port = 0;
static pthread_t g_srv_thr;
static _Atomic int g_stop = 0;
static _Atomic int g_mode = MOCK_MODE_OK;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int g_reqs = 0;
static char g_last_method[128] = "";
static char g_last_params[512] = "";
static char g_last_jsonrpc[16] = "";
static _Atomic int g_last_id = -1;
static char g_last_path[64] = "";

static void mock_reset(void)
{
    pthread_mutex_lock(&g_lock);
    g_reqs = 0;
    g_last_method[0] = '\0';
    g_last_params[0] = '\0';
    g_last_jsonrpc[0] = '\0';
    g_last_id = -1;
    g_last_path[0] = '\0';
    pthread_mutex_unlock(&g_lock);
}

static void mock_send(int fd, int status, const char *body)
{
    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      status, status == 200 ? "OK" : "Error", strlen(body));
    (void)send(fd, hdr, (size_t)hn, MSG_NOSIGNAL);
    (void)send(fd, body, strlen(body), MSG_NOSIGNAL);
}

static void mock_handle(int fd)
{
    char buf[8192];
    size_t n = 0;
    /* 读请求头至 "\r\n\r\n" */
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
    buf[n] = '\0';

    char method[8] = "";
    char path[64] = "";
    (void)sscanf(buf, "%7s %63s HTTP/1.1", method, path);

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

    /* 补齐 body 至 Content-Length 声明长度（clen==0 时不得进入阻塞 recv） */
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
    g_reqs++;
    snprintf(g_last_path, sizeof(g_last_path), "%s", path);
    if (body && *body) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            cJSON *m = cJSON_GetObjectItem(root, "method");
            cJSON *id = cJSON_GetObjectItem(root, "id");
            cJSON *jr = cJSON_GetObjectItem(root, "jsonrpc");
            cJSON *p = cJSON_GetObjectItem(root, "params");
            if (cJSON_IsString(m))
                snprintf(g_last_method, sizeof(g_last_method), "%s", m->valuestring);
            if (cJSON_IsNumber(id))
                g_last_id = (int)id->valuedouble;
            if (cJSON_IsString(jr))
                snprintf(g_last_jsonrpc, sizeof(g_last_jsonrpc), "%s", jr->valuestring);
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
    int mode = g_mode;
    pthread_mutex_unlock(&g_lock);

    /* /health 按真实网关契约：仅 GET 返回 200 healthy，其他方法 404
     * -32601（方法契约锁进 mock，防止探针方法回归）。 */
    if (strcmp(path, "/health") == 0) {
        if (strcmp(method, "GET") == 0)
            mock_send(fd, 200, "{\"status\":\"healthy\",\"version\":\"0.1.16\"}");
        else
            mock_send(fd, 404, "{\"jsonrpc\":\"2.0\",\"id\":null,"
                               "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
        return;
    }
    if (mode == MOCK_MODE_SILENT) {
        usleep(800 * 1000); /* 静默至客户端预算耗尽 */
        return;
    }
    if (mode == MOCK_MODE_ERROR) {
        mock_send(fd, 200, "{\"jsonrpc\":\"2.0\",\"id\":1,"
                           "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
        return;
    }
    if (mode == MOCK_MODE_BADJSON) {
        mock_send(fd, 200, "<html>gateway panic</html>");
        return;
    }
    if (mode == MOCK_MODE_NORESULT) {
        mock_send(fd, 200, "{\"jsonrpc\":\"2.0\",\"id\":1}");
        return;
    }
    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"echo\":true,\"method\":\"%s\"}}",
             g_last_method);
    mock_send(fd, 200, resp);
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
            /* mock_stop() 的自连接唤醒连接：不为它服务，直接收线程 */
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
    addr.sin_port = 0; /* 由内核分配，避免 ctest -j 并行端口冲突 */
    if (bind(g_srv_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -1;
    socklen_t alen = sizeof(addr);
    if (getsockname(g_srv_fd, (struct sockaddr *)&addr, &alen) != 0)
        return -1;
    g_srv_port = ntohs(addr.sin_port);
    if (listen(g_srv_fd, 8) != 0)
        return -1;
    if (pthread_create(&g_srv_thr, NULL, mock_thread, NULL) != 0)
        return -1;
    return 0;
}

static void mock_stop(void)
{
    g_stop = 1;
    /* 唤醒阻塞在 accept() 的 mock 线程：shutdown() 对监听套接字在 Linux 上
     * 可唤醒 accept()，但 macOS/BSD 语义下返回 ENOTCONN 且不唤醒（0.1.16
     * macos cli_gw_e2e 超时实证：pthread_join 永久阻塞至 ctest 240s 掐断）。
     * 改用自连接——向本服务端口发起一次连接使 accept() 返回，线程见 g_stop
     * 即收（见 mock_thread），跨平台行为一致，不依赖平台 socket 细节。 */
    if (g_srv_fd >= 0) {
        int w = socket(AF_INET, SOCK_STREAM, 0);
        if (w >= 0) {
            struct sockaddr_in a;
            memset(&a, 0, sizeof(a));
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.sin_port = htons((unsigned short)g_srv_port);
            (void)connect(w, (struct sockaddr *)&a, sizeof(a));
            close(w);
        }
    }
    pthread_join(g_srv_thr, NULL);
    if (g_srv_fd >= 0) {
        close(g_srv_fd);
        g_srv_fd = -1;
    }
}

/* ── 辅助 ─────────────────────────────────────────────────────────── */

static void set_url_port(int port)
{
    char url[64];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d", port);
    setenv("AIRY_GATEWAY_URL", url, 1);
}

/* ── 用例 1-5：端点发现 ───────────────────────────────────────────── */

static void test_endpoint_default(const char *tmpdir)
{
    unsetenv("AIRY_GATEWAY_URL");
    setenv("AIRY_HOME", tmpdir, 1); /* 目录内无 run/gateway.port → 回落默认 */
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    CHECK(strcmp(host, "127.0.0.1") == 0, "endpoint_default host=127.0.0.1");
    CHECK(port == 8080, "endpoint_default port=8080");
}

static void test_endpoint_url_http_prefix(void)
{
    setenv("AIRY_GATEWAY_URL", "http://127.0.0.1:18099", 1);
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    CHECK(strcmp(host, "127.0.0.1") == 0, "endpoint_url(http://) host");
    CHECK(port == 18099, "endpoint_url(http://) port=18099");
}

static void test_endpoint_url_bare(void)
{
    setenv("AIRY_GATEWAY_URL", "10.0.0.5:18098", 1);
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    CHECK(strcmp(host, "10.0.0.5") == 0, "endpoint_url(bare) host");
    CHECK(port == 18098, "endpoint_url(bare) port=18098");
}

static void test_endpoint_runtime_port(const char *tmpdir)
{
    unsetenv("AIRY_GATEWAY_URL");
    setenv("AIRY_HOME", tmpdir, 1);
    char path[512];
    snprintf(path, sizeof(path), "%s/run/gateway.port", tmpdir);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs("18091\n", fp);
        fclose(fp);
    } else {
        CHECK(0, "endpoint_runtime_port write gateway.port");
        return;
    }
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    CHECK(strcmp(host, "127.0.0.1") == 0, "endpoint_runtime host");
    CHECK(port == 18091, "endpoint_runtime port=18091");
}

static void test_endpoint_runtime_port_invalid(const char *tmpdir)
{
    unsetenv("AIRY_GATEWAY_URL");
    setenv("AIRY_HOME", tmpdir, 1);
    char path[512];
    snprintf(path, sizeof(path), "%s/run/gateway.port", tmpdir);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs("abc\n", fp);
        fclose(fp);
    } else {
        CHECK(0, "endpoint_runtime_invalid write gateway.port");
        return;
    }
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    CHECK(port == 8080, "endpoint_runtime invalid port -> 8080");
}

/* ── 用例 6-10：调用往返与失败分诊 ────────────────────────────────── */

static void test_call_ok_roundtrip(void)
{
    mock_reset();
    g_mode = MOCK_MODE_OK;
    set_url_port(g_srv_port);
    char *out = NULL;
    int rc = cli_gw_call("echo", "{\"a\":1}", 3000, &out);
    CHECK(rc == 0, "call_ok rc==0");
    CHECK(out != NULL, "call_ok out!=NULL");
    /* envelope 逐字段抵达服务端 */
    CHECK(strcmp(g_last_method, "echo") == 0, "call_ok server saw method=echo");
    CHECK(g_last_id == 1, "call_ok server saw id=1");
    CHECK(strcmp(g_last_jsonrpc, "2.0") == 0, "call_ok server saw jsonrpc=2.0");
    CHECK(strcmp(g_last_params, "{\"a\":1}") == 0, "call_ok server saw params echoed");
    CHECK(strcmp(g_last_path, "/") == 0, "call_ok path=/");
    /* result 原文 */
    if (out) {
        cJSON *root = cJSON_Parse(out);
        cJSON *echo = root ? cJSON_GetObjectItem(root, "echo") : NULL;
        cJSON *m = root ? cJSON_GetObjectItem(root, "method") : NULL;
        CHECK(echo && cJSON_IsTrue(echo), "call_ok result.echo==true");
        CHECK(m && cJSON_IsString(m) && strcmp(m->valuestring, "echo") == 0,
              "call_ok result.method==echo");
        if (root)
            cJSON_Delete(root);
        free(out); /* cli_gw_call 契约：OWNER AIRY_FREE */
    }
    g_mode = MOCK_MODE_OK;
}

static void test_call_error_32601(void)
{
    mock_reset();
    g_mode = MOCK_MODE_ERROR;
    set_url_port(g_srv_port);
    char *out = NULL;
    int rc = cli_gw_call("no.such.method", "{}", 3000, &out);
    CHECK(rc == -1, "call_error_32601 rc==-1");
    CHECK(out == NULL, "call_error_32601 out==NULL");
    CHECK(strstr(cli_gw_last_err(), "不支持该方法") != NULL,
          "call_error_32601 err mentions 不支持该方法");
    g_mode = MOCK_MODE_OK;
}

static void test_call_badjson(void)
{
    mock_reset();
    g_mode = MOCK_MODE_BADJSON;
    set_url_port(g_srv_port);
    char *out = NULL;
    int rc = cli_gw_call("echo", "{}", 3000, &out);
    CHECK(rc == -1, "call_badjson rc==-1");
    CHECK(out == NULL, "call_badjson out==NULL");
    CHECK(strstr(cli_gw_last_err(), "不是合法 JSON") != NULL,
          "call_badjson err mentions 不是合法 JSON");
    g_mode = MOCK_MODE_OK;
}

static void test_call_noresult(void)
{
    mock_reset();
    g_mode = MOCK_MODE_NORESULT;
    set_url_port(g_srv_port);
    char *out = NULL;
    int rc = cli_gw_call("echo", "{}", 3000, &out);
    CHECK(rc == -1, "call_noresult rc==-1");
    CHECK(out == NULL, "call_noresult out==NULL");
    CHECK(strstr(cli_gw_last_err(), "缺少 result") != NULL,
          "call_noresult err mentions 缺少 result");
    g_mode = MOCK_MODE_OK;
}

static void test_call_offline(void)
{
    setenv("AIRY_GATEWAY_URL", "http://127.0.0.1:1", 1); /* 1 端口不可达 */
    char *out = NULL;
    int rc = cli_gw_call("echo", "{}", 1000, &out);
    CHECK(rc == -1, "call_offline rc==-1");
    CHECK(out == NULL, "call_offline out==NULL");
    CHECK(strstr(cli_gw_last_err(), "网关不在线") != NULL, "call_offline err mentions 网关不在线");
}

static void test_call_timeout(void)
{
    mock_reset();
    g_mode = MOCK_MODE_SILENT;
    set_url_port(g_srv_port);
    char *out = NULL;
    int rc = cli_gw_call("slow.method", "{}", 400, &out);
    CHECK(rc == -1, "call_timeout rc==-1");
    CHECK(out == NULL, "call_timeout out==NULL");
    CHECK(strstr(cli_gw_last_err(), "响应超时") != NULL, "call_timeout err mentions 响应超时");
    /* 超时 ≠ 离线：文案不得误报"网关不在线" */
    CHECK(strstr(cli_gw_last_err(), "网关不在线") == NULL, "call_timeout not misreported offline");
    g_mode = MOCK_MODE_OK;
}

static void test_call_canceled(void)
{
    mock_reset();
    g_mode = MOCK_MODE_OK;
    set_url_port(g_srv_port);
    g_cli_cancel = 1; /* 模拟 SIGINT 已置位 */
    char *out = NULL;
    int rc = cli_gw_call("echo", "{}", 3000, &out);
    g_cli_cancel = 0;
    CHECK(rc == AIRY_ERR_CANCELED, "call_canceled rc==AIRY_ERR_CANCELED");
    CHECK(out == NULL, "call_canceled out==NULL");
}

/* ── 用例 11-12：/health 可达性 ───────────────────────────────────── */

static void test_health_online(void)
{
    mock_reset();
    g_mode = MOCK_MODE_OK;
    set_url_port(g_srv_port);
    int h = cli_gw_health(2000);
    CHECK(h == 1, "health_online == 1");
    CHECK(strcmp(g_last_path, "/health") == 0, "health_online path=/health");
}

static void test_health_offline(void)
{
    setenv("AIRY_GATEWAY_URL", "http://127.0.0.1:1", 1);
    int h = cli_gw_health(500);
    CHECK(h == 0, "health_offline == 0");
}

int main(void)
{
    printf("=== airy_cli cli_gw (gateway client) E2E Tests ===\n\n");

    /* 隔离的工作目录：供运行时端口文件用例写入 */
    char tmpdir[256];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/airy_cli_gw_e2e_%d", (int)getpid());
    (void)mkdir(tmpdir, 0700);
    char runsub[320];
    snprintf(runsub, sizeof(runsub), "%s/run", tmpdir);
    (void)mkdir(runsub, 0700);

    test_endpoint_default(tmpdir);
    test_endpoint_url_http_prefix();
    test_endpoint_url_bare();
    test_endpoint_runtime_port(tmpdir);
    test_endpoint_runtime_port_invalid(tmpdir);

    if (mock_start() != 0) {
        fprintf(stderr, "FAILED: mock server start\n");
        return 1;
    }

    test_call_ok_roundtrip();
    test_call_error_32601();
    test_call_badjson();
    test_call_noresult();
    test_call_offline();
    test_call_timeout();
    test_call_canceled();
    test_health_online();
    test_health_offline();

    mock_stop();
    unsetenv("AIRY_GATEWAY_URL");
    unsetenv("AIRY_HOME");

    printf("\n%d/%d passed\n", g_tests_passed, g_tests_run);
    if (g_tests_passed != g_tests_run) {
        fprintf(stderr, "FAILED: %d/%d tests passed\n", g_tests_passed, g_tests_run);
        return 1;
    }
    return 0;
}
