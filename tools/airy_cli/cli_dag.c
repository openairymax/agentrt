// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_dag.c
 * @brief airy_cli WorkHall DAG domain: submit/poll/wait for remote sched_d blueprints.
 *
 * When AIRY_SCHED_SOCK is set, DAG execution goes through daemon RPC
 * (dag_submit / dag_status / dag_cancel on sched.sock) and the final state
 * aggregates each node's real output/error; otherwise it falls back to the
 * embedded work hall. Ctrl+C cancels the remote DAG.
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

/* ==================== Remote daemon mode (think_d dual-thinking / sched_d blueprint DAG)
 * ====================
 */

/*
  * AIRY_THINK_SOCK and AIRY_SCHED_SOCK are explicit enable switches:
  *   - Set and non-empty -> cognition/DAG runs over daemon RPC ($AIRY_RUNTIME_DIR/think.sock, sched.sock)
  *   - Unset / failed calls -> fall back to the embedded engine (backward compatible)
 *
  * Protocol alignment (strictly matches the daemon's JSON-RPC 2.0 over Unix socket):
  *   - think.process: params {"prompt":<input>}; result is a JSON string value (think_d wraps
  *     it with cJSON_CreateString; contains plan/feedback/stats), double-parse needed
 *   - sched.dag_submit: params {"dag":{name,nodes:[{id,goal,role,depends}]}};
 *     returns {"dag_id","status"}
  *   - sched.dag_status: params {"dag_id"}, returns a board snapshot (status/progress/node_count/nodes[])
 */

static const char *cli_handler_role(const char *handler)
{
    if (!handler || handler[0] == '\0')
        return "coding";
    if (strncmp(handler, "agent:", 6) == 0)
        return handler + 6;
    return handler;
}

/* Serialize a CLI workflow into the remote DAG JSON protocol.
 *
 * task_input is the raw user task text (same value the embedded hall passes to
 * airy_work_hall_submit). It travels as a top-level "input" field; sched_d
 * falls back to it for nodes whose goal is only a plan label (goal==id), so
 * remote agents receive the actual task instead of "reactive_1_step1". */
static char *cli_workflow_to_dag_json(const taskflow_workflow_t *wf, const char *task_input)
{
    if (!wf || wf->node_count == 0 || !wf->nodes)
        return NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON_AddStringToObject(root, "name",
                            wf->name[0] ? wf->name : (wf->id[0] ? wf->id : "airy_cli_dag"));
    if (task_input && task_input[0])
        cJSON_AddStringToObject(root, "input", task_input);

    cJSON *nodes = cJSON_CreateArray();
    if (!nodes) {
        cJSON_Delete(root);
        return NULL;
    }
    for (size_t i = 0; i < wf->node_count; i++) {
        const taskflow_node_t *nd = &wf->nodes[i];
        if (nd->id[0] == '\0')
            continue;
        cJSON *nj = cJSON_CreateObject();
        if (!nj)
            continue;
        cJSON_AddStringToObject(nj, "id", nd->id);
        const char *goal = nd->name[0] ? nd->name : nd->id;
        if (strcmp(goal, nd->id) == 0 && task_input && task_input[0])
            goal = task_input;
        cJSON_AddStringToObject(nj, "goal", goal);
        cJSON_AddStringToObject(nj, "role", cli_handler_role(nd->task_handler_name));

        cJSON *deps = cJSON_CreateArray();
        for (size_t e = 0; e < wf->edge_count; e++) {
            const taskflow_edge_t *de = &wf->edges[e];
            if (strcmp(de->target_node_id, nd->id) == 0 && de->source_node_id[0] != '\0')
                cJSON_AddItemToArray(deps, cJSON_CreateString(de->source_node_id));
        }
        cJSON_AddItemToObject(nj, "depends", deps);
        cJSON_AddItemToArray(nodes, nj);
    }
    cJSON_AddItemToObject(root, "nodes", nodes);

    if (cJSON_GetArraySize(nodes) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

airy_err_t cli_dag_submit_remote(const char *sched_sock, const taskflow_workflow_t *wf,
                                        const char *task_input, char **out_dag_id)
{
    if (!sched_sock || !sched_sock[0] || !wf || !out_dag_id)
        return AIRY_ERR_INVALID_PARAM;
    *out_dag_id = NULL;

    char *dag_json = cli_workflow_to_dag_json(wf, task_input);
    if (!dag_json)
        return AIRY_ERR_OUT_OF_MEMORY;

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        AIRY_FREE(dag_json);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    cJSON *dag_obj = cJSON_Parse(dag_json);
    if (!dag_obj) {
        AIRY_FREE(dag_json);
        cJSON_Delete(params);
        return AIRY_ERR_PARSE_ERROR;
    }
    cJSON_AddItemToObject(params, "dag", dag_obj);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    AIRY_FREE(dag_json);
    if (!params_json)
        return AIRY_ERR_OUT_OF_MEMORY;

    char *rpc_result = NULL;
    int rc = daemon_rpc_call(sched_sock, "dag_submit", params_json, &rpc_result, 30000);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        return (airy_err_t)rc;
    }

    cJSON *root = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!root)
        return AIRY_ERR_PARSE_ERROR;

    cJSON *did = cJSON_GetObjectItem(root, "dag_id");
    if (!cJSON_IsString(did) || !did->valuestring || !did->valuestring[0]) {
        cJSON_Delete(root);
        return AIRY_ERR_STATE_ERROR;
    }
    *out_dag_id = AIRY_STRDUP(did->valuestring);
    cJSON_Delete(root);
    return *out_dag_id ? AIRY_EOK : AIRY_ERR_OUT_OF_MEMORY;
}

