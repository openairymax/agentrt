// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_live.c
 * @brief cli_live（会话存活感知）端到端单测（0.1.19 §3.7.2 N4）。
 *
 * 覆盖后端重启后的会话自愈主链：空闲节拍经 gateway 探测 /health 代次与
 * per-daemon health_check，命中迁移时提示并触发一次重建。测试用本进程内
 * mock HTTP/1.1 服务端（AF_INET 127.0.0.1:0 真实 socket，非桩函数）驱动：
 *   - 基线：首轮只建立代次/可用性视图，不得误判为重启；
 *   - 代次：epoch 变化 → 重建恰一次，稳态不重复；
 *   - 兜底：网关未发布 epoch 时，服务 down→up 才触发重建（下线时刻不重建）；
 *   - 不确定：探测超时保持既有视图，且不污染业务诊断文本；
 *   - 断连：网关不可达不动视图，恢复后凭代次变化重建；
 *   - 关闭：AIRY_CLI_LIVE_MS=0 时零请求零重建。
 *
 * @owner: team-B
 */

#include "cli_live.h"

#include "cli_gw.h"
#include "platform.h"

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
#include <sys/stat.h>
#include <unistd.h>

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

#define MOCK_SVC_N 6

static const char *const g_ns[MOCK_SVC_N] = {"mem", "think", "llm", "tool", "agent", "sched"};

static int g_srv_fd = -1;
static int g_srv_port = 0;
static pthread_t g_srv_thr;
static _Atomic int g_stop = 0;
static _Atomic int g_epoch = 111;    /* 0 → /health 不携带 epoch（旧版网关） */
static _Atomic unsigned g_offline = 0; /* bit i → 第 i 个服务 health_check 失败 */
static _Atomic int g_silent = 0;     /* 1 → health_check 不应答，驱动探测超时 */
static _Atomic int g_health_reqs = 0;
static _Atomic int g_probe_reqs = 0;
static _Atomic int g_heals = 0;

