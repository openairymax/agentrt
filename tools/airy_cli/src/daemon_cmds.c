// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file daemon_cmds.c
 * @brief airy_cli daemon commands (JSON-RPC over Unix socket).
 *
 * Wires all 16 Unix-socket daemon namespaces (agent/tool/hook/plugin/think/
 * monit/sched/channel/market/llm/cupolas/mem/info/notify/observe/a2a)
 * through daemon_rpc_call; gateway_d's TCP service is handled separately.
 *
 * Common interface:
 *   - /rpc <ns>.<method> [json] passes through any method
 *   - /daemons full health check
 *   - the rest are wrappers for common methods (/agents /tools /mem ...)
 */

#include "daemon_cmds.h"
#include "daemon_rpc_client.h"
#include "cli_gw.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "logger.h"
#include "platform.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
/* winsock2.h 须先于 windows.h（惯用顺序，避免 winsock.h/winsock2.h 冲突） */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <arpa/inet.h>
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

#define CLI_RPC_TIMEOUT_MS 10000
/* gateway TCP 探测超时：未监听端口在部分网络环境（WSL2/云 NAT/防火墙
 * drop）下 connect 会阻塞至内核超时（~130s）。非阻塞 connect + poll
 * 限时，保证 /daemons 在离线环境快速返回（1.11.6 稳定性）。 */
#define CLI_GW_PROBE_TIMEOUT_MS 1000

typedef struct {
    const char *ns;
    const char *sock;
    const char *health_method;
} cli_daemon_desc_t;

static const cli_daemon_desc_t CLI_DAEMONS[] = {
    {"agent", "agent.sock", "health_check"},
    {"tool", "tool.sock", "health_check"},
    {"hook", "hook.sock", "health_check"},
    {"plugin", "plugin.sock", "health_check"},
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

#define CLI_DAEMONS_COUNT (sizeof(CLI_DAEMONS) / sizeof(CLI_DAEMONS[0]))

const char *cli_rt_dir(void)
{
    return airy_runtime_dir();
}

/* Resolve a daemon namespace against the known table. Accepts both the
 * canonical name ("tool") and the process-name form ("tool_d", the daemon
 * binary suffix) so /rpc tool_d.list_tools and /rpc tool.list_tools both
 * work. Returns 0 on match (out holds the canonical name), non-zero when
 * the namespace is unknown. */
static int cli_ns_resolve(const char *in, char *out, size_t out_cap)
{
    for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
        if (strcmp(in, CLI_DAEMONS[i].ns) == 0) {
            AIRY_STRNCPY_TERM(out, CLI_DAEMONS[i].ns, out_cap);
            return 0;
        }
    }
    size_t in_len = strlen(in);
    if (in_len > 2 && strcmp(in + in_len - 2, "_d") == 0) {
        char trimmed[64];
        size_t tlen = in_len - 2;
        if (tlen >= sizeof(trimmed))
            tlen = sizeof(trimmed) - 1;
        __builtin_memcpy(trimmed, in, tlen);
        trimmed[tlen] = '\0';
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
            if (strcmp(trimmed, CLI_DAEMONS[i].ns) == 0) {
                AIRY_STRNCPY_TERM(out, CLI_DAEMONS[i].ns, out_cap);
                return 0;
            }
        }
    }
    return 1;
}

static const char *cli_ns_sock(const char *ns)
{
    static char buf[512];
#ifdef _WIN32
    /* Windows daemon IPC 走 TCP 回环（daemon_main.h parse_args 强制），
     * socket_path 约定 "host:port"。端口与各 daemon DEFAULT_TCP_PORT /
     * *_DEFAULT_PORT 对齐（含 info=8088/observe=8091 的已调优端口、
     * hook=8093/channel=8094）。 */
    static const struct { const char *ns; const char *ep; } WIN_NS_TCP[] = {
        {"llm", "127.0.0.1:8080"},     {"tool", "127.0.0.1:8081"},
        {"market", "127.0.0.1:8082"},   {"sched", "127.0.0.1:8083"},
        {"notify", "127.0.0.1:8084"},   {"mem", "127.0.0.1:8085"},
        {"agent", "127.0.0.1:8086"},    {"a2a", "127.0.0.1:8087"},
        {"info", "127.0.0.1:8088"},     {"cupolas", "127.0.0.1:8089"},
        {"think", "127.0.0.1:8090"},    {"observe", "127.0.0.1:8091"},
        {"plugin", "127.0.0.1:8092"},   {"hook", "127.0.0.1:8093"},
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

/**
 * @brief 通用 daemon 服务方法调用（统一经 gateway 派发）。
 *
 * 架构约束（2026-08-25）：所有客户端（CLI/TUI/其他）必须经 gateway 派发
 * 到微核心服务，禁止直连 daemon socket。因此业务方法调用（<ns>.<method>）
 * 一律走 cli_gw_call → gateway（gateway 内部再经 SYS_SVC_CALL 派发到 daemon）。
 * 注：daemon 生命周期管理（start/stop/status 探测、shutdown）属宿主进程
 * 管理范畴，仍直连 daemon socket（见 cli_daemon_online/cli_daemon_stop）。
 */
static void cli_rpc_print(const char *ns, const char *method, const char *params_json)
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
    /* One-shot server mode: a command's RPC response IS the result, so print
     * it raw (JSON) instead of routing it through the conversation renderer
     * that -p suppresses. */
    if (g_cli_print_mode) {
        cli_outf("%s\n", result);
        AIRY_FREE(result);
        return;
    }
    cli_render_sub_agent(ns, result);
    AIRY_FREE(result);
}

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
    if (ns_len >= sizeof(ns))
        ns_len = sizeof(ns) - 1;
    __builtin_memcpy(ns, arg, ns_len);
    ns[ns_len] = '\0';

    /* Namespace tolerance: accept "tool_d" and "tool" alike. */
    char ns_resolved[64];
    if (cli_ns_resolve(ns, ns_resolved, sizeof(ns_resolved)) != 0) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "RPC",
                             "未知 daemon 命名空间，/daemons 查看在线列表");
        return 0;
    }

    const char *rest = dot + 1;
    char method[128];
    size_t mlen = 0;
    while (rest[mlen] && rest[mlen] != ' ')
        mlen++;
    if (mlen >= sizeof(method))
        mlen = sizeof(method) - 1;
    __builtin_memcpy(method, rest, mlen);
    method[mlen] = '\0';

    const char *params = NULL;
    if (rest[mlen] == ' ')
        params = rest + mlen + 1;

    cli_rpc_print(ns_resolved, method, params);
    return 0;
}

