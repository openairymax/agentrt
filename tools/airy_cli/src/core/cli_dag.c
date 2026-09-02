// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_dag.c
 * @brief airy_cli 任务执行域：plan → gateway → sched_d 唯一通路的提交/轮询/等待。
 *
 * DAG 序列化直接消费 airy_task_plan_t（Unify Design SSoT：计划即任务树，
 * 经 gateway 派发 sched.dag_submit / dag_status / dag_cancel），最终态聚合
 * 各节点真实 output/error 供展示。Ctrl+C 取消远端 DAG。
 * 0.1.9 M1 1c 引擎壳化：本地 hall 降级与 sched.sock 开关已退役。
 */

#include "cli_internal.h"

#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */
#include "id_utils.h"

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

/* ==================== sched_d blueprint DAG over gateway ====================
 */

/*
 * Protocol alignment (gateway 转发 sched_d 的 JSON-RPC 2.0 线格式):
 *   - sched.dag_submit: params {"dag":{name,input,workspace_dir,
 *     nodes:[{id,goal,role,depends}]}}; returns {"dag_id","status"}
 *   - sched.dag_status: params {"dag_id"}, returns a board snapshot
 *     (status/progress/node_count/nodes[])
 *   - sched.dag_cancel: params {"dag_id"}
 */

static const char *cli_handler_role(const char *handler)
{
    if (!handler || handler[0] == '\0')
        return "coding";
    if (strncmp(handler, "agent:", 6) == 0)
        return handler + 6;
    return handler;
}

/* 节点 handler 归一（与 plan_to_dag 蓝图语义一致）：显式 handler_name 优先，
 * 缺省由 agent_role 派生；无 "agent:" 前缀则补齐。buf 置空串表示无 handler。 */
void cli_node_handler(const airy_task_node_t *nd, char *buf, size_t cap)
{
    if (!nd || !buf || cap == 0)
        return;
    buf[0] = '\0';
    const char *src = nd->task_node_handler_name;
    if ((!src || src[0] == '\0') && nd->task_node_agent_role &&
        nd->task_node_agent_role[0] != '\0')
        src = nd->task_node_agent_role;
    if (!src || src[0] == '\0')
        return;
    if (strncmp(src, "agent:", 6) == 0) {
        AIRY_STRNCPY_TERM(buf, src, cap);
        return;
    }
    snprintf(buf, cap, "agent:%s", src);
}

size_t cli_plan_deps_count(const airy_task_plan_t *plan)
{
    if (!plan)
        return 0;
    size_t total = 0;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        const airy_task_node_t *nd =
            plan->task_plan_nodes ? plan->task_plan_nodes[i] : NULL;
        if (nd && nd->task_node_depends_count > 0 && nd->task_node_depends_on)
            total += nd->task_node_depends_count;
    }
    return total;
}

/* 将 airy_task_plan_t 序列化为 sched_d 的 DAG JSON 线格式（唯一真相源）。
 *
 * task_input 是用户原始任务文本，作为顶层 "input" 字段传递；goal 只是计划
 * 标签（goal==id）的节点由 sched_d 回退到 input，远端 agent 因此收到真实
 * 任务而非 "reactive_1_step1"。workspace_dir（未设时 NULL）作为顶层字段，
 * 令 sched_d -> agent_d -> runner 执行前 chdir 进任务目录，产物留在调用方
 * 工程树而非 daemon 的 cwd。 */
