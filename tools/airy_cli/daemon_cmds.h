// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file daemon_cmds.h
 * @brief airy_cli daemon command layer.
 *
 * Centralizes JSON-RPC wiring between the CLI and the daemon namespaces:
 * generic /rpc passthrough plus wrappers for common namespaces. main.c
 * only registers the command table.
 */

#ifndef AIRY_CLI_DAEMON_CMDS_H
#define AIRY_CLI_DAEMON_CMDS_H

#ifdef __cplusplus
extern "C" {
#endif

int cmd_daemons(const char *arg, void *ctx);
int cmd_daemon(const char *arg, void *ctx);
int cmd_rpc(const char *arg, void *ctx);
int cmd_stats(const char *arg, void *ctx);
int cmd_agents(const char *arg, void *ctx);
int cmd_tools(const char *arg, void *ctx);
int cmd_hooks(const char *arg, void *ctx);
int cmd_plugins(const char *arg, void *ctx);
int cmd_channels(const char *arg, void *ctx);
int cmd_market(const char *arg, void *ctx);
int cmd_models(const char *arg, void *ctx);
int cmd_mem(const char *arg, void *ctx);
int cmd_a2a(const char *arg, void *ctx);
int cmd_metrics(const char *arg, void *ctx);
int cmd_alerts(const char *arg, void *ctx);
int cmd_tasks(const char *arg, void *ctx);
int cmd_info(const char *arg, void *ctx);
int cmd_notify(const char *arg, void *ctx);
int cmd_vault(const char *arg, void *ctx);
int cmd_perm(const char *arg, void *ctx);
int cmd_sanitize(const char *arg, void *ctx);
int cmd_security(const char *arg, void *ctx);

/* 阶段 2 生命周期层 reconcile：agent 自愈重启（声明式自愈第三层）。
 * init 在 main 启动时调用（desired 集合 + 限流配置）；reconcile_once 由
 * 主循环每轮驱动（与 work_hall redispatch_once 并列）。 */
void cli_daemon_lifecycle_init(const char *agents_csv);
int cli_daemon_lifecycle_reconcile_once(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_DAEMON_CMDS_H */