int cmd_daemons(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    int online = 0;
    if (!g_cli_print_mode)
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "daemon",
                             "health check");
    for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
        char *result = NULL;
        int rc = daemon_rpc_call(cli_ns_sock(CLI_DAEMONS[i].ns), CLI_DAEMONS[i].health_method, NULL,
                                 &result, 6000);
        if (rc == 0 && result) {
            if (g_cli_print_mode)
                cli_outf("%s online\n", CLI_DAEMONS[i].ns);
            else
                cli_render_task_line(NULL, CLI_DAEMONS[i].ns, "online", 1.0);
            online++;
        } else {
            if (g_cli_print_mode)
                cli_outf("%s offline\n", CLI_DAEMONS[i].ns);
            else
                cli_render_task_line(NULL, CLI_DAEMONS[i].ns, "offline", 0.0);
        }
        AIRY_FREE(result);
    }
    /* gateway 是 HTTP 网关（无 Unix socket，CLI_DAEMONS 表不含）——
     * 以 TCP 连接探测其就绪端口（AIRY_GATEWAY_URL 或默认 8080）补报。 */
    {
        const char *gw = getenv("AIRY_GATEWAY_URL");
        int gw_port = 8080;
        if (gw && *gw) {
            const char *colon = strrchr(gw, ':');
            if (colon && colon[1])
                gw_port = atoi(colon + 1);
        }
        int gw_ok = 0;
#ifdef _WIN32
        /* Windows：非阻塞 connect + select 限时探测（WSAEWOULDBLOCK 后
         * 可写即连接建立；避免离线端口阻塞拖慢 /daemons）。 */
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
            if (s != INVALID_SOCKET) {
                u_long nb = 1;
                ioctlsocket(s, FIONBIO, &nb);
                struct sockaddr_in sa;
                sa.sin_family = AF_INET;
                sa.sin_port = htons((unsigned short)gw_port);
                sa.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
                    gw_ok = 1;
                } else {
                    fd_set wf;
                    FD_ZERO(&wf);
                    FD_SET(s, &wf);
                    struct timeval tv;
                    tv.tv_sec = CLI_GW_PROBE_TIMEOUT_MS / 1000;
                    tv.tv_usec = (long)(CLI_GW_PROBE_TIMEOUT_MS % 1000) * 1000L;
                    if (select(0, NULL, &wf, NULL, &tv) > 0) {
                        /* 连接被拒（RST）/网络不可达时 select 同样报告可写：
                         * 须用 SO_ERROR 区分（对齐 POSIX 分支），避免误报 online。 */
                        int soerr = 0;
                        int slen = sizeof(soerr);
                        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &slen) == 0 &&
                            soerr == 0)
                            gw_ok = 1;
                    }
                }
                closesocket(s);
            }
            WSACleanup();
        }
#else
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s >= 0) {
            /* P2-2：F_GETFL 失败时 flags=-1，F_SETFL 会破坏 fd 标志位 */
            int flags = fcntl(s, F_GETFL, 0);
            if (flags >= 0)
                (void)fcntl(s, F_SETFL, flags | O_NONBLOCK);
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_port = htons((unsigned short)gw_port);
            sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
                gw_ok = 1;
            } else if (errno == EINPROGRESS) {
                /* 未监听端口在 WSL2/防火墙 drop 环境会丢 SYN，阻塞 connect
                 * 将挂起至内核超时（约 130s）——poll 限时后以 SO_ERROR 判定。 */
                struct pollfd pfd;
                pfd.fd = s;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                if (poll(&pfd, 1, CLI_GW_PROBE_TIMEOUT_MS) > 0) {
                    int soerr = 0;
                    socklen_t slen = sizeof(soerr);
                    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &slen) == 0 && soerr == 0)
                        gw_ok = 1;
                }
            }
            close(s);
        }
#endif
        if (g_cli_print_mode)
            cli_outf("gateway %s\n", gw_ok ? "online" : "offline");
        else
            cli_render_task_line(NULL, "gateway", gw_ok ? "online" : "offline", gw_ok ? 1.0 : 0.0);
        if (gw_ok)
            online++;
    }
    {
        char line[64];
        snprintf(line, sizeof(line), "online %d/%zu", online, CLI_DAEMONS_COUNT + 1);
        if (g_cli_print_mode)
            cli_outf("%s\n", line);
        else
            cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "daemon", line);
    }
    return 0;
}

/* ==================== daemon 生命周期管理（/daemon start|stop|restart|status） ====================
 *
 * 服务器工业场景：CLI 需能一键管理 agentrt 的全部 daemon，无需手动逐个
 * 拉起进程。daemon 二进制约定在 $AIRY_HOME/bin/<ns>_d（Windows 为
 * <ns>_d.exe），日志写入 $AIRY_HOME/data/agentrt/logs/<ns>_d.log；停止优先走 daemon
 * 注册的优雅 "shutdown" RPC，避免直接杀进程丢失状态。 */

/* AIRY_HOME 根目录（$AIRY_HOME，缺省 $HOME/.airymaxrt）。 */
static const char *cli_rt_base(void)
{
    return airy_home_dir();
}

