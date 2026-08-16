/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_exec_review.h
 * @brief CLI execution-review pipeline wiring (improvement 6, P3).
 *
 * Creates an airy_execution_review_t for the CLI work hall with LLM-backed
 * semantic delegates: t2 (A) semantic drift review and t1-f (B) final
 * adjudication, both served through the chat LLM adapter (llm_d). The
 * review pipeline itself is deterministic-orchestrated (gate -> t2 -> t1-f)
 * with a degradation chain; when llm_d is unreachable the delegates
 * return -1 and the review falls back to the deterministic gate only.
 */

#ifndef AIRY_CLI_EXEC_REVIEW_H
#define AIRY_CLI_EXEC_REVIEW_H

#include "execution_review.h"

/**
 * @brief Create the CLI execution reviewer (OWNER).
 *
 * The reviewer embeds the LLM semantic delegates; destroy with
 * airy_execution_review_destroy().
 *
 * @return reviewer handle; NULL on OOM
 */
airy_execution_review_t *cli_exec_review_create(void);

#endif /* AIRY_CLI_EXEC_REVIEW_H */