static char *cli_plan_to_dag_json(const airy_task_plan_t *plan, const char *task_input,
                                  const char *workspace_dir)
{
    if (!plan || plan->task_plan_node_count == 0 || !plan->task_plan_nodes)
        return NULL;

    char pid[64] = {0};
    if (plan->task_plan_id && plan->task_plan_id[0])
        AIRY_STRNCPY_TERM(pid, plan->task_plan_id, sizeof(pid));
    else
        airy_generate_plan_id(pid, sizeof(pid));

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    char name[80];
    snprintf(name, sizeof(name), "plan_%s", pid);
    cJSON_AddStringToObject(root, "name", name);
    if (task_input && task_input[0])
        cJSON_AddStringToObject(root, "input", task_input);
    if (workspace_dir && workspace_dir[0])
        cJSON_AddStringToObject(root, "workspace_dir", workspace_dir);

    cJSON *nodes = cJSON_CreateArray();
    if (!nodes) {
        cJSON_Delete(root);
        return NULL;
    }
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        const airy_task_node_t *nd = plan->task_plan_nodes[i];
        if (!nd)
            continue;
        char nid[64];
        snprintf(nid, sizeof(nid), "%s", nd->task_node_id ? nd->task_node_id : "node");
        cJSON *nj = cJSON_CreateObject();
        if (!nj)
            continue;
        cJSON_AddStringToObject(nj, "id", nid);
        const char *goal = nd->task_node_goal ? nd->task_node_goal : nid;
        if (strcmp(goal, nid) == 0 && task_input && task_input[0])
            goal = task_input;
        cJSON_AddStringToObject(nj, "goal", goal);
        char handler[96];
        cli_node_handler(nd, handler, sizeof(handler));
        cJSON_AddStringToObject(nj, "role", cli_handler_role(handler));

        cJSON *deps = cJSON_CreateArray();
        if (deps && nd->task_node_depends_on && nd->task_node_depends_count > 0) {
            for (size_t e = 0; e < nd->task_node_depends_count; e++) {
                const char *dep = nd->task_node_depends_on[e];
                if (dep && dep[0] != '\0')
                    cJSON_AddItemToArray(deps, cJSON_CreateString(dep));
            }
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

airy_err_t cli_dag_submit_remote(const airy_task_plan_t *plan, const char *task_input,
                                 const char *workspace_dir, char **out_dag_id)
{
    if (!plan || !out_dag_id)
        return AIRY_ERR_INVALID_PARAM;
    *out_dag_id = NULL;

    char *dag_json = cli_plan_to_dag_json(plan, task_input, workspace_dir);
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

    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_submit →
     * gateway → SYS_SVC_CALL → sched_d），禁止直连 sched.sock。 */
    char *rpc_result = NULL;
    int rc = cli_gw_call("sched.dag_submit", params_json, 30000, &rpc_result);
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
cli_dag_poll_rc_t cli_dag_poll_remote(const char *dag_id, double *out_progress, char *out_state,
                                      size_t state_cap, char **out_result)
{
    if (!dag_id || !out_progress || !out_state || state_cap == 0)
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

    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_status） */
    char *rpc_result = NULL;
    int rc = cli_gw_call("sched.dag_status", params_json, 10000, &rpc_result);
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

/* Wait dag_status until the final state. Ctrl+C propagates: really calls
 * sched.dag_cancel to abort the remote DAG. */
#define CLI_DAG_WAIT_MAX_POLLS 36000

airy_err_t cli_dag_wait_remote(const char *dag_id, char **out_result)
{
    if (!dag_id || !out_result)
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
                    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_cancel） */
                    (void)cli_gw_call("sched.dag_cancel", cparams_json, 5000, &cres);
                    AIRY_FREE(cparams_json);
                    AIRY_FREE(cres);
                }
            }
            return AIRY_ERR_CANCELED;
        }

        double prog = 0.0;
        char st[16];
        char *final_result = NULL;
        cli_dag_poll_rc_t prc = cli_dag_poll_remote(dag_id, &prog, st, sizeof(st), &final_result);
        if (prc == CLI_DAG_POLL_DONE) {
            if (final_result) {
                *out_result = final_result;
                return AIRY_EOK;
            }
            return AIRY_ERR_STATE_ERROR;
        }
        if (prc == CLI_DAG_POLL_ERROR)
            return AIRY_ERR_GENERIC_FAIL;
        airy_sleep_ms(200);
    }
    return AIRY_ERR_TIMEOUT;
}

/* ==================== node-level progress board ====================
 * Per-node live board for remote DAGs (Claude Code convention): the poll
 * loop renders one compact line per node with a state icon and the goal,
 * re-printing a node only when its state actually changed (no 200ms spam).
 * State icons: □ pending · ◇ running · ✓ completed · ✗ failed/canceled.
 * The board is created per DAG (cli_dag_node_board_create), driven from
 * the polling loop and released when the DAG finishes. */

#define CLI_DAG_BOARD_MAX_NODES 64

typedef struct {
    char id[64];
    char status[16];
    int printed;
} cli_dag_board_node_t;

struct cli_dag_board_s {
    cli_dag_board_node_t nodes[CLI_DAG_BOARD_MAX_NODES];
    size_t node_count;
};

static void cli_dag_board_icon_color(const char *status, const char **icon,
                                     const char **color)
{
    if (strcmp(status, "completed") == 0) {
        *icon = CLI_ICON_CHECK;
        *color = CLR_GREEN;
    } else if (strcmp(status, "failed") == 0 || strcmp(status, "canceled") == 0) {
        *icon = CLI_ICON_CROSS;
        *color = CLR_RED;
    } else if (strcmp(status, "running") == 0 || strcmp(status, "active") == 0 ||
               strcmp(status, "queued") == 0) {
        *icon = CLI_ICON_DIAMOND;
        *color = CLR_YELLOW;
    } else {
        *icon = CLI_ICON_TODO;
        *color = CLR_DIM;
    }
}

static void cli_dag_board_print(cli_dag_board_node_t *n, const char *goal)
{
    const char *icon, *color;
    cli_dag_board_icon_color(n->status, &icon, &color);
    cli_outf("%s%s%s%s %s%s%s %s%s%s\n", cli_gutter_pad(4), cli_c(color), icon,
           cli_c(CLR_RESET), cli_c(CLR_CYAN), n->id, cli_c(CLR_RESET), cli_c(CLR_DIM),
           goal ? goal : "", cli_c(CLR_RESET));
}

