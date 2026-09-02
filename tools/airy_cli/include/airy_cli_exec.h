// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_exec.h
 * @brief Task execution helpers: background wait, stdin polling, result rendering.
 *
 * Extracted from main.c.  The main loop calls these during the task
 * execution phase (submit → poll → wait → result summary).
 */

#ifndef AIRY_CLI_EXEC_H
#define AIRY_CLI_EXEC_H

#include "airy_rt.h"
#include "airy_cli_pipeline.h" /* cli_runtime_ctx_t / airy_task_plan_t */

#include <signal.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Background wait context: the wait runs on a worker thread so the main
 * thread can poll stdin for user interruption / interjected chat. */
typedef struct {
    const char *exec_id;
    airy_err_t err;
    char *result;
    volatile int done;
} cli_task_wait_ctx_t;

/* Thread worker: drives the remote DAG (gateway → sched_d) to completion.
 * Set ctx->done on return. */
void *cli_task_wait_worker(void *arg);

/* Poll stdin during task execution.  Returns:
 *   0  = no input
 *  -1  = interrupt requested (g_cli_cancel set)
 *  +1  = interjected chat rendered, task continues */
int cli_task_poll_input(void);

/* Record a task submission event in the decision-chain hall store. */
void cli_chain_record_submit(const char *exec_id, const airy_task_plan_t *plan);

/* Render the task result (JSON parse for real success/failure, metrics).
 * Returns 1 when the task truly succeeded (caller uses this for L2 cache
 * absorb decision). */
int cli_task_result_render(const char *result, airy_err_t err, const char *exec_id,
                           int canceled);

/* Run one full task turn: cognition planning → submit (gateway → sched_d,
 * the only execution path) → board polling → wait → result summary.
 * Extracted from main.c's main
 * loop (2026-08-27 domain split).  Returns 1 when the caller should
 * continue the loop early (planning / submission failure), 0 on normal
 * completion. */
int cli_run_task_pipeline(cli_runtime_ctx_t *rt, airy_cognition_engine_t *cog,
                          const char *input, size_t input_len, uint64_t turn_start);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_EXEC_H */