/* daemon 二进制完整路径（返回 1=存在，0=不存在）。 */
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

/* daemon 是否在线（health_check RPC 成功）。超时与 cmd_daemons 的 6000ms
 * 对齐——observe_d/notify_d 的 health_check 偶发慢响应（实测 max~4.5s，
 * 事件驱动下偶被长任务排队），2000ms 会误报 offline。 */
static int cli_daemon_online(const char *ns)
{
    char *result = NULL;
    int rc = daemon_rpc_call(cli_ns_sock(ns), "health_check", NULL, &result, 6000);
    AIRY_FREE(result);
    return (rc == 0) ? 1 : 0;
}

/* 启动一个 daemon（detach 守护化，日志落盘）；返回 0=成功，非零=失败。 */
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
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        __builtin_memset(&si, 0, sizeof(si));
        __builtin_memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        /* 日志重定向：先打开追加句柄，作为子进程 stdout/stderr。 */
        SECURITY_ATTRIBUTES sa;
        __builtin_memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE log_h = CreateFileA(logf, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (log_h != INVALID_HANDLE_VALUE) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = log_h;
            si.hStdError = log_h;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }
        char cmd[560];
        snprintf(cmd, sizeof(cmd), "\"%s\"", bin);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, DETACHED_PROCESS | CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi)) {
            if (log_h != INVALID_HANDLE_VALUE)
                CloseHandle(log_h);
            return -1;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (log_h != INVALID_HANDLE_VALUE)
            CloseHandle(log_h);
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0)
            return -1;
        if (pid == 0) {
            /* 子进程：脱离会话、重定向日志、exec daemon。 */
            setsid();
            int fd = open(logf, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execl(bin, bin, (char *)NULL);
            _exit(127);
        }
        /* 父进程：等一个短间隔，让健康检查能看到新进程。 */
    }
#endif
    return 0;
}

/* 等待 daemon 离线（轮询 health_check），超时返回 0。 */
static int cli_daemon_wait_offline(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (cli_daemon_online(ns)) {
        airy_sleep_ms(200);
        waited += 200;
        if (waited >= timeout_ms)
            return 0;
    }
    return 1;
}

/* 等待 daemon 上线，超时返回 0。 */
static int cli_daemon_wait_online(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (!cli_daemon_online(ns)) {
        airy_sleep_ms(200);
        waited += 200;
        if (waited >= timeout_ms)
            return 0;
    }
    return 1;
}

/* 停止一个 daemon：优先优雅 shutdown RPC，等待离线。 */
static int cli_daemon_stop(const char *ns)
{
    if (!cli_daemon_online(ns)) {
        cli_render_sub_agent_line(CLI_ROLE_TRACE, ns, "already offline");
        return 0;
    }
    char *result = NULL;
    daemon_rpc_call(cli_ns_sock(ns), "shutdown", "{}", &result, 3000);
    AIRY_FREE(result);
    if (!cli_daemon_wait_offline(ns, 5000))
        return -1;
    return 0;
}

/* ==================== 生命周期层 reconcile：agent 自愈重启（阶段 2） ====================
 *
 * 平台本质 = 声明式自愈（reconcile），三层的第三层（生命周期/资源层）：
 *   - 认知层 reconcile：蓝图 desired → GRAD 收敛 → 执行 → 复核 DRIFT → 回灌（roadmap_sched）
 *   - 执行层 reconcile：任务失败自动重调度（work_hall redispatch，P23）
 *   - 生命周期层 reconcile（本模块）：期望在线（desired）的 daemon 实际离线
 *     （current）→ 自动拉起（restart），带重启上限 + 退避，防重启风暴。
 *
 * desired 集合：AIRY_SELF_HEAL_AGENTS（逗号分隔 ns；缺省全部 16 daemon）。
 * 启用：AIRY_SELF_HEAL=1（或设置了 AGENTS 列表即视为启用）。
 * 限流：AIRY_SELF_HEAL_MAX_RESTARTS（每 daemon 最大重启数，缺省 3）、
 *       AIRY_SELF_HEAL_BACKOFF_MS（同 daemon 两次重启最小间隔，缺省 10000）、
 *       AIRY_SELF_HEAL_POLL_MS（全量探测节拍，缺省 5000）。
 * 驱动：main.c 主循环每轮调用 cli_daemon_lifecycle_reconcile_once()（与
 *       work_hall redispatch_once 并列），失败自愈与任务重调度共享同一控制器节奏。 */

#define CLI_SELF_HEAL_MAX_AGENTS CLI_DAEMONS_COUNT

typedef struct {
    char ns[32];
    int restarts;
    uint64_t last_restart_ms;
} cli_selfheal_agent_t;

typedef struct {
    int enabled;
    int max_restarts;
    uint64_t backoff_ms;
    uint64_t poll_interval_ms;
    uint64_t last_scan_ms;
    size_t agent_count;
    cli_selfheal_agent_t agents[CLI_SELF_HEAL_MAX_AGENTS];
} cli_selfheal_t;

static cli_selfheal_t g_selfheal;

