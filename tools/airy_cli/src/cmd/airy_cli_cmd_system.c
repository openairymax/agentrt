// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_system.c
 * @brief System-face daemon commands: RPC infrastructure, health check,
 *        daemon lifecycle (start/stop/restart), self-heal reconcile.
 *
 * Split from daemon_cmds.c.  Owns the shared CLI_DAEMONS table, namespace
 * resolution, socket path building, the generic RPC printer, the /daemons
 * health check (with gateway TCP probe), and the declarative self-heal
 * reconcile loop.
 */

#include "daemon_cmds.h"
#include "airy_cli_cmd_internal.h"
#include "daemon_rpc_client.h"
#include "cli_gw.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "logger.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CLI_GW_PROBE_TIMEOUT_MS 1000

/* ==================== shared infrastructure ==================== */

const cli_daemon_desc_t CLI_DAEMONS[] = {
    {"agent", "agent.sock", "health_check"},
    {"tool", "tool.sock", "health_check"},
    {"hook", "hook.sock", "health_check"},
    {"think", "think.sock", "health_check"},
    {"monit", "monit.sock", "health_check"},
    {"sched", "sched.sock", "health_check"},
    {"channel", "channel.sock", "health_check"},
    {"market", "market.sock", "health_check"},
    {"llm", "llm.sock", "health_check"},
    {"cupolas", "cupolas.sock", "health_check"},
    {"mem", "mem.sock", "health_check"},
    {"info", "info.sock", "health_check"},
    {"notify", "notify.sock", "health_check"},
    {"observe", "observe.sock", "health_check"},
    {"a2a", "a2a.sock", "health_check"},
};

/* 运行时自动发现（0.1.7）：枚举 $AIRY_HOME/bin 下所有 *_d 后缀可执行
   （排除 gateway_d 与 maths_d——前者为 HTTP 服务、后者为符号计算后端
   无 socket）动态生成 daemon 表。daemon 增删后 /daemons 与 /rpc 自动
   适配，无需改硬编码表。
   socket 名约定 = ns + ".sock"（与 CLI_DAEMONS 一致）。无 AIRY_HOME 或
   目录不可枚举时返回 0，调用方回退 CLI_DAEMONS 默认表。 */
static size_t cli_daemons_discover(cli_daemon_desc_t *out, size_t cap)
{
    size_t n = 0;
    const char *home = getenv("AIRY_HOME");
    if (!home || !*home) return 0;
    char bin[1024];
    snprintf(bin, sizeof(bin), "%s/bin", home);
    DIR *dir = opendir(bin);
    if (!dir) return 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && n < cap) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len < 3 || strcmp(name + len - 2, "_d") != 0) continue;
        if (strcmp(name, "gateway_d") == 0 || strcmp(name, "maths_d") == 0) continue;
        static char ns_pool[32][32];
        static char sock_pool[32][64];
        size_t nlen = len - 2;
        if (nlen >= sizeof(ns_pool[n])) nlen = sizeof(ns_pool[n]) - 1;
        __builtin_memcpy(ns_pool[n], name, nlen);
        ns_pool[n][nlen] = '\0';
        snprintf(sock_pool[n], sizeof(sock_pool[n]), "%s.sock", ns_pool[n]);
        out[n].ns = ns_pool[n];
        out[n].sock = sock_pool[n];
        out[n].health_method = "health_check";
        n++;
    }
    closedir(dir);
    return n;
}

const char *cli_rt_dir(void)
{
    return airy_runtime_dir();
}

int cli_ns_resolve(const char *in, char *out, size_t out_cap)
{
    size_t i, in_len = strlen(in);
    char trimmed[64];
    const char *probe = in;
    if (in_len > 2 && strcmp(in + in_len - 2, "_d") == 0) {
        size_t tlen = in_len - 2;
        if (tlen >= sizeof(trimmed)) tlen = sizeof(trimmed) - 1;
        __builtin_memcpy(trimmed, in, tlen);
        trimmed[tlen] = '\0';
        probe = trimmed;
    }
    for (i = 0; i < CLI_DAEMONS_COUNT; i++) {
        if (strcmp(probe, CLI_DAEMONS[i].ns) == 0) {
            AIRY_STRNCPY_TERM(out, CLI_DAEMONS[i].ns, out_cap);
            return 0;
        }
    }
    /* 0.1.6h：默认表未命中时运行时发现（daemon 增删后自动可寻址） */
    {
        cli_daemon_desc_t dyn[32];
        size_t n = cli_daemons_discover(dyn, 32);
        for (i = 0; i < n; i++) {
            if (strcmp(probe, dyn[i].ns) == 0) {
                AIRY_STRNCPY_TERM(out, dyn[i].ns, out_cap);
                return 0;
            }
        }
    }
    return 1;
}