static void on_stale(void)
{
    g_heals++;
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

static int mock_svc_index(const char *method)
{
    for (int i = 0; i < MOCK_SVC_N; i++) {
        char pre[16];
        int n = snprintf(pre, sizeof(pre), "%s.", g_ns[i]);
        if (n > 0 && strncmp(method, pre, (size_t)n) == 0)
            return i;
    }
    return -1;
}

static void mock_health(int fd)
{
    g_health_reqs++;
    char body[96];
    int e = g_epoch;
    if (e > 0)
        snprintf(body, sizeof(body), "{\"status\":\"healthy\",\"epoch\":%d}", e);
    else
        snprintf(body, sizeof(body), "{\"status\":\"healthy\"}");
    mock_send(fd, 200, body);
}

static void mock_rpc(int fd, const char *body)
{
    g_probe_reqs++;
    char method[128] = "";
    cJSON *root = cJSON_Parse(body);
    if (root) {
        cJSON *m = cJSON_GetObjectItem(root, "method");
        if (cJSON_IsString(m))
            snprintf(method, sizeof(method), "%s", m->valuestring);
        cJSON_Delete(root);
    }
    if (g_silent) {
        airy_sleep_ms(300); /* 超过探测预算 200ms，驱动客户端超时 */
        return;
    }
    int idx = mock_svc_index(method);
    if (idx < 0) {
        mock_send(fd, 200,
                  "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32601,"
                  "\"message\":\"Method not found\"}}");
        return;
    }
    if (g_offline & (1u << idx)) {
        mock_send(fd, 200,
                  "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32603,"
                  "\"message\":\"service restarting\"}}");
        return;
    }
    mock_send(fd, 200, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"ok\":true}}");
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

    if (strcmp(path, "/health") == 0) {
        mock_health(fd);
        return;
    }
    mock_rpc(fd, buf + hlen);
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
    addr.sin_port = 0; /* 内核分配端口，ctest -j 并行不冲突 */
    if (bind(g_srv_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return -1;
    socklen_t alen = sizeof(addr);
    if (getsockname(g_srv_fd, (struct sockaddr *)&addr, &alen) != 0)
        return -1;
    g_srv_port = ntohs(addr.sin_port);
    if (listen(g_srv_fd, 16) != 0)
        return -1;
    if (pthread_create(&g_srv_thr, NULL, mock_thread, NULL) != 0)
        return -1;
    return 0;
}

static void mock_stop(void)
{
    g_stop = 1;
    /* 自连接唤醒 accept：macOS/BSD 下 shutdown() 不唤醒监听套接字。 */
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

static void live_reset(void)
{
    g_heals = 0;
    cli_live_bind(on_stale);
    cli_live_init();
}

/* 节拍周期 50ms：等待一拍后推进，避免同一拍被 now<next 门挡住 */
static void tick(void)
{
    airy_sleep_ms(70);
    cli_live_tick();
}

/* ── 用例 ─────────────────────────────────────────────────────────── */

static void test_disabled(void)
{
    setenv("AIRY_CLI_LIVE_MS", "0", 1);
    live_reset();
    int h0 = g_health_reqs;
    int p0 = g_probe_reqs;
    tick();
    CHECK(g_health_reqs == h0 && g_probe_reqs == p0, "disabled: no probing");
    CHECK(g_heals == 0, "disabled: no rebuild");
    setenv("AIRY_CLI_LIVE_MS", "50", 1);
}

static void test_baseline(void)
{
    g_epoch = 111;
    g_offline = 0;
    g_silent = 0;
    live_reset();
    int h0 = g_health_reqs;
    int p0 = g_probe_reqs;
    tick();
    CHECK(g_health_reqs - h0 == 1, "baseline: one gateway probe");
    CHECK(g_probe_reqs - p0 == MOCK_SVC_N, "baseline: probes every service");
    CHECK(g_heals == 0, "baseline: first tick is not a restart");
    tick();
    CHECK(g_heals == 0, "baseline: steady state does not rebuild");
}

static void test_epoch_change(void)
{
    g_epoch = 111;
    g_offline = 0;
    g_silent = 0;
    live_reset();
    tick();
    g_epoch = 222;
    tick();
    CHECK(g_heals == 1, "epoch: change rebuilds exactly once");
    tick();
    CHECK(g_heals == 1, "epoch: no repeat while steady");
}

static void test_fallback(void)
{
    g_epoch = 0; /* 旧版网关：无代次，仅靠服务 down→up 判定 */
    g_offline = 0;
    g_silent = 0;
    live_reset();
    tick();
    CHECK(g_heals == 0, "fallback: baseline");
    g_offline = 1u << 0; /* mem 下线 */
    tick();
    CHECK(g_heals == 0, "fallback: going down is not a rebuild");
    g_offline = 0;
    tick();
    CHECK(g_heals == 1, "fallback: down->up rebuilds");
    tick();
    CHECK(g_heals == 1, "fallback: no repeat while steady");
}

static void test_daemon_restart(void)
{
    g_epoch = 111; /* 网关代次不变：只有单个 daemon 重建 */
    g_offline = 0;
    g_silent = 0;
    live_reset();
    tick();
    CHECK(g_heals == 0, "daemon: baseline");
    g_offline = 1u << 2; /* llm 下线 */
    tick();
    CHECK(g_heals == 0, "daemon: going down is not a rebuild");
    g_offline = 0;
    tick();
    CHECK(g_heals == 1, "daemon: down->up rebuilds under a stable epoch");
    tick();
    CHECK(g_heals == 1, "daemon: no repeat while steady");
}

static void test_timeout(void)
{
    g_epoch = 0;
    g_offline = 0;
    g_silent = 0;
    live_reset();
    tick();
    cli_gw_err_set("业务原文");
    g_silent = 1;
    tick(); /* 探测全部超时：结果不确定，视图不得变化 */
    g_silent = 0;
    airy_sleep_ms(1200); /* 排空 mock 队列中积压的静默连接 */
    CHECK(g_heals == 0, "timeout: uncertain result keeps view");
    CHECK(strcmp(cli_gw_last_err(), "业务原文") == 0, "timeout: business diag preserved");
    tick();
    CHECK(g_heals == 0, "timeout: recovery is not a rebuild");
}

static void test_gateway_outage(void)
{
    g_epoch = 111;
    g_offline = 0;
    g_silent = 0;
    set_url_port(g_srv_port);
    live_reset();
    tick();
    setenv("AIRY_GATEWAY_URL", "http://127.0.0.1:1", 1); /* 不可达 */
    tick();
    CHECK(g_heals == 0, "outage: unreachable gateway keeps view");
    set_url_port(g_srv_port);
    tick();
    CHECK(g_heals == 0, "outage: same epoch on recovery");
    g_epoch = 333; /* 网关进程重建 */
    tick();
    CHECK(g_heals == 1, "outage: rebuilt epoch triggers rebuild");
}

static void test_unbound(void)
{
    g_epoch = 111;
    g_offline = 0;
    g_silent = 0;
    g_heals = 0;
    cli_live_bind(NULL);
    cli_live_init();
    tick();
    g_epoch = 999;
    tick();
    CHECK(g_heals == 0, "unbound: stale detected without callback");
}

int main(void)
{
    printf("=== airy_cli cli_live (session liveness) E2E Tests ===\n\n");

    /* 隔离工作目录：无 run/gateway.port，端点只由 AIRY_GATEWAY_URL 决定 */
    char tmpdir[256];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/airy_cli_live_e2e_%d", (int)getpid());
    (void)mkdir(tmpdir, 0700);
    setenv("AIRY_HOME", tmpdir, 1);

    if (mock_start() != 0) {
        fprintf(stderr, "FAILED: mock server start\n");
        return 1;
    }
    set_url_port(g_srv_port);
    setenv("AIRY_CLI_LIVE_MS", "50", 1);

    test_disabled();
    test_baseline();
    test_epoch_change();
    test_fallback();
    test_daemon_restart();
    test_timeout();
    test_gateway_outage();
    test_unbound();

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
