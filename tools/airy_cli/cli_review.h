/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_review.h
 * @brief Cognition-stage parallel sub-agent review (item 4, 2026-08-16).
 *
 * After the cognition pipeline produces a task plan, two reviewer sub-agents
 * (agent_d spawn+invoke, run on parallel threads) independently audit the
 * plan — fact/coverage check and risk/boundary check — and the verdicts are
 * merged into a single review report. The caller records it into the hall
 * event flow (preflight verify) so the cognition decision is fully visible
 * on the decision chain, without blocking execution.
 *
 * Degradation contract: agent_d unreachable, AIRY_COGNITION_REVIEW=0, or an
 * empty plan yields a silent skip (return 0) — the main task pipeline is
 * never blocked by the review stage.
 */

#ifndef AIRY_RT_CLI_REVIEW_H
#define AIRY_RT_CLI_REVIEW_H

#include "cognition.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the parallel cognition review (fact + risk sub-agents).
 * @param agent_sock agent_d socket path (Unix socket / Windows pipe name)
 * @param task       original user task text (BORROW)
 * @param plan       generated task plan (BORROW)
 * @param out_report merged review report JSON (OWNER, caller AIRY_FREE),
 *                   or NULL to discard
 * @return 1 = review completed (report produced), 0 = skipped/degraded
 */
int cli_cognition_review(const char *agent_sock, const char *task, const airy_task_plan_t *plan,
                         char **out_report);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_REVIEW_H */