const char *cli_ns_sock(const char *ns)
{
    static char buf[512];
#ifdef _WIN32
    static const struct { const char *ns; const char *ep; } WIN_NS_TCP[] = {
        {"llm", "127.0.0.1:8080"},     {"tool", "127.0.0.1:8081"},
        {"market", "127.0.0.1:8082"},   {"sched", "127.0.0.1:8083"},
        {"notify", "127.0.0.1:8084"},   {"mem", "127.0.0.1:8085"},
        {"agent", "127.0.0.1:8086"},    {"a2a", "127.0.0.1:8087"},
        {"info", "127.0.0.1:8088"},     {"cupolas", "127.0.0.1:8089"},
        {"think", "127.0.0.1:8090"},    {"observe", "127.0.0.1:8091"},
        {"hook", "127.0.0.1:8093"},
        {"channel", "127.0.0.1:8094"},  {"monit", "127.0.0.1:9090"},
    };
    for (size_t i = 0; i < sizeof(WIN_NS_TCP) / sizeof(WIN_NS_TCP[0]); i++) {
        if (strcmp(ns, WIN_NS_TCP[i].ns) == 0) {
            snprintf(buf, sizeof(buf), "%s", WIN_NS_TCP[i].ep);
            return buf;
        }
    }
    snprintf(buf, sizeof(buf), "%s\\%s.sock", cli_rt_dir(), ns);
    return buf;
#else
    snprintf(buf, sizeof(buf), "%s/%s.sock", cli_rt_dir(), ns);
    return buf;
#endif
}

void cli_rpc_print(const char *ns, const char *method, const char *params_json)
{
    char *result = NULL;
    char full_method[160];
    snprintf(full_method, sizeof(full_method), "%s.%s", ns, method);
    int rc = cli_gw_call(full_method, params_json, CLI_RPC_TIMEOUT_MS, &result);
    if (rc != 0 || !result) {
        char line[160];
        snprintf(line, sizeof(line), "%s.%s 调用失败：%s", ns, method, cli_err_desc(rc));
        cli_render_sub_agent_line(CLI_ROLE_ERROR, ns, line);
        AIRY_FREE(result);
        return;
    }
    if (g_cli_print_mode) {
        cli_outf("%s\n", result);
        AIRY_FREE(result);
        return;
    }
    cli_render_sub_agent(ns, result);
    AIRY_FREE(result);
}

/* ==================== /rpc + /daemons ==================== */

int cmd_rpc(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/rpc <ns>.<method> [json参数]");
        cli_outf("    例: /rpc mem.search {\"query\":\"hello\"}\n");
        cli_outf("        /rpc cupolas.check_permission {\"agent_id\":\"a\",\"action\":\"read\","
               "\"resource\":\"fs:///tmp/x\"}\n");
        return 0;
    }
    const char *dot = strchr(arg, '.');
    if (!dot) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "用法",
                              "需要 <ns>.<method> 形式");
        return 0;
    }
    char ns[64];
    size_t ns_len = (size_t)(dot - arg);
    if (ns_len >= sizeof(ns)) ns_len = sizeof(ns) - 1;
    __builtin_memcpy(ns, arg, ns_len);
    ns[ns_len] = '\0';

    char ns_resolved[64];
    if (cli_ns_resolve(ns, ns_resolved, sizeof(ns_resolved)) != 0) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "RPC",
                             "未知 daemon 命名空间，/daemons 查看在线列表");
        return 0;
    }

    const char *rest = dot + 1;
    char method[128];
    size_t mlen = 0;
    while (rest[mlen] && rest[mlen] != ' ') mlen++;
    if (mlen >= sizeof(method)) mlen = sizeof(method) - 1;
    __builtin_memcpy(method, rest, mlen);
    method[mlen] = '\0';

    const char *params = NULL;
    if (rest[mlen] == ' ') params = rest + mlen + 1;

    cli_rpc_print(ns_resolved, method, params);
    return 0;
}

