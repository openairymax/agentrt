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
#include "work_hall.h"
#include "plan_to_dag.h"
#include "taskflow_advanced.h"

#include <signal.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Background wait context: the wait runs on a worker thread so the main
 * thread can poll stdin for user interruption / interjected chat. */
typedef struct {
    airy_work_hall_t *hall;
    const char *sched_sock;
    const char *exec_id;
    int sched_remote;
    airy_err_t err;
    char *result;
    volatile int done;
} cli_task_wait_ctx_t;

/* Thread worker: drives the engine to completion (local hall or remote
 * sched_d).  Set ctx->done on return. */
void *cli_task_wait_worker(void *arg);

/* Poll stdin during task execution.  Returns:
 *   0  = no input
 *  -1  = interrupt requested (g_cli_cancel set)
 *  +1  = interjected chat rendered, task continues */
int cli_task_poll_input(void);

/* Record a task submission event in the decision-chain hall store. */
void cli_chain_record_submit(const char *exec_id, const airy_task_plan_t *plan,
                              const taskflow_workflow_t *wf);

/* Render the task result (JSON parse for real success/failure, metrics,
 * validation gate annotation).  Returns 1 when the task truly succeeded
 * (caller uses this for L2 cache absorb decision). */
int cli_task_result_render(const char *result, airy_err_t err, const char *exec_id,
                            int canceled, airy_work_hall_t *hall, uint32_t vf_before);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_EXEC_H */
