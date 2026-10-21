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

#include "agent_vocab.h"
#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */
#include "id_utils.h"

#include "cli_prelude.h"

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

/* 单字段 params 信封 {"key": value} → 紧凑 JSON；失败返回 NULL。 */
static char *dag_params1(const char *key, const char *val)
{
    cJSON *obj = cJSON_CreateObject();
    char *js;

    if (!obj)
        return NULL;
    cJSON_AddStringToObject(obj, key, val ? val : "");
    js = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return js;
}

/* 经 gateway 调一处 DAG 方法并解析回包根；失败返回 NULL 且写 *err。 */
static cJSON *dag_rpc(const char *method, const char *params_json, int timeout_ms, int *err)
{
    char *rpc_result = NULL;
    int rc = cli_gw_call(method, params_json, timeout_ms, &rpc_result);
    cJSON *root;

    *err = (rc == AIRY_SUCCESS && !rpc_result) ? AIRY_ERR_GENERIC_FAIL : rc;
    if (*err != AIRY_SUCCESS) {
        AIRY_FREE(rpc_result);
        return NULL;
    }
    root = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!root)
        *err = AIRY_ERR_PARSE_ERROR;
    return root;
}

static const char *cli_handler_role(const char *handler)
{
    if (!handler || handler[0] == '\0')
        return AGENT_VOCAB_FALLBACK;
    if (strncmp(handler, "agent:", 6) == 0)
        return handler + 6;
    return handler;
}

/* 节点 handler 归一（与 DAG 蓝图语义一致）：显式 handler_name 优先，
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
    int err = 0;
    cJSON *root = dag_rpc("sched.dag_submit", params_json, 30000, &err);

    AIRY_FREE(params_json);
    if (!root)
        return (airy_err_t)err;

    cJSON *did = cJSON_GetObjectItem(root, "dag_id");
    if (!cJSON_IsString(did) || !did->valuestring || !did->valuestring[0]) {
        cJSON_Delete(root);
        return AIRY_ERR_STATE_ERROR;
    }
    *out_dag_id = AIRY_STRDUP(did->valuestring);
    cJSON_Delete(root);
    return *out_dag_id ? AIRY_EOK : AIRY_ERR_OUT_OF_MEMORY;
}

/* ==================== dag_status 快照（单次拉取 SSoT） ====================
 * 板卡轮询每轮只发一次 sched.dag_status，状态面与节点面共用同一份解析结果。
 * 此前两条消费路径各自发起一次 RPC，每轮 2 次重复请求把 sched_d 打成
 * 200ms 周期的短连接洪泛（0.1.17 R4-① 根因）。 */

struct cli_dag_snapshot {
    cJSON *root;
    char dag_id[64];
};

cli_dag_snapshot_t *cli_dag_snapshot_fetch(const char *dag_id)
{
    if (!dag_id || !dag_id[0])
        return NULL;

    char *params_json = dag_params1("dag_id", dag_id);
    if (!params_json)
        return NULL;

    /* 架构约束（2026-08-25）：统一经 gateway 派发（sched.dag_status） */
    int err = 0;
    cJSON *root = dag_rpc("sched.dag_status", params_json, 10000, &err);

    AIRY_FREE(params_json);
    if (!root)
        return NULL;

    cli_dag_snapshot_t *snap = (cli_dag_snapshot_t *)AIRY_CALLOC(1, sizeof(cli_dag_snapshot_t));
    if (!snap) {
        cJSON_Delete(root);
        return NULL;
    }
    snap->root = root;
    AIRY_STRNCPY_TERM(snap->dag_id, dag_id, sizeof(snap->dag_id));
    return snap;
}

void cli_dag_snapshot_free(cli_dag_snapshot_t *snap)
{
    if (!snap)
        return;
    if (snap->root)
        cJSON_Delete(snap->root);
    AIRY_FREE(snap);
}

/* cJSON 字符串取值：非字符串、空指针一律回落 dflt（空串表示字段缺失）。 */
static const char *json_str(const cJSON *obj, const char *key, const char *dflt)
{
    const cJSON *v = cJSON_GetObjectItem(obj, key);

    return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : dflt;
}