void cli_daemon_lifecycle_init(const char *agents_csv)
{
    if (g_selfheal.enabled)
        return;
    g_selfheal.max_restarts = 3;
    g_selfheal.backoff_ms = 10000;
    g_selfheal.poll_interval_ms = 5000;

    const char *e_max = getenv("AIRY_SELF_HEAL_MAX_RESTARTS");
    if (e_max && e_max[0]) {
        long v = strtol(e_max, NULL, 10);
        if (v >= 0)
            g_selfheal.max_restarts = (int)v;
    }
    const char *e_bk = getenv("AIRY_SELF_HEAL_BACKOFF_MS");
    if (e_bk && e_bk[0]) {
        long long v = strtoll(e_bk, NULL, 10);
        if (v >= 0)
            g_selfheal.backoff_ms = (uint64_t)v;
    }
    const char *e_poll = getenv("AIRY_SELF_HEAL_POLL_MS");
    if (e_poll && e_poll[0]) {
        long long v = strtoll(e_poll, NULL, 10);
        if (v >= 100)
            g_selfheal.poll_interval_ms = (uint64_t)v;
    }

    /* desired 集合：显式列表（逗号分隔）或缺省全部 daemon */
    if (agents_csv && agents_csv[0]) {
        char csv[512];
        AIRY_STRNCPY_TERM(csv, agents_csv, sizeof(csv));
        char *tok = csv;
        while (tok && *tok && g_selfheal.agent_count < CLI_SELF_HEAL_MAX_AGENTS) {
            char *comma = strchr(tok, ',');
            if (comma)
                *comma = '\0';
            /* 去掉首尾空白 */
            char *p = tok;
            while (*p == ' ' || *p == '\t')
                p++;
            size_t n = strlen(p);
            while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
                p[--n] = '\0';
            if (p[0]) {
                /* 校验 ns 属于已知 daemon 表 */
                int known = 0;
                for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
                    if (strcmp(CLI_DAEMONS[i].ns, p) == 0) {
                        known = 1;
                        break;
                    }
                }
                if (known) {
                    /* AIRY_STRNCPY_TERM 对 dst 二次求值：先取 index 再写，
                     * 避免 agent_count 双递增产生空 ns 条目 */
                    size_t idx = g_selfheal.agent_count++;
                    AIRY_STRNCPY_TERM(g_selfheal.agents[idx].ns, p,
                                      sizeof(g_selfheal.agents[0].ns));
                }
            }
            tok = comma ? comma + 1 : NULL;
        }
    } else {
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
            size_t idx = g_selfheal.agent_count++;
            AIRY_STRNCPY_TERM(g_selfheal.agents[idx].ns, CLI_DAEMONS[i].ns,
                              sizeof(g_selfheal.agents[0].ns));
        }
    }

    if (g_selfheal.agent_count == 0) {
        AIRY_LOG_INFO("cli_lifecycle: no valid agents, self-heal disabled");
        return;
    }
    g_selfheal.enabled = 1;
    AIRY_LOG_INFO("cli_lifecycle: agent self-heal enabled (agents=%zu, max_restarts=%d, "
                  "backoff_ms=%llu, poll_ms=%llu)",
                  g_selfheal.agent_count, g_selfheal.max_restarts,
                  (unsigned long long)g_selfheal.backoff_ms,
                  (unsigned long long)g_selfheal.poll_interval_ms);
}

/* 声明式自愈一轮：探测 desired 集合，离线且未超限/退避 → 自动重启。
 * 返回本次重启的 daemon 数（0 = 无动作/未启用/节流中）。 */
int cli_daemon_lifecycle_reconcile_once(void)
{
    if (!g_selfheal.enabled || g_selfheal.agent_count == 0)
        return 0;

    uint64_t now = cli_now_ms();
    if (g_selfheal.last_scan_ms != 0 &&
        now - g_selfheal.last_scan_ms < g_selfheal.poll_interval_ms)
        return 0; /* 探测节流 */
    g_selfheal.last_scan_ms = now;

    int restarted = 0;
    for (size_t i = 0; i < g_selfheal.agent_count; i++) {
        cli_selfheal_agent_t *a = &g_selfheal.agents[i];
        if (cli_daemon_online(a->ns))
            continue; /* current == desired：健康 */

        /* 离线 → 尝试收敛到 desired：限流 + 退避 + 二进制存在性 */
        if (g_selfheal.max_restarts >= 0 && a->restarts >= g_selfheal.max_restarts)
            continue;
        if (g_selfheal.backoff_ms > 0 && now - a->last_restart_ms < g_selfheal.backoff_ms)
            continue;
        char bin[512];
        if (!cli_daemon_bin(a->ns, bin, sizeof(bin))) {
            /* 二进制缺失：无法自愈（不计数、不打日志刷屏） */
            continue;
        }
        if (cli_daemon_start(a->ns) != 0) {
            cli_render_sub_agent_line(CLI_ROLE_ERROR, a->ns,
                                      "self-heal restart failed");
            continue;
        }
        a->restarts++;
        a->last_restart_ms = now;
        restarted++;
        char line[128];
        snprintf(line, sizeof(line), "offline → auto-restarted (%d/%d)",
                 a->restarts, g_selfheal.max_restarts);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, a->ns, line);
    }
    return restarted;
}