cli_dag_board_t *cli_dag_node_board_create(void)
{
    cli_dag_board_t *b = (cli_dag_board_t *)AIRY_CALLOC(1, sizeof(cli_dag_board_t));
    return b;
}

void cli_dag_node_board_destroy(cli_dag_board_t *b)
{
    AIRY_FREE(b);
}

/* Query dag_status, diff node states against the last snapshot and print
 * every node whose state changed. Returns 0 while the DAG is still active,
 * 1 once a terminal state is observed (caller stops polling). */
int cli_dag_node_board_tick(cli_dag_board_t *b, const char *dag_id)
{
    if (!b || !dag_id)
        return 0;

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return 0;
    cJSON_AddStringToObject(params, "dag_id", dag_id);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json)
        return 0;

    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_status） */
    char *rpc_result = NULL;
    int rc = cli_gw_call("sched.dag_status", params_json, 10000, &rpc_result);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        return 0;
    }

    cJSON *root = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!root)
        return 0;

    cJSON *st = cJSON_GetObjectItem(root, "status");
    const char *status = (cJSON_IsString(st) && st->valuestring) ? st->valuestring : "unknown";
    int terminal = (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                    strcmp(status, "canceled") == 0);

    cJSON *nodes = cJSON_GetObjectItem(root, "nodes");
    int nsz = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    if (nsz > CLI_DAG_BOARD_MAX_NODES)
        nsz = CLI_DAG_BOARD_MAX_NODES;

    for (int i = 0; i < nsz; i++) {
        cJSON *nj = cJSON_GetArrayItem(nodes, i);
        if (!nj)
            continue;
        cJSON *nid = cJSON_GetObjectItem(nj, "id");
        cJSON *nst = cJSON_GetObjectItem(nj, "status");
        cJSON *goal = cJSON_GetObjectItem(nj, "goal");
        const char *id_s = (cJSON_IsString(nid) && nid->valuestring) ? nid->valuestring : "?";
        const char *st_s = (cJSON_IsString(nst) && nst->valuestring) ? nst->valuestring : "pending";
        const char *goal_s = (cJSON_IsString(goal) && goal->valuestring) ? goal->valuestring : "";

        /* Find or register the node slot. */
        cli_dag_board_node_t *slot = NULL;
        for (size_t k = 0; k < b->node_count; k++) {
            if (strcmp(b->nodes[k].id, id_s) == 0) {
                slot = &b->nodes[k];
                break;
            }
        }
        if (!slot && b->node_count < CLI_DAG_BOARD_MAX_NODES) {
            slot = &b->nodes[b->node_count++];
            AIRY_STRNCPY_TERM(slot->id, id_s, sizeof(slot->id));
            slot->printed = 0;
        }
        if (!slot)
            continue;

        int changed = (strcmp(slot->status, st_s) != 0);
        AIRY_STRNCPY_TERM(slot->status, st_s, sizeof(slot->status));
        if (!slot->printed || changed) {
            slot->printed = 1;
            cli_dag_board_print(slot, goal_s);
        }
    }

    cJSON_Delete(root);
    return terminal ? 1 : 0;
}

/* Remote DAG per-node state snapshot (live plan board feeder): parses
 * dag_status and reports every node's (id, state) through cb without
 * printing anything — the live plan board renders the block itself.
 * Returns 1 once a terminal state is observed, 0 otherwise. */
int cli_dag_board_snapshot(const char *dag_id, void (*cb)(const char *node_id, const char *state))
{
    if (!dag_id || !cb)
        return 0;

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return 0;
    cJSON_AddStringToObject(params, "dag_id", dag_id);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json)
        return 0;

    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_status） */
    char *rpc_result = NULL;
    int rc = cli_gw_call("sched.dag_status", params_json, 10000, &rpc_result);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        return 0;
    }

    cJSON *root = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!root)
        return 0;

    cJSON *st = cJSON_GetObjectItem(root, "status");
    const char *status = (cJSON_IsString(st) && st->valuestring) ? st->valuestring : "unknown";
    int terminal = (strcmp(status, "completed") == 0 || strcmp(status, "failed") == 0 ||
                    strcmp(status, "canceled") == 0);

    cJSON *nodes = cJSON_GetObjectItem(root, "nodes");
    int nsz = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    for (int i = 0; i < nsz; i++) {
        cJSON *nj = cJSON_GetArrayItem(nodes, i);
        if (!nj)
            continue;
        cJSON *nid = cJSON_GetObjectItem(nj, "id");
        cJSON *nst = cJSON_GetObjectItem(nj, "status");
        if (cJSON_IsString(nid) && nid->valuestring) {
            const char *st_s = (cJSON_IsString(nst) && nst->valuestring) ? nst->valuestring
                                                                         : "pending";
            cb(nid->valuestring, st_s);
        }
    }

    cJSON_Delete(root);
    return terminal ? 1 : 0;
}