static const char *cli_dag_snapshot_state(const cli_dag_snapshot_t *snap)
{
    if (!snap || !snap->root)
        return "unknown";
    return json_str(snap->root, "status", "unknown");
}

/* 节点自述文本：output 优先，其次 error；均为空则返回 NULL。
 * out_id 可缺省，非空时写回节点编号（缺失记 "?"）。 */
static const char *node_text(const cJSON *nj, const char **out_id)
{
    const char *out = json_str(nj, "output", "");
    const char *err = json_str(nj, "error", "");

    if (out_id)
        *out_id = json_str(nj, "id", "?");
    if (out[0])
        return out;
    return err[0] ? err : NULL;
}

/* 汇总缓冲预算：基座 + 每段编号/分隔/正文余量。 */
static size_t agg_cap(const cJSON *nodes, int nsz)
{
    size_t cap = 4096;

    for (int i = 0; i < nsz; i++) {
        const cJSON *nj = cJSON_GetArrayItem(nodes, i);
        const char *txt = nj ? node_text(nj, NULL) : NULL;

        if (txt)
            cap += strlen(txt) + 128;
    }
    return cap;
}

/* 追加 "[id] text"，段间双换行；越界即置 off=cap 令调用方停写。 */
static void agg_append(char *buf, size_t cap, size_t *off, const char *id, const char *txt)
{
    int w;

    if (*off > 0 && *off < cap - 1) {
        snprintf(buf + *off, cap - *off, "\n\n");
        *off += 2;
    }
    w = snprintf(buf + *off, cap - *off, "[%s] %s", id, txt);
    if (w < 0 || (size_t)w >= cap - *off)
        *off = cap;
    else
        *off += (size_t)w;
}

/* 终态汇总信封：dag_id/status 恒定，output 为逐节点文本拼接。 */
static char *dag_aggregate(const cli_dag_snapshot_t *snap, const char *status)
{
    cJSON *agg = cJSON_CreateObject();
    cJSON *nodes;
    int nsz;
    size_t cap;
    char *buf;
    char *out;

    if (!agg)
        return NULL;
    cJSON_AddStringToObject(agg, "dag_id", snap->dag_id);
    cJSON_AddStringToObject(agg, "status", status);
    nodes = cJSON_GetObjectItem(snap->root, "nodes");
    nsz = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    cap = agg_cap(nodes, nsz);
    buf = (char *)AIRY_MALLOC(cap);
    if (buf) {
        size_t off = 0;

        buf[0] = '\0';
        for (int i = 0; i < nsz && off < cap; i++) {
            const cJSON *nj = cJSON_GetArrayItem(nodes, i);
            const char *id = "?";
            const char *txt = nj ? node_text(nj, &id) : NULL;

            if (txt)
                agg_append(buf, cap, &off, id, txt);
        }
        cJSON_AddStringToObject(agg, "output", buf);
        AIRY_FREE(buf);
    }
    out = cJSON_PrintUnformatted(agg);
    cJSON_Delete(agg);
    return out;
}

/* Consume one fetched snapshot: progress = done nodes / node_count. At the
 * final state, aggregate node outputs/errors into a root-level output. */
cli_dag_poll_rc_t cli_dag_snapshot_poll(const cli_dag_snapshot_t *snap, double *out_progress,
                                        char *out_state, size_t state_cap, char **out_result)
{
    const char *status;
    cJSON *nc;
    cJSON *pg;
    double node_n;
    double done_n;
    int terminal;

    if (out_result)
        *out_result = NULL;
    if (!snap || !snap->root || !out_progress || !out_state || state_cap == 0)
        return CLI_DAG_POLL_ERROR;

    status = cli_dag_snapshot_state(snap);
    snprintf(out_state, state_cap, "%s", status);
    nc = cJSON_GetObjectItem(snap->root, "node_count");
    pg = cJSON_GetObjectItem(snap->root, "progress");
    node_n = cJSON_IsNumber(nc) ? nc->valuedouble : 0.0;
    done_n = cJSON_IsNumber(pg) ? pg->valuedouble : 0.0;
    *out_progress = node_n > 0.0 ? done_n / node_n : 0.0;

    terminal = cli_state_terminal(status);
    if (terminal && out_result)
        *out_result = dag_aggregate(snap, status);
    return terminal ? CLI_DAG_POLL_DONE : CLI_DAG_POLL_ACTIVE;
}