int cmd_daemons(const char *arg, void *ctx)
{
    (void)arg; (void)ctx;
    int online = 0;
    /* 0.1.7：运行时发现 daemon 表（daemon 增删自动适配），失败回退默认表 */
    cli_daemon_desc_t dyn[32];
    size_t dcount = cli_daemons_discover(dyn, 32);
    size_t total = dcount ? dcount : CLI_DAEMONS_COUNT;
    const cli_daemon_desc_t *tab = dcount ? dyn : CLI_DAEMONS;
    if (!g_cli_print_mode)
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "daemon", "health check");
    for (size_t i = 0; i < total; i++) {
        char *result = NULL;
        int rc = daemon_rpc_call(cli_ns_sock(tab[i].ns), tab[i].health_method,
                                 NULL, &result, 6000);
        if (rc == 0 && result) {
            if (g_cli_print_mode) cli_outf("%s online\n", tab[i].ns);
            else cli_render_task_line(NULL, tab[i].ns, "online", 1.0);
            online++;
        } else {
            if (g_cli_print_mode) cli_outf("%s offline\n", tab[i].ns);
            else cli_render_task_line(NULL, tab[i].ns, "offline", 0.0);
        }
        AIRY_FREE(result);
    }
    /* gateway TCP probe */
    {
        /* 0.1.6h：统一经 cli_gw_endpoint 取实际端口（run/gateway.port
         * 优先，端口漂移兼容），不再裸用 8080——完整启动器漂移到 8083+
         * 后 /daemons 会误报 gateway offline。 */
        char gw_host[128] = "127.0.0.1";
        int gw_port = 8080;
        cli_gw_endpoint(gw_host, sizeof(gw_host), &gw_port);
        int gw_ok = 0;
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
            if (s != INVALID_SOCKET) {
                u_long nb = 1;
                ioctlsocket(s, FIONBIO, &nb);
                struct sockaddr_in sa;
                sa.sin_family = AF_INET;
                sa.sin_port = htons((unsigned short)gw_port);
                sa.sin_addr.s_addr = inet_addr(gw_host);
                if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
                    gw_ok = 1;
                } else {
                    fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
                    struct timeval tv;
                    tv.tv_sec = CLI_GW_PROBE_TIMEOUT_MS / 1000;
                    tv.tv_usec = (long)(CLI_GW_PROBE_TIMEOUT_MS % 1000) * 1000L;
                    if (select(0, NULL, &wf, NULL, &tv) > 0) {
                        int soerr = 0; int slen = sizeof(soerr);
                        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &slen) == 0 &&
                            soerr == 0) gw_ok = 1;
                    }
                }
                closesocket(s);
            }
            WSACleanup();
        }
#else
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s >= 0) {
            int flags = fcntl(s, F_GETFL, 0);
            if (flags >= 0) (void)fcntl(s, F_SETFL, flags | O_NONBLOCK);
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_port = htons((unsigned short)gw_port);
            sa.sin_addr.s_addr = inet_addr(gw_host);
            if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
                gw_ok = 1;
            } else if (errno == EINPROGRESS) {
                struct pollfd pfd;
                pfd.fd = s; pfd.events = POLLOUT; pfd.revents = 0;
                if (poll(&pfd, 1, CLI_GW_PROBE_TIMEOUT_MS) > 0) {
                    int soerr = 0; socklen_t slen = sizeof(soerr);
                    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &slen) == 0 && soerr == 0)
                        gw_ok = 1;
                }
            }
            close(s);
        }
#endif
        if (g_cli_print_mode) cli_outf("gateway %s\n", gw_ok ? "online" : "offline");
        else cli_render_task_line(NULL, "gateway", gw_ok ? "online" : "offline", gw_ok ? 1.0 : 0.0);
        if (gw_ok) online++;
    }
    {
        char line[64];
        snprintf(line, sizeof(line), "online %d/%zu", online, total + 1);
        if (g_cli_print_mode) cli_outf("%s\n", line);
        else cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "daemon", line);
    }
    return 0;
}

/* ==================== daemon lifecycle ==================== */

static const char *cli_rt_base(void) { return airy_home_dir(); }