int cmd_daemon(const char *arg, void *ctx)
{
    (void)ctx;
    char action[16] = "status";
    const char *ns_list[CLI_DAEMONS_COUNT];
    size_t ns_count = 0;

    /* 解析 "start|stop|restart|status [ns...]"。 */
    const char *p = arg;
    while (p && *p == ' ')
        p++;
    if (p && *p) {
        size_t alen = 0;
        while (p[alen] && p[alen] != ' ')
            alen++;
        if (alen >= sizeof(action))
            alen = sizeof(action) - 1;
        __builtin_memcpy(action, p, alen);
        action[alen] = '\0';
        p += alen;
        while (p && *p == ' ')
            p++;
        /* 其余 token 为命名空间列表（容错 ns_d 后缀）。 */
        while (p && *p && ns_count < CLI_DAEMONS_COUNT) {
            const char *tok = p;
            size_t tlen = 0;
            while (tok[tlen] && tok[tlen] != ' ')
                tlen++;
            char tokbuf[64];
            size_t tc = tlen < sizeof(tokbuf) - 1 ? tlen : sizeof(tokbuf) - 1;
            __builtin_memcpy(tokbuf, tok, tc);
            tokbuf[tc] = '\0';
            char resolved[64];
            if (cli_ns_resolve(tokbuf, resolved, sizeof(resolved)) == 0)
                ns_list[ns_count++] = AIRY_STRDUP(resolved);
            else
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, tokbuf,
                                     "未知 daemon 命名空间，已忽略");
            p = tok + tlen;
            while (*p == ' ')
                p++;
        }
    }
    /* 未指定 ns：默认全部。 */
    if (ns_count == 0) {
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++)
            ns_list[i] = CLI_DAEMONS[i].ns;
        ns_count = CLI_DAEMONS_COUNT;
    }

    if (strcmp(action, "status") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            int up = cli_daemon_online(ns_list[i]);
            if (g_cli_print_mode)
                cli_outf("%s %s\n", ns_list[i], up ? "online" : "offline");
            else
                cli_render_task_line(NULL, ns_list[i], up ? "online" : "offline", up ? 1.0 : 0.0);
        }
    } else if (strcmp(action, "start") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            if (cli_daemon_start(ns_list[i]) == 0) {
                const char *st = cli_daemon_wait_online(ns_list[i], 8000) ? "started" : "starting";
                if (g_cli_print_mode)
                    cli_outf("%s %s\n", ns_list[i], st);
                else
                    cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], st);
            }
        }
    } else if (strcmp(action, "stop") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            if (cli_daemon_stop(ns_list[i]) == 0) {
                if (g_cli_print_mode)
                    cli_outf("%s stopped\n", ns_list[i]);
                else
                    cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], "stopped");
            } else {
                if (g_cli_print_mode)
                    cli_outf("%s stop failed (timeout)\n", ns_list[i]);
                else
                    cli_render_sub_agent_line(CLI_ROLE_ERROR, ns_list[i], "stop failed (timeout)");
            }
        }
    } else if (strcmp(action, "restart") == 0) {
        for (size_t i = 0; i < ns_count; i++) {
            cli_daemon_stop(ns_list[i]);
            if (cli_daemon_start(ns_list[i]) == 0) {
                const char *st = cli_daemon_wait_online(ns_list[i], 8000) ? "restarted"
                                                                          : "restarting";
                if (g_cli_print_mode)
                    cli_outf("%s %s\n", ns_list[i], st);
                else
                    cli_render_sub_agent_line(CLI_ROLE_TRACE, ns_list[i], st);
            }
        }
    } else {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "usage",
                             "/daemon start|stop|restart|status [ns...]（默认全部）");
    }

    /* 释放用户输入解析出的 strdup 命名空间名（静态表条目不释放）。 */
    for (size_t i = 0; i < ns_count; i++) {
        int is_static = 0;
        for (size_t j = 0; j < CLI_DAEMONS_COUNT; j++) {
            if (ns_list[i] == CLI_DAEMONS[j].ns) {
                is_static = 1;
                break;
            }
        }
        if (!is_static)
            AIRY_FREE((void *)ns_list[i]);
    }
    return 0;
}

int cmd_stats(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && arg[0])
        cli_rpc_print(arg, "get_stats", NULL);
    else {
        for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++)
            cli_rpc_print(CLI_DAEMONS[i].ns, "get_stats", NULL);
    }
    return 0;
}

int cmd_agents(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("agent", arg && arg[0] ? arg : "list", NULL);
    return 0;
}

int cmd_tools(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("tool", "list_tools", NULL);
    return 0;
}

int cmd_hooks(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("hook", "list", NULL);
    return 0;
}

int cmd_plugins(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("plugin", "list", NULL);
    return 0;
}

int cmd_channels(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("channel", "list", NULL);
    return 0;
}

int cmd_market(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && strstr(arg, "skill"))
        cli_rpc_print("market", "search_skills", NULL);
    else
        cli_rpc_print("market", "search_agents", NULL);
    return 0;
}

int cmd_models(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("llm", "list_models", NULL);
    return 0;
}

/* ================================================================
 * 模型 API Key 配置（/apikey，q8f）
 *
 * /apikey list         → 列出 secrets.env 中全部 MODEL_*_API_KEY 位（打码）
 * /apikey set <N> <key> → 更新/追加 MODEL_N_API_KEY=<key>（N 为 model.yaml
 *                         连接表行号，与 api_key_env 自动映射一致）
 *
 * 直接读写 $AIRY_HOME/config/secrets.env（chmod 600），无需重启 daemon
 * （llm_d 每次请求实时读 api_key_env 对应变量）。
 * ================================================================ */

/* secrets.env 绝对路径：AIRY_CONFIG_DIR 优先，回退 AIRY_HOME/config */
static int cli_secrets_path(char *buf, size_t cap)
{
    const char *cdir = getenv("AIRY_CONFIG_DIR");
    if (cdir && cdir[0]) {
        snprintf(buf, cap, "%s/secrets.env", cdir);
        return 1;
    }
    const char *home = getenv("AIRY_HOME");
    if (home && home[0]) {
        snprintf(buf, cap, "%s/config/secrets.env", home);
        return 1;
    }
    return 0;
}

/* key 打码：sk-xxxx…wxyz（前 4 后 4，中间省略）；短 key 全打码 */
static void cli_secret_masked(const char *val, char *out, size_t cap)
{
    size_t len = val ? strlen(val) : 0;
    if (len == 0) {
        snprintf(out, cap, "(empty)");
        return;
    }
    if (len <= 8) {
        snprintf(out, cap, "****");
        return;
    }
    snprintf(out, cap, "%.*s…%s", 4, val, val + len - 4);
}

