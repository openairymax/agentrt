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
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLR_CYAN "\033[36m"
#define CLR_GREEN "\033[32m"
#define CLR_YELLOW "\033[33m"
#define CLR_RED "\033[31m"
#define CLR_RESET "\033[0m"

#define CLI_RPC_TIMEOUT_MS 10000

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

static const char *cli_rt_dir(void)
{
    static char buf[512];
    const char *rdir = getenv("AIRY_RUNTIME_DIR");
    if (rdir && rdir[0])
        return rdir;
    const char *home = getenv("AIRY_HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s/run", home);
        return buf;
    }
    const char *uhome = getenv("HOME");
    if (uhome && uhome[0]) {
        snprintf(buf, sizeof(buf), "%s/.airymaxrt/run", uhome);
        return buf;
    }
    return "/tmp/agentrt";
}

static const char *cli_ns_sock(const char *ns)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s.sock", cli_rt_dir(), ns);
    return buf;
}

static void cli_rpc_print(const char *ns, const char *method, const char *params_json)
{
    char *result = NULL;
    int rc = daemon_rpc_call(cli_ns_sock(ns), method, params_json, &result, CLI_RPC_TIMEOUT_MS);
    if (rc != 0 || !result) {
        printf("  %s[%s.%s]%s 调用失败（err=%d）\n", CLR_RED, ns, method, CLR_RESET, rc);
        AIRY_FREE(result);
        return;
    }
    printf("  %s[%s.%s]%s %s\n", CLR_GREEN, ns, method, CLR_RESET, result);
    AIRY_FREE(result);
}

int cmd_rpc(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        printf("  %s用法%s: /rpc <ns>.<method> [json参数]\n", CLR_YELLOW, CLR_RESET);
        printf("    例: /rpc mem.search {\"query\":\"hello\"}\n");
        printf("        /rpc cupolas.check_permission {\"agent_id\":\"a\",\"action\":\"read\","
               "\"resource\":\"fs:///tmp/x\"}\n");
        return 0;
    }

    const char *dot = strchr(arg, '.');
    if (!dot) {
        printf("  %s格式错误%s: 需要 <ns>.<method> 形式\n", CLR_RED, CLR_RESET);
        return 0;
    }
    char ns[64];
    size_t ns_len = (size_t)(dot - arg);
    if (ns_len >= sizeof(ns))
        ns_len = sizeof(ns) - 1;
    __builtin_memcpy(ns, arg, ns_len);
    ns[ns_len] = '\0';

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

    cli_rpc_print(ns, method, params);
    return 0;
}

int cmd_daemons(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    int online = 0;
    printf("  %s[daemon 巡检]%s\n", CLR_GREEN, CLR_RESET);
    for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) {
        char *result = NULL;
        int rc = daemon_rpc_call(cli_ns_sock(CLI_DAEMONS[i].ns), CLI_DAEMONS[i].health_method, NULL,
                                 &result, 6000);
        if (rc == 0 && result) {
            printf("    %s%-10s%s 在线  %s%.60s%s\n", CLR_CYAN, CLI_DAEMONS[i].ns, CLR_RESET,
                   CLR_GREEN, result, CLR_RESET);
            online++;
        } else {
            printf("    %s%-10s%s 离线（err=%d）\n", CLR_CYAN, CLI_DAEMONS[i].ns, CLR_RESET, rc);
        }
        AIRY_FREE(result);
    }
    printf("  %s[daemon]%s 在线 %d/%zu\n", CLR_GREEN, CLR_RESET, online, CLI_DAEMONS_COUNT);
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

int cmd_mem(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_rpc_print("mem", "count", NULL);
        return 0;
    }
    char params[1024];
    snprintf(params, sizeof(params), "{\"query\":\"%s\"}", arg);
    cli_rpc_print("mem", "search", params);
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
        printf("  %s用法%s: /notify <channel> <message>\n", CLR_YELLOW, CLR_RESET);
        return 0;
    }
    const char *space = strchr(arg, ' ');
    if (!space) {
        printf("  %s用法%s: /notify <channel> <message>\n", CLR_YELLOW, CLR_RESET);
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
        printf("  %s用法%s: /perm <agent_id> <action> <resource>\n", CLR_YELLOW, CLR_RESET);
        printf("    例: /perm agent-1 read fs:///tmp/x\n");
        return 0;
    }
    char agent[128];
    char action[64];
    const char *r1 = arg;
    const char *s1 = strchr(r1, ' ');
    if (!s1) {
        printf("  %s用法%s: /perm <agent_id> <action> <resource>\n", CLR_YELLOW, CLR_RESET);
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
        printf("  %s用法%s: /perm <agent_id> <action> <resource>\n", CLR_YELLOW, CLR_RESET);
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
        printf("  %s用法%s: /sanitize <input>\n", CLR_YELLOW, CLR_RESET);
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