static int cli_daemon_bin(const char *ns, char *buf, size_t cap)
{
#ifdef _WIN32
    snprintf(buf, cap, "%s\\bin\\%s_d.exe", cli_rt_base(), ns);
#else
    snprintf(buf, cap, "%s/bin/%s_d", cli_rt_base(), ns);
#endif
#ifdef _WIN32
    struct _stat st;
    return (_stat(buf, &st) == 0 && (st.st_mode & _S_IFREG)) ? 1 : 0;
#else
    struct stat st;
    return (stat(buf, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
#endif
}

static int cli_daemon_online(const char *ns)
{
    char *result = NULL;
    int rc = daemon_rpc_call(cli_ns_sock(ns), "health_check", NULL, &result, 6000);
    AIRY_FREE(result);
    return (rc == 0) ? 1 : 0;
}

static int cli_daemon_start(const char *ns)
{
    char bin[512];
    if (!cli_daemon_bin(ns, bin, sizeof(bin))) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, ns,
                             "二进制不存在（$AIRY_HOME/bin 下未找到），先安装/构建");
        return -1;
    }
    if (cli_daemon_online(ns)) {
        cli_render_sub_agent_line(CLI_ROLE_TRACE, ns, "already online");
        return 0;
    }
    char logf[640];
    snprintf(logf, sizeof(logf), "%s/%s_d.log", airy_log_dir(), ns);
#ifdef _WIN32
    {
        STARTUPINFOA si; PROCESS_INFORMATION pi;
        __builtin_memset(&si, 0, sizeof(si));
        __builtin_memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        SECURITY_ATTRIBUTES sa;
        __builtin_memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
        HANDLE log_h = CreateFileA(logf, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (log_h != INVALID_HANDLE_VALUE) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = log_h; si.hStdError = log_h;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }
        char cmd[560]; snprintf(cmd, sizeof(cmd), "\"%s\"", bin);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, DETACHED_PROCESS | CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi)) {
            if (log_h != INVALID_HANDLE_VALUE) CloseHandle(log_h);
            return -1;
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        if (log_h != INVALID_HANDLE_VALUE) CloseHandle(log_h);
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            setsid();
            int fd = open(logf, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
            execl(bin, bin, (char *)NULL);
            _exit(127);
        }
    }
#endif
    return 0;
}

static int cli_daemon_wait_offline(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (cli_daemon_online(ns)) {
        airy_sleep_ms(200); waited += 200;
        if (waited >= timeout_ms) return 0;
    }
    return 1;
}

static int cli_daemon_wait_online(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (!cli_daemon_online(ns)) {
        airy_sleep_ms(200); waited += 200;
        if (waited >= timeout_ms) return 0;
    }
    return 1;
}

static int cli_daemon_stop(const char *ns)
{
    if (!cli_daemon_online(ns)) {
        cli_render_sub_agent_line(CLI_ROLE_TRACE, ns, "already offline");
        return 0;
    }
    char *result = NULL;
    daemon_rpc_call(cli_ns_sock(ns), "shutdown", "{}", &result, 3000);
    AIRY_FREE(result);
    if (!cli_daemon_wait_offline(ns, 5000)) return -1;
    return 0;
}

/* ==================== self-heal reconcile ==================== */

#define CLI_SELF_HEAL_MAX_AGENTS CLI_DAEMONS_COUNT

typedef struct { char ns[32]; int restarts; uint64_t last_restart_ms; } cli_selfheal_agent_t;
typedef struct {
    int enabled; int max_restarts;
    uint64_t backoff_ms; uint64_t poll_interval_ms; uint64_t last_scan_ms;
    size_t agent_count;
    cli_selfheal_agent_t agents[CLI_SELF_HEAL_MAX_AGENTS];
} cli_selfheal_t;

static cli_selfheal_t g_selfheal;