int cmd_apikey(const char *arg, void *ctx)
{
    (void)ctx;
    char path[AIRY_PATH_MAX];
    if (!cli_secrets_path(path, sizeof(path))) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                             "无法定位 secrets.env（未设置 AIRY_HOME/AIRY_CONFIG_DIR）");
        return 0;
    }

    if (!arg || arg[0] == '\0' || strncmp(arg, "list", 4) == 0) {
        /* ── list：逐行列出 MODEL_*_API_KEY（打码） ── */
        FILE *f = fopen(path, "r");
        if (!f) {
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                                 "无法读取 secrets.env（文件不存在或不可读）");
            return 0;
        }
        char line[1024];
        int found = 0;
        cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUB_AGENT, "apikey",
                             "已配置模型 Key 位（值打码，完整值见 secrets.env）:");
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (!eq)
                continue;
            *eq = '\0';
            char *val = eq + 1;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r'))
                val[--vlen] = '\0';
            if (strncmp(line, "MODEL_", 6) == 0 && strstr(line, "_API_KEY") != NULL) {
                char masked[64];
                cli_secret_masked(val, masked, sizeof(masked));
                cli_outf("  %s %s %s= %s%s\n", cli_c(CLR_GREEN), line, cli_c(CLR_RESET),
                         cli_c(CLR_DIM), masked);
                cli_outf("%s", cli_c(CLR_RESET));
                found = 1;
            }
        }
        fclose(f);
        if (!found)
            cli_outf("  %s（无 Key 位；使用 /apikey set <N> <key> 添加）%s\n",
                     cli_c(CLR_DIM), cli_c(CLR_RESET));
        return 0;
    }

    if (strncmp(arg, "set ", 4) != 0) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                             "/apikey list | set <N> <key>（N = model.yaml 连接表行号）");
        return 0;
    }

    /* ── set <N> <key> ── */
    const char *rest = arg + 4;
    char *sp = strchr(rest, ' ');
    if (!sp) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                             "/apikey set <N> <key>");
        return 0;
    }
    size_t nlen = (size_t)(sp - rest);
    if (nlen == 0 || nlen > 4) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                             "行号 N 非法（1-64）");
        return 0;
    }
    char num[8];
    __builtin_memcpy(num, rest, nlen);
    num[nlen] = '\0';
    for (size_t i = 0; i < nlen; i++) {
        if (num[i] < '0' || num[i] > '9') {
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                                 "行号 N 非法（须为数字）");
            return 0;
        }
    }
    int idx = atoi(num);
    if (idx < 1 || idx > 64) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                             "行号 N 超出范围（1-64）");
        return 0;
    }
    const char *key = sp + 1;
    if (!key[0]) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "key 为空");
        return 0;
    }

    /* 读入现有内容（保留注释与其它行） */
    FILE *f = fopen(path, "r");
    char **lines = NULL;
    size_t line_count = 0;
    if (f) {
        char lbuf[1024];
        while (fgets(lbuf, sizeof(lbuf), f)) {
            char **nl = (char **)AIRY_REALLOC(lines, (line_count + 1) * sizeof(char *));
            if (!nl) {
                for (size_t i = 0; i < line_count; i++)
                    AIRY_FREE(lines[i]);
                AIRY_FREE(lines);
                fclose(f);
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "OOM");
                return 0;
            }
            lines = nl;
            lines[line_count] = AIRY_STRDUP(lbuf);
            line_count++;
        }
        fclose(f);
    }

    char var_name[32];
    snprintf(var_name, sizeof(var_name), "MODEL_%d_API_KEY", idx);
    int replaced = 0;

    /* 写入：匹配现有行（忽略行首空白），否则追加到文件尾部 */
    const char *tmp_path = NULL;
    FILE *wf = NULL;
    {
        char tmp[AIRY_PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        tmp_path = tmp;
        wf = fopen(tmp, "w");
    }
    if (!wf) {
        for (size_t i = 0; i < line_count; i++)
            AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                             "无法写入 secrets.env");
        return 0;
    }

    for (size_t i = 0; i < line_count; i++) {
        const char *src = lines[i];
        const char *p = src;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, var_name, strlen(var_name)) == 0 && p[strlen(var_name)] == '=') {
            fprintf(wf, "%s=%s\n", var_name, key);
            replaced = 1;
        } else {
            fputs(src, wf);
        }
    }
    if (!replaced)
        fprintf(wf, "%s=%s\n", var_name, key);
    if (fclose(wf) != 0) {
        for (size_t i = 0; i < line_count; i++)
            AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        remove(tmp_path);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "写入失败");
        return 0;
    }

#if defined(_WIN32)
    _chmod(tmp_path, _S_IREAD | _S_IWRITE);
#else
    chmod(tmp_path, 0600);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        for (size_t i = 0; i < line_count; i++)
            AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "写回失败");
        return 0;
    }
    for (size_t i = 0; i < line_count; i++)
        AIRY_FREE(lines[i]);
    AIRY_FREE(lines);

    char ok[128];
    snprintf(ok, sizeof(ok), "%s 已%s（%s，权限 600）", var_name,
             replaced ? "更新" : "添加", path);
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "apikey", ok);
    return 0;
}

/* ================================================================
 * 记忆链展示（mem.*，2026-08-25）
 *
 * /mem          → 记忆链：count + 最近记录（mem.recent），逐条渲染
 * /mem <query>  → 检索记忆：mem.search 命中后逐条 mem.get 取内容
 * /mem get <id> → 单条记忆详情
 *
 * 全部经 gateway 派发（cli_gw_call），不直连 daemon socket。
 * ================================================================ */

/* Unix 时间戳 → "MM-DD HH:MM"（本地时区；线程安全 localtime_r） */
static void cli_mem_time(uint64_t ts, char *out, size_t cap)
{
    time_t t = (time_t)ts;
    struct tm ltm;
#if defined(_WIN32)
    if (localtime_s(&ltm, &t) == 0) {
        strftime(out, cap, "%m-%d %H:%M", &ltm);
    } else {
        snprintf(out, cap, "%llu", (unsigned long long)ts);
    }
#else
    if (localtime_r(&t, &ltm)) {
        strftime(out, cap, "%m-%d %H:%M", &ltm);
    } else {
        snprintf(out, cap, "%llu", (unsigned long long)ts);
    }
#endif
}

