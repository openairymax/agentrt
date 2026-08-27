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
#define CLI_DAEMONS_COUNT (sizeof(CLI_DAEMONS) / sizeof(CLI_DAEMONS[0]))

/* Resolve a daemon namespace (accepts "tool" and "tool_d" forms).
 * Returns 0 on match, non-zero when unknown. */
int cli_ns_resolve(const char *in, char *out, size_t out_cap);

/* Build the socket/endpoint path for a namespace. Returns a static buffer. */
const char *cli_ns_sock(const char *ns);

/* Generic daemon method call via gateway, render result as sub-agent line. */
void cli_rpc_print(const char *ns, const char *method, const char *params_json);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_CMD_INTERNALS_H */
