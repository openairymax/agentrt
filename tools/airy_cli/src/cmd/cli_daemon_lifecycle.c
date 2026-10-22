// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_daemon_lifecycle.c
 * @brief Daemon 进程生命周期与自愈协调：拉起、停止、在线探测、定期重扫。
 *
 * 0.1.19 §458 自 airy_cli_cmd_system.c 拆出。拆分边界取「只读视图」与
 * 「改变状态」：前者（/daemons、/daemon status）只查询，后者要派生进程、
 * 等待端口就绪，对平台 API 的依赖是 spawn + stat，与 TCP 探活毫无交集。
 * 拆开后 cmd_system 不再需要任何进程/套接字平台头。
 *
 * 本文件只经 A-IPC（daemon_rpc_client）与 daemon 对话，不经 gateway：
 * cli_gw.h 立法 G-1 显式例外——监管者不得依赖被监管者。
 */

#include "airy_cli_cmd_internal.h"
#include "daemon_rpc_client.h"
#include "airy_defaults.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "logger.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static const char *cli_rt_base(void) { return airy_home_dir(); }

int cli_daemon_bin(const char *ns, char *buf, size_t cap)
{
#ifdef _WIN32
    snprintf(buf, cap, "%s\\bin\\%s_d.exe", cli_rt_base(), ns);
    struct _stat st;
    return (_stat(buf, &st) == 0 && (st.st_mode & _S_IFREG)) ? 1 : 0;
#else
    snprintf(buf, cap, "%s/bin/%s_d", cli_rt_base(), ns);
    struct stat st;
    return (stat(buf, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
#endif
}

int cli_daemon_online(const char *ns)
{
    char *result = NULL;
    int rc = daemon_rpc_call(cli_ns_sock(ns), "health_check", NULL, &result, 6000);
    AIRY_FREE(result);
    return (rc == 0) ? 1 : 0;
}

int cli_daemon_start(const char *ns)
{
    char bin[512];
    if (!cli_daemon_bin(ns, bin, sizeof(bin))) {
        char hdr[80];
        snprintf(hdr, sizeof(hdr), "Sub Agent:%s", ns);
        cli_print_line(hdr, "二进制不存在（$AIRY_HOME/bin 下未找到），先安装/构建");
        return -1;
    }
    if (cli_daemon_online(ns)) {
        cli_sub_print(0, ns, "already online");
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
            if (log_h != INVALID_HANDLE_VALUE) CloseHandle(log_h);
            return -1;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (log_h != INVALID_HANDLE_VALUE) CloseHandle(log_h);
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
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
    }
#endif
    return 0;
}

static int cli_daemon_wait_offline(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (cli_daemon_online(ns)) {
        airy_sleep_ms(200);
        waited += 200;
        if (waited >= timeout_ms) return 0;
    }
    return 1;
}

int cli_daemon_wait_online(const char *ns, int timeout_ms)
{
    int waited = 0;
    while (!cli_daemon_online(ns)) {
        airy_sleep_ms(200);
        waited += 200;
        if (waited >= timeout_ms) return 0;
    }
    return 1;
}

int cli_daemon_stop(const char *ns)
{
    if (!cli_daemon_online(ns)) {
        cli_sub_print(0, ns, "already offline");
        return 0;
    }
    char *result = NULL;
    daemon_rpc_call(cli_ns_sock(ns), "shutdown", "{}", &result, 3000);
    AIRY_FREE(result);
    if (!cli_daemon_wait_offline(ns, 5000)) return -1;
    return 0;
}

/* ==================== self-heal reconcile ==================== */

/* 自愈表容量：须 ≥ 默认 daemon 表容量（cli_daemons_count），此处按上限
 * 预留，不以表项数作数组界（表扩容不应改动结构布局）。 */
#define CLI_SELF_HEAL_MAX_AGENTS 32

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

/* 环境变量取数：缺省或低于 floor 一律回落（负值无物理意义，poll 过密会
 * 把自愈扫描变成忙等）。overflow 时 strtoll 饱和，向下截断为 int 的口径
 * 与既有 strtol 实现一致。 */
static long long env_num(const char *name, long long fallback, long long floor)
{
    const char *v = getenv(name);
    if (!v || !v[0]) return fallback;
    long long n = strtoll(v, NULL, 10);
    return (n >= floor) ? n : fallback;
}

/* 原地裁去首尾空白；空串或长于 ns 字段容量返回 0。截断到 31 字节的
 * 命名空间不可能命中白名单，故拒绝超长与截断后比较等价。 */
static int ns_take(char *s, const char **out)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
    if (n == 0 || n >= sizeof(g_selfheal.agents[0].ns)) return 0;
    *out = s;
    return 1;
}

static int ns_known(const char *ns)
{
    size_t total = cli_daemons_count();
    for (size_t i = 0; i < total; i++)
        if (strcmp(CLI_DAEMONS[i].ns, ns) == 0) return 1;
    return 0;
}

static void agent_add(const char *ns)
{
    if (g_selfheal.agent_count >= CLI_SELF_HEAL_MAX_AGENTS) return;
    size_t idx = g_selfheal.agent_count++;
    AIRY_STRNCPY_TERM(g_selfheal.agents[idx].ns, ns, sizeof(g_selfheal.agents[0].ns));
}

static void agents_parse(const char *csv_in)
{
    char csv[512];
    AIRY_STRNCPY_TERM(csv, csv_in, sizeof(csv));
    char *tok = csv;
    size_t cap = cli_daemons_count();
    while (tok && *tok && g_selfheal.agent_count < cap) {
        char *comma = strchr(tok, ',');
        if (comma) *comma = '\0';
        const char *ns = NULL;
        if (ns_take(tok, &ns) && ns_known(ns)) agent_add(ns);
        tok = comma ? comma + 1 : NULL;
    }
}

static void agents_all(void)
{
    size_t total = cli_daemons_count();
    for (size_t i = 0; i < total; i++) agent_add(CLI_DAEMONS[i].ns);
}

void cli_daemon_lifecycle_init(const char *agents_csv)
{
    if (g_selfheal.enabled) return;
    g_selfheal.max_restarts = (int)env_num("AIRY_SELF_HEAL_MAX_RESTARTS", 3, 0);
    g_selfheal.backoff_ms = (uint64_t)env_num("AIRY_SELF_HEAL_BACKOFF_MS", 10000, 0);
    g_selfheal.poll_interval_ms = (uint64_t)env_num("AIRY_SELF_HEAL_POLL_MS", 5000, 100);

    if (agents_csv && agents_csv[0])
        agents_parse(agents_csv);
    else
        agents_all();
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
            cli_sub_print(1, a->ns, "self-heal restart failed");
            continue;
        }
        a->restarts++;
        a->last_restart_ms = now;
        restarted++;
        char line[128];
        snprintf(line, sizeof(line), "offline → auto-restarted (%d/%d)", a->restarts,
                 g_selfheal.max_restarts);
        cli_sub_print(1, a->ns, line);
    }
    return restarted;
}
