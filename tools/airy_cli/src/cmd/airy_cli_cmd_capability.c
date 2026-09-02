// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_capability.c
 * @brief Capability-face and security-face commands: market, info, notify,
 *        vault, perm, sanitize, security.
 *
 * Split from daemon_cmds.c.  Each command is a thin wrapper around
 * cli_rpc_print targeting the appropriate daemon namespace.
 */

#include "daemon_cmds.h"
#include "airy_cli_cmd_internal.h"
#include "cli_render.h"
#include "airy_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== capability face ==================== */

int cmd_market(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && strstr(arg, "skill"))
        cli_rpc_print("market", "search_skills", NULL);
    else
        cli_rpc_print("market", "search_agents", NULL);
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
                              "/notify <topic> <message>");
        return 0;
    }
    const char *space = strchr(arg, ' ');
    if (!space) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/notify <topic> <message>");
        return 0;
    }
    char topic[128];
    size_t tlen = (size_t)(space - arg);
    if (tlen >= sizeof(topic)) tlen = sizeof(topic) - 1;
    __builtin_memcpy(topic, arg, tlen);
    topic[tlen] = '\0';
    const char *msg = space + 1;
    char params[2048];
    snprintf(params, sizeof(params), "{\"topic\":\"%s\",\"message\":\"%s\"}", topic, msg);
    cli_rpc_print("notify", "publish", params);
    return 0;
}

/* ==================== security face ==================== */

int cmd_vault(const char *arg, void *ctx)
{
    (void)ctx;
    (void)arg;
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
    if (a1 >= sizeof(agent)) a1 = sizeof(agent) - 1;
    __builtin_memcpy(agent, r1, a1); agent[a1] = '\0';
    const char *r2 = s1 + 1;
    const char *s2 = strchr(r2, ' ');
    if (!s2) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                              "/perm <agent_id> <action> <resource>");
        return 0;
    }
    size_t a2 = (size_t)(s2 - r2);
    if (a2 >= sizeof(action)) a2 = sizeof(action) - 1;
    __builtin_memcpy(action, r2, a2); action[a2] = '\0';
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
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage", "/sanitize <input>");
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
    (void)arg;
    cli_rpc_print("cupolas", "net_get_stats", NULL);
    return 0;
}