/* Enumerate remote DAGs over the gateway (sched.dag_list). 0.1.9 M1-1c:
 * CLI /status 与 TUI board 查询面迁到 sched_d 权威 DAG 表（本地 work_hall
 * 实例表已无写入方，展示迁远程后本地 hall 仅剩生命周期，C2d 退役）。
 * items 由调用方提供容量 cap，实际条数写回 *out_count（超出截断）。 */
airy_err_t cli_dag_list_remote(cli_dag_item_t *items, size_t cap, size_t *out_count)
{
    if (!items || cap == 0 || !out_count)
        return AIRY_ERR_INVALID_PARAM;
    *out_count = 0;

    int err = 0;
    cJSON *root = dag_rpc("sched.dag_list", "{}", 10000, &err);

    if (!root)
        return (airy_err_t)err;

    cJSON *dags = cJSON_GetObjectItem(root, "dags");
    int n = (dags && cJSON_IsArray(dags)) ? cJSON_GetArraySize(dags) : 0;
    if (n > (int)cap)
        n = (int)cap;
    for (int i = 0; i < n; i++) {
        cJSON *dj = cJSON_GetArrayItem(dags, i);
        if (!dj)
            continue;
        cli_dag_item_t *it = &items[*out_count];
        AIRY_STRNCPY_TERM(it->dag_id, json_str(dj, "dag_id", ""), sizeof(it->dag_id));
        AIRY_STRNCPY_TERM(it->name, json_str(dj, "name", ""), sizeof(it->name));
        AIRY_STRNCPY_TERM(it->status, json_str(dj, "status", ""), sizeof(it->status));
        cJSON *nc = cJSON_GetObjectItem(dj, "node_count");
        cJSON *pg = cJSON_GetObjectItem(dj, "progress");
        it->node_count = (cJSON_IsNumber(nc) && nc->valuedouble > 0)
                             ? (size_t)nc->valuedouble
                             : 0;
        it->done = (cJSON_IsNumber(pg) && pg->valuedouble > 0) ? (size_t)pg->valuedouble : 0;
        (*out_count)++;
    }

    cJSON_Delete(root);
    return AIRY_EOK;
}

/* Cancel a remote DAG over the gateway (shared by Ctrl+C wait abort and the
 * TUI task-board cancel action). 架构约束（2026-08-25）：统一经 gateway 派发
 * （sched.dag_cancel → gateway → SYS_SVC_CALL → sched_d），禁止直连。 */
airy_err_t cli_dag_cancel_remote(const char *dag_id)
{
    if (!dag_id || !dag_id[0])
        return AIRY_ERR_INVALID_PARAM;

    char *cparams_json = dag_params1("dag_id", dag_id);
    if (!cparams_json)
        return AIRY_ERR_OUT_OF_MEMORY;

    char *cres = NULL;
    int rc = cli_gw_call("sched.dag_cancel", cparams_json, 5000, &cres);
    AIRY_FREE(cparams_json);
    AIRY_FREE(cres);
    return rc == AIRY_SUCCESS ? AIRY_EOK : (airy_err_t)rc;
}

/* Wait dag_status until the final state. Ctrl+C propagates: really calls
 * sched.dag_cancel to abort the remote DAG. Poll interval backs off
 * 200ms → 1.6s while the DAG makes no progress (reset on any change), so a
 * long-running task no longer floods the daemon with short-lived
 * connections at ~5Hz; the ~2h budget is preserved (4500 polls ≈ 2h at
 * the capped interval). */
#define CLI_DAG_WAIT_MAX_POLLS 4500
#define CLI_DAG_WAIT_BASE_MS 200
#define CLI_DAG_WAIT_MAX_MS 1600
#define CLI_DAG_WAIT_SLICE_MS 100

