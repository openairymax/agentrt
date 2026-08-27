// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_pipeline.h
 * @brief Runtime context assembly/teardown and blueprint fastpath.
 *
 * cli_runtime_ctx_t aggregates every long-lived component the CLI main loop
 * needs (roadmap scheduler, work hall, hall store, governance, panels,
 * reviewer, language gateway).  cli_setup_runtime builds them in dependency
 * order; cli_teardown_runtime releases them in reverse.
 *
 * cli_blueprint_fastpath implements the three-tier blueprint routing
 * (L1 state-machine / L2 semantic-cache / L3 miss) that short-circuits
 * the full cognition pipeline when a repeated or similar task is detected.
 */

#ifndef AIRY_CLI_PIPELINE_H
#define AIRY_CLI_PIPELINE_H

#include "airy_rt.h"
#include "loop.h"
#include "platform.h"
#include "cognition.h"
#include "work_hall.h"
#include "hall_store.h"
#include "governance.h"
#include "roadmap_sched.h"
#include "lang_gateway.h"
#include "cli_tui.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime context: every long-lived component the CLI main loop needs.
 * cli_setup_runtime fills it; cli_teardown_runtime releases it.
 * main() owns the struct on its stack and passes it by pointer. */
typedef struct {
    airy_roadmap_sched_t *rsched;
    airy_work_hall_t *hall;
    airy_hall_store_t *hall_store;
    void *board_ud;
    void *events_ud;
    void *mem_ud;
    airy_artifact_validator_t *validator;
    airy_governance_t *governance;
    void *reviewer;
    const char *main_workspace_dir;
    airy_lang_gateway_t *lang_gateway;
} cli_runtime_ctx_t;

/* Core engine assembly: loop create + memory engine inject + cognition
 * wiring (GCCP callback, TC3 models, GRAD feedback).  Returns NULL on
 * failure.  out_cog may be NULL if the caller does not need the cognition
 * engine handle. */
airy_core_loop_t *cli_setup_core_engines(const char *m_s2, const char *m_verify,
                                          const char *m_expert,
                                          airy_cognition_engine_t **out_cog);

/* Full runtime assembly: rsched → validator → reviewer → hall_store →
 * governance → work_hall → chat adapter → TUI panels → lang_gateway.
 * Returns AIRY_EOK on success, error code on failure (caller must clean up). */
airy_err_t cli_setup_runtime(airy_core_loop_t *loop, cli_tui_t *tui,
                              cli_runtime_ctx_t *rt);

/* Symmetric teardown of cli_setup_runtime.  Idempotent: zeroes the struct
 * after release so repeated calls are safe. */
void cli_teardown_runtime(cli_runtime_ctx_t *rt);

/* Blueprint three-tier fastpath: L1 (zero-token state-machine hit),
 * L2 (low-token semantic-cache hit), L3 miss (semantic hint).
 * Returns 1 when the fastpath handled the input (caller should continue
 * the main loop), 0 when the full pipeline must run. */
int cli_blueprint_fastpath(airy_roadmap_sched_t *rsched, const char *input,
                            uint64_t turn_start);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_PIPELINE_H */