/* Poll sched.dag_status once: parse the snapshot (progress=done nodes/node_count).
  * At the final state, aggregate node outputs/errors into a root-level output for display. */
cli_dag_poll_rc_t cli_dag_poll_remote(const char *sched_sock, const char *dag_id,
                                             double *out_progress, char *out_state,
                                             size_t state_cap, char **out_result)
{
    if (!sched_sock || !dag_id || !out_progress || !out_state || state_cap == 0)
        return CLI_DAG_POLL_ERROR;
    if (out_result)
        *out_result = NULL;

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return CLI_DAG_POLL_ERROR;
    cJSON_AddStringToObject(params, "dag_id", dag_id);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json)
        return CLI_DAG_POLL_ERROR;

    char *rpc_result = NULL;
    int rc = daemon_rpc_call(sched_sock, "dag_status", params_json, &rpc_result, 10000);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        return CLI_DAG_POLL_ERROR;
    }

    cJSON *root = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!root)
        return CLI_DAG_POLL_ERROR;

    cJSON *st = cJSON_GetObjectItem(root, "status");
    const char *status = (cJSON_IsString(st) && st->valuestring) ? st->valuestring : "unknown";
    snprintf(out_state, state_cap, "%s", status);

    cJSON *nc = cJSON_GetObjectItem(root, "node_count");
    cJSON *pg = cJSON_GetObjectItem(root, "progress");
    double node_n = cJSON_IsNumber(nc) ? nc->valuedouble : 0.0;
    double done_n = cJSON_IsNumber(pg) ? pg->valuedouble : 0.0;
    *out_progress = node_n > 0.0 ? done_n / node_n : 0.0;

    int terminal = (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                    strcmp(status, "canceled") == 0);
    if (terminal && out_result) {
        cJSON *agg = cJSON_CreateObject();
        if (agg) {
            cJSON_AddStringToObject(agg, "dag_id", dag_id);
            cJSON_AddStringToObject(agg, "status", status);
            cJSON *nodes = cJSON_GetObjectItem(root, "nodes");
            int nsz = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;

            size_t cap = 4096;
            for (int i = 0; i < nsz; i++) {
                cJSON *nj = cJSON_GetArrayItem(nodes, i);
                if (!nj)
                    continue;
                cJSON *o = cJSON_GetObjectItem(nj, "output");
                cJSON *e = cJSON_GetObjectItem(nj, "error");
                if (cJSON_IsString(o) && o->valuestring)
                    cap += strlen(o->valuestring) + 128;
                if (cJSON_IsString(e) && e->valuestring)
                    cap += strlen(e->valuestring) + 128;
            }
            char *buf = (char *)AIRY_MALLOC(cap);
            if (buf) {
                size_t off = 0;
                buf[0] = '\0';
                for (int i = 0; i < nsz; i++) {
                    cJSON *nj = cJSON_GetArrayItem(nodes, i);
                    if (!nj)
                        continue;
                    cJSON *o = cJSON_GetObjectItem(nj, "output");
                    cJSON *e = cJSON_GetObjectItem(nj, "error");
                    const char *text = NULL;
                    if (cJSON_IsString(o) && o->valuestring && o->valuestring[0])
                        text = o->valuestring;
                    else if (cJSON_IsString(e) && e->valuestring && e->valuestring[0])
                        text = e->valuestring;
                    if (!text)
                        continue;
                    cJSON *nid = cJSON_GetObjectItem(nj, "id");
                    const char *nid_s =
                        (cJSON_IsString(nid) && nid->valuestring) ? nid->valuestring : "?";
                    if (off > 0 && off < cap - 1) {
                        snprintf(buf + off, cap - off, "\n\n");
                        off += 2;
                    }
                    int w = snprintf(buf + off, cap - off, "[%s] %s", nid_s, text);
                    if (w < 0)
                        break;
                    off += (size_t)w;
                    if (off >= cap)
                        break;
                }
                cJSON_AddStringToObject(agg, "output", buf);
                AIRY_FREE(buf);
            }
            *out_result = cJSON_PrintUnformatted(agg);
            cJSON_Delete(agg);
        }
    }

    cJSON_Delete(root);
    return terminal ? CLI_DAG_POLL_DONE : CLI_DAG_POLL_ACTIVE;
}