airy_err_t cli_dag_wait_remote(const char *dag_id, char **out_result)
{
    if (!dag_id || !out_result)
        return AIRY_ERR_INVALID_PARAM;
    *out_result = NULL;

    int interval_ms = CLI_DAG_WAIT_BASE_MS;
    char last_state[16] = "";
    double last_prog = -1.0;

    for (int poll = 0; poll < CLI_DAG_WAIT_MAX_POLLS; poll++) {
        if (g_cli_cancel) {
            (void)cli_dag_cancel_remote(dag_id);
            return AIRY_ERR_CANCELED;
        }

        double prog = 0.0;
        char st[16];
        char *final_result = NULL;
        cli_dag_snapshot_t *snap = cli_dag_snapshot_fetch(dag_id);
        if (!snap)
            return AIRY_ERR_GENERIC_FAIL;
        cli_dag_poll_rc_t prc =
            cli_dag_snapshot_poll(snap, &prog, st, sizeof(st), &final_result);
        cli_dag_snapshot_free(snap);
        if (prc == CLI_DAG_POLL_DONE) {
            if (final_result) {
                *out_result = final_result;
                return AIRY_EOK;
            }
            return AIRY_ERR_STATE_ERROR;
        }

        if (strcmp(st, last_state) != 0 || (prog - last_prog) >= 0.01) {
            interval_ms = CLI_DAG_WAIT_BASE_MS;
            snprintf(last_state, sizeof(last_state), "%s", st);
            last_prog = prog;
        } else if (interval_ms < CLI_DAG_WAIT_MAX_MS) {
            interval_ms *= 2;
        }

        int remain = interval_ms;
        while (remain > 0 && !g_cli_cancel) {
            int slice =
                (remain > CLI_DAG_WAIT_SLICE_MS) ? CLI_DAG_WAIT_SLICE_MS : remain;
            airy_sleep_ms((unsigned)slice);
            remain -= slice;
        }
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

/* 节点态图标：semantic_failed（进程成功但无产出）以「半叉」降级色呈现，
 * 与 completed 绿、failed/canceled 红区分，让「跑完了但结果不可用」一眼可见。 */
static void cli_dag_board_icon_color(const char *status, const char **icon,
                                     const char **color)
{
    if (strcmp(status, "completed") == 0) {
        *icon = CLI_ICON_CHECK;
        *color = CLR_GREEN;
    } else if (strcmp(status, "failed") == 0 || strcmp(status, "canceled") == 0) {
        *icon = CLI_ICON_CROSS;
        *color = CLR_RED;
    } else if (strcmp(status, "semantic_failed") == 0) {
        *icon = CLI_ICON_CROSS;
        *color = CLR_YELLOW;
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
    cli_outf("%s%s%s%s %s%s%s %s%s%s\n", cli_gutter(4), cli_c(color), icon,
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

/* Diff node states in a fetched snapshot against the last board state and
 * print every node whose state changed. Returns 0 while the DAG is still
 * active, 1 once a terminal state is observed (caller stops polling).
 * 0.1.17 R4-①: consumes the caller's snapshot instead of issuing its own
 * sched.dag_status RPC. */
int cli_dag_node_board_tick(cli_dag_board_t *b, const cli_dag_snapshot_t *snap)
{
    if (!b || !snap || !snap->root)
        return 0;

    const char *status = cli_dag_snapshot_state(snap);
    int terminal = cli_state_terminal(status);

    cJSON *nodes = cJSON_GetObjectItem(snap->root, "nodes");
    int nsz = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    if (nsz > CLI_DAG_BOARD_MAX_NODES)
        nsz = CLI_DAG_BOARD_MAX_NODES;

    for (int i = 0; i < nsz; i++) {
        cJSON *nj = cJSON_GetArrayItem(nodes, i);
        if (!nj)
            continue;
        const char *id_s = json_str(nj, "id", "?");
        const char *st_s = json_str(nj, "status", "pending");
        const char *goal_s = json_str(nj, "goal", "");

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

    return terminal ? 1 : 0;
}