/* 取记录内容首行（user: 之后的部分），作为列表摘要 */
static void cli_mem_summary(const char *data, char *out, size_t cap)
{
    const char *p = data ? data : "";
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    /* 跳过 "user: " 前缀，摘要聚焦用户意图 */
    if (strncmp(p, "user: ", 6) == 0) {
        p += 6;
        len = (len > 6) ? len - 6 : 0;
    }
    if (len > 60)
        len = 60;
    while (len > 0 && ((unsigned char)p[len] & 0xC0) == 0x80)
        len--;
    if (len >= cap)
        len = cap - 1;
    __builtin_memcpy(out, p, len);
    out[len] = '\0';
}

/* 单条记忆记录渲染（含序号与时间） */
static void cli_mem_render_item(size_t idx, cJSON *item)
{
    cJSON *rid = cJSON_GetObjectItem(item, "record_id");
    cJSON *ts = cJSON_GetObjectItem(item, "created_at");
    cJSON *data = cJSON_GetObjectItem(item, "data");
    cJSON *lenj = cJSON_GetObjectItem(item, "len");

    const char *id = cJSON_IsString(rid) ? rid->valuestring : "?";
    uint64_t created = cJSON_IsNumber(ts) ? (uint64_t)ts->valuedouble : 0;
    const char *d = cJSON_IsString(data) ? data->valuestring : "";
    long dlen = cJSON_IsNumber(lenj) ? (long)lenj->valuedouble : (long)strlen(d);

    char timestr[32], sum[64];
    cli_mem_time(created, timestr, sizeof(timestr));
    cli_mem_summary(d, sum, sizeof(sum));

    cli_out(cli_c(CLR_DIM));
    cli_outf("  %-2zu%s", idx, CLI_ICON_BULLET);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf(" [%s] ", timestr);
    cli_out(cli_c(CLR_RESET));
    cli_out(sum);
    if (dlen > 60)
        cli_outf("…");
    cli_out(cli_c(CLR_DIM));
    cli_outf("  (%ldB %.*s)", dlen, 8, id);
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* /mem：记忆链（count + 最近记录） */
static void cli_mem_render_chain(void)
{
    char *res = NULL;
    int total = 0;
    if (cli_gw_call("mem.count", NULL, CLI_RPC_TIMEOUT_MS, &res) == 0 && res) {
        cJSON *r = cJSON_Parse(res);
        if (r) {
            cJSON *c = cJSON_GetObjectItem(r, "count");
            if (cJSON_IsNumber(c))
                total = (int)c->valuedouble;
            cJSON_Delete(r);
        }
        AIRY_FREE(res);
    } else {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆服务不可用（mem.count）");
        return;
    }

    char hdr[160];
    snprintf(hdr, sizeof(hdr), "记忆链 · 共 %d 条（最近 %d 条）", total, 6);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);

    if (cli_gw_call("mem.recent", "{\"limit\":6}", CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆链读取失败（mem.recent）");
        return;
    }
    cJSON *root = cJSON_Parse(res);
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆链响应解析失败");
        return;
    }
    cJSON *records = cJSON_GetObjectItem(root, "records");
    size_t n = cJSON_IsArray(records) ? (size_t)cJSON_GetArraySize(records) : 0;
    if (n == 0) {
        cli_outf("  %s 暂无记忆记录，对话一次即可生成记忆\n", CLI_ICON_INFO);
        cJSON_Delete(root);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(records, (int)i);
        cli_mem_render_item(i + 1, item);
    }
    cJSON_Delete(root);
}

/* /mem <query>：检索记忆并逐条取内容 */
static void cli_mem_render_search(const char *query)
{
    char params[1024];
    snprintf(params, sizeof(params), "{\"query\":\"%s\",\"limit\":5}", query);

    char *res = NULL;
    if (cli_gw_call("mem.search", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆检索失败（mem.search）");
        return;
    }
    cJSON *root = cJSON_Parse(res);
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆检索响应解析失败");
        return;
    }
    cJSON *results = cJSON_GetObjectItem(root, "results");
    size_t n = cJSON_IsArray(results) ? (size_t)cJSON_GetArraySize(results) : 0;
    char hdr[192];
    snprintf(hdr, sizeof(hdr), "检索记忆 “%s” · %zu 条命中", query, n);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);

    for (size_t i = 0; i < n; i++) {
        cJSON *hit = cJSON_GetArrayItem(results, (int)i);
        cJSON *rid = cJSON_GetObjectItem(hit, "record_id");
        cJSON *score = cJSON_GetObjectItem(hit, "score");
        const char *id = cJSON_IsString(rid) ? rid->valuestring : "";

        /* 逐条取内容（命中记录在服务端可被删除，取失败则仅展示 id）。
         * 注意：data 指针指向 cJSON 内部内存，必须先拷贝到本地缓冲再
         * cJSON_Delete（否则后续摘要读取为 use-after-free）。 */
        char gp[256];
        snprintf(gp, sizeof(gp), "{\"record_id\":\"%s\"}", id);
        char *g = NULL;
        cJSON *detail = NULL;
        if (cli_gw_call("mem.get", gp, CLI_RPC_TIMEOUT_MS, &g) == 0 && g) {
            detail = cJSON_Parse(g);
            AIRY_FREE(g);
        } else {
            AIRY_FREE(g);
        }

        char dcopy[8192];
        dcopy[0] = '\0';
        long dlen = 0;
        if (detail) {
            cJSON *dj = cJSON_GetObjectItem(detail, "data");
            cJSON *lj = cJSON_GetObjectItem(detail, "length");
            if (cJSON_IsString(dj) && dj->valuestring) {
                size_t dl = strlen(dj->valuestring);
                if (dl >= sizeof(dcopy))
                    dl = sizeof(dcopy) - 1;
                __builtin_memcpy(dcopy, dj->valuestring, dl);
                dcopy[dl] = '\0';
                dlen = cJSON_IsNumber(lj) ? (long)lj->valuedouble : (long)dl;
            }
            cJSON_Delete(detail);
        }

        char sum[64];
        cli_mem_summary(dcopy, sum, sizeof(sum));
        cli_out(cli_c(CLR_DIM));
        cli_outf("  %-2zu%s", i + 1, CLI_ICON_BULLET);
        cli_out(cli_c(CLR_RESET));
        cli_outf(" %.3f  ", cJSON_IsNumber(score) ? score->valuedouble : 0.0);
        cli_out(sum);
        if (dlen > 60)
            cli_outf("…");
        cli_out(cli_c(CLR_DIM));
        cli_outf("  (%.*s)", 8, id);
        cli_out(cli_c(CLR_RESET));
        cli_outc('\n');
    }
    cJSON_Delete(root);
}

