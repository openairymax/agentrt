// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_internal.h
 * @brief Internal shared header for daemon_cmds split files.
 *
 * Exposes the daemon table, namespace resolution, socket path resolution
 * and the generic RPC printer so that cmd_system / cmd_cognition /
 * cmd_capability can all call cli_rpc_print without duplicating code.
 */

#ifndef AIRY_CLI_CMD_INTERNALS_H
#define AIRY_CLI_CMD_INTERNALS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_RPC_TIMEOUT_MS 10000

typedef struct {
    const char *ns;
    const char *sock;
    const char *health_method;
} cli_daemon_desc_t;

extern const cli_daemon_desc_t CLI_DAEMONS[];

/* 表项数量。extern 不完整数组的 sizeof 在其他 TU 不可用（0.1.19 §458
 * 实证：原 CLI_DAEMONS_COUNT 宏只在定义方成立），故以函数暴露；
 * 定义方仍用私有 sizeof 宏，容量口径唯一。 */
size_t cli_daemons_count(void);

/* Resolve a daemon namespace (accepts "tool" and "tool_d" forms).
 * Returns 0 on match, non-zero when unknown. */
int cli_ns_resolve(const char *in, char *out, size_t out_cap);

/* Build the socket/endpoint path for a namespace. Returns a static buffer. */
const char *cli_ns_sock(const char *ns);

/* Generic daemon method call via gateway, render result as sub-agent line. */
void cli_rpc_print(const char *ns, const char *method, const char *params_json);

/* Render one sub-agent line: header "Sub <ns> Agent" is built at runtime
 * because the namespace is dynamic. is_err picks the -p-visible channel. */
void cli_sub_print(int is_err, const char *ns, const char *text);

/* Daemon process lifecycle (cli_daemon_lifecycle.c).  These go through
 * A-IPC on purpose: the supervisor must not depend on the supervised. */
int cli_daemon_bin(const char *ns, char *buf, size_t cap);
int cli_daemon_online(const char *ns);
int cli_daemon_start(const char *ns);
int cli_daemon_stop(const char *ns);
int cli_daemon_wait_online(const char *ns, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_CMD_INTERNALS_H */
