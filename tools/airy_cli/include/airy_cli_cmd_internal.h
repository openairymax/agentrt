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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cjson/cJSON.h>

#include "cli_timeout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 管理 RPC 缺省档（0.1.21 D-8）：实际毫秒数经超时阶梯解析（--timeout >
 * env > model.yaml > 缺省），不再各自写死 10s。 */
#define CLI_RPC_TIMEOUT_MS cli_tmo_ms(CLI_TMO_RPC)

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

/* 熔断读数视图（cli_brk.c，0.1.21 §33.2 #5）：llm.health_check 发布的
 * breakers[] 与 circuit_breaks 是唯一来源，此处只读取出，不设阈值。 */
typedef struct {
    int open;      /* state != closed 的 breaker 数 */
    int total;     /* breaker 总数 */
    double breaks; /* 累计跳闸（rejects + budget_tmo） */
} cli_brk_t;

/* 结果体含熔断字段返回 1 并填 out；否则返回 0（旧版 daemon 静默跳过）。 */
int cli_brk_read(const char *json, cli_brk_t *out);

/* /daemon(s) 的附加行：同一判据的渲染点。 */
void cli_brk_show(const char *ns, const char *json);

/* 本地模型注册表子命令（cli_model_reg.c，0.1.21 D-6）：
 * list / use / remove；返回 0 成功、2 失败（与 cli_cmd_* 约定一致）。 */
int cli_model_reg(int argc, char *argv[]);

/* 密钥装配引导写盘件（cli_key.c，0.1.21 D-2）：选行 → 填 key → 写
 * secrets.env → llm.probe 立即验证；命令入口 cli_cmd_key 见 cli_internal.h。
 * 中途取消不落盘。 */

/* secrets.env 变量位原子写入（tmp + fsync + rename，权限 600）。
 * 与 /apikey set 同源；replaced 非空时回传是否覆盖既有键。 */
int airy_cli_secret_store(const char *path, const char *var, const char *key, int *replaced);

/* Render one sub-agent line: header "Sub <ns> Agent" is built at runtime
 * because the namespace is dynamic. is_err picks the -p-visible channel. */
void cli_sub_print(int is_err, const char *ns, const char *text);

/* Daemon process lifecycle (cli_daemon_lifecycle.c).  These go through
 * A-IPC on purpose: the supervisor must not depend on the supervised. */
int cli_daemon_bin(const char *ns, char *buf, size_t cap);
int cli_daemon_online(const char *ns);
int cli_daemon_start(const char *ns);
int cli_daemon_stop(const char *ns);
int cli_daemon_wait(const char *ns, int want_online, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_CMD_INTERNALS_H */