/* /mem get <id>：单条记忆详情（多行完整内容） */
static void cli_mem_render_get(const char *record_id)
{
    char params[512];
    snprintf(params, sizeof(params), "{\"record_id\":\"%s\"}", record_id);

    char *res = NULL;
    if (cli_gw_call("mem.get", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆读取失败（mem.get）");
        return;
    }
    cJSON *root = cJSON_Parse(res);
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆响应解析失败");
        return;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *lenj = cJSON_GetObjectItem(root, "length");
    cJSON *meta = cJSON_GetObjectItem(root, "metadata");
    char hdr[160];
    snprintf(hdr, sizeof(hdr), "记忆 %s · %ldB", record_id,
             cJSON_IsNumber(lenj) ? (long)lenj->valuedouble : 0L);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);
    if (cJSON_IsString(meta)) {
        cli_out(cli_c(CLR_DIM));
        cli_outf("  元数据: %s\n", meta->valuestring);
        cli_out(cli_c(CLR_RESET));
    }
    if (cJSON_IsString(data) && data->valuestring) {
        cli_out(cli_c(CLR_RESET));
        cli_outf("%s\n", data->valuestring);
    } else {
        cli_outf("  %s 记录为空或已删除\n", CLI_ICON_INFO);
    }
    cJSON_Delete(root);
}

int cmd_mem(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && strncmp(arg, "get ", 4) == 0) {
        cli_mem_render_get(arg + 4);
        return 0;
    }
    if (arg && arg[0] != '\0') {
        cli_mem_render_search(arg);
        return 0;
    }
    cli_mem_render_chain();
    return 0;
}

int cmd_a2a(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("a2a", "discover_agents", NULL);
    return 0;
}

int cmd_metrics(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("observe", "query_metrics", NULL);
    return 0;
}

int cmd_alerts(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("monit", "get_alerts", NULL);
    return 0;
}

int cmd_tasks(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("sched", "get_stats", NULL);
    cli_rpc_print("sched", "checkpoint_save", NULL);
    return 0;
}

int cmd_info(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("info", "system", NULL);
    return 0;
}

int cmd_notify(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/notify <channel> <message>");
        return 0;
    }
    const char *space = strchr(arg, ' ');
    if (!space) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/notify <channel> <message>");
        return 0;
    }
    char channel[128];
    size_t clen = (size_t)(space - arg);
    if (clen >= sizeof(channel))
        clen = sizeof(channel) - 1;
    __builtin_memcpy(channel, arg, clen);
    channel[clen] = '\0';
    const char *msg = space + 1;
    char params[2048];
    snprintf(params, sizeof(params), "{\"channel\":\"%s\",\"message\":\"%s\"}", channel, msg);
    cli_rpc_print("notify", "publish", params);
    return 0;
}

int cmd_vault(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && strncmp(arg, "list", 4) == 0)
        cli_rpc_print("cupolas", "vault_list", "{\"type\":0}");
    else
        cli_rpc_print("cupolas", "vault_list", "{\"type\":0}");
    return 0;
}

int cmd_perm(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/perm <agent_id> <action> <resource>");
        cli_outf("    例: /perm agent-1 read fs:///tmp/x\n");
        return 0;
    }
    char agent[128];
    char action[64];
    const char *r1 = arg;
    const char *s1 = strchr(r1, ' ');
    if (!s1) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/perm <agent_id> <action> <resource>");
        return 0;
    }
    size_t a1 = (size_t)(s1 - r1);
    if (a1 >= sizeof(agent))
        a1 = sizeof(agent) - 1;
    __builtin_memcpy(agent, r1, a1);
    agent[a1] = '\0';
    const char *r2 = s1 + 1;
    const char *s2 = strchr(r2, ' ');
    if (!s2) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/perm <agent_id> <action> <resource>");
        return 0;
    }
    size_t a2 = (size_t)(s2 - r2);
    if (a2 >= sizeof(action))
        a2 = sizeof(action) - 1;
    __builtin_memcpy(action, r2, a2);
    action[a2] = '\0';
    const char *resource = s2 + 1;
    char params[1024];
    snprintf(params, sizeof(params), "{\"agent_id\":\"%s\",\"action\":\"%s\",\"resource\":\"%s\"}",
             agent, action, resource);
    cli_rpc_print("cupolas", "check_permission", params);
    return 0;
}

int cmd_sanitize(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/sanitize <input>");
        return 0;
    }
    char params[2048];
    snprintf(params, sizeof(params), "{\"input\":\"%s\"}", arg);
    cli_rpc_print("cupolas", "sanitize", params);
    return 0;
}

int cmd_security(const char *arg, void *ctx)
{
    (void)ctx;
    cli_rpc_print("cupolas", "net_get_stats", NULL);
    return 0;
}