void cli_daemon_lifecycle_init(const char *agents_csv)
{
    if (g_selfheal.enabled) return;
    g_selfheal.max_restarts = 3;
    g_selfheal.backoff_ms = 10000;
    g_selfheal.poll_interval_ms = 5000;

    const char *e_max = getenv("AIRY_SELF_HEAL_MAX_RESTARTS");
    if (e_max && e_max[0]) { long v = strtol(e_max, NULL, 10); if (v >= 0) g_selfheal.max_restarts = (int)v; }
    const char *e_bk = getenv("AIRY_SELF_HEAL_BACKOFF_MS");
    if (e_bk && e_bk[0]) { long long v = strtoll(e_bk, NULL, 10); if (v >= 0) g_selfheal.backoff_ms = (uint64_t)v; }
    const char *e_poll = getenv("AIRY_SELF_HEAL_POLL_MS");
    if (e_poll && e_poll[0]) { long long v = strtoll(e_poll, NULL, 10); if (v >= 100) g_selfheal.poll_interval_ms = (uint64_t)v; }

    if (agents_csv && agents_csv[0]) {
        char csv[512]; AIRY_STRNCPY_TERM(csv, agents_csv, sizeof(csv));
        char *tok = csv;
        while (tok && *tok && g_selfheal.agent_count < CLI_SELF_HEAL_MAX_AGENTS) {
            char *comma = strchr(tok, ',');
            if (comma) *comma = '\0';
            char *p = tok;
            while (*p == ' ' || *p == '\t') p++;
            size_t n = strlen(p);
            while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = '\0';
            if (p[0]) {
                int known = 0;
                for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
                    if (strcmp(CLI_DAEMONS[i].ns, p) == 0) { known = 1; break; }
                }
                if (known) {
                    size_t idx = g_selfheal.agent_count++;
                    AIRY_STRNCPY_TERM(g_selfheal.agents[idx].ns, p, sizeof(g_selfheal.agents[0].ns));
                }
            }
            tok = comma ? comma + 1 : NULL;
        }
    } else {
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
            size_t idx = g_selfheal.agent_count++;
            AIRY_STRNCPY_TERM(g_selfheal.agents[idx].ns, CLI_DAEMONS[i].ns, sizeof(g_selfheal.agents[0].ns));
        }
    }
    if (g_selfheal.agent_count == 0) {
        AIRY_LOG_INFO("cli_lifecycle: no valid agents, self-heal disabled"); return;
    }
    g_selfheal.enabled = 1;
    AIRY_LOG_INFO("cli_lifecycle: agent self-heal enabled (agents=%zu, max_restarts=%d, "
                  "backoff_ms=%llu, poll_ms=%llu)",
                  g_selfheal.agent_count, g_selfheal.max_restarts,
                  (unsigned long long)g_selfheal.backoff_ms, (unsigned long long)g_selfheal.poll_interval_ms);
}

int cli_daemon_lifecycle_reconcile_once(void)
{
    if (!g_selfheal.enabled || g_selfheal.agent_count == 0) return 0;
    uint64_t now = cli_now_ms();
    if (g_selfheal.last_scan_ms != 0 && now - g_selfheal.last_scan_ms < g_selfheal.poll_interval_ms)
        return 0;
    g_selfheal.last_scan_ms = now;

    int restarted = 0;
    for (size_t i = 0; i < g_selfheal.agent_count; i++) {
        cli_selfheal_agent_t *a = &g_selfheal.agents[i];
        if (cli_daemon_online(a->ns)) continue;
        if (g_selfheal.max_restarts >= 0 && a->restarts >= g_selfheal.max_restarts) continue;
        if (g_selfheal.backoff_ms > 0 && now - a->last_restart_ms < g_selfheal.backoff_ms) continue;
        char bin[512];
        if (!cli_daemon_bin(a->ns, bin, sizeof(bin))) continue;
        if (cli_daemon_start(a->ns) != 0) {
            cli_render_sub_agent_line(CLI_ROLE_ERROR, a->ns, "self-heal restart failed"); continue;
        }
        a->restarts++; a->last_restart_ms = now; restarted++;
        char line[128];
        snprintf(line, sizeof(line), "offline → auto-restarted (%d/%d)", a->restarts, g_selfheal.max_restarts);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, a->ns, line);
    }
    return restarted;
}

/* ==================== /daemon + /stats + simple wrappers ==================== */