/* Remote wait: poll dag_status until the final state (replaces airy_work_hall_wait).
  * Ctrl+C propagates: really calls sched.dag_cancel to abort the remote DAG. */
#define CLI_DAG_WAIT_MAX_POLLS 36000

airy_err_t cli_dag_wait_remote(const char *sched_sock, const char *dag_id, char **out_result)
{
    if (!sched_sock || !dag_id || !out_result)
        return AIRY_ERR_INVALID_PARAM;
    *out_result = NULL;

    for (int poll = 0; poll < CLI_DAG_WAIT_MAX_POLLS; poll++) {
        if (g_cli_cancel) {

            cJSON *cparams = cJSON_CreateObject();
            if (cparams) {
                cJSON_AddStringToObject(cparams, "dag_id", dag_id);
                char *cparams_json = cJSON_PrintUnformatted(cparams);
                cJSON_Delete(cparams);
                if (cparams_json) {
                    char *cres = NULL;
                    (void)daemon_rpc_call(sched_sock, "dag_cancel", cparams_json, &cres, 5000);
                    AIRY_FREE(cparams_json);
                    AIRY_FREE(cres);
                }
            }
            return AIRY_ERR_CANCELED;
        }

        double prog = 0.0;
        char st[16];
        char *final_result = NULL;
        cli_dag_poll_rc_t prc =
            cli_dag_poll_remote(sched_sock, dag_id, &prog, st, sizeof(st), &final_result);
        if (prc == CLI_DAG_POLL_DONE) {
            if (final_result) {
                *out_result = final_result;
                return AIRY_EOK;
            }
            return AIRY_ERR_STATE_ERROR;
        }
        if (prc == CLI_DAG_POLL_ERROR)
            return AIRY_ERR_FAIL;
#ifdef _WIN32
        Sleep(200);
#else
        usleep(200 * 1000);
#endif
    }
    return AIRY_ERR_TIMEOUT;
}
