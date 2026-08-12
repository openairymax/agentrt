// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_think.c
 * @brief airy_cli dual-thinking domain: remote plan parsing via think_d.
 *
 * When AIRY_THINK_SOCK is set, cognition planning goes through daemon RPC
 * (process method on think.sock, 120s timeout); the response is parsed
 * twice and the plan segment is restored to airy_task_plan_t. On failure
 * no data is fabricated; the caller falls back to the embedded engine.
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* think.process plan segment -> airy_task_plan_t (nodes/goal/depends filled,
  * then the usual plan -> workflow -> hall chain) */
static int cli_think_plan_from_json(cJSON *plan_json, airy_task_plan_t **out_plan)
{
    if (!plan_json || !out_plan)
        return AIRY_ERR_INVALID_PARAM;
    *out_plan = NULL;

    cJSON *nodes = cJSON_GetObjectItem(plan_json, "nodes");
    int node_n = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    if (node_n <= 0)
        return AIRY_ERR_INVALID_PARAM;

    airy_task_plan_t *plan = (airy_task_plan_t *)AIRY_CALLOC(1, sizeof(airy_task_plan_t));
    if (!plan)
        return AIRY_ERR_OUT_OF_MEMORY;

    cJSON *pid = cJSON_GetObjectItem(plan_json, "task_plan_id");
    if (cJSON_IsString(pid) && pid->valuestring && pid->valuestring[0]) {
        plan->task_plan_id = AIRY_STRDUP(pid->valuestring);
        plan->task_plan_id_len = plan->task_plan_id ? strlen(plan->task_plan_id) : 0;
    }

    plan->task_plan_node_count = (size_t)node_n;
    plan->task_plan_nodes =
        (airy_task_node_t **)AIRY_CALLOC((size_t)node_n, sizeof(airy_task_node_t *));
    if (!plan->task_plan_nodes) {
        plan->task_plan_node_count = 0;
        goto fail;
    }

    for (int i = 0; i < node_n; i++) {
        cJSON *nj = cJSON_GetArrayItem(nodes, i);
        if (!nj)
            continue;
        airy_task_node_t *nd = (airy_task_node_t *)AIRY_CALLOC(1, sizeof(airy_task_node_t));
        if (!nd)
            goto fail;
        plan->task_plan_nodes[i] = nd;

        cJSON *f = cJSON_GetObjectItem(nj, "id");
        if (cJSON_IsString(f) && f->valuestring && f->valuestring[0]) {
            nd->task_node_id = AIRY_STRDUP(f->valuestring);
            nd->task_node_id_len = nd->task_node_id ? strlen(nd->task_node_id) : 0;
        }
        f = cJSON_GetObjectItem(nj, "goal");
        if (cJSON_IsString(f) && f->valuestring)
            nd->task_node_goal = AIRY_STRDUP(f->valuestring);
        f = cJSON_GetObjectItem(nj, "handler");
        if (cJSON_IsString(f) && f->valuestring)
            nd->task_node_handler_name = AIRY_STRDUP(f->valuestring);
        f = cJSON_GetObjectItem(nj, "role");
        if (cJSON_IsString(f) && f->valuestring) {
            nd->task_node_agent_role = AIRY_STRDUP(f->valuestring);
            nd->task_node_role_len =
                nd->task_node_agent_role ? strlen(nd->task_node_agent_role) : 0;
        }

        cJSON *deps = cJSON_GetObjectItem(nj, "depends");
        int dep_n = (deps && cJSON_IsArray(deps)) ? cJSON_GetArraySize(deps) : 0;
        if (dep_n > 0) {
            nd->task_node_depends_on = (char **)AIRY_CALLOC((size_t)dep_n, sizeof(char *));
            if (!nd->task_node_depends_on)
                goto fail;
            for (int d = 0; d < dep_n; d++) {
                cJSON *dj = cJSON_GetArrayItem(deps, d);
                if (!cJSON_IsString(dj) || !dj->valuestring)
                    continue;
                nd->task_node_depends_on[nd->task_node_depends_count] =
                    AIRY_STRDUP(dj->valuestring);
                if (!nd->task_node_depends_on[nd->task_node_depends_count])
                    goto fail;
                nd->task_node_depends_count++;
            }
        }

        f = cJSON_GetObjectItem(nj, "cost_time_ms");
        if (cJSON_IsNumber(f))
            nd->task_node_cost_time_ms = (int64_t)f->valuedouble;
        f = cJSON_GetObjectItem(nj, "cost_mem_mb");
        if (cJSON_IsNumber(f))
            nd->task_node_cost_mem_mb = (int64_t)f->valuedouble;
        f = cJSON_GetObjectItem(nj, "invariant_guard");
        if (f && cJSON_IsTrue(f))
            nd->task_node_invariant_guard = 1;
    }

    cJSON *entry = cJSON_GetObjectItem(plan_json, "entry_points");
    int entry_n = (entry && cJSON_IsArray(entry)) ? cJSON_GetArraySize(entry) : 0;
    if (entry_n > 0) {
        plan->task_plan_entry_points = (char **)AIRY_CALLOC((size_t)entry_n, sizeof(char *));
        if (!plan->task_plan_entry_points)
            goto fail;
        for (int e = 0; e < entry_n; e++) {
            cJSON *ej = cJSON_GetArrayItem(entry, e);
            if (!cJSON_IsString(ej) || !ej->valuestring)
                continue;
            plan->task_plan_entry_points[plan->task_plan_entry_count] =
                AIRY_STRDUP(ej->valuestring);
            if (!plan->task_plan_entry_points[plan->task_plan_entry_count])
                goto fail;
            plan->task_plan_entry_count++;
        }
    } else {
        plan->task_plan_entry_points = (char **)AIRY_CALLOC((size_t)node_n, sizeof(char *));
        if (!plan->task_plan_entry_points)
            goto fail;
        for (int i = 0; i < node_n; i++) {
            const airy_task_node_t *nd = plan->task_plan_nodes[i];
            if (!nd || !nd->task_node_id || nd->task_node_depends_count > 0)
                continue;
            plan->task_plan_entry_points[plan->task_plan_entry_count] =
                AIRY_STRDUP(nd->task_node_id);
            if (!plan->task_plan_entry_points[plan->task_plan_entry_count])
                goto fail;
            plan->task_plan_entry_count++;
        }
    }

    *out_plan = plan;
    return AIRY_SUCCESS;

fail:
    airy_task_plan_free(plan);
    return AIRY_ERR_OUT_OF_MEMORY;
}