int cmd_daemon(const char *arg, void *ctx)
{
    (void)ctx;
    char action[16] = "status";
    const char *ns_list[CLI_DAEMONS_COUNT];
    size_t ns_count = 0;

    const char *p = arg;
    while (p && *p == ' ') p++;
    if (p && *p) {
        size_t alen = 0;
        while (p[alen] && p[alen] != ' ') alen++;
        if (alen >= sizeof(action)) alen = sizeof(action) - 1;
        __builtin_memcpy(action, p, alen); action[alen] = '\0';
        p += alen;
        while (p && *p == ' ') p++;
        while (p && *p && ns_count < CLI_DAEMONS_COUNT) {
            const char *tok = p;
            size_t tlen = 0;
            while (tok[tlen] && tok[tlen] != ' ') tlen++;
            char tokbuf[64];
            size_t tc = tlen < sizeof(tokbuf) - 1 ? tlen : sizeof(tokbuf) - 1;
            __builtin_memcpy(tokbuf, tok, tc); tokbuf[tc] = '\0';
            char resolved[64];
            if (cli_ns_resolve(tokbuf, resolved, sizeof(resolved)) == 0)
                ns_list[ns_count++] = AIRY_STRDUP(resolved);
            else
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, tokbuf, "未知 daemon 命名空间，已忽略");
            p = tok + tlen;
            while (*p == ' ') p++;
        }
    }
    if (ns_count == 0) {
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) ns_list[i] = CLI_DAEMONS[i].ns;
        ns_count = CLI_DAEMONS_COUNT;
    }

    if (strcmp(action, "status") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            int up = cli_daemon_online(ns_list[i]);
            if (g_cli_print_mode) cli_outf("%s %s\n", ns_list[i], up ? "online" : "offline");
            else cli_render_task_line(NULL, ns_list[i], up ? "online" : "offline", up ? 1.0 : 0.0);
        }
    } else if (strcmp(action, "start") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            if (cli_daemon_start(ns_list[i]) == 0) {
                const char *st = cli_daemon_wait_online(ns_list[i], 8000) ? "started" : "starting";
                if (g_cli_print_mode) cli_outf("%s %s\n", ns_list[i], st);
                else cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], st);
            }
        }
    } else if (strcmp(action, "stop") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            if (cli_daemon_stop(ns_list[i]) == 0) {
                if (g_cli_print_mode) cli_outf("%s stopped\n", ns_list[i]);
                else cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], "stopped");
            } else {
                if (g_cli_print_mode) cli_outf("%s stop failed (timeout)\n", ns_list[i]);
                else cli_render_sub_agent_line(CLI_ROLE_ERROR, ns_list[i], "stop failed (timeout)");
            }
        }
    } else if (strcmp(action, "restart") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            cli_daemon_stop(ns_list[i]);
            if (cli_daemon_start(ns_list[i]) == 0) {
                const char *st = cli_daemon_wait_online(ns_list[i], 8000) ? "restarted" : "restarting";
                if (g_cli_print_mode) cli_outf("%s %s\n", ns_list[i], st);
                else cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], st);
            }
        }
    } else {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "usage",
                             "/daemon start|stop|restart|status [ns...]（默认全部）");
    }

    for (size_t i = 0; i < ns_count; i++) {
        int is_static = 0;
        for (size_t j = 0; j < CLI_DAEMONS_COUNT; j++) {
            if (ns_list[i] == CLI_DAEMONS[j].ns) { is_static = 1; break; }
        }
        if (!is_static) AIRY_FREE((void *)ns_list[i]);
    }
    return 0;
}

int cmd_stats(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && arg[0]) cli_rpc_print(arg, "get_stats", NULL);
    else { for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) cli_rpc_print(CLI_DAEMONS[i].ns, "get_stats", NULL); }
    return 0;
}

int cmd_agents(const char *arg, void *ctx)    { (void)ctx; cli_rpc_print("agent", arg && arg[0] ? arg : "list", NULL); return 0; }
int cmd_tools(const char *arg, void *ctx)     { (void)ctx; cli_rpc_print("tool", "list_tools", NULL); return 0; }
int cmd_hooks(const char *arg, void *ctx)     { (void)ctx; cli_rpc_print("hook", "list", NULL); return 0; }
int cmd_plugins(const char *arg, void *ctx)   { (void)ctx; cli_rpc_print("plugin", "list", NULL); return 0; }
int cmd_channels(const char *arg, void *ctx)  { (void)ctx; cli_rpc_print("channel", "list", NULL); return 0; }
int cmd_models(const char *arg, void *ctx)    { (void)ctx; cli_rpc_print("llm", "list_models", NULL); return 0; }
