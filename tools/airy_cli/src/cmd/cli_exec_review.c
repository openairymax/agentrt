// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_exec_review.c
 * @brief CLI execution-review semantic delegates (t2 / t1-f).
 *
 * Policy side of the improvement-6 review pipeline. The mechanism (gate ->
 * t2 -> t1-f orchestration + degradation chain) lives in execution_review.c
 * (coreloopthree, kept); the LLM judgment is served by think_d via
 * gateway -> think.review (M1-1c): CLI no longer embeds review prompts or
 * drives LLM calls directly. Every failure path returns -1 so the pipeline
 * degrades deterministically (gate-only / adopt t2).
 */

#include "cli_exec_review.h"

#include "cli_gw.h"
#include "cli_internal.h"
#include "cli_render.h"
#include "logging.h"

#include <cjson/cJSON.h>
#include <string.h>

#define CLI_EXEC_REVIEW_TIMEOUT_MS 30000
#define CLI_EXEC_REVIEW_REASON_MAX 128

/* t2 (A) semantic review: artifact vs blueprint node contract, judged by
 * think_d (think.review stage=t2). */
static int cli_exec_t2_review(const airy_review_input_t *in, const char *fact_base_json,
                              char *reason_out, size_t reason_sz, void *user_data)
{
    (void)fact_base_json;
    (void)user_data;
    if (!in)
        return -1;

#ifdef AIRY_HAS_CJSON
    cJSON *p = cJSON_CreateObject();
    if (!p)
        return -1;
    cJSON_AddStringToObject(p, "stage", "t2");
    if (in->node_goal)
        cJSON_AddStringToObject(p, "node_goal", in->node_goal);
    if (in->output_json)
        cJSON_AddStringToObject(p, "output_json", in->output_json);
    if (in->output_signatures && in->output_signature_count > 0) {
        cJSON *sa = cJSON_CreateArray();
        for (size_t i = 0; i < in->output_signature_count; i++) {
            if (in->output_signatures[i])
                cJSON_AddItemToArray(sa, cJSON_CreateString(in->output_signatures[i]));
        }
        cJSON_AddItemToObject(p, "output_signatures", sa);
    }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    if (!pj)
        return -1;

    char *res = NULL;
    int rc = -1;
    if (cli_gw_call("think.review", pj, CLI_EXEC_REVIEW_TIMEOUT_MS, &res) == 0 && res) {
        cJSON *r = cJSON_Parse(res);
        if (r) {
            cJSON *v = cJSON_GetObjectItem(r, "verdict");
            cJSON *rs = cJSON_GetObjectItem(r, "reason");
            if (cJSON_IsNumber(v))
                rc = (int)v->valuedouble;
            if (rc == 1 && reason_out && reason_sz > 0 && cJSON_IsString(rs) &&
                rs->valuestring && rs->valuestring[0])
                snprintf(reason_out, reason_sz, "%s", rs->valuestring);
            cJSON_Delete(r);
        }
        AIRY_FREE(res);
    } else {
        AIRY_FREE(res);
        cli_trace("review", "exec t2: think.review unavailable, degrade to gate-only");
    }
    AIRY_FREE(pj);
    return rc;
#else
    (void)reason_out;
    (void)reason_sz;
    return -1;
#endif
}

/* t1-f (B) final adjudication: combine gate result and t2 drift, judged by
 * think_d (think.review stage=t1f). */
static int cli_exec_t1f_adjudicate(const airy_review_input_t *in, int drift,
                                   const char *gate_reason, char *verdict_out,
                                   size_t verdict_sz, void *user_data)
{
    (void)user_data;
    if (!in)
        return -1;

#ifdef AIRY_HAS_CJSON
    cJSON *p = cJSON_CreateObject();
    if (!p)
        return -1;
    cJSON_AddStringToObject(p, "stage", "t1f");
    if (in->node_goal)
        cJSON_AddStringToObject(p, "node_goal", in->node_goal);
    if (in->output_json)
        cJSON_AddStringToObject(p, "output_json", in->output_json);
    cJSON_AddNumberToObject(p, "drift", drift);
    if (gate_reason && gate_reason[0])
        cJSON_AddStringToObject(p, "gate_reason", gate_reason);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    if (!pj)
        return -1;

    char *res = NULL;
    int rc = -1;
    if (cli_gw_call("think.review", pj, CLI_EXEC_REVIEW_TIMEOUT_MS, &res) == 0 && res) {
        cJSON *r = cJSON_Parse(res);
        if (r) {
            cJSON *v = cJSON_GetObjectItem(r, "verdict");
            cJSON *rs = cJSON_GetObjectItem(r, "reason");
            if (cJSON_IsNumber(v))
                rc = (int)v->valuedouble;
            /* 无论 accept/reject，都把判断理由带回（verify 事件信息完整） */
            if (rc >= 0 && verdict_out && verdict_sz > 0 && cJSON_IsString(rs) &&
                rs->valuestring && rs->valuestring[0])
                snprintf(verdict_out, verdict_sz, "%s", rs->valuestring);
            cJSON_Delete(r);
        }
        AIRY_FREE(res);
    } else {
        AIRY_FREE(res);
        cli_trace("review", "exec t1-f: think.review unavailable, adopt t2 verdict");
    }
    AIRY_FREE(pj);
    return rc;
#else
    (void)drift;
    (void)gate_reason;
    (void)verdict_out;
    (void)verdict_sz;
    return -1;
#endif
}

airy_execution_review_t *cli_exec_review_create(void)
{
    airy_execution_review_t *rv = airy_execution_review_create(NULL);
    if (!rv)
        return NULL;

    airy_review_semantic_ops_t ops;
    __builtin_memset(&ops, 0, sizeof(ops));
    ops.t2_review = cli_exec_t2_review;
    ops.t1f_adjudicate = cli_exec_t1f_adjudicate;
    ops.user_data = NULL;
    airy_execution_review_set_semantic_ops(rv, &ops);
    return rv;
}