/* Remote dual-thinking via AIRY_THINK_SOCK: daemon_rpc_call(think.sock, "process", ..., 120s).
  * Non-zero on failure (no fake data); the caller falls back to airy_cognition_process. */
airy_err_t cli_think_process_remote(const char *think_sock, const char *input,
                                           size_t input_len, airy_task_plan_t **out_plan)
{
    (void)input_len;
    if (!think_sock || !think_sock[0] || !input || !out_plan)
        return AIRY_ERR_INVALID_PARAM;
    *out_plan = NULL;

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return AIRY_ERR_OUT_OF_MEMORY;
    cJSON *prompt_str = cJSON_CreateString(input);
    if (!prompt_str) {
        cJSON_Delete(params);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    cJSON_AddItemToObject(params, "prompt", prompt_str);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json)
        return AIRY_ERR_OUT_OF_MEMORY;

    char *rpc_result = NULL;
    int rc = daemon_rpc_call(think_sock, "process", params_json, &rpc_result, 120000);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        return (airy_err_t)rc;
    }

    cJSON *outer = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!outer)
        return AIRY_ERR_PARSE_ERROR;
    cJSON *inner_root = NULL;
    if (cJSON_IsString(outer) && outer->valuestring)
        inner_root = cJSON_Parse(outer->valuestring);
    if (!inner_root) {
        cJSON_Delete(outer);
        return AIRY_ERR_PARSE_ERROR;
    }

    cJSON *plan_json = cJSON_GetObjectItem(inner_root, "plan");
    int perr = AIRY_SUCCESS;
    if (!cJSON_IsObject(plan_json))
        perr = AIRY_ERR_PARSE_ERROR;
    else
        perr = cli_think_plan_from_json(plan_json, out_plan);
    if (perr == AIRY_SUCCESS && !*out_plan)
        perr = AIRY_ERR_PARSE_ERROR;

    cJSON_Delete(inner_root);
    cJSON_Delete(outer);
    return (airy_err_t)perr;
}
