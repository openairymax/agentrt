// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_internal.h
 * @brief Internal shared header for airy_cli: macros, types, globals and prototypes.
 *
 * Shared compile-time constants (terminal colors, CLI_SEP, AIRY_CLI_VERSION,
 * CLI_HISTORY_MAX_MSGS), shared structs (cli_command_t / cli_cmd_ctx_t /
 * cli_dag_poll_rc_t), extern globals (g_cli_cancel / g_chat_adapter /
 * g_history_roles / g_history_contents / g_history_count / CLI_COMMANDS)
 * and cross-file function prototypes.
 */

#ifndef AIRY_CLI_INTERNAL_H
#define AIRY_CLI_INTERNAL_H

#include "airy_rt.h"
#include "loop.h"
#include "roadmap_sched.h"
#include "platform.h"
#include "cognition.h"
#include "gccp.h"
#include "work_hall.h"
#include "hall_store.h"
#include "governance.h"
#include "plan_to_dag.h"
#include "taskflow_advanced.h"
#include "llm_svc_adapter.h"
#include "logger.h"
#include "logging.h"
#include "airy_memory.h"
#include "string_compat.h"
#include "daemon_rpc_client.h"
#include "daemon_cmds.h"
#include "cli_render.h"
#include "cli_tui.h"

#include <signal.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_SEP "  ──────────────────────────────────────────────"

#define AIRY_CLI_VERSION "0.1.1"

/* Max chat history messages: 30 by default (~15 rounds); AIRY_CHAT_HISTORY_ROUNDS
  * overrides it in rounds (messages = rounds*2). Capped at 60, aligned with
  * the 64-message cap in build_llm_request_json. When full, the oldest round
  * (user+assistant) is dropped, keeping FIFO. */
#define CLI_HISTORY_MAX_MSGS 60

typedef struct {
    const char *name;
    const char *desc;
    int needs_args;
    int (*fn)(const char *arg, void *ctx);
} cli_command_t;

typedef struct {
    airy_work_hall_t *hall;
    int *quit;
} cli_cmd_ctx_t;

typedef enum { CLI_DAG_POLL_ACTIVE = 0, CLI_DAG_POLL_DONE, CLI_DAG_POLL_ERROR } cli_dag_poll_rc_t;

/* Global runtime state (defined in main.c) */
extern volatile sig_atomic_t g_cli_cancel;
extern llm_svc_adapter_t *g_chat_adapter;

/* Chat history buffer (defined in cli_chat.c) */
extern char *g_history_roles[CLI_HISTORY_MAX_MSGS];
extern char *g_history_contents[CLI_HISTORY_MAX_MSGS];
extern size_t g_history_count;

/* Command table (defined in main.c; enumerated by cmd_help in cli_cmds.c) */
extern const cli_command_t CLI_COMMANDS[];
size_t cli_commands_count(void);

char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data);
void cli_history_clear(void);
int cli_classify_input(const char *input);
void cli_chat_reply(const char *input);

airy_err_t cli_think_process_remote(const char *think_sock, const char *input, size_t input_len,
                                    airy_task_plan_t **out_plan);

airy_err_t cli_dag_submit_remote(const char *sched_sock, const taskflow_workflow_t *wf,
                                 const char *task_input, char **out_dag_id);
cli_dag_poll_rc_t cli_dag_poll_remote(const char *sched_sock, const char *dag_id,
                                      double *out_progress, char *out_state, size_t state_cap,
                                      char **out_result);
airy_err_t cli_dag_wait_remote(const char *sched_sock, const char *dag_id, char **out_result);

void cli_print_banner(void);
void cli_print_model_config(const char *t2, const char *t1f, const char *t1p);
void cli_print_system_header(const char *t2, const char *t1f, const char *t1p);
void cli_print_result(const char *result);
void cli_print_plan_list(const taskflow_workflow_t *wf);
void cli_progress_cb(const char *execution_id, const char *node_id, taskflow_state_t state,
                     double progress, void *user_data);
void cli_board_line(const char *tag, const char *id, const char *state, double progress);
int cmd_help(const char *arg, void *ctx);
int cmd_clear(const char *arg, void *ctx);
int cmd_status(const char *arg, void *ctx);
int cmd_quit(const char *arg, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_INTERNAL_H */
